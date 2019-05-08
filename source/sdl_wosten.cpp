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

#define TRANSFORM_IDENTITY {{}, 0.0f, 1.0f}



rect DebugRect = MakeRect(100.0f, 300.0f, 150.0f, 100.0f);



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
    f32 LevelTime;
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
    bool isGameover;    
    bool inEditMode;
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

void update(game_state *state, f32 deltaSeconds){    
    
    collision collisions[1024];
    u32 collisionCount = 0;
    auto buffer = &state->entities;
    
    for(u32 i = 0; i < buffer->Count; i++) {
        bool doBreak = false;
        
        for (u32 j = i + 1; j < buffer->Count; j++){
            
            if (!(buffer->Base[i].collisionTypeMask & FLAG(buffer->Base[j].type)))
                continue;
            
            if (!(buffer->Base[j].collisionTypeMask & FLAG(buffer->Base[i].type)))
                continue;
            
            if (areIntersecting(circle{buffer->Base[i].xForm.pos, buffer->Base[i].collisionRadius}, circle{buffer->Base[j].xForm.pos, buffer->Base[j].collisionRadius})) 
            {
                if (collisionCount >= ARRAY_COUNT(collisions)) {
                    doBreak = true;
                    break;
                }
                
                auto newCollision = collisions + (collisionCount++);               
                
                if (buffer->Base[i].type < buffer->Base[j].type) {
                    newCollision->entities[0] = (buffer->Base) + i;
                    newCollision->entities[1] = (buffer->Base) + j;
                }
                else {
                    newCollision->entities[1] = (buffer->Base) + i;
                    newCollision->entities[0] = (buffer->Base) + j;
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
                
                if (lengthSquared(distance) <= Powerup_Collect_Radius * Powerup_Collect_Radius)
                {
                    powerup->markedForDeletion = true;
                    player->player.power++;
                }
                else
                {
                    powerup->xForm.pos = powerup->xForm.pos + normalizeOrZero(distance) * (Powerup_Magnet_Speed * deltaSeconds);
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
                
                state->isGameover = true;
                Mix_FadeOutMusic(500);
                Mix_HaltChannel(-1);
                Mix_PlayChannel(0, loadChunk(Sfx_Death), 0);
                return;
                
            } break;  
        }
    }
    
    
    
    for(u32 i = 0; i < buffer->Count; i++) {
        
        auto e = buffer->Base + i;
        
        switch(e->type) {
            case Entity_Type_Bullet: {
                
                vec2 dir = normalizeOrZero(transformPoint(e->xForm, {0, 1}, 1) - e->xForm.pos);
                
                //e->xForm.pos.y += speed * deltaSeconds;
                f32 speed = 1.0f; 
                e->xForm.pos = e->xForm.pos + dir * (speed * deltaSeconds);
                
                if (ABS(e->xForm.pos.y) >= 1) {
                    e->markedForDeletion = true;
                }                               
            } break;
            
            case Entity_Type_Bomb: {
                e->collisionRadius += deltaSeconds * 3.0f;
                e->xForm.scale = e->collisionRadius * 3.0f / (state->bombTexture.height * Default_World_Units_Per_Texel);
                
                if (e->collisionRadius > 6.0f) {
                    e->markedForDeletion = true;
                }
            } break;
            
            case Entity_Type_Fly:{
                if (e->hp <= 0) {
                    e->markedForDeletion = true;
                    
                    entity *powerup = nextEntity(buffer);
                    
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
                
                f32 t = deltaSeconds;
                
                while (e->fly.flipCountdown < t) {
                    e->xForm.pos = e->xForm.pos + e->fly.velocity * e->fly.flipCountdown;
                    t -= e->fly.flipCountdown;
                    e->fly.velocity = -(e->fly.velocity);
                    e->fly.flipCountdown += e->fly.flipInterval;
                }
                
                e->fly.flipCountdown -= deltaSeconds;
                e->xForm.pos = e->xForm.pos + e->fly.velocity * t;
                
                e->fly.fireCountdown -= deltaSeconds;
                if (e->fly.fireCountdown <= 0)
                {
                    e->fly.fireCountdown += lerp(Fly_Min_Fire_Interval, Fly_Max_Fire_Interval, randZeroToOne());
                    
                    auto bullet = nextEntity(buffer);
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
                
                if (e->blinkTime > 0) e->blinkTime -= deltaSeconds;
            } break;
            
            case Entity_Type_Boss:{
                if (e->blinkTime > 0) e->blinkTime -= deltaSeconds;
            } break;
            
            case Entity_Type_Powerup: {                
                f32 fallSpeed = 0.8f; 
                e->xForm.pos = e->xForm.pos + vec2{0, -1} * (fallSpeed * deltaSeconds);          
            } break;
        }
    }
    
    u32 i = 0;
    while (i < buffer->Count) {
        if (buffer->Base[i].markedForDeletion) {
            buffer->Base[i] = buffer->Base[(buffer->Count) - 1];
            (buffer->Count)--;
        }
        else{ 
            i++;
        }
    }    
}



f32 debugHeightOrWidth = 0;

f32 lookAtRotation(vec2 eye, vec2 target) {
    f32 alpha = acos(dot(vec2{0, 1}, normalizeOrZero(target - eye)));
    
    if (eye.x <= target.x)
        alpha = -alpha;
    
    return alpha ;
}

//input
struct key {
    bool isPressed;
    bool hasChanged;
};

// gl functions

PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = NULL;

struct input {
    union {
        key keys[9];
        
        struct {
            key upKey;
            key downKey;
            key leftKey;
            key rightKey;
            key fireKey;
            key bombKey;
            key enterKey;
            key slowMovementKey;
            key toggleEditModeKey;
        };
    };
};


bool wasPressed(key k) {
    return (k.isPressed && k.hasChanged);
}

void initGame (game_state *gameState, entity **player) {
    gameState->entities.Count = 0;
    for (u32 i = 0; i < gameState->Level.SpawnInfos.Count; i++) {
        gameState->Level.SpawnInfos[i].WasNotSpawned = true;
    }
    
    gameState->Level.Time = 0.0f;
    
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

void editModeUpdate(game_state *gameState, f32 deltaSeconds, input gameInput, f32 heightOverWidth) {
    if (gameInput.upKey.isPressed) {
        gameState->Level.Time += 3 * deltaSeconds;
    }
    
    if (gameInput.downKey.isPressed) {
        gameState->Level.Time -= 3 * deltaSeconds;
    }
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    for (u32 SpawnIndex = 0; SpawnIndex < gameState->Level.SpawnInfos.Count; SpawnIndex++) {
        auto Info = gameState->Level.SpawnInfos.Base + SpawnIndex;
        if (Info->LevelTime <= gameState->Level.Time) {
            transform collisionTransform = Info->Blueprint.xForm;
            collisionTransform.scale = 2 * Info->Blueprint.collisionRadius;
            drawCircle(collisionTransform, heightOverWidth, color{0.3f, 0.3f, 0.0f, 1.0f}, false, 16, -0.5f);
        }
    }
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
    
    Mix_PlayMusic(bgm, -1);
    Mix_VolumeMusic(30);
    
    //sfx
    Mix_AllocateChannels(2);
    Mix_Chunk *sfxBomb = loadChunk(Sfx_Bomb);
    //Mix_Chunk *sfxShoot = loadChunk(Sfx_Shoot); 
    
    if(!sfxBomb) {
        printf("Error loading music file: %s \n", Mix_GetError());
    }
    
    histogram frameRateHistogram = {};
    
    ui_context ui = {};
    
    bool doContinue = true;
    
    u64 ticks = SDL_GetPerformanceFrequency();
    u64 lastTime = SDL_GetPerformanceCounter();
    f32 scaleAlpha = 0;
    
    texture levelLayer1 = loadTexture("data/level_1.png");
    texture levelLayer2 = loadTexture("data/level_1_layer_2.png");
    texture playerTexture = loadTexture("data/Kenney/Animals/giraffe.png");
    texture bossTexture = loadTexture("data/Kenney/Animals/parrot.png");
    texture flyTexture = loadTexture("data/Kenney/Animals/chicken.png");
    texture bulletTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_014.png");  
    texture bulletPoweredUpTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_001.png");
    texture bulletMaxPoweredUpTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_006.png");
    texture bombCountTexture = loadTexture("data/Kenney/Missiles/spaceMissiles_021.png");
    texture powerupTexture = loadTexture("data/Kenney/Letter Tiles/letter_P.png");
    
    SDL_RWops* op = SDL_RWFromFile("C:/Windows/Fonts/Arial.ttf", "rb");
    s64 byteCount = op->size(op);
    u8 *data = new u8[byteCount];   
    usize ok = SDL_RWread(op, data, byteCount, 1);
    assert (ok == 1);
    
    stbtt_fontinfo StbFont;
    stbtt_InitFont(&StbFont, data, stbtt_GetFontOffsetForIndex(data,0));
    
    f32 scale = stbtt_ScaleForPixelHeight(&StbFont, 23);
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
        FontGlyph->DrawYOffset = -y0 - FontGlyph->Height;
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
    
    f32 worldHeightOverWidth = 3.0f / 4.0f; 
    f32 worldCameraHeight = 2.0f;  
    f32 worldWidth = worldCameraHeight / worldHeightOverWidth;
    
    entity _entitieEntries[100];
    game_state gameState = {};
    gameState.Level.Duration = 30.0f;
    gameState.Level.Time = 0.0f;
    gameState.Level.LayersWorldUnitsPerPixels[0] = worldWidth / levelLayer1.width;
    gameState.Level.LayersWorldUnitsPerPixels[1] = worldWidth / levelLayer2.width;
    gameState.Level.WorldHeight = gameState.Level.LayersWorldUnitsPerPixels[0] * levelLayer1.height;  
    gameState.entities = { ARRAY_WITH_COUNT(_entitieEntries) };
    gameState.bombTexture = loadTexture("data/Kenney/particlePackCircle.png");
    entity *player;
    
    
    auto bossSpawnInfo  = Push(&gameState.Level.SpawnInfos);
    assert(bossSpawnInfo);
    bossSpawnInfo->LevelTime = 10.0f;
    bossSpawnInfo->WasNotSpawned = true;
    
    bossSpawnInfo->Blueprint.xForm.pos = vec2{0.0f, 0.5f};  
    bossSpawnInfo->Blueprint.xForm.rotation = 0.0f;
    bossSpawnInfo->Blueprint.collisionRadius = 0.35f;
    bossSpawnInfo->Blueprint.xForm.scale = bossSpawnInfo->Blueprint.collisionRadius * 2.0f / (bossTexture.height * Default_World_Units_Per_Texel);
    bossSpawnInfo->Blueprint.relativeDrawCenter = vec2{0.5f, 0.5f};
    bossSpawnInfo->Blueprint.maxHp = 500;
    bossSpawnInfo->Blueprint.hp = bossSpawnInfo->Blueprint.maxHp;
    bossSpawnInfo->Blueprint.type = Entity_Type_Boss;
    bossSpawnInfo->Blueprint.collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    bossSpawnInfo->Blueprint.blinkDuration = 0.1f;
    bossSpawnInfo->Blueprint.blinkTime = 0.0f;
    
    initGame(&gameState, &player);
    
    //timer init
    
    f32 bulletSpawnCooldown = 0;
	
    f32 chickenSpawnCooldown = 5.0f;
    
    input gameInput = {};
    
    
    //game loop   
    while (doContinue) {
        for (s32 i = 0; i < ARRAY_COUNT(gameInput.keys); i++) {
            gameInput.keys[i].hasChanged = false;
        }
        
        //window events 
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:{
                    doContinue = false;
                } break;
                
                case SDL_WINDOWEVENT:{
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE:{
                            
                        }
                    }
                }
                //keyboard input             
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    
                    if (event.key.repeat > 0) 
                        break;
                    
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_A: {
                            gameInput.leftKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.leftKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_W: {
                            gameInput.upKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.upKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_S: {
                            gameInput.downKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.downKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_D: {
                            gameInput.rightKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.rightKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_K: {
                            gameInput.fireKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.fireKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_L: {
                            gameInput.bombKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.bombKey.hasChanged = true;    
                        } break;
                        
                        case SDL_SCANCODE_RETURN: {
                            gameInput.enterKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.enterKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_LSHIFT: {
                            gameInput.slowMovementKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.slowMovementKey.hasChanged = true;
                        } break;
                        
                        case SDL_SCANCODE_F1: {
                            gameInput.toggleEditModeKey.isPressed = (event.key.type == SDL_KEYDOWN);
                            gameInput.toggleEditModeKey.hasChanged = true;
                        } break;
                        
                    }
                } break;
            }
        }
        //time        
        u64 currentTime = SDL_GetPerformanceCounter();
        double deltaSeconds = (double)(currentTime - lastTime) / (double)ticks;
        lastTime = currentTime;
        
        // render begin
        s32 width, height;
        SDL_GetWindowSize(window, &width, &height);
        
        f32 heightOverWidth = height / (f32)width;        
        f32 worldPixelWidth = height / worldHeightOverWidth;
        
        ui.width = width;
        ui.height = height;
        
        debugHeightOrWidth = heightOverWidth;
        
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
        
        frameRateHistogram.values[frameRateHistogram.currentIndex] = 1 / deltaSeconds;
        frameRateHistogram.currentIndex++;
        if (frameRateHistogram.currentIndex == ARRAY_COUNT(frameRateHistogram.values)) {
            frameRateHistogram.currentIndex = 0;
        }
        
        //game update
        
        auto entities = &gameState.entities;
        entity *boss = NULL;
        
        //same as gameState.inEditMode ^= wasPressed(...)
        if (wasPressed(gameInput.toggleEditModeKey)) {
            gameState.inEditMode = !gameState.inEditMode;
        }
        
        if (gameState.inEditMode) {
            editModeUpdate(&gameState, deltaSeconds, gameInput, heightOverWidth);
        }
        else {
            gameState.Level.Time += deltaSeconds;
            gameState.Level.Time = MIN(gameState.Level.Time, gameState.Level.Duration);
            
            
            for (u32 i = 0; i <entities->Count; i++) {
                if (entities->Base[i].type == Entity_Type_Boss) {
                    boss = entities->Base + i;
                    break;
                } 
            }
            
            for (u32 SpawnIndex = 0; SpawnIndex < ARRAY_COUNT(gameState.Level.SpawnInfos); SpawnIndex++) {
                auto Info = gameState.Level.SpawnInfos.Base + SpawnIndex;
                if (Info->WasNotSpawned && (Info->LevelTime <= gameState.Level.Time)) {
                    auto Entity = nextEntity(entities);
                    if (Entity != NULL) {
                        *Entity = Info->Blueprint;
                        Info->WasNotSpawned = false;
                    }
                }
                
            }
            
            if(gameState.isGameover) {                   
                if (wasPressed(gameInput.enterKey)) {
                    initGame(&gameState, &player);
                    Mix_FadeInMusic(bgm, -1, 500);
                    gameState.isGameover = false;
                    continue;
                }
                player->xForm.rotation += 2 * PI * deltaSeconds;
                
            }
            else {
                
                //bullet movement        
                if (bulletSpawnCooldown > 0) bulletSpawnCooldown -= deltaSeconds;
                
                vec2 direction = {};
                f32 speed = 1.0f;
                
                update(&gameState, deltaSeconds);
                
                //player movement
                if (gameInput.leftKey.isPressed) {
                    direction.x -= 1;
                    //player->xForm.rotation -= 0.5f * PI * deltaSeconds;
                }
                
                if (gameInput.rightKey.isPressed) {
                    direction.x += 1; 
                    //player->xForm.rotation += 0.5f * PI * deltaSeconds;
                }
                
                if (gameInput.upKey.isPressed) {
                    direction.y += 1; 
                }
                
                if (gameInput.downKey.isPressed) {
                    direction.y -= 1; 
                }
                
                if (gameInput.slowMovementKey.isPressed) {
                    speed = speed * 0.5f;
                }
                
                direction = normalizeOrZero(direction);            
                player->xForm.pos = player->xForm.pos + direction * (speed * deltaSeconds);
                
                player->xForm.pos.y = CLAMP(player->xForm.pos.y , -1.0f + player->collisionRadius, 1.0f - player->collisionRadius);
                
                // worldWidth = windowWidth / windowHeight * worldHeight 
                // worldHeight = 2 (from -1 to 1)               
                
                player->xForm.pos.x = CLAMP(player->xForm.pos.x, -worldWidth * 0.5f + player->collisionRadius, worldWidth * 0.5f - player->collisionRadius);
                
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
                        player->xForm.rotation = lookAtRotation(player->xForm.pos, boss->xForm.pos);
                    
                    
#endif
                }
                {
                    chickenSpawnCooldown -= deltaSeconds;
                    
                    if(chickenSpawnCooldown <= 0) {
                        //unleash the chicken!
                        entity *chicken = nextEntity(entities);
                        
                        if (chicken != NULL) {               
                            chicken->xForm.rotation = 0.0f;
                            chicken->xForm.scale = 0.09f;
                            chicken->collisionRadius = chicken->xForm.scale * 0.65;
                            chicken->maxHp = 10;
                            chicken->hp = chicken->maxHp;
                            chicken->type = Entity_Type_Fly;
                            chicken->collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
                            chicken->fly.flipInterval = 1.5f;
                            chicken->fly.flipCountdown = chicken->fly.flipInterval * randZeroToOne();
                            chicken->fly.velocity = vec2{1.0f, 0.1f};    
                            chicken->xForm.pos = vec2{chicken->fly.velocity.x * chicken->fly.flipInterval * -0.5f, randZeroToOne()} + chicken->fly.velocity * (chicken->fly.flipInterval - chicken->fly.flipCountdown);   
                            chicken->relativeDrawCenter = vec2 {0.5f, 0.44f};
                            chicken->blinkDuration = 0.1f;
                        }        
                        chickenSpawnCooldown += 1.0f;
                    }
                }
                
                if(gameInput.fireKey.isPressed) {
                    if ((bulletSpawnCooldown <= 0)) {
                        entity *bullet = nextEntity(entities);
                        
                        if (bullet != NULL) {
                            bullet->xForm.pos = player->xForm.pos;
                            bullet->xForm.scale = 0.2f;
                            bullet->xForm.rotation = 0;
                            bullet->collisionRadius = bullet->xForm.scale * 0.2;
                            bullet->type = Entity_Type_Bullet;
                            bullet->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly);
                            bullet->relativeDrawCenter = vec2 {0.5f, 0.5f};
                            
                            bullet->bullet.damage = (player->player.power / 20) + 1;
                            bullet->bullet.damage = MIN(bullet->bullet.damage, 3);
                            
                            bulletSpawnCooldown += 0.05f;
                            
                            //                        Mix_PlayChannel(0, sfxShoot, 0);
                        }
                    }
                }
                
                if(wasPressed(gameInput.bombKey)) {                       
                    if (player->player.bombs > 0) {
                        
                        entity *bomb = nextEntity(entities);
                        
                        if (bomb != NULL) {
                            bomb->xForm.pos = player->xForm.pos;
                            bomb->collisionRadius = 0.1f;
                            bomb->xForm.scale = bomb->collisionRadius * 3.0f / (gameState.bombTexture.height * Default_World_Units_Per_Texel);
                            bomb->xForm.rotation = 0;
                            bomb->type = Entity_Type_Bomb;
                            bomb->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Bullet);
                            bomb->relativeDrawCenter = vec2 {0.5f, 0.5f};
                            
                            player->player.bombs--;
                            
                            Mix_PlayChannel(1, sfxBomb, 0);
                        }
                    }
                }
            }
        }
        // render
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GEQUAL, 0.1f);
        
        //background
        transform backgroundXForm = TRANSFORM_IDENTITY;
        f32 currentLevelYPosition = lerp(worldCameraHeight * 0.5f, gameState.Level.WorldHeight - worldCameraHeight * 0.5f, gameState.Level.Time / gameState.Level.Duration); 
        drawTexturedQuad(backgroundXForm, heightOverWidth, levelLayer1, White_Color, vec2 {0.5f,  currentLevelYPosition / gameState.Level.WorldHeight}, gameState.Level.LayersWorldUnitsPerPixels[0], 0.9f);
        drawTexturedQuad(backgroundXForm, heightOverWidth, levelLayer2, White_Color, vec2 {0.5f,  currentLevelYPosition / gameState.Level.WorldHeight}, gameState.Level.LayersWorldUnitsPerPixels[1], 0.8f);
        
        //draw player and all entities
        drawTexturedQuad(player->xForm, heightOverWidth, playerTexture, White_Color, player->relativeDrawCenter);
        
        for (u32 i = 0; i < entities->Count; i++) {
            auto entity = entities->Base + i;
            
            switch (entity->type) {
                
                case Entity_Type_Bullet: {
                    if (entity->bullet.damage == 1) {
                        drawTexturedQuad(entity->xForm, heightOverWidth, bulletTexture, White_Color, entity->relativeDrawCenter);
                    } 
                    else if (entity->bullet.damage == 2){
                        drawTexturedQuad(entity->xForm, heightOverWidth, bulletPoweredUpTexture, White_Color, entity->relativeDrawCenter);  
                    }
                    else {
                        drawTexturedQuad(entity->xForm, heightOverWidth, bulletMaxPoweredUpTexture, White_Color, entity->relativeDrawCenter);
                    }
                } break;
                
                case Entity_Type_Boss: {
                    if (entity->blinkTime <= 0) {
                        drawTexturedQuad(entity->xForm, heightOverWidth, bossTexture, White_Color, entity->relativeDrawCenter); 
                    } 
                    else {
                        color blinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, entity->blinkTime / entity->blinkDuration); 
                        drawTexturedQuad(entity->xForm, heightOverWidth, bossTexture, blinkColor, entity->relativeDrawCenter);          
                    }   
                } break;
                
                case Entity_Type_Fly: {
                    if (entity->blinkTime <= 0) {
                        drawTexturedQuad(entity->xForm, heightOverWidth, flyTexture, White_Color, entity->relativeDrawCenter); 
                    } 
                    else {
                        color blinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, entity->blinkTime / entity->blinkDuration); 
                        drawTexturedQuad(entity->xForm, heightOverWidth, flyTexture, blinkColor, entity->relativeDrawCenter);          
                    }   
                } break;
                
                case Entity_Type_Bomb: {
                    drawTexturedQuad(entity->xForm, heightOverWidth, gameState.bombTexture, color{randZeroToOne(), randZeroToOne(), randZeroToOne(), 1.0f}, entity->relativeDrawCenter);    
                } break;
                
                case Entity_Type_Powerup: {
                    drawTexturedQuad(entity->xForm, heightOverWidth, powerupTexture, White_Color, entity->relativeDrawCenter);
                } break;
            }    
        }
        
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        //debug framerate, hitbox and player/boss normalized x, y coordinates
        
#define DEBUG_UI
#ifdef DEBUG_UI
        drawHistogram(frameRateHistogram);
        
        for (u32 i = 0; i < entities->Count; i++) {
            transform collisionTransform = (*entities)[i].xForm;
            collisionTransform.scale = 2 * entities->Base[i].collisionRadius;
            drawCircle(collisionTransform, heightOverWidth, color{0.3f, 0.3f, 0.0f, 1.0f}, false);
            drawLine(collisionTransform, heightOverWidth, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
            drawLine(collisionTransform, heightOverWidth, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
        }        
        
#endif //DEBUG_UI
        
        //GUI
        {
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GEQUAL, 0.1f);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            
            //bomb count           
            for (int bomb = 0; bomb < player->player.bombs; bomb++){
                uiTexturedRect(&ui, bombCountTexture, 20 + bomb * (bombCountTexture.width + 5), ui.height - 250, bombCountTexture.width, bombCountTexture.height, 0, 0, bombCountTexture.width, bombCountTexture.height, White_Color);
            }
            
            //boss hp
            if (boss) {
                auto cursor = uiBeginText(&ui, &DefaultFont, 20, ui.height - 90);
                UiWrite(&cursor, "BOSS ");
                UiWrite(&cursor, "hp: %i / %i", boss->hp, boss->maxHp);
                
                uiBar(&ui, 20, ui.height - 60, ui.width - 40, 40, boss->hp / (f32) boss->maxHp, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
            }
            
            //player power
            uiBar(&ui, 20, ui.height - 200, 120,40, (player->player.power % 20) / 20.0f, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
            
            //game over
            if(gameState.isGameover) {   
                auto cursor = uiBeginText(&ui, &DefaultFont, ui.width / 2, ui.height / 2, true, color{1.0f, 0.0f, 0.0f, 1.0f}, 5.0f);
                UiWrite(&cursor, 
                        "Game Over");
                
                cursor.Color = White_Color;
                cursor.Scale = 1.0f;
                UiWrite(&cursor, "\n"
                        "press ");
                
                cursor.Color = color {0.0f, 1.0f, 0.0f, 1.0f};
                UiWrite(&cursor, "Enter ");
                
                cursor.Color = White_Color;
                UiWrite(&cursor, "to continue");
            }
            
            //rect test
            char *Items[] = {
                "New Game",
                "Settings",
                "High Score", 
                "Quit Game"};
            auto Cursor = uiBeginText(&ui, &DefaultFont, ui.width / 2, ui.height / 2, true, color{0.0f, 1.0f, 1.0f, 1.0f}, 4.0f);
            for (s32 i = ARRAY_COUNT(Items) - 1; i >= 0; i--) {
                
                
                uiRect(&ui, Cursor.CurrentX - 2, Cursor.CurrentY - 2, 5, 5, color{0.5f, 1.0f, 0.2f, 1.0f}, true);
                auto Rect = UiAlignedWrite(Cursor, vec2{0.5f, 0.0f}, Items[i]);
                
                uiRect(&ui, Rect.BottomLeft.x, Rect.BottomLeft.y, Rect.TopRight.x - Rect.BottomLeft.x, Rect.TopRight.y - Rect.BottomLeft.y, color{1.0f, 0.0f, 0.0f, 1.0f}, false);
                Cursor.CurrentY += 20 * Cursor.Scale + Rect.TopRight.y - Rect.BottomLeft.y;
            }
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_ALPHA_TEST);
        }
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