#include "SDL.h"
#include <stdio.h>
#include "SDL_opengl.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#define STBTT_assert
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
//#include <cstring>

#include "defines.h"
#include "render.h"

#include "ui_control.h"

#define UI_FILE_ID ((u64)1)

#define TRANSFORM_IDENTITY {{}, 0.0f, 1.0f}

f32 WorldCameraHeight = 2.0f;  
//rect DebugRect = MakeRect(100.0f, 300.0f, 150.0f, 100.0f);

enum mode
{
    Mode_Title,
    Mode_Game,
    Mode_Game_Over,
    Mode_Editor
};

enum entity_type{
    Entity_Type_Player,
    Entity_Type_Boss,
    Entity_Type_Fly,    
    Entity_Type_Bullet,
    Entity_Type_Bomb,
    Entity_Type_Powerup,    
    
    Entity_Type_Count
};

enum sfx {
    Sfx_Death,
    Sfx_Bomb,
    Sfx_Shoot,
    
    Sfx_Count
};

//input
struct key {
    bool IsPressed;
    bool HasChanged;
};

struct input {
    union {
        key Keys[11];
        
        struct {
            key UpKey;
            key DownKey;
            key LeftKey;
            key RightKey;
            key FireKey;
            key BombKey;
            key EnterKey;
            key SlowMovementKey;
            key ToggleEditModeKey;
            
            key LeftMouseKey;
            key RightMouseKey;
        };
    };
    
    vec2 MousePos;
};

bool WasPressed(key Key) {
    return (Key.IsPressed && Key.HasChanged);
}

bool WasReleased(key Key) {
    return (!Key.IsPressed && Key.HasChanged);
}

const f32 Fly_Min_Fire_Interval = 0.8f;
const f32 Fly_Max_Fire_Interval = 1.7f;

const f32 Powerup_Collect_Radius = 0.1f;
const f32 Powerup_Magnet_Speed   = 0.5f;

struct entity {
    transform XForm;
    f32 CollisionRadius;
    s32 Hp, MaxHp;
    
    entity_type Type;
    bool MarkedForDeletion;
    u32 CollisionTypeMask;
    f32 BlinkTime, BlinkDuration;
    vec2 RelativeDrawCenter;
    
    union {
        struct {
            f32 FlipCountdown, FlipInterval;
            vec2 Velocity;
            f32 FireCountdown;
        } fly;
        
        struct {
            u32 Power;
            u32 Bombs;            
        } player;
        
        struct {
            u32 Damage;   
        } bullet;
    };
    
};

#define template_array_name entity_buffer
#define template_array_data_type entity
#define template_array_is_buffer
#include "template_array.h"

entity* NextEntity(entity_buffer *Buffer){
    auto Result = Push(Buffer);
    if (Result != NULL) {
        *Result = {};
    }
    
    return Result;
}

struct collision {
    entity *Entities[2];
};

struct entity_spawn_info {
    entity Blueprint;
    f32 SpawnTime;
    bool WasNotSpawned;
};

#define template_array_name      entity_spawn_infos
#define template_array_data_type entity_spawn_info
#define template_array_is_buffer 
#define template_array_static_count 256
#include "template_array.h"

struct level {
    entity_spawn_infos SpawnInfos;
    f32 Time;
    f32 Duration;
    f32 WorldHeight;
    f32 LayersWorldUnitsPerPixels[2];
};

// TODO: move all assets into game_state
struct game_state {
    entity_buffer Entities;
    level Level;
    camera Camera;
    mode Mode;    
    bool DeleteButtonSelected;
    texture BombTexture;
};

f32 randZeroToOne(){
    return ((rand()  %  RAND_MAX) / (f32) RAND_MAX);
}

f32 randMinusOneToOne(){
    return (randZeroToOne() * 2 - 1.0f);
}


Mix_Chunk *loadChunk(int sfxEnum) {
    switch (sfxEnum) {
        case Sfx_Death: return Mix_LoadWAV("data/Gravity Sound/Low Health.wav");
        
        case Sfx_Bomb: return Mix_LoadWAV("data/Gravity Sound/Level Up 4.wav");
        
        case Sfx_Shoot: return Mix_LoadWAV("data/Gravity Sound/Dropping Item 6.wav");
        
        default: return NULL;
    }
}

level LoadLevel(char *FileName) {
    SDL_RWops* File = SDL_RWFromFile(FileName, "rb");
    
    if (File == NULL) {
        level Default = {};
        Default.Duration = 60.0f;
        
        return (Default);
    }
    level Result;
    
    size_t ReadObjectCount = SDL_RWread(File, &Result, sizeof(Result), 1);
    
    assert(ReadObjectCount == 1);
    SDL_RWclose(File);
    return Result;
}

void SaveLevel(char *FileName, level Level) {
    SDL_RWops* File = SDL_RWFromFile(FileName, "wb");
    assert(File);
    
    size_t WriteObjectCount = SDL_RWwrite(File, &Level, sizeof(Level), 1);
    
    assert(WriteObjectCount == 1);
    SDL_RWclose(File);
}

struct config {
    s32 Width, Height;
    s32 BgmVolume, SfxVolume;

};

config LoadConfig(char *FileName) {
    SDL_RWops* File = SDL_RWFromFile(FileName, "rb");
    
    if (File == NULL)
        return {640, 480,   //window size
                30, 30      //volume
        };
    
    config Result;
    
    size_t ReadObjectCount = SDL_RWread(File, &Result, sizeof(Result), 1);
    
    assert(ReadObjectCount == 1);
    SDL_RWclose(File);
    return Result;
}

void SaveConfig(char *FileName, SDL_Window *Window) {
    SDL_RWops* File = SDL_RWFromFile(FileName, "wb");
    assert(File);
    
    config Config;
    
    SDL_GetWindowSize(Window, &Config.Width, &Config.Height);
    Config.BgmVolume = Mix_VolumeMusic(-1);
    Config.SfxVolume = Mix_Volume(0, -1);

    size_t WriteObjectCount = SDL_RWwrite(File, &Config, sizeof(Config), 1);
    
    assert(WriteObjectCount == 1);
    SDL_RWclose(File);
}

f32 LookAtRotation(vec2 Eye, vec2 Target) {
    f32 Alpha = acos(dot(vec2{0, 1}, normalizeOrZero(Target - Eye)));
    
    if (Eye.X <= Target.X)
        Alpha = -Alpha;
    
    return Alpha;
}

entity MakeChicken(vec2 WorldPositionOffset) {
    entity Result = {};
    
    Result.XForm.Rotation = 0.0f;
    Result.XForm.Scale = 0.09f;
    Result.CollisionRadius = Result.XForm.Scale * 0.65;
    Result.MaxHp = 10;
    Result.Hp = Result.MaxHp;
    Result.Type = Entity_Type_Fly;
    Result.CollisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    Result.fly.FlipInterval = 1.5f;
    Result.fly.FlipCountdown = Result.fly.FlipInterval * randZeroToOne();
    Result.fly.FireCountdown = 0.25f;
    Result.fly.Velocity = vec2{1.0f, 0.1f};    
    Result.XForm.Pos = vec2{Result.fly.Velocity.X * Result.fly.FlipInterval * -0.5f, randZeroToOne()} + Result.fly.Velocity * (Result.fly.FlipInterval - Result.fly.FlipCountdown) + WorldPositionOffset;   
    Result.RelativeDrawCenter = vec2 {0.5f, 0.44f};
    Result.BlinkDuration = 0.1f;
    Result.BlinkTime = 0.0f;
    
    return Result;
}

void DrawEntity(game_state *State, textures Textures, entity *Entity){
    switch (Entity->Type) {
        
        case Entity_Type_Player: {
            DrawTexturedQuad(State->Camera, Entity->XForm, Textures.PlayerTexture, White_Color, Entity->RelativeDrawCenter);
        } break; 
        
        case Entity_Type_Bullet: {
            if (Entity->bullet.Damage == 1) {
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.BulletTexture, White_Color, Entity->RelativeDrawCenter);
            } 
            else if (Entity->bullet.Damage == 2){
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.BulletPoweredUpTexture, White_Color, Entity->RelativeDrawCenter);  
            }
            else {
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.BulletMaxPoweredUpTexture, White_Color, Entity->RelativeDrawCenter);
            }
        } break;
        
        case Entity_Type_Boss: {
            if (Entity->BlinkTime <= 0) {
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.BossTexture, White_Color, Entity->RelativeDrawCenter); 
            } 
            else {
                color BlinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, Entity->BlinkTime / Entity->BlinkDuration); 
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.BossTexture, BlinkColor, Entity->RelativeDrawCenter);          
            }   
        } break;
        
        case Entity_Type_Fly: {
            if (Entity->BlinkTime <= 0) {
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.FlyTexture, White_Color, Entity->RelativeDrawCenter); 
            } 
            else {
                color BlinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, Entity->BlinkTime / Entity->BlinkDuration); 
                DrawTexturedQuad(State->Camera, Entity->XForm, Textures.FlyTexture, BlinkColor, Entity->RelativeDrawCenter);          
            }   
        } break;
        
        case Entity_Type_Bomb: {
            DrawTexturedQuad(State->Camera, Entity->XForm, State->BombTexture, color{randZeroToOne(), randZeroToOne(), randZeroToOne(), 1.0f}, Entity->RelativeDrawCenter);    
        } break;
        
        case Entity_Type_Powerup: {
            DrawTexturedQuad(State->Camera, Entity->XForm, Textures.PowerupTexture, White_Color, Entity->RelativeDrawCenter);
        } break;
        
        default: {
            DrawTexturedQuad(State->Camera, Entity->XForm, Textures.FlyTexture, White_Color, Entity->RelativeDrawCenter);     
        }
    }    
}

void DrawAllEntities(game_state *State, textures Textures) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 0.1f);    
    
    for (u32 i = 0; i < State->Entities.Count; i++) {
        auto Entity = State->Entities.Base + i;
        DrawEntity(State, Textures, Entity);
    }
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void initGame (game_state *State, entity **Player) {
    State->Entities.Count = 0;
    for (u32 i = 0; i < State->Level.SpawnInfos.Count; i++) {
        State->Level.SpawnInfos[i].WasNotSpawned = true;
    }
    
    State->Level.Time = 0.0f;
    State->Camera.WorldPosition = { 0.0f, WorldCameraHeight * 0.5f };
    
    *Player = NextEntity(&State->Entities); 
    (*Player)->XForm = TRANSFORM_IDENTITY;
    (*Player)->XForm.Scale = 0.1f;
    (*Player)->CollisionRadius = (*Player)->XForm.Scale * 0.5;
    (*Player)->MaxHp = 1;
    (*Player)->Hp = (*Player)->MaxHp;
    (*Player)->player.Power = 0;
    (*Player)->player.Bombs = 3;
    (*Player)->Type = Entity_Type_Player;
    (*Player)->CollisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Powerup);
    (*Player)->RelativeDrawCenter = vec2 {0.5f, 0.4f};  
};

void UpdateTitle(game_state *State, ui_context *Ui, ui_control *UiControl, f32 DeltaSeconds, input GameInput, textures Textures, bool *DoContinue){
    char *Items[] = {
        "New game",
        "Load Game",
        "Settings",                 
        "Quit Game"
    };
    auto Cursor = UiBeginText(Ui, Ui->CurrentFont, Ui->Width * 0.5f, Ui->Height * 0.7f, true, color{0.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
    
    for (u32 i = 0; i < ARRAY_COUNT(Items); i++) {
        //uiRect(&ui, Cursor.CurrentX - 2, Cursor.CurrentY - 2, 5, 5, color{0.5f, 1.0f, 0.2f, 1.0f}, true);
        
        rect Rect;
        {
            auto DummyCursor = Cursor;
            DummyCursor.DoRender = false;
            Rect = UiWrite(&DummyCursor, Items[i]);
        }
        
        f32 Border = Cursor.Scale * 15;
        auto MinRect = MakeRectWithSize(Rect.Left, Cursor.CurrentY - Ui->CurrentFont->BaselineBottomOffset * Cursor.Scale - Border, 0, Ui->CurrentFont->MaxGlyphHeight * Cursor.Scale + 2 * Border);
        Rect = Merge(Rect, MinRect);
        
        Rect.Left  -= Border;
        Rect.Right += Border;
        
        auto Offset = (Rect.Right - Rect.Left) * 0.5f;
        Rect.Left  -= Offset;
        Rect.Right -= Offset;
        
        u64 Id = UI_ID(i);
        
        f32 CursorYOffset = 0;
        
        if (UiControl->ActiveId == Id) {
            Cursor.Color = color{ 1.0f, 0.5f, 0.0f, 1.0f };
        }
        else if (UiControl->HotId == Id) {
            Cursor.Color = color{ 0.95f, 0.95f, 0.0f, 1.0f };
        }
        else {
            Cursor.Color = color{ 1.0f, 1.0f, 1.0f, 1.0f };            
            Rect.Top += Cursor.Scale * 4;
            CursorYOffset = Cursor.Scale * 4;
        }
        
        if (UiButton(UiControl, Id, Rect)) {
            switch (i) {
                
                case 0: { State->Mode = Mode_Game;
                } break;
                
                case 3: { *DoContinue = false;
                } break;
                default: printf("you selected %s\n", Items[i]);
            }
        }
        
        if ((UiControl->ActiveId == Id) || (UiControl->HotId == Id))
            UiTexturedRect(Ui, Textures.HotButtonTexture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, 0, 0, Textures.HotButtonTexture.Width, Textures.HotButtonTexture.Height, White_Color);
        else
            UiTexturedRect(Ui, Textures.IdleButtonTexture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, 0, 0, Textures.IdleButtonTexture.Width, Textures.IdleButtonTexture.Height, White_Color);
        
        Cursor.CurrentX -= Offset;
        Cursor.CurrentY += CursorYOffset;
        UiBegin();
        UiWrite(&Cursor, "%s\n", Items[i]);
        UiEnd();
        Cursor.CurrentY -= 20 * Cursor.Scale + 2 * Border + CursorYOffset;
    }
}

void UpdateEditor(game_state *State, ui_context *Ui, ui_control *UiControl, f32 DeltaSeconds, input GameInput, textures Textures) {
#if 0
    if (GameInput.UpKey.IsPressed) {
        State->Level.Time += 3 * DeltaSeconds;
        State->Camera.WorldPosition.y += 3 * DeltaSeconds; 
    }
    
    if (GameInput.DownKey.IsPressed) {
        State->Level.Time -= 3 * DeltaSeconds;
        State->Camera.WorldPosition.y -= 3 * DeltaSeconds;
        
        State->Camera.WorldPosition.y = MAX(State->Camera.WorldPosition.y, WorldCameraHeight * 0.5f);
    }
#endif
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    UiBegin();
    
    if (State->Level.SpawnInfos.Count < State->Level.SpawnInfos.Capacity) {    
        rect FlyBlueprintRect = MakeRectWithSize(20, Ui->Height - 80, 60, 60);
        UiTexturedRect(Ui, Textures.FlyTexture, FlyBlueprintRect, MakeRectWithSize(0, 0, Textures.FlyTexture.Width, Textures.FlyTexture.Height));  
        
        if (UiButton(UiControl, UI_ID0, FlyBlueprintRect)) {
            entity_spawn_info *Info = Push(&State->Level.SpawnInfos);
            Info->WasNotSpawned = true;
            Info->SpawnTime = State->Level.Time;
            Info->Blueprint = MakeChicken(State->Camera.WorldPosition);
        }
    }
    rect DeleteRect = MakeRectWithSize(20, 80, 60, 60);
    
    if (UiButton(UiControl, UI_ID0, DeleteRect)) {
        State->DeleteButtonSelected = !State->DeleteButtonSelected;
    }
    
    if (State->DeleteButtonSelected) {
        UiTexturedRect(Ui, Textures.DeleteButtonTexture, DeleteRect, MakeRectWithSize(0, 0, Textures.DeleteButtonTexture.Width, Textures.DeleteButtonTexture.Height), color{1.0f, 0.2f, 0.2f, 1.0f});
    }
    else {
        UiTexturedRect(Ui, Textures.DeleteButtonTexture, DeleteRect, MakeRectWithSize(0, 0, Textures.DeleteButtonTexture.Width, Textures.DeleteButtonTexture.Height));
    }
    
    
    f32 TimeLineWidth = 100;
    rect TimeLineRect = MakeRect(Ui->Width - 10 - TimeLineWidth, 50, Ui->Width - 10, Ui->Height - 50);
    
    color TimeLineColor = color{ 1, 1, 0, 1};
    
    UiRect(Ui, TimeLineRect, TimeLineColor, false);
    
    auto Cursor = UiBeginText(Ui, Ui->CurrentFont, TimeLineRect.Left, TimeLineRect.Bottom, true, TimeLineColor);
    
    s32 Y = (State->Level.Time / State->Level.Duration) * (TimeLineRect.Top - TimeLineRect.Bottom) + TimeLineRect.Bottom;
    vec2 Delta;
    if (UiDragable(UiControl, UI_ID0, MakeRect(TimeLineRect.Left - 20, Y - 5, TimeLineRect.Right + 10, Y + 5), &Delta)) {
        State->Level.Time += (State->Level.Duration * Delta.Y) / (TimeLineRect.Top - TimeLineRect.Bottom);
        
        Y += Delta.Y;
        if ((Y) > TimeLineRect.Top) {
            Y = TimeLineRect.Top;
            State->Level.Time = State->Level.Duration;
        } else if ((Y) < TimeLineRect.Bottom) {
            Y = TimeLineRect.Bottom;
            State->Level.Time = 0;
        } 
    }
    
    UiLine(Ui, TimeLineRect.Left - 20, Y, TimeLineRect.Right + 10, Y, color{1, 0.7f, 0, 1});
    
    UiAlignedWrite(Cursor, { 1.0f, 1.0f },"Time: %f", State->Level.Time);
    
    u32 SpawnIndex = 0;
    
    
    while (SpawnIndex < State->Level.SpawnInfos.Count) {
        auto Info = State->Level.SpawnInfos.Base + SpawnIndex;
        transform CollisionTransform = Info->Blueprint.XForm;
        CollisionTransform.Scale = 2 * Info->Blueprint.CollisionRadius;
        DrawCircle(State->Camera, CollisionTransform, color{0.3f, 0.3f, 0.0f, 1.0f}, false, 16, -0.5f);
        
        glEnable(GL_TEXTURE_2D);
        DrawEntity(State, Textures, &Info->Blueprint);
        glDisable(GL_TEXTURE_2D);
        // collision center
        auto CanvasPoint = WorldToCanvasPoint(State->Camera, Info->Blueprint.XForm.Pos);
        auto UiPoint = CanvasToUiPoint(Ui, CanvasPoint);
        
        auto CollisionBorderWorldPoint = vec2 {Info->Blueprint.XForm.Pos.X + Info->Blueprint.CollisionRadius, Info->Blueprint.XForm.Pos.Y}; 
        auto CollisionBorderCanvasPoint = WorldToCanvasPoint(State->Camera, CollisionBorderWorldPoint);
        auto CollisionBorderUiPoint = CanvasToUiPoint(Ui, CollisionBorderCanvasPoint);
        
        f32 UiRadius = CollisionBorderUiPoint.X - UiPoint.X;
        
        rect Rect = MakeRectWithSize(UiPoint.X - UiRadius, UiPoint.Y - UiRadius, 2 * UiRadius, 2 * UiRadius);
        
        u64 Id = UI_ID(SpawnIndex);
        color Color;
        if (UiControl->HotId == Id)
        {
            Color = { 0.1f, 0.2f, 0.8f, 1.0f };
        }
        else
        {
            Color = { 0.6f, 0.23f, 0.6234f, 1.0f };
        }
        
        UiRect(Ui, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, Color, false);
        
        if(State->DeleteButtonSelected) {
            if (UiButton(UiControl, Id, Rect)) 
            {
                State->Level.SpawnInfos[SpawnIndex] = State->Level.SpawnInfos[State->Level.SpawnInfos.Count - 1];
                Pop(&State->Level.SpawnInfos);  
            }
        }
        else {
            vec2 DeltaPosition;
            if (UiDragable(UiControl, Id, Rect, &DeltaPosition))
            {
                auto Cursor = UiBeginText(Ui, Ui->CurrentFont, Ui->Width * 0.5f, Ui->Height * 0.5f);
                UiWrite(&Cursor, "center: %f, %f", Info->Blueprint.XForm.Pos.X, Info->Blueprint.XForm.Pos.Y);
                
                UiPoint = UiPoint + DeltaPosition;
                auto NewCenterCanvasPoint = UiToCanvasPoint(Ui, UiPoint);
                Info->Blueprint.XForm.Pos = CanvasToWorldPoint(State->Camera, NewCenterCanvasPoint);
            }
        }
        
        SpawnIndex++;
    }
    
    UiEnd();
}

void UpdateGameOver(game_state *State, input GameInput, ui_context Ui,f32 DeltaSeconds, entity *Player,  Mix_Music *bgm, textures Textures){
    Player->XForm.Rotation += 2 * PI * DeltaSeconds;
    
    if (WasPressed(GameInput.EnterKey)) {
        initGame(State, &Player);
        Mix_FadeInMusic(bgm, -1, 500);
        State->Mode = Mode_Game;        
    }
    
    auto Cursor = UiBeginText(&Ui, Ui.CurrentFont, Ui.Width / 2, Ui.Height / 2, true, color{1.0f, 0.0f, 0.0f, 1.0f}, 5.0f);
    UiWrite(&Cursor, 
            "Game Over");
    
    Cursor.Color = White_Color;
    Cursor.Scale = 1.0f;
    UiWrite(&Cursor, "\n"
            "press ");
    
    Cursor.Color = color {0.0f, 1.0f, 0.0f, 1.0f};
    UiWrite(&Cursor, "Enter ");
    
    Cursor.Color = White_Color;
    UiWrite(&Cursor, "to continue");
    
    DrawAllEntities(State, Textures);                            
}

void UpdateGame(game_state *State, input GameInput, ui_context *Ui, ui_control UiControl, entity *Player, f32 DeltaSeconds, f32 *BulletSpawnCooldown, f32 *ChickenSpawnCooldown, Mix_Chunk *SfxBomb, f32 WorldWidth, textures Textures, histogram FrameRateHistogram){    
    
    //State->Camera.WorldPosition.y += DeltaSeconds;
    
    auto Entities = &State->Entities;
    entity *Boss = NULL;
    
    //same as State.inEditMode ^= WasPressed(...)
    
    State->Level.Time += DeltaSeconds;
    State->Level.Time = MIN(State->Level.Time, State->Level.Duration);
    
    for (u32 i = 0; i <Entities->Count; i++) {
        if (Entities->Base[i].Type == Entity_Type_Boss) {
            Boss = Entities->Base + i;
            break;
        } 
    }
    
    for (u32 SpawnIndex = 0; SpawnIndex < State->Level.SpawnInfos.Count; SpawnIndex++) {
        auto Info = State->Level.SpawnInfos.Base + SpawnIndex;
        if (Info->WasNotSpawned && (Info->SpawnTime <= State->Level.Time))
        {
            auto Entity = NextEntity(Entities);
            
            if (Entity != NULL) {
                *Entity = Info->Blueprint;
                Info->WasNotSpawned = false;
            }
        }  
    }      
    
    //bullet movement        
    if (*BulletSpawnCooldown > 0) *BulletSpawnCooldown -= DeltaSeconds;
    
    vec2 Direction = {};
    f32 Speed = 1.0f;
    
    collision Collisions[1024];
    u32 CollisionCount = 0;
    
    for(u32 i = 0; i < Entities->Count; i++) {
        bool DoBreak = false;
        
        for (u32 j = i + 1; j < Entities->Count; j++){
            
            if (!(Entities->Base[i].CollisionTypeMask & FLAG(Entities->Base[j].Type)))
                continue;
            
            if (!(Entities->Base[j].CollisionTypeMask & FLAG(Entities->Base[i].Type)))
                continue;
            
            if (areIntersecting(circle{Entities->Base[i].XForm.Pos, Entities->Base[i].CollisionRadius}, circle{Entities->Base[j].XForm.Pos, Entities->Base[j].CollisionRadius})) 
            {
                if (CollisionCount >= ARRAY_COUNT(Collisions)) {
                    DoBreak = true;
                    break;
                }
                
                auto newCollision = Collisions + (CollisionCount++);               
                
                if (Entities->Base[i].Type < Entities->Base[j].Type) {
                    newCollision->Entities[0] = (Entities->Base) + i;
                    newCollision->Entities[1] = (Entities->Base) + j;
                }
                else {
                    newCollision->Entities[1] = (Entities->Base) + i;
                    newCollision->Entities[0] = (Entities->Base) + j;
                }
            }
        }
        
        if(DoBreak) {
            break;
        }
    }
    
    for (u32 i = 0; i < CollisionCount; i++) {
        auto Current = Collisions + i;
        
        switch (FLAG(Current->Entities[0]->Type) | FLAG(Current->Entities[1]->Type)) {
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Powerup)): {
                auto Player = Current->Entities[0];
                auto Powerup = Current->Entities[1];
                assert((Player->Type == Entity_Type_Player) && (Powerup->Type == Entity_Type_Powerup));
                
                
                auto Distance = Player->XForm.Pos - Powerup->XForm.Pos;
                
                if (lengthSquared(Distance) <= Powerup_Collect_Radius * Powerup_Collect_Radius) {
                    Powerup->MarkedForDeletion = true;
                    Player->player.Power++;
                }
                else {
                    Powerup->XForm.Pos = Powerup->XForm.Pos + normalizeOrZero(Distance) * (Powerup_Magnet_Speed * DeltaSeconds);
                }                
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly)): {
                auto Fly = Current->Entities[0];
                auto Bullet = Current->Entities[1];
                
                assert((Fly->Type == Entity_Type_Fly) && (Bullet->Type == Entity_Type_Bullet));
                
                Bullet->MarkedForDeletion = true;
                Fly->Hp -= Bullet->bullet.Damage;  
                Fly->BlinkTime = Fly->BlinkDuration;
                
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Boss)): {
                auto Boss = Current->Entities[0];
                auto Bullet = Current->Entities[1];
                
                assert((Boss->Type == Entity_Type_Boss) && (Bullet->Type == Entity_Type_Bullet));
                
                Bullet->MarkedForDeletion = true;
                Boss->Hp -= Bullet->bullet.Damage;
                Boss->BlinkTime = Boss->BlinkDuration;
                
            } break;   
            
            case (FLAG(Entity_Type_Bomb) | FLAG(Entity_Type_Bullet)): {
                auto Bullet = Current->Entities[0];
                auto Bomb = Current->Entities[1];
                
                assert((Bomb->Type == Entity_Type_Bomb) && (Bullet->Type == Entity_Type_Bullet));
                
                Bullet->MarkedForDeletion = true;               
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Player)): 
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Boss)): 
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Fly)): {
                
                auto Player = Current->Entities[0];
                //auto Enemy = Current->Entities[1];
                
                assert(Player->Type == Entity_Type_Player);
                
                State->Mode = Mode_Game_Over;
                Mix_FadeOutMusic(500);
                Mix_HaltChannel(-1);
                Mix_PlayChannel(0, loadChunk(Sfx_Death), 0);
                return;
                
            } break;  
        }
    }
    
    for(u32 i = 0; i < Entities->Count; i++) {
        
        auto E = Entities->Base + i;
        
        switch(E->Type) {
            case Entity_Type_Bullet: {
                
                vec2 dir = normalizeOrZero(TransformPoint(E->XForm, {0, 1}) - E->XForm.Pos);
                
                //E->XForm.Pos.y += Speed * DeltaSeconds;
                f32 Speed = 1.0f; 
                E->XForm.Pos = E->XForm.Pos + dir * (Speed * DeltaSeconds);
                
                if (ABS(E->XForm.Pos.Y - State->Camera.WorldPosition.Y) >= WorldCameraHeight) {
                    E->MarkedForDeletion = true;
                }                               
            } break;
            
            case Entity_Type_Bomb: {
                E->CollisionRadius += DeltaSeconds * 3.0f;
                E->XForm.Scale = E->CollisionRadius * 3.0f / (State->BombTexture.Height * Default_World_Units_Per_Texel);
                
                if (E->CollisionRadius > 6.0f) {
                    E->MarkedForDeletion = true;
                }
            } break;
            
            case Entity_Type_Fly:{
                if (E->Hp <= 0) {
                    E->MarkedForDeletion = true;
                    
                    entity *Powerup = NextEntity(Entities);
                    
                    if (Powerup)
                    {
                        Powerup->XForm.Pos = E->XForm.Pos;  
                        Powerup->XForm.Rotation = 0.0f;
                        Powerup->XForm.Scale = 0.02f;
                        Powerup->CollisionRadius = Powerup_Collect_Radius * 3;
                        Powerup->Type = Entity_Type_Powerup;
                        Powerup->CollisionTypeMask = FLAG(Entity_Type_Player); 
                        Powerup->RelativeDrawCenter = vec2 {0.5f, 0.5f};
                    }
                } 
                
                f32 t = DeltaSeconds;
                
                while (E->fly.FlipCountdown < t) {
                    E->XForm.Pos = E->XForm.Pos + E->fly.Velocity * E->fly.FlipCountdown;
                    t -= E->fly.FlipCountdown;
                    E->fly.Velocity = -(E->fly.Velocity);
                    E->fly.FlipCountdown += E->fly.FlipInterval;
                }
                
                E->fly.FlipCountdown -= DeltaSeconds;
                E->XForm.Pos = E->XForm.Pos + E->fly.Velocity * t;
                
                E->fly.FireCountdown -= DeltaSeconds;
                if (E->fly.FireCountdown <= 0)
                {
                    E->fly.FireCountdown += lerp(Fly_Min_Fire_Interval, Fly_Max_Fire_Interval, randZeroToOne());
                    
                    auto bullet = NextEntity(Entities);
                    if (bullet)
                    {
                        bullet->Type = Entity_Type_Bullet;
                        bullet->CollisionTypeMask |= FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bomb);
                        bullet->XForm.Pos = E->XForm.Pos;  
                        bullet->XForm.Rotation = lerp(PI * 0.75f, PI * 1.25f, randZeroToOne()); // points downwards
                        bullet->XForm.Scale = 0.2f;
                        bullet->CollisionRadius = bullet->XForm.Scale * 0.2;
                        bullet->RelativeDrawCenter = vec2 {0.5f, 0.5f};
                    }
                }
                
                if (E->BlinkTime > 0) E->BlinkTime -= DeltaSeconds;
            } break;
            
            case Entity_Type_Boss:{
                if (E->BlinkTime > 0) E->BlinkTime -= DeltaSeconds;
                if (State->Camera.WorldPosition.Y + WorldCameraHeight * 0.5f < E->XForm.Pos.Y + E->CollisionRadius * 1.5f) {
                    E->XForm.Pos.Y -= DeltaSeconds;
                }
            } break;
            
            case Entity_Type_Powerup: {                
                f32 fallSpeed = 0.8f; 
                E->XForm.Pos = E->XForm.Pos + vec2{0, -1} * (fallSpeed * DeltaSeconds);          
            } break;
        }
    }
    
    u32 i = 0;
    while (i < Entities->Count) {
        if (Entities->Base[i].MarkedForDeletion) {
            Entities->Base[i] = Entities->Base[(Entities->Count) - 1];
            Pop(Entities);
        }
        else{ 
            i++;
        }
    }    
    
    //player movement
    if (GameInput.LeftKey.IsPressed) {
        Direction.X -= 1;
        //player->XForm.Rotation -= 0.5f * PI * DeltaSeconds;
    }
    
    if (GameInput.RightKey.IsPressed) {
        Direction.X += 1; 
        //player->XForm.Rotation += 0.5f * PI * DeltaSeconds;
    }
    
    if (GameInput.UpKey.IsPressed) {
        Direction.Y += 1; 
    }
    
    if (GameInput.DownKey.IsPressed) {
        Direction.Y -= 1; 
    }
    
    if (GameInput.SlowMovementKey.IsPressed) {
        Speed = Speed * 0.5f;
    }
    
    Direction = normalizeOrZero(Direction);            
    Player->XForm.Pos = Player->XForm.Pos + Direction * (Speed * DeltaSeconds);
    
    Player->XForm.Pos.Y = CLAMP(Player->XForm.Pos.Y , -1.0f + Player->CollisionRadius + State->Camera.WorldPosition.Y, 1.0f - Player->CollisionRadius + State->Camera.WorldPosition.Y);
    
    // WorldWidth = windowWidth / windowHeight * worldHeight 
    // worldHeight = 2 (from -1 to 1)               
    Player->XForm.Pos.X = CLAMP(Player->XForm.Pos.X, -WorldWidth * 0.5f + Player->CollisionRadius, WorldWidth * 0.5f - Player->CollisionRadius);
    {
#if 0
        f32 Rotation = asin((enemies[0].XForm.Pos.x - player->XForm.Pos.x) / length(enemies[0].XForm.Pos - player->XForm.Pos));
        if (enemies[0].XForm.Pos.y < player->XForm.Pos.y) {
            Rotation = Rotation - PI;
        }
        else {
            Rotation = 2 * PI - Rotation;
        }
        
        player->XForm.Rotation = Rotation;
        
#else
        if (Boss)
            Player->XForm.Rotation = LookAtRotation(Player->XForm.Pos, Boss->XForm.Pos);
        
        
#endif
    }
    {
        *ChickenSpawnCooldown -= DeltaSeconds;
        
        if(0){
            //if(*ChickenSpawnCooldown <= 0) {
            //unleash the chicken!
            entity *Chicken = NextEntity(Entities);
            
            if (Chicken != NULL) {               
                *Chicken = MakeChicken(State->Camera.WorldPosition);
            }        
            *ChickenSpawnCooldown += 1.0f;
        }
    }
    
    if(GameInput.FireKey.IsPressed) {
        if ((*BulletSpawnCooldown <= 0)) {
            entity *Bullet = NextEntity(Entities);
            
            if (Bullet != NULL) {
                Bullet->XForm.Pos = Player->XForm.Pos;
                Bullet->XForm.Scale = 0.2f;
                Bullet->XForm.Rotation = 0;
                Bullet->CollisionRadius = Bullet->XForm.Scale * 0.2;
                Bullet->Type = Entity_Type_Bullet;
                Bullet->CollisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly);
                Bullet->RelativeDrawCenter = vec2 {0.5f, 0.5f};
                
                Bullet->bullet.Damage = (Player->player.Power / 20) + 1;
                Bullet->bullet.Damage = MIN(Bullet->bullet.Damage, 3);
                
                *BulletSpawnCooldown += 0.05f;
                
                //                        Mix_PlayChannel(0, sfxShoot, 0);
            }
        }
    }
    
    if(WasPressed(GameInput.BombKey)) {                       
        if (Player->player.Bombs > 0) {
            
            entity *Bomb = NextEntity(Entities);
            
            if (Bomb != NULL) {
                Bomb->XForm.Pos = Player->XForm.Pos;
                Bomb->CollisionRadius = 0.1f;
                Bomb->XForm.Scale = Bomb->CollisionRadius * 3.0f / (State->BombTexture.Height * Default_World_Units_Per_Texel);
                Bomb->XForm.Rotation = 0;
                Bomb->Type = Entity_Type_Bomb;
                Bomb->CollisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Bullet);
                Bomb->RelativeDrawCenter = vec2 {0.5f, 0.5f};
                
                Player->player.Bombs--;
                
                Mix_PlayChannel(0, SfxBomb, 0);
            }
        }
    }
    
    
    // render
    
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 0.1f);
    
#if 0
    //background
    transform backgroundXForm = TRANSFORM_IDENTITY;
    f32 currentLevelYPosition = lerp(worldCameraHeight * 0.5f, State.Level.WorldHeight - worldCameraHeight * 0.5f, State.Level.Time / State.Level.Duration); 
    drawTexturedQuad(State.Camera, backgroundXForm, levelLayer1, White_Color, vec2 {0.5f,  currentLevelYPosition / State.Level.WorldHeight}, State.Level.LayersWorldUnitsPerPixels[0], 0.9f);
    drawTexturedQuad(State.Camera, backgroundXForm, levelLayer2, White_Color, vec2 {0.5f,  currentLevelYPosition / State.Level.WorldHeight}, State.Level.LayersWorldUnitsPerPixels[1], 0.8f);
#endif
    
    DrawAllEntities(State, Textures);
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    //debug framerate, hitbox and player/boss normalized x, y coordinates
    
#define DEBUG_UI
#ifdef DEBUG_UI
    DrawHistogram(FrameRateHistogram);
    
    for (u32 i = 0; i < Entities->Count; i++) {
        transform collisionTransform = (*Entities)[i].XForm;
        collisionTransform.Scale = 2 * Entities->Base[i].CollisionRadius;
        DrawCircle(State->Camera, collisionTransform, color{0.3f, 0.3f, 0.0f, 1.0f}, false);
        DrawLine(State->Camera, collisionTransform, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        DrawLine(State->Camera, collisionTransform, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
    }        
    
#endif //DEBUG_UI
    
    //GUI
    {
        UiBegin();
        
        //bomb count           
        for (int Bomb = 0; Bomb < Player->player.Bombs; Bomb++){
            UiTexturedRect(Ui, Textures.BombCountTexture, 20 + Bomb * (Textures.BombCountTexture.Width + 5), Ui->Height - 250, Textures.BombCountTexture.Width, Textures.BombCountTexture.Height, 0, 0, Textures.BombCountTexture.Width, Textures.BombCountTexture.Height, White_Color);
        }
        
        //boss Hp
        if (Boss) {
            auto Cursor = UiBeginText(Ui, Ui->CurrentFont, 20, Ui->Height - 90);
            UiWrite(&Cursor, "BOSS ");
            UiWrite(&Cursor, "Hp: %i / %i", Boss->Hp, Boss->MaxHp);
            
            UiBar(Ui, 20, Ui->Height - 60, Ui->Width - 40, 40, Boss->Hp / (f32) Boss->MaxHp, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
        }
        
        //player Power
        UiBar(Ui, 20, Ui->Height - 200, 120,40, (Player->player.Power % 20) / 20.0f, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
        UiEnd();
    }
}
// gl functions

PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = NULL;

void APIENTRY
wostenGLDebugCallback(GLenum source, GLenum Type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void * user_param)
{
    char *severity_text = NULL;
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
        severity_text = "high";
        break;
        
        case GL_DEBUG_SEVERITY_MEDIUM:
        severity_text = "medium";
        break;
        
        case GL_DEBUG_SEVERITY_LOW:
        severity_text = "low";
        break;
        
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        severity_text = "info";
        return;
        
        default:
        severity_text = "unkown severity";
    }
    
    char *type_text = NULL;
    switch (Type)
    {
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_text = "depricated behavior";
        break;
        case GL_DEBUG_TYPE_ERROR:
        type_text = "error";
        break;
        case GL_DEBUG_TYPE_MARKER:
        type_text = "marker";
        break;
        case GL_DEBUG_TYPE_OTHER:
        type_text = "other";
        break;
        case GL_DEBUG_TYPE_PERFORMANCE:
        type_text = "performance";
        break;
        case GL_DEBUG_TYPE_POP_GROUP:	
        type_text = "pop group";
        break;
        case GL_DEBUG_TYPE_PORTABILITY:
        type_text = "portability";
        break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
        type_text = "push group";
        break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_text = "undefined behavior";
        break;
        default:
        type_text = "unkown Type";
    }
    
    printf("[gl %s prio %s | %u ]: %s\n", severity_text, type_text, id, message);
    
    // filter harmless errors
    switch (id) {
        // shader compilation failed
        case 2000:
        return;
    }
    
    assert(Type != GL_DEBUG_TYPE_ERROR);
}	

int main(int argc, char* argv[]) {
    srand (time(NULL));
    
    SDL_Window *Window;                                     // Declare a pointer
    
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);              // Initialize SDL2
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    // Create an application window with the following settings:
    
    config Config = LoadConfig("data/config.bin");
    
    Window = SDL_CreateWindow(
        "wosten",                          // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        Config.Width,                      // Width, in pixels
        Config.Height,                     // Height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
        | SDL_WINDOW_RESIZABLE
        );
    
    // Check that the window was successfully created
    if (Window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }
    
    // The window is open: could enter program loop here (see SDL_PollEvent())
    
    SDL_GLContext glContext = SDL_GL_CreateContext(Window);
    glDebugMessageCallback =(PFNGLDEBUGMESSAGECALLBACKPROC) SDL_GL_GetProcAddress("glDebugMessageCallback");
    
    if (glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // message will be generatet in function call scope
        glDebugMessageCallback(wostenGLDebugCallback, NULL);
    }    
    //sound init
    int MixInit = Mix_Init(MIX_INIT_MP3);
    if(MixInit&MIX_INIT_MP3 != MIX_INIT_MP3) {
        printf("Error initializing mix: %s \n", Mix_GetError());
    }
    
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 4096)) {
        printf("Error Mix_OpenAudio: %s \n", Mix_GetError());
    }
    Mix_Music *Bgm = Mix_LoadMUS("data/Gravity Sound/Gravity Sound - Rain Delay CC BY 4.0.mp3");
    if(!Bgm) {
        printf("Error loading music file: %s \n", Mix_GetError());
    } 
    else {    
        Mix_PlayMusic(Bgm, -1);
        Mix_VolumeMusic(Config.BgmVolume);
    }
    
    //sfx
    Mix_AllocateChannels(1);
    Mix_Volume(0, Config.SfxVolume);
    Mix_Chunk *SfxBomb = loadChunk(Sfx_Bomb);
    //Mix_Chunk *sfxShoot = loadChunk(Sfx_Shoot); 
    
    if(!SfxBomb) {
        printf("Error loading music file: %s \n", Mix_GetError());
    }
    
    histogram FrameRateHistogram = {};
    
    ui_context Ui = {};
    ui_control UiControl = {};
    
    bool DoContinue = true;
    
    u64 Ticks = SDL_GetPerformanceFrequency();
    u64 LastTime = SDL_GetPerformanceCounter();
    f32 ScaleAlpha = 0;
    
    textures Textures = {};
    Textures.LevelLayer1 = LoadTexture("data/level_1.png");
    Textures.LevelLayer2 = LoadTexture("data/level_1_layer_2.png");
    Textures.PlayerTexture = LoadTexture("data/Kenney/Animals/giraffe.png");
    Textures.BossTexture = LoadTexture("data/Kenney/Animals/parrot.png");
    Textures.FlyTexture = LoadTexture("data/Kenney/Animals/chicken.png");
    Textures.BulletTexture = LoadTexture("data/Kenney/Missiles/spaceMissiles_014.png");  
    Textures.BulletPoweredUpTexture = LoadTexture("data/Kenney/Missiles/spaceMissiles_001.png");
    Textures.BulletMaxPoweredUpTexture = LoadTexture("data/Kenney/Missiles/spaceMissiles_006.png");
    Textures.BombCountTexture = LoadTexture("data/Kenney/Missiles/spaceMissiles_021.png");
    Textures.PowerupTexture = LoadTexture("data/Kenney/Letter Tiles/letter_P.png");
    
    // UI
    Textures.IdleButtonTexture = LoadTexture("data/Kenney/PNG/blue_button02.png");
    Textures.HotButtonTexture = LoadTexture("data/Kenney/PNG/blue_button03.png");
    Textures.DeleteButtonTexture = LoadTexture("data/Kenney/PNG/grey_boxCross.png");
    
    SDL_RWops* Op = SDL_RWFromFile("C:/Windows/Fonts/Arial.ttf", "rb");
    s64 ByteCount = Op->size(Op);
    u8 *Data = new u8[ByteCount];   
    usize Ok = SDL_RWread(Op, Data, ByteCount, 1);
    assert (Ok == 1);
    
    stbtt_fontinfo StbFont;
    stbtt_InitFont(&StbFont, Data, stbtt_GetFontOffsetForIndex(Data,0));
    
    f32 Scale = stbtt_ScaleForPixelHeight(&StbFont, 48);
    s32 Ascent;
    stbtt_GetFontVMetrics(&StbFont, &Ascent,0,0);
    s32 Baseline = (s32) (Ascent*Scale);
    
    const s32 BitmapWidth = 512;
    u8 Bitmap[BitmapWidth * BitmapWidth] = {};
    s32 XOffset = 0;
    s32 YOffset = 0;
    s32 MaxHight = 0;
    font DefaultFont = {};
    
    //    while (text[ch]) 
    
    for (u32 i = ' '; i < 256; i++) {   
        glyph *FontGlyph = DefaultFont.Glyphs + i;
        FontGlyph->Code = i;
        
        s32 UnscaledXAdvance;  
        stbtt_GetCodepointHMetrics(&StbFont, FontGlyph->Code, &UnscaledXAdvance, &FontGlyph->DrawXOffset);
        FontGlyph->DrawXAdvance = UnscaledXAdvance * Scale;
        
        s32 X0, X1, Y0, Y1;
        stbtt_GetCodepointBitmapBox(&StbFont, FontGlyph->Code, Scale, Scale, &X0, &Y0, &X1, &Y1);
        FontGlyph->Width = X1 - X0;
        FontGlyph->Height = Y1 - Y0;
        FontGlyph->DrawXOffset = X0;
        // y0 is top corner, but its also negative ...
        // we draw from bottom left corner
        FontGlyph->DrawYOffset = -(Y0 + FontGlyph->Height);
        DefaultFont.BaselineTopOffset    = MIN(DefaultFont.BaselineTopOffset, FontGlyph->Height + FontGlyph->DrawYOffset);
        DefaultFont.BaselineBottomOffset = MAX(DefaultFont.BaselineBottomOffset, -FontGlyph->DrawYOffset);
        
        if ((XOffset + FontGlyph->Width) >= BitmapWidth) {
            XOffset = 0;
            YOffset += MaxHight + 1;
            MaxHight = 0;
        }
        assert(FontGlyph->Width <= BitmapWidth);
        
        stbtt_MakeCodepointBitmap(&StbFont, Bitmap + XOffset + YOffset * BitmapWidth, FontGlyph->Width, FontGlyph->Height, BitmapWidth, Scale, Scale, FontGlyph->Code);
        FontGlyph->X = XOffset;
        // we flip the texture so we need to change the y to the inverse
        FontGlyph->Y = BitmapWidth - YOffset - FontGlyph->Height;
        XOffset += FontGlyph->Width + 1;
        MaxHight = MAX(MaxHight, FontGlyph->Height);
        DefaultFont.MaxGlyphWidth = MAX(DefaultFont.MaxGlyphWidth, FontGlyph->Width);
        DefaultFont.MaxGlyphHeight = MAX(DefaultFont.MaxGlyphHeight, FontGlyph->Height);
    }
    
    DefaultFont.Texture = LoadTexture(Bitmap, BitmapWidth, BitmapWidth, 1, GL_NEAREST);    
    
    Ui.CurrentFont = &DefaultFont;
    
    f32 WorldHeightOverWidth = 3.0f / 4.0f; 
    f32 WorldWidth = WorldCameraHeight / WorldHeightOverWidth;
    
    entity _entitieEntries[100];
    game_state State = {};
    State.Mode = Mode_Game;
    State.Level.Duration = 30.0f;
    State.Level.Time = 0.0f;
    State.Level.LayersWorldUnitsPerPixels[0] = WorldWidth / Textures.LevelLayer1.Width;
    State.Level.LayersWorldUnitsPerPixels[1] = WorldWidth / Textures.LevelLayer2.Width;
    State.Level.WorldHeight = State.Level.LayersWorldUnitsPerPixels[0] * Textures.LevelLayer1.Height;  
    State.Entities = { ARRAY_WITH_COUNT(_entitieEntries) };
    State.BombTexture = LoadTexture("data/Kenney/particlePackCircle.png");
    State.DeleteButtonSelected = false;
    entity *Player;
    
    /*
    auto bossSpawnInfo  = Push(&State.Level.SpawnInfos);
    assert(bossSpawnInfo);
    bossSpawnInfo->WasNotSpawned = true;
    
    bossSpawnInfo->Blueprint.XForm.Pos = vec2{0.0f, 0.5f};  
    bossSpawnInfo->Blueprint.XForm.Rotation = 0.0f;
    bossSpawnInfo->Blueprint.CollisionRadius = 0.35f;
    bossSpawnInfo->Blueprint.XForm.Scale = bossSpawnInfo->Blueprint.CollisionRadius * 2.0f / (bossTexture.Height * Default_World_Units_Per_Texel);
    bossSpawnInfo->Blueprint.RelativeDrawCenter = vec2{0.5f, 0.5f};
    bossSpawnInfo->Blueprint.MaxHp = 500;
    bossSpawnInfo->Blueprint.Hp = bossSpawnInfo->Blueprint.MaxHp;
    bossSpawnInfo->Blueprint.Type = Entity_Type_Boss;
    bossSpawnInfo->Blueprint.CollisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    bossSpawnInfo->Blueprint.blinkDuration = 0.1f;
    bossSpawnInfo->Blueprint.BlinkTime = 0.0f;
    */
    
    State.Level = LoadLevel("data/levels/Level.bin");
    SaveLevel("data/levels/LevelBackup.bin", State.Level);
    initGame(&State, &Player);
    
    //timer init
    
    f32 BulletSpawnCooldown = 0;
	
    f32 ChickenSpawnCooldown = 5.0f;
    
    input GameInput = {};
    
    
    //game loop   
    while (DoContinue) {
        for (s32 i = 0; i < ARRAY_COUNT(GameInput.Keys); i++) {
            GameInput.Keys[i].HasChanged = false;
        }
        
        //window events 
        SDL_Event Event;
        while (SDL_PollEvent(&Event)) {
            switch (Event.type) {
                case SDL_QUIT:{
                    DoContinue = false;
                    SaveLevel("data/levels/Level.bin", State.Level);
                    
                    SaveConfig("data/config.bin", Window);
                    
                } break;
                
                case SDL_WINDOWEVENT:{
                    switch(Event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE:{
                            
                        }
                    }
                }
                
                // who needs this?
#if 0                
                // mouse move
                case SDL_MOUSEMOTION: {
                    GameInput.MousePos.x = event.motion.x;
                    GameInput.MousePos.y = event.motion.y;
                } break;
                
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT: {
                            GameInput.LeftMouseKey.IsPressed = (event.button.state == SDL_PRESSED);
                            GameInput.LeftMouseKey.HasChanged = true;
                        } break;
                        
                        case SDL_BUTTON_RIGHT: {
                            GameInput.RightMouseKey.IsPressed = (event.button.state == SDL_PRESSED);
                            GameInput.RightMouseKey.HasChanged = true;
                        } break;
                    }                
                } break;
#endif
                
                //keyboard input             
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    
                    if (Event.key.repeat > 0) 
                        break;
                    
                    switch (Event.key.keysym.scancode) {
                        case SDL_SCANCODE_A: {
                            GameInput.LeftKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.LeftKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_W: {
                            GameInput.UpKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.UpKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_S: {
                            GameInput.DownKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.DownKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_D: {
                            GameInput.RightKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.RightKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_K: {
                            GameInput.FireKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.FireKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_L: {
                            GameInput.BombKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.BombKey.HasChanged = true;    
                        } break;
                        
                        case SDL_SCANCODE_RETURN: {
                            GameInput.EnterKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.EnterKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_LSHIFT: {
                            GameInput.SlowMovementKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.SlowMovementKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_F1: {
                            GameInput.ToggleEditModeKey.IsPressed = (Event.key.type == SDL_KEYDOWN);
                            GameInput.ToggleEditModeKey.HasChanged = true;
                        } break;
                    }
                } break;
            }
        }
        
        {
            s32 WX, WY;
            SDL_GetWindowPosition(Window, &WX, &WY);
            
            s32 X, Y;
            auto ButtonStates = SDL_GetGlobalMouseState(&X, &Y);
            GameInput.MousePos.X = X - WX;
            GameInput.MousePos.Y = Y - WY;
            
            GameInput.LeftMouseKey.HasChanged = GameInput.LeftMouseKey.IsPressed != ((ButtonStates & SDL_BUTTON_LMASK) > 0);
            GameInput.LeftMouseKey.IsPressed = ((ButtonStates & SDL_BUTTON_LMASK) > 0);
            
            GameInput.RightMouseKey.HasChanged = GameInput.RightMouseKey.IsPressed != ((ButtonStates & SDL_BUTTON_LMASK) > 0);
            GameInput.RightMouseKey.IsPressed = ((ButtonStates & SDL_BUTTON_LMASK) > 0);
        }
        
        //time        
        u64 CurrentTime = SDL_GetPerformanceCounter();
        double DeltaSeconds = (double)(CurrentTime - LastTime) / (double)Ticks;
        LastTime = CurrentTime;
        
        // render begin
        s32 Width, Height;
        SDL_GetWindowSize(Window, &Width, &Height);
        
        State.Camera.HeightOverWidth = Height / (f32)Width;        
        f32 WorldPixelWidth = Height / WorldHeightOverWidth;
        
        Ui.Width = Width;
        Ui.Height = Height;
        
        UiFrameStart(&UiControl, vec2{ GameInput.MousePos.X, Ui.Height - GameInput.MousePos.Y }, WasPressed(GameInput.LeftMouseKey), WasReleased(GameInput.LeftMouseKey));
        
        glViewport(0, 0, Width, Height);
        glScissor(0, 0, Width, Height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_SCISSOR_TEST);
        //        glViewport((Width - worldPixelWidth) * 0.5f, 0, worldPixelWidth, Height);
        glScissor((Width - WorldPixelWidth) * 0.5f, 0, WorldPixelWidth, Height);
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glEnable(GL_DEPTH_TEST);
        
        FrameRateHistogram.Values[FrameRateHistogram.CurrentIndex] = 1 / DeltaSeconds;
        FrameRateHistogram.CurrentIndex++;
        if (FrameRateHistogram.CurrentIndex == ARRAY_COUNT(FrameRateHistogram.Values)) {
            FrameRateHistogram.CurrentIndex = 0;
        }
        
        //game update
        switch (State.Mode) {
            case Mode_Title: {
                UpdateTitle(&State, &Ui, &UiControl, DeltaSeconds, GameInput,  Textures, &DoContinue);   
            } break;
            
            case Mode_Game: {
                if (WasPressed(GameInput.ToggleEditModeKey)) {
                    State.Mode = Mode_Editor;
                    break;
                };
                UpdateGame(&State, GameInput, &Ui, UiControl, Player, DeltaSeconds, &BulletSpawnCooldown, &ChickenSpawnCooldown, SfxBomb, WorldWidth, Textures, FrameRateHistogram);
            } break;
            
            case Mode_Game_Over: {
                UpdateGameOver(&State, GameInput, Ui, DeltaSeconds, Player, Bgm, Textures);
            } break;
            
            case Mode_Editor: { 
                if (WasPressed(GameInput.ToggleEditModeKey)) {
                    State.Mode = Mode_Game;
                    break;
                };
                UpdateEditor(&State, &Ui, &UiControl, DeltaSeconds, GameInput, Textures);
            } break;
            
            default: {
                assert(0);
            } break;
        }            
        
        UiBegin();
        {
            auto Cursor = UiBeginText(&Ui, &DefaultFont, 10, Ui.Height / 2, true, color{1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
            UiWrite(&Cursor, "mouse Pos: %f, %f [%i, %i]\n", UiControl.Cursor.X, UiControl.Cursor.Y, GameInput.LeftMouseKey.IsPressed, GameInput.LeftMouseKey.HasChanged);            
            UiWrite(&Cursor, "UiControl: [active: %llu, hot: %llu]\n", UiControl.ActiveId, UiControl.HotId);
            UiWrite(&Cursor, "Entities: [%llu / %llu] \n", State.Entities.Count, State.Entities.Capacity);
        }           
        
        UiRect(&Ui, UiControl.Cursor.X - 10, UiControl.Cursor.Y - 10, 20, 20, color { 1.0f, 0, 0, 1.0f });
        
        UiEnd();        
        // render end
        
        auto glError = glGetError();
        if(glError != GL_NO_ERROR) {
            printf("gl error:%d \n", glError);
        }
        
        SDL_GL_SwapWindow(Window);   
    }
    // Close and destroy the window
    SDL_DestroyWindow(Window);
    
    // Clean up
    SDL_Quit();
    return 0;
}