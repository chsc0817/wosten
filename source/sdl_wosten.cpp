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
        key keys[11];
        
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
    transform xForm;
    f32 collisionRadius;
    s32 hp, maxHp;
    
    entity_type type;
    bool markedForDeletion;
    u32 collisionTypeMask;
    f32 blinkTime, blinkDuration;
    vec2 relativeDrawCenter;
    
    union {
        struct {
            f32 flipCountdown, flipInterval;
            vec2 velocity;
            f32 fireCountdown;
        } fly;
        
        struct {
            u32 power;
            u32 bombs;            
        } player;
        
        struct {
            u32 damage;   
        } bullet;
    };
    
};

#define template_array_name entity_buffer
#define template_array_data_type entity
#define template_array_is_buffer
#include "template_array.h"

entity* nextEntity(entity_buffer *buffer){
    auto result = Push(buffer);
    if (result != NULL) {
        *result = {};
    }
    
    return result;
}

struct collision {
    entity *entities[2];
};

struct entity_spawn_info {
    entity Blueprint;
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

struct game_state {
    entity_buffer entities;
    level Level;
    camera Camera;
    mode Mode;    
    bool DeleteButtonSelected;
    texture bombTexture;
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
    assert(File);
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

f32 lookAtRotation(vec2 eye, vec2 target) {
    f32 alpha = acos(dot(vec2{0, 1}, normalizeOrZero(target - eye)));
    
    if (eye.x <= target.x)
        alpha = -alpha;
    
    return alpha ;
}

entity MakeChicken(vec2 WorldPositionOffset) {
    entity Result;

    Result.xForm.rotation = 0.0f;
    Result.xForm.scale = 0.09f;
    Result.collisionRadius = Result.xForm.scale * 0.65;
    Result.maxHp = 10;
    Result.hp = Result.maxHp;
    Result.type = Entity_Type_Fly;
    Result.collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    Result.fly.flipInterval = 1.5f;
    Result.fly.flipCountdown = Result.fly.flipInterval * randZeroToOne();
    Result.fly.velocity = vec2{1.0f, 0.1f};    
    Result.xForm.pos = vec2{Result.fly.velocity.x * Result.fly.flipInterval * -0.5f, randZeroToOne()} + Result.fly.velocity * (Result.fly.flipInterval - Result.fly.flipCountdown) + WorldPositionOffset;   
    Result.relativeDrawCenter = vec2 {0.5f, 0.44f};
    Result.blinkDuration = 0.1f;

    return Result;
}

void DrawEntity(game_state *State, textures Textures, entity *Entity){
    switch (Entity->type) {
        
        case Entity_Type_Player: {
            drawTexturedQuad(State->Camera, Entity->xForm, Textures.PlayerTexture, White_Color, Entity->relativeDrawCenter);
        } break; 

        case Entity_Type_Bullet: {
            if (Entity->bullet.damage == 1) {
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.BulletTexture, White_Color, Entity->relativeDrawCenter);
            } 
            else if (Entity->bullet.damage == 2){
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.BulletPoweredUpTexture, White_Color, Entity->relativeDrawCenter);  
            }
            else {
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.BulletMaxPoweredUpTexture, White_Color, Entity->relativeDrawCenter);
            }
        } break;
        
        case Entity_Type_Boss: {
            if (Entity->blinkTime <= 0) {
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.BossTexture, White_Color, Entity->relativeDrawCenter); 
            } 
            else {
                color blinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, Entity->blinkTime / Entity->blinkDuration); 
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.BossTexture, blinkColor, Entity->relativeDrawCenter);          
            }   
        } break;
        
        case Entity_Type_Fly: {
            if (Entity->blinkTime <= 0) {
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.FlyTexture, White_Color, Entity->relativeDrawCenter); 
            } 
            else {
                color blinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, Entity->blinkTime / Entity->blinkDuration); 
                drawTexturedQuad(State->Camera, Entity->xForm, Textures.FlyTexture, blinkColor, Entity->relativeDrawCenter);          
            }   
        } break;
        
        case Entity_Type_Bomb: {
            drawTexturedQuad(State->Camera, Entity->xForm, State->bombTexture, color{randZeroToOne(), randZeroToOne(), randZeroToOne(), 1.0f}, Entity->relativeDrawCenter);    
        } break;
        
        case Entity_Type_Powerup: {
            drawTexturedQuad(State->Camera, Entity->xForm, Textures.PowerupTexture, White_Color, Entity->relativeDrawCenter);
        } break;

        default: {
            drawTexturedQuad(State->Camera, Entity->xForm, Textures.FlyTexture, White_Color, Entity->relativeDrawCenter);     
        }
    }    
}

void DrawAllEntities(game_state *State, textures Textures) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 0.1f);    
         
    for (u32 i = 0; i < State->entities.Count; i++) {
        auto Entity = State->entities.Base + i;
        DrawEntity(State, Textures, Entity);
    }
     
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void initGame (game_state *gameState, entity **player) {
    gameState->entities.Count = 0;
    for (u32 i = 0; i < gameState->Level.SpawnInfos.Count; i++) {
        gameState->Level.SpawnInfos[i].WasNotSpawned = true;
    }
    
    gameState->Level.Time = 0.0f;
    gameState->Camera.WorldPosition = vec2{0.0f, 0.0f};
    
    *player = nextEntity(&gameState->entities); 
    (*player)->xForm = TRANSFORM_IDENTITY;
    (*player)->xForm.scale = 0.1f;
    (*player)->collisionRadius = (*player)->xForm.scale * 0.5;
    (*player)->maxHp = 1;
    (*player)->hp = (*player)->maxHp;
    (*player)->player.power = 0;
    (*player)->player.bombs = 3;
    (*player)->type = Entity_Type_Player;
    (*player)->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Powerup);
    (*player)->relativeDrawCenter = vec2 {0.5f, 0.4f};  
};

void UpdateTitle(game_state *State, ui_context *Ui, ui_control *UiControl, f32 DeltaSeconds, input GameInput, textures Textures, font DefaultFont, bool *DoContinue){
            char *Items[] = {
                "New game",
                "Load Game",
                "Settings",                 
                "Quit Game"
            };
            auto Cursor = uiBeginText(Ui, &DefaultFont, Ui->width * 0.5f, Ui->height * 0.7f, true, color{0.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
            
            for (u32 i = 0; i < ARRAY_COUNT(Items); i++) {
                //uiRect(&ui, Cursor.CurrentX - 2, Cursor.CurrentY - 2, 5, 5, color{0.5f, 1.0f, 0.2f, 1.0f}, true);
                
                rect Rect;
                {
                    auto DummyCursor = Cursor;
                    DummyCursor.DoRender = false;
                    Rect = UiWrite(&DummyCursor, Items[i]);
                }
                
                f32 Border = Cursor.Scale * 15;
                auto MinRect = MakeRectWithSize(Rect.Left, Cursor.CurrentY - DefaultFont.BaselineYOffset * Cursor.Scale - Border, 0, DefaultFont.MaxGlyphHeight * Cursor.Scale + 2 * Border);
                Rect = Merge(Rect, MinRect);
                
                Rect.Left  -= Border;
                Rect.Right += Border;
                
                auto offset = (Rect.Right - Rect.Left) * 0.5f;
                Rect.Left  -= offset;
                Rect.Right -= offset;
                
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
                    uiTexturedRect(Ui, Textures.HotButtonTexture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, 0, 0, Textures.HotButtonTexture.Width, Textures.HotButtonTexture.Height, White_Color);
                else
                    uiTexturedRect(Ui, Textures.IdleButtonTexture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, 0, 0, Textures.IdleButtonTexture.Width, Textures.IdleButtonTexture.Height, White_Color);
                
                Cursor.CurrentX -= offset;
                Cursor.CurrentY += CursorYOffset;
                UiBegin();
                UiWrite(&Cursor, "%s\n", Items[i]);
                UiEnd();
                Cursor.CurrentY -= 20 * Cursor.Scale + 2 * Border + CursorYOffset;
            }
}

void UpdateEditor(game_state *GameState, ui_context *Ui, ui_control *UiControl, f32 DeltaSeconds, input GameInput, textures Textures) {
    if (GameInput.UpKey.IsPressed) {
        GameState->Level.Time += 3 * DeltaSeconds;
        GameState->Camera.WorldPosition.y += 3 * DeltaSeconds; 
    }
    
    if (GameInput.DownKey.IsPressed) {
        GameState->Level.Time -= 3 * DeltaSeconds;
        GameState->Camera.WorldPosition.y -= 3 * DeltaSeconds;
    }
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    UiBegin();
    
    if (GameState->Level.SpawnInfos.Count < GameState->Level.SpawnInfos.Capacity) {    
        rect FlyBlueprintRect = MakeRectWithSize(20, Ui->height - 80, 60, 60);
        UiTexturedRect(Ui, Textures.FlyTexture, FlyBlueprintRect, MakeRectWithSize(0, 0, Textures.FlyTexture.Width, Textures.FlyTexture.Height));  

        if (UiButton(UiControl, UI_ID0, FlyBlueprintRect)) {
            entity_spawn_info *Info = Push(&GameState->Level.SpawnInfos);
            Info->WasNotSpawned = true;
            Info->Blueprint = MakeChicken(GameState->Camera.WorldPosition);
        }
    }
    rect DeleteRect = MakeRectWithSize(20, 80, 60, 60);

    if (UiButton(UiControl, UI_ID0, DeleteRect)) {
        GameState->DeleteButtonSelected = !GameState->DeleteButtonSelected;
    }

    if (GameState->DeleteButtonSelected) {
        UiTexturedRect(Ui, Textures.DeleteButtonTexture, DeleteRect, MakeRectWithSize(0, 0, Textures.DeleteButtonTexture.Width, Textures.DeleteButtonTexture.Height), color{1.0f, 0.2f, 0.2f, 1.0f});
    }
    else {
        UiTexturedRect(Ui, Textures.DeleteButtonTexture, DeleteRect, MakeRectWithSize(0, 0, Textures.DeleteButtonTexture.Width, Textures.DeleteButtonTexture.Height));
    } 

    u32 SpawnIndex = 0;
    

    while (SpawnIndex < GameState->Level.SpawnInfos.Count) {
        auto Info = GameState->Level.SpawnInfos.Base + SpawnIndex;
        transform CollisionTransform = Info->Blueprint.xForm;
        CollisionTransform.scale = 2 * Info->Blueprint.collisionRadius;
        drawCircle(GameState->Camera, CollisionTransform, color{0.3f, 0.3f, 0.0f, 1.0f}, false, 16, -0.5f);

        glEnable(GL_TEXTURE_2D);
        DrawEntity(GameState, Textures, &Info->Blueprint);
        glDisable(GL_TEXTURE_2D);
        // collision center
        auto CanvasPoint = WorldToCanvasPoint(GameState->Camera, Info->Blueprint.xForm.pos);
        auto UiPoint = CanvasToUiPoint(Ui, CanvasPoint);

        auto CollisionBorderWorldPoint = vec2 {Info->Blueprint.xForm.pos.x + Info->Blueprint.collisionRadius, Info->Blueprint.xForm.pos.y}; 
        auto CollisionBorderCanvasPoint = WorldToCanvasPoint(GameState->Camera, CollisionBorderWorldPoint);
        auto CollisionBorderUiPoint = CanvasToUiPoint(Ui, CollisionBorderCanvasPoint);

        f32 UiRadius = CollisionBorderUiPoint.x - UiPoint.x;

        rect Rect = MakeRectWithSize(UiPoint.x - UiRadius, UiPoint.y - UiRadius, 2 * UiRadius, 2 * UiRadius);

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

        uiRect(Ui, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, Color, false);
       
        vec2 DeltaPosition;
        UiDragable(UiControl, Id, Rect, &DeltaPosition);
        
        //drag or delete selected entity
        if (UiControl->ActiveId == Id)
        {
            if(GameState->DeleteButtonSelected) {
                GameState->Level.SpawnInfos[SpawnIndex] = GameState->Level.SpawnInfos[GameState->Level.SpawnInfos.Count - 1];
                Pop(&GameState->Level.SpawnInfos);  

                UiControl->ActiveId = NULL;
            }
            else {
                auto Cursor = uiBeginText(Ui, Ui->CurrentFont, Ui->width * 0.5f, Ui->height * 0.5f);
                UiWrite(&Cursor, "center: %f , \n %f", Info->Blueprint.xForm.pos.x, Info->Blueprint.xForm.pos.y);


                UiPoint = UiPoint + DeltaPosition;
                auto NewCenterCanvasPoint = UiToCanvasPoint(Ui, UiPoint);
                Info->Blueprint.xForm.pos = CanvasToWorldPoint(GameState->Camera, NewCenterCanvasPoint);
            }
        }

        SpawnIndex++;
        
        
    }

    UiEnd();
}

void updateGameOver(game_state *State, input GameInput, ui_context Ui,f32 DeltaSeconds, entity *Player,  Mix_Music *bgm, font DefaultFont, textures Textures){
    Player->xForm.rotation += 2 * PI * DeltaSeconds;
    
    if (WasPressed(GameInput.EnterKey)) {
        initGame(State, &Player);
        Mix_FadeInMusic(bgm, -1, 500);
        State->Mode = Mode_Game;        
    }

    auto Cursor = uiBeginText(&Ui, &DefaultFont, Ui.width / 2, Ui.height / 2, true, color{1.0f, 0.0f, 0.0f, 1.0f}, 5.0f);
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

void UpdateGame(game_state *State, input GameInput, ui_context *Ui, ui_control UiControl, entity *Player, f32 DeltaSeconds, f32 *bulletSpawnCooldown, f32 *chickenSpawnCooldown, Mix_Chunk *sfxBomb, f32 worldWidth, textures Textures, histogram frameRateHistogram, font DefaultFont){    
     
    State->Camera.WorldPosition.y += DeltaSeconds;

    auto entities = &State->entities;
    entity *boss = NULL;
        
    //same as State.inEditMode ^= WasPressed(...)
    
    State->Level.Time += DeltaSeconds;
    State->Level.Time = MIN(State->Level.Time, State->Level.Duration);
        
    for (u32 i = 0; i <entities->Count; i++) {
        if (entities->Base[i].type == Entity_Type_Boss) {
            boss = entities->Base + i;
            break;
        } 
    }
    
    for (u32 SpawnIndex = 0; SpawnIndex < ARRAY_COUNT(State->Level.SpawnInfos); SpawnIndex++) {
        auto Info = State->Level.SpawnInfos.Base + SpawnIndex;
        if (Info->WasNotSpawned && (State->Camera.WorldPosition.y + WorldCameraHeight * 0.5f >= Info->Blueprint.xForm.pos.y - Info->Blueprint.collisionRadius)) {
            auto Entity = nextEntity(entities);
            if (Entity != NULL) {
                *Entity = Info->Blueprint;
                Info->WasNotSpawned = false;
            }
        }  
    }      
        
    //bullet movement        
    if (*bulletSpawnCooldown > 0) *bulletSpawnCooldown -= DeltaSeconds;
    
    vec2 direction = {};
    f32 speed = 1.0f;
    

    collision collisions[1024];
    u32 collisionCount = 0;
    
    for(u32 i = 0; i < entities->Count; i++) {
        bool doBreak = false;
        
        for (u32 j = i + 1; j < entities->Count; j++){
            
            if (!(entities->Base[i].collisionTypeMask & FLAG(entities->Base[j].type)))
                continue;
            
            if (!(entities->Base[j].collisionTypeMask & FLAG(entities->Base[i].type)))
                continue;
            
            if (areIntersecting(circle{entities->Base[i].xForm.pos, entities->Base[i].collisionRadius}, circle{entities->Base[j].xForm.pos, entities->Base[j].collisionRadius})) 
            {
                if (collisionCount >= ARRAY_COUNT(collisions)) {
                    doBreak = true;
                    break;
                }
                
                auto newCollision = collisions + (collisionCount++);               
                
                if (entities->Base[i].type < entities->Base[j].type) {
                    newCollision->entities[0] = (entities->Base) + i;
                    newCollision->entities[1] = (entities->Base) + j;
                }
                else {
                    newCollision->entities[1] = (entities->Base) + i;
                    newCollision->entities[0] = (entities->Base) + j;
                }
            }
        }
        
        if(doBreak) {
            break;
        }
    }
    
    for (u32 i = 0; i < collisionCount; i++) {
        auto current = collisions + i;
        
        switch (FLAG(current->entities[0]->type) | FLAG(current->entities[1]->type)) {
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Powerup)): {
                auto player = current->entities[0];
                auto powerup = current->entities[1];
                assert((player->type == Entity_Type_Player) && (powerup->type == Entity_Type_Powerup));
                
                
                auto distance = player->xForm.pos - powerup->xForm.pos;
                
                if (lengthSquared(distance) <= Powerup_Collect_Radius * Powerup_Collect_Radius) {
                    powerup->markedForDeletion = true;
                    player->player.power++;
                }
                else {
                    powerup->xForm.pos = powerup->xForm.pos + normalizeOrZero(distance) * (Powerup_Magnet_Speed * DeltaSeconds);
                }                
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly)): {
                auto fly = current->entities[0];
                auto bullet = current->entities[1];
                
                assert((fly->type == Entity_Type_Fly) && (bullet->type == Entity_Type_Bullet));
                
                bullet->markedForDeletion = true;
                fly->hp -= bullet->bullet.damage;  
                fly->blinkTime = fly->blinkDuration;
                
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Boss)): {
                auto boss = current->entities[0];
                auto bullet = current->entities[1];
                
                assert((boss->type == Entity_Type_Boss) && (bullet->type == Entity_Type_Bullet));
                
                bullet->markedForDeletion = true;
                boss->hp -= bullet->bullet.damage;
                boss->blinkTime = boss->blinkDuration;
                
            } break;   
            
            case (FLAG(Entity_Type_Bomb) | FLAG(Entity_Type_Bullet)): {
                auto bullet = current->entities[0];
                auto bomb = current->entities[1];
                
                assert((bomb->type == Entity_Type_Bomb) && (bullet->type == Entity_Type_Bullet));
                
                bullet->markedForDeletion = true;               
            } break;
            
            case (FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Player)): 
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Boss)): 
            case (FLAG(Entity_Type_Player) | FLAG(Entity_Type_Fly)): {
                
                auto player = current->entities[0];
                auto enemy = current->entities[1];
                
                assert(player->type == Entity_Type_Player);
                
                State->Mode = Mode_Game_Over;
                Mix_FadeOutMusic(500);
                Mix_HaltChannel(-1);
                Mix_PlayChannel(0, loadChunk(Sfx_Death), 0);
                return;
                
            } break;  
        }
    }
    
    for(u32 i = 0; i < entities->Count; i++) {
        
        auto e = entities->Base + i;
            
        e->xForm.pos.y += DeltaSeconds;

        switch(e->type) {
            case Entity_Type_Bullet: {
                
                vec2 dir = normalizeOrZero(TransformPoint(e->xForm, {0, 1}) - e->xForm.pos);
                
                //e->xForm.pos.y += speed * DeltaSeconds;
                f32 speed = 1.0f; 
                e->xForm.pos = e->xForm.pos + dir * (speed * DeltaSeconds);
                
                if (ABS(e->xForm.pos.y - State->Camera.WorldPosition.y) >= WorldCameraHeight) {
                    e->markedForDeletion = true;
                }                               
            } break;
            
            case Entity_Type_Bomb: {
                e->collisionRadius += DeltaSeconds * 3.0f;
                e->xForm.scale = e->collisionRadius * 3.0f / (State->bombTexture.Height * Default_World_Units_Per_Texel);
                
                if (e->collisionRadius > 6.0f) {
                    e->markedForDeletion = true;
                }
            } break;
            
            case Entity_Type_Fly:{
                if (e->hp <= 0) {
                    e->markedForDeletion = true;
                    
                    entity *powerup = nextEntity(entities);
                    
                    if (powerup)
                    {
                        powerup->xForm.pos = e->xForm.pos;  
                        powerup->xForm.rotation = 0.0f;
                        powerup->xForm.scale = 0.02f;
                        powerup->collisionRadius = Powerup_Collect_Radius * 3;
                        powerup->type = Entity_Type_Powerup;
                        powerup->collisionTypeMask = FLAG(Entity_Type_Player); 
                        powerup->relativeDrawCenter = vec2 {0.5f, 0.5f};
                    }
                } 
                
                f32 t = DeltaSeconds;
                
                while (e->fly.flipCountdown < t) {
                    e->xForm.pos = e->xForm.pos + e->fly.velocity * e->fly.flipCountdown;
                    t -= e->fly.flipCountdown;
                    e->fly.velocity = -(e->fly.velocity);
                    e->fly.flipCountdown += e->fly.flipInterval;
                }
                
                e->fly.flipCountdown -= DeltaSeconds;
                e->xForm.pos = e->xForm.pos + e->fly.velocity * t;
                
                e->fly.fireCountdown -= DeltaSeconds;
                if (e->fly.fireCountdown <= 0)
                {
                    e->fly.fireCountdown += lerp(Fly_Min_Fire_Interval, Fly_Max_Fire_Interval, randZeroToOne());
                    
                    auto bullet = nextEntity(entities);
                    if (bullet)
                    {
                        bullet->type = Entity_Type_Bullet;
                        bullet->collisionTypeMask |= FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bomb);
                        bullet->xForm.pos = e->xForm.pos;  
                        bullet->xForm.rotation = lerp(PI * 0.75f, PI * 1.25f, randZeroToOne()); // points downwards
                        bullet->xForm.scale = 0.2f;
                        bullet->collisionRadius = bullet->xForm.scale * 0.2;
                        bullet->relativeDrawCenter = vec2 {0.5f, 0.5f};
                    }
                }
                
                if (e->blinkTime > 0) e->blinkTime -= DeltaSeconds;
            } break;
            
            case Entity_Type_Boss:{
                if (e->blinkTime > 0) e->blinkTime -= DeltaSeconds;
                if (State->Camera.WorldPosition.y + WorldCameraHeight * 0.5f < e->xForm.pos.y + e->collisionRadius * 1.5f) {
                    e->xForm.pos.y -= DeltaSeconds;
                }
            } break;
            
            case Entity_Type_Powerup: {                
                f32 fallSpeed = 0.8f; 
                e->xForm.pos = e->xForm.pos + vec2{0, -1} * (fallSpeed * DeltaSeconds);          
            } break;
        }
    }
    
    u32 i = 0;
    while (i < entities->Count) {
        if (entities->Base[i].markedForDeletion) {
            entities->Base[i] = entities->Base[(entities->Count) - 1];
            (entities->Count)--;
        }
        else{ 
            i++;
        }
    }    

    //player movement
    if (GameInput.LeftKey.IsPressed) {
        direction.x -= 1;
        //player->xForm.rotation -= 0.5f * PI * DeltaSeconds;
    }
    
    if (GameInput.RightKey.IsPressed) {
        direction.x += 1; 
        //player->xForm.rotation += 0.5f * PI * DeltaSeconds;
    }
    
    if (GameInput.UpKey.IsPressed) {
        direction.y += 1; 
    }
    
    if (GameInput.DownKey.IsPressed) {
        direction.y -= 1; 
    }
    
    if (GameInput.SlowMovementKey.IsPressed) {
        speed = speed * 0.5f;
    }
    
    direction = normalizeOrZero(direction);            
    Player->xForm.pos = Player->xForm.pos + direction * (speed * DeltaSeconds);
    
    Player->xForm.pos.y = CLAMP(Player->xForm.pos.y , -1.0f + Player->collisionRadius + State->Camera.WorldPosition.y, 1.0f - Player->collisionRadius + State->Camera.WorldPosition.y);
    
    // worldWidth = windowWidth / windowHeight * worldHeight 
    // worldHeight = 2 (from -1 to 1)               
    Player->xForm.pos.x = CLAMP(Player->xForm.pos.x, -worldWidth * 0.5f + Player->collisionRadius, worldWidth * 0.5f - Player->collisionRadius);
    {
#if 0
        f32 rotation = asin((enemies[0].xForm.pos.x - player->xForm.pos.x) / length(enemies[0].xForm.pos - player->xForm.pos));
        if (enemies[0].xForm.pos.y < player->xForm.pos.y) {
            rotation = rotation - PI;
        }
        else {
            rotation = 2 * PI - rotation;
        }
        
        player->xForm.rotation = rotation;
        
#else
        if (boss)
            Player->xForm.rotation = lookAtRotation(Player->xForm.pos, boss->xForm.pos);
        
        
#endif
    }
    {
        *chickenSpawnCooldown -= DeltaSeconds;
        
        if(0){
        //if(*chickenSpawnCooldown <= 0) {
            //unleash the chicken!
            entity *chicken = nextEntity(entities);
            
            if (chicken != NULL) {               
                *chicken = MakeChicken(State->Camera.WorldPosition);
            }        
            *chickenSpawnCooldown += 1.0f;
        }
    }
            
    if(GameInput.FireKey.IsPressed) {
        if ((*bulletSpawnCooldown <= 0)) {
            entity *bullet = nextEntity(entities);
            
            if (bullet != NULL) {
                bullet->xForm.pos = Player->xForm.pos;
                bullet->xForm.scale = 0.2f;
                bullet->xForm.rotation = 0;
                bullet->collisionRadius = bullet->xForm.scale * 0.2;
                bullet->type = Entity_Type_Bullet;
                bullet->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly);
                bullet->relativeDrawCenter = vec2 {0.5f, 0.5f};
                
                bullet->bullet.damage = (Player->player.power / 20) + 1;
                bullet->bullet.damage = MIN(bullet->bullet.damage, 3);
                
                *bulletSpawnCooldown += 0.05f;
                
                //                        Mix_PlayChannel(0, sfxShoot, 0);
            }
        }
    }
            
    if(WasPressed(GameInput.BombKey)) {                       
        if (Player->player.bombs > 0) {

            entity *bomb = nextEntity(entities);
            
            if (bomb != NULL) {
                bomb->xForm.pos = Player->xForm.pos;
                bomb->collisionRadius = 0.1f;
                bomb->xForm.scale = bomb->collisionRadius * 3.0f / (State->bombTexture.Height * Default_World_Units_Per_Texel);
                bomb->xForm.rotation = 0;
                bomb->type = Entity_Type_Bomb;
                bomb->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Bullet);
                bomb->relativeDrawCenter = vec2 {0.5f, 0.5f};
                
                Player->player.bombs--;
                
                Mix_PlayChannel(1, sfxBomb, 0);
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
    f32 currentLevelYPosition = lerp(worldCameraHeight * 0.5f, gameState.Level.WorldHeight - worldCameraHeight * 0.5f, gameState.Level.Time / gameState.Level.Duration); 
    drawTexturedQuad(gameState.Camera, backgroundXForm, levelLayer1, White_Color, vec2 {0.5f,  currentLevelYPosition / gameState.Level.WorldHeight}, gameState.Level.LayersWorldUnitsPerPixels[0], 0.9f);
    drawTexturedQuad(gameState.Camera, backgroundXForm, levelLayer2, White_Color, vec2 {0.5f,  currentLevelYPosition / gameState.Level.WorldHeight}, gameState.Level.LayersWorldUnitsPerPixels[1], 0.8f);
    #endif

    DrawAllEntities(State, Textures);
     
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    //debug framerate, hitbox and player/boss normalized x, y coordinates
        
#define DEBUG_UI
#ifdef DEBUG_UI
    drawHistogram(frameRateHistogram);
        
    for (u32 i = 0; i < entities->Count; i++) {
        transform collisionTransform = (*entities)[i].xForm;
        collisionTransform.scale = 2 * entities->Base[i].collisionRadius;
        drawCircle(State->Camera, collisionTransform, color{0.3f, 0.3f, 0.0f, 1.0f}, false);
        drawLine(State->Camera, collisionTransform, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        drawLine(State->Camera, collisionTransform, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
    }        
        
#endif //DEBUG_UI
        
    //GUI
    {
        UiBegin();

        //bomb count           
        for (int bomb = 0; bomb < Player->player.bombs; bomb++){
            uiTexturedRect(Ui, Textures.BombCountTexture, 20 + bomb * (Textures.BombCountTexture.Width + 5), Ui->height - 250, Textures.BombCountTexture.Width, Textures.BombCountTexture.Height, 0, 0, Textures.BombCountTexture.Width, Textures.BombCountTexture.Height, White_Color);
        }
        
        //boss hp
        if (boss) {
            auto cursor = uiBeginText(Ui, &DefaultFont, 20, Ui->height - 90);
            UiWrite(&cursor, "BOSS ");
            UiWrite(&cursor, "hp: %i / %i", boss->hp, boss->maxHp);
            
            uiBar(Ui, 20, Ui->height - 60, Ui->width - 40, 40, boss->hp / (f32) boss->maxHp, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
        }
        
        //player power
        uiBar(Ui, 20, Ui->height - 200, 120,40, (Player->player.power % 20) / 20.0f, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});

        UiEnd();
    }
}
// gl functions

PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = NULL;

void APIENTRY
wostenGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void * user_param)
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
    switch (type)
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
        type_text = "unkown type";
    }
    
    printf("[gl %s prio %s | %u ]: %s\n", severity_text, type_text, id, message);
    
    // filter harmless errors
    switch (id) {
        // shader compilation failed
        case 2000:
        return;
    }
    
    assert(type != GL_DEBUG_TYPE_ERROR);
}	

int main(int argc, char* argv[]) {
    srand (time(NULL));
    
    SDL_Window *window;                    // Declare a pointer
    
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);              // Initialize SDL2
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "wosten",                  // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        640,                               // width, in pixels
        480,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
        | SDL_WINDOW_RESIZABLE
        );
    
    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }
    
    // The window is open: could enter program loop here (see SDL_PollEvent())
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    glDebugMessageCallback =(PFNGLDEBUGMESSAGECALLBACKPROC) SDL_GL_GetProcAddress("glDebugMessageCallback");
    
    if (glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // message will be generatet in function call scope
        glDebugMessageCallback(wostenGLDebugCallback, NULL);
    }
    
    //sound init
    int mixInit = Mix_Init(MIX_INIT_MP3);
    if(mixInit&MIX_INIT_MP3 != MIX_INIT_MP3) {
        printf("Error initializing mix: %s \n", Mix_GetError());
    }
    
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 4096)) {
        printf("Error Mix_OpenAudio: %s \n", Mix_GetError());
    }
    Mix_Music *bgm;
    bgm = Mix_LoadMUS("data/Gravity Sound/Gravity Sound - Rain Delay CC BY 4.0.mp3");
    if(!bgm) {
        printf("Error loading music file: %s \n", Mix_GetError());
    } 
    else {    
        Mix_PlayMusic(bgm, -1);
        Mix_VolumeMusic(30);
    }

    //sfx
    Mix_AllocateChannels(2);
    Mix_Chunk *sfxBomb = loadChunk(Sfx_Bomb);
    //Mix_Chunk *sfxShoot = loadChunk(Sfx_Shoot); 
    
    if(!sfxBomb) {
        printf("Error loading music file: %s \n", Mix_GetError());
    }
    
    histogram frameRateHistogram = {};
    
    ui_context ui = {};
    ui_control UiControl = {};
    
    bool doContinue = true;
    
    u64 ticks = SDL_GetPerformanceFrequency();
    u64 lastTime = SDL_GetPerformanceCounter();
    f32 scaleAlpha = 0;
    
    textures Textures = {};
    Textures.LevelLayer1 = loadTexture("data/level_1.png");
    Textures.LevelLayer2 = loadTexture("data/level_1_layer_2.png");
    Textures.PlayerTexture = loadTexture("data/Kenney/Animals/giraffe.png");
    Textures.BossTexture = loadTexture("data/Kenney/Animals/parrot.png");
    Textures.FlyTexture = loadTexture("data/Kenney/Animals/chicken.png");
    Textures.BulletTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_014.png");  
    Textures.BulletPoweredUpTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_001.png");
    Textures.BulletMaxPoweredUpTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_006.png");
    Textures.BombCountTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_021.png");
    Textures.PowerupTexture = loadTexture("data/Kenney/Letter Tiles/letter_P.png");
    
    // UI
    Textures.IdleButtonTexture = loadTexture("data/Kenney/PNG/blue_button02.png");
    Textures.HotButtonTexture = loadTexture("data/Kenney/PNG/blue_button03.png");
    Textures.DeleteButtonTexture = loadTexture("data/Kenney/PNG/grey_boxCross.png");
    
    
    SDL_RWops* op = SDL_RWFromFile("C:/Windows/Fonts/Arial.ttf", "rb");
    s64 byteCount = op->size(op);
    u8 *data = new u8[byteCount];   
    usize ok = SDL_RWread(op, data, byteCount, 1);
    assert (ok == 1);
    
    stbtt_fontinfo StbFont;
    stbtt_InitFont(&StbFont, data, stbtt_GetFontOffsetForIndex(data,0));
    
    f32 scale = stbtt_ScaleForPixelHeight(&StbFont, 48);
    s32 ascent;
    stbtt_GetFontVMetrics(&StbFont, &ascent,0,0);
    s32 baseline = (s32) (ascent*scale);
    
    const s32 bitmapWidth = 512;
    u8 bitmap[bitmapWidth * bitmapWidth] = {};
    s32 xOffset = 0;
    s32 yOffset = 0;
    s32 maxHight = 0;
    font DefaultFont = {};
    
    //    while (text[ch]) 
    
    for (u32 i = ' '; i < 256; i++) {   
        glyph *FontGlyph = DefaultFont.Glyphs + i;
        FontGlyph->Code = i;
        
        s32 unscaledXAdvance;  
        stbtt_GetCodepointHMetrics(&StbFont, FontGlyph->Code, &unscaledXAdvance, &FontGlyph->DrawXOffset);
        FontGlyph->DrawXAdvance = unscaledXAdvance * scale;
        
        s32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&StbFont, FontGlyph->Code, scale, scale, &x0, &y0, &x1, &y1);
        FontGlyph->Width = x1 - x0;
        FontGlyph->Height = y1 - y0;
        FontGlyph->DrawXOffset = x0;
        // y0 is top corner, but its also negative ...
        // we draw from bottom left corner
        FontGlyph->DrawYOffset = -(y0 + FontGlyph->Height);
        DefaultFont.BaselineYOffset = MAX(DefaultFont.BaselineYOffset, -FontGlyph->DrawYOffset);
        if ((xOffset + FontGlyph->Width) >= bitmapWidth) {
            xOffset = 0;
            yOffset += maxHight + 1;
            maxHight = 0;
        }
        assert(FontGlyph->Width <= bitmapWidth);
        
        stbtt_MakeCodepointBitmap(&StbFont, bitmap + xOffset + yOffset * bitmapWidth, FontGlyph->Width, FontGlyph->Height, bitmapWidth, scale, scale, FontGlyph->Code);
        FontGlyph->X = xOffset;
        // we flip the texture so we need to change the y to the inverse
        FontGlyph->Y = bitmapWidth - yOffset - FontGlyph->Height;
        xOffset += FontGlyph->Width + 1;
        maxHight = MAX(maxHight, FontGlyph->Height);
        DefaultFont.MaxGlyphWidth = MAX(DefaultFont.MaxGlyphWidth, FontGlyph->Width);
        DefaultFont.MaxGlyphHeight = MAX(DefaultFont.MaxGlyphHeight, FontGlyph->Height);
    }
    
    DefaultFont.Texture = loadTexture(bitmap, bitmapWidth, bitmapWidth, 1, GL_NEAREST);    
    
    ui.CurrentFont = &DefaultFont;

    f32 WorldHeightOverWidth = 3.0f / 4.0f; 
    f32 worldWidth = WorldCameraHeight / WorldHeightOverWidth;
    
    entity _entitieEntries[100];
    game_state gameState = {};
    gameState.Mode = Mode_Title;
    gameState.Level.Duration = 30.0f;
    gameState.Level.Time = 0.0f;
    gameState.Level.LayersWorldUnitsPerPixels[0] = worldWidth / Textures.LevelLayer1.Width;
    gameState.Level.LayersWorldUnitsPerPixels[1] = worldWidth / Textures.LevelLayer2.Width;
    gameState.Level.WorldHeight = gameState.Level.LayersWorldUnitsPerPixels[0] * Textures.LevelLayer1.Height;  
    gameState.entities = { ARRAY_WITH_COUNT(_entitieEntries) };
    gameState.bombTexture = loadTexture("data/Kenney/particlePackCircle.png");
    gameState.DeleteButtonSelected = false;
    entity *player;
    
    /*
    auto bossSpawnInfo  = Push(&gameState.Level.SpawnInfos);
    assert(bossSpawnInfo);
    bossSpawnInfo->WasNotSpawned = true;
    
    bossSpawnInfo->Blueprint.xForm.pos = vec2{0.0f, 0.5f};  
    bossSpawnInfo->Blueprint.xForm.rotation = 0.0f;
    bossSpawnInfo->Blueprint.collisionRadius = 0.35f;
    bossSpawnInfo->Blueprint.xForm.scale = bossSpawnInfo->Blueprint.collisionRadius * 2.0f / (bossTexture.Height * Default_World_Units_Per_Texel);
    bossSpawnInfo->Blueprint.relativeDrawCenter = vec2{0.5f, 0.5f};
    bossSpawnInfo->Blueprint.maxHp = 500;
    bossSpawnInfo->Blueprint.hp = bossSpawnInfo->Blueprint.maxHp;
    bossSpawnInfo->Blueprint.type = Entity_Type_Boss;
    bossSpawnInfo->Blueprint.collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    bossSpawnInfo->Blueprint.blinkDuration = 0.1f;
    bossSpawnInfo->Blueprint.blinkTime = 0.0f;
    */

    gameState.Level = LoadLevel("data/levels/Level.bin");
    SaveLevel("data/levels/LevelBackup.bin", gameState.Level);
    initGame(&gameState, &player);
    
    //timer init
    
    f32 bulletSpawnCooldown = 0;
	
    f32 chickenSpawnCooldown = 5.0f;
    
    input GameInput = {};
    
    
    //game loop   
    while (doContinue) {
        for (s32 i = 0; i < ARRAY_COUNT(GameInput.keys); i++) {
            GameInput.keys[i].HasChanged = false;
        }
        
        //window events 
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:{
                    doContinue = false;
                    SaveLevel("data/levels/Level.bin", gameState.Level);

                } break;
                
                case SDL_WINDOWEVENT:{
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE:{
                            
                        }
                    }
                }
                
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
                
                //keyboard input             
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    
                    if (event.key.repeat > 0) 
                        break;
                    
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_A: {
                            GameInput.LeftKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.LeftKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_W: {
                            GameInput.UpKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.UpKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_S: {
                            GameInput.DownKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.DownKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_D: {
                            GameInput.RightKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.RightKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_K: {
                            GameInput.FireKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.FireKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_L: {
                            GameInput.BombKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.BombKey.HasChanged = true;    
                        } break;
                        
                        case SDL_SCANCODE_RETURN: {
                            GameInput.EnterKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.EnterKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_LSHIFT: {
                            GameInput.SlowMovementKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.SlowMovementKey.HasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_F1: {
                            GameInput.ToggleEditModeKey.IsPressed = (event.key.type == SDL_KEYDOWN);
                            GameInput.ToggleEditModeKey.HasChanged = true;
                        } break;
                    }
                } break;
            }
        }
        //time        
        u64 currentTime = SDL_GetPerformanceCounter();
        double DeltaSeconds = (double)(currentTime - lastTime) / (double)ticks;
        lastTime = currentTime;
        
        // render begin
        s32 width, height;
        SDL_GetWindowSize(window, &width, &height);
        
        gameState.Camera.HeightOverWidth = height / (f32)width;        
        f32 worldPixelWidth = height / WorldHeightOverWidth;
        
        ui.width = width;
        ui.height = height;
        
        UiFrameStart(&UiControl, vec2{ GameInput.MousePos.x, ui.height - GameInput.MousePos.y }, WasPressed(GameInput.LeftMouseKey), WasReleased(GameInput.LeftMouseKey));
        
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_SCISSOR_TEST);
        //        glViewport((width - worldPixelWidth) * 0.5f, 0, worldPixelWidth, height);
        glScissor((width - worldPixelWidth) * 0.5f, 0, worldPixelWidth, height);
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glEnable(GL_DEPTH_TEST);
        
        frameRateHistogram.values[frameRateHistogram.currentIndex] = 1 / DeltaSeconds;
        frameRateHistogram.currentIndex++;
        if (frameRateHistogram.currentIndex == ARRAY_COUNT(frameRateHistogram.values)) {
            frameRateHistogram.currentIndex = 0;
        }
        
        //game update
        switch (gameState.Mode) {
            case Mode_Title: {
                UpdateTitle(&gameState, &ui, &UiControl, DeltaSeconds, GameInput,  Textures, DefaultFont, &doContinue);   
            } break;

            case Mode_Game: {
                if (WasPressed(GameInput.ToggleEditModeKey)) {
                    gameState.Mode = Mode_Editor;
                    break;
                };
                UpdateGame(&gameState, GameInput, &ui, UiControl, player, DeltaSeconds, &bulletSpawnCooldown, &chickenSpawnCooldown, sfxBomb, worldWidth, Textures, frameRateHistogram, DefaultFont);
            } break;

            case Mode_Game_Over: {
                updateGameOver(&gameState, GameInput, ui, DeltaSeconds, player, bgm, DefaultFont, Textures);
            } break;

            case Mode_Editor: { 
                if (WasPressed(GameInput.ToggleEditModeKey)) {
                    gameState.Mode = Mode_Game;
                    break;
                };
                UpdateEditor(&gameState, &ui, &UiControl, DeltaSeconds, GameInput, Textures);
            } break;

            default: {
                assert(0);
            } break;
        }            
        
        UiBegin();
        {
            auto Cursor = uiBeginText(&ui, &DefaultFont, 10, ui.height / 2, true, color{1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
            UiWrite(&Cursor, "mouse pos: %f, %f [%i, %i]\n", UiControl.Cursor.x, UiControl.Cursor.y, GameInput.LeftMouseKey.IsPressed, GameInput.LeftMouseKey.HasChanged);            
            UiWrite(&Cursor, "UiControl: [active: %llu, hot: %llu]\n", UiControl.ActiveId, UiControl.HotId);
            UiWrite(&Cursor, "Entities: [%llu / %llu] \n", gameState.entities.Count, gameState.entities.Capacity);
        }           
            
        uiRect(&ui, UiControl.Cursor.x - 10, UiControl.Cursor.y - 10, 20, 20, color { 1.0f, 0, 0, 1.0f });
           
        UiEnd();
        
        // render end
        
        auto glError = glGetError();
        if(glError != GL_NO_ERROR) {
            printf("gl error:%d \n", glError);
        }
        
        SDL_GL_SwapWindow(window);   
    }
    // Close and destroy the window
    SDL_DestroyWindow(window);
    
    // Clean up
    SDL_Quit();
    return 0;
}