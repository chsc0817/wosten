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
#include <cstring>

#include "defines.h"
#include "render.h"

#define TRANSFORM_IDENTITY {{}, 0.0f, 1.0f}

struct histogram {
    f32 values[10 * 60];
    u32 currentIndex;
};

void drawHistogram(histogram h) {
    glBegin(GL_LINES);
    
    f32 maxValue = 0;
    
    for(u32 i = 0; i < (ARRAY_COUNT(h.values)); i++) {
        if (h.values[i] > maxValue) {
            maxValue = h.values[i]; 
        }
    }
    
    f32 scale = 0.5 / 60;
    
    for(u32 i = 0; i < (ARRAY_COUNT(h.values) - 1); i++) {
        glVertex2f( i * (2 / (f32) ARRAY_COUNT(h.values)) - 1, h.values[i] * scale);
        glVertex2f((i + 1) * (2 / (f32) ARRAY_COUNT(h.values)) - 1, h.values[i + 1] * scale);
    } 
    
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glVertex2f(-1, scale * 60);
    glVertex2f(1, scale * 60);
    
    glEnd();
}

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

struct entity_buffer {
    entity *entries;
    u32 entityCapacity;
    u32 entityCount;
};

entity* nextEntity(entity_buffer *buffer){
    if (buffer->entityCount < buffer->entityCapacity) {
        auto result = buffer->entries + ((buffer->entityCount)++);
        *result = {};
        
        return result;
    }
    
    return NULL;
}

struct collision {
    entity *entities[2];
};

struct game_state {
    entity_buffer entities;
    bool isGameover;    
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
    
    for(u32 i = 0; i < buffer->entityCount; i++) {
        bool doBreak = false;
        
        for (u32 j = i + 1; j < buffer->entityCount; j++){
            
            if (!(buffer->entries[i].collisionTypeMask & FLAG(buffer->entries[j].type)))
                continue;
            
            if (!(buffer->entries[j].collisionTypeMask & FLAG(buffer->entries[i].type)))
                continue;
            
            if (areIntersecting(circle{buffer->entries[i].xForm.pos, buffer->entries[i].collisionRadius}, circle{buffer->entries[j].xForm.pos, buffer->entries[j].collisionRadius})) 
            {
                if (collisionCount >= ARRAY_COUNT(collisions)) {
                    doBreak = true;
                    break;
                }
                
                auto newCollision = collisions + (collisionCount++);               
                
                if (buffer->entries[i].type < buffer->entries[j].type) {
                    newCollision->entities[0] = (buffer->entries) + i;
                    newCollision->entities[1] = (buffer->entries) + j;
                }
                else {
                    newCollision->entities[1] = (buffer->entries) + i;
                    newCollision->entities[0] = (buffer->entries) + j;
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
    
    
    
    for(u32 i = 0; i < buffer->entityCount; i++) {
        
        auto e = buffer->entries + i;
        
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
                e->xForm.scale = e->collisionRadius * 3.0f / (state->bombTexture.height * Default_Texel_Scale);
                
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
    while (i < buffer->entityCount) {
        if (buffer->entries[i].markedForDeletion) {
            buffer->entries[i] = buffer->entries[(buffer->entityCount) - 1];
            (buffer->entityCount)--;
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
        key keys[8];
        
        struct {
            key upKey;
            key downKey;
            key leftKey;
            key rightKey;
            key fireKey;
            key bombKey;
            key enterKey;
            key slowMovementKey;
        };
    };
};


bool wasPressed(key k) {
    return (k.isPressed && k.hasChanged);
}

void initGame (game_state *gameState, entity **player, entity **boss) {
    gameState->entities.entityCount = 0;
    
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
    
    *boss  = nextEntity(&gameState->entities);    
    (*boss)->xForm.pos = vec2{0.0f, 0.5f};  
    (*boss)->xForm.rotation = 0.0f;
    (*boss)->xForm.scale = 0.7f;
    (*boss)->collisionRadius = (*boss)->xForm.scale * 0.5;
    (*boss)->maxHp = 500;
    (*boss)->hp = (*boss)->maxHp;
    (*boss)->type = Entity_Type_Boss;
    (*boss)->collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    (*boss)->blinkDuration = 0.1f;
    (*boss)->blinkTime = 0.0f;
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
    //    Mix_Chunk *oof = loadChunk(Sfx_Death);   
    Mix_Chunk *sfxBomb = loadChunk(Sfx_Bomb);
    Mix_Chunk *sfxShoot = loadChunk(Sfx_Shoot); 
    
    if(!sfxBomb || !sfxShoot) {
        printf("Error loading music file: %s \n", Mix_GetError());
    }
    
    histogram frameRateHistogram = {};
    
    ui_context ui = {};
    
    bool doContinue = true;
    
    u64 ticks = SDL_GetPerformanceFrequency();
    u64 lastTime = SDL_GetPerformanceCounter();
    f32 scaleAlpha = 0;
    
    texture playerTexture = loadTexture("data/Kenney/Animals/giraffe.png");
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
    
    stbtt_fontinfo stbfont;
    stbtt_InitFont(&stbfont, data, stbtt_GetFontOffsetForIndex(data,0));
    
    f32 scale = stbtt_ScaleForPixelHeight(&stbfont, 23);
    s32 ascent;
    stbtt_GetFontVMetrics(&stbfont, &ascent,0,0);
    s32 baseline = (s32) (ascent*scale);
    
    const s32 bitmapWidth = 512;
    u8 bitmap[bitmapWidth * bitmapWidth] = {};
    s32 xOffset = 0;
    s32 yOffset = 0;
    s32 maxHight = 0;
    font defaultFont = {};
    
    //    while (text[ch]) 
    
    for (u32 i = ' '; i < 256; i++) {   
        glyph *fontGlyph = defaultFont.glyphs + i;
        fontGlyph->code = i;
        
        s32 unscaledXAdvance;  
        stbtt_GetCodepointHMetrics(&stbfont, fontGlyph->code, &unscaledXAdvance, &fontGlyph->drawXOffset);
        fontGlyph->drawXAdvance = unscaledXAdvance * scale;
        
        s32 x0, x1, y0, y1;
        stbtt_GetCodepointBitmapBox(&stbfont, fontGlyph->code, scale, scale, &x0, &y0, &x1, &y1);
        fontGlyph->width = x1 - x0;
        fontGlyph->height = y1 - y0;
        fontGlyph->drawXOffset = x0;
        // y0 is top corner, but its also negative ...
        // we draw from bottom left corner
        fontGlyph->drawYOffset = -y0 - fontGlyph->height;
        if ((xOffset + fontGlyph->width) >= bitmapWidth) {
            xOffset = 0;
            yOffset += maxHight + 1;
            maxHight = 0;
        }
        assert(fontGlyph->width <= bitmapWidth);
        
        stbtt_MakeCodepointBitmap(&stbfont, bitmap + xOffset + yOffset * bitmapWidth, fontGlyph->width, fontGlyph->height, bitmapWidth, scale, scale, fontGlyph->code);
        fontGlyph->x = xOffset;
        // we flip the texture so we need to change the y to the inverse
        fontGlyph->y = bitmapWidth - yOffset - fontGlyph->height;
        xOffset += fontGlyph->width + 1;
        maxHight = MAX(maxHight, fontGlyph->height);
        defaultFont.maxGlyphWidth = MAX(defaultFont.maxGlyphWidth, fontGlyph->width);
        defaultFont.maxGlyphHeight = MAX(defaultFont.maxGlyphHeight, fontGlyph->height);
    }
    
    defaultFont.texture = loadTexture(bitmap, bitmapWidth, bitmapWidth, 1, GL_NEAREST);    
    
    entity _entitieEntries[100];
    game_state gameState = {};
    gameState.entities = { ARRAY_WITH_COUNT(_entitieEntries) };
    gameState.bombTexture = loadTexture("data/Kenney/particlePackCircle.png");
    entity *player, *boss;
    
    initGame(&gameState, &player, &boss);
    
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
        f32 worldHeightOverWidth = 3.0f / 4.0f; 
        f32 worldWidthHalf = 1 / worldHeightOverWidth;
        f32 worldPixelWidth = height / worldHeightOverWidth;
        
        ui.width = width;
        ui.height = height;
        
        debugHeightOrWidth = heightOverWidth;
        
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glEnable(GL_SCISSOR_TEST);
//        glViewport((width - worldPixelWidth) * 0.5f, 0, worldPixelWidth, height);
        glScissor((width - worldPixelWidth) * 0.5f, 0, worldPixelWidth, height);
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        frameRateHistogram.values[frameRateHistogram.currentIndex] = 1 / deltaSeconds;
        frameRateHistogram.currentIndex++;
        if (frameRateHistogram.currentIndex == ARRAY_COUNT(frameRateHistogram.values)) {
            frameRateHistogram.currentIndex = 0;
        }
        
        auto entities = &gameState.entities;
        if(gameState.isGameover) {                   
            if (wasPressed(gameInput.enterKey)) {
                initGame(&gameState, &player, &boss);
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
            
            player->xForm.pos.x = CLAMP(player->xForm.pos.x, -worldWidthHalf + player->collisionRadius, worldWidthHalf - player->collisionRadius);
            
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
                        
                        Mix_PlayChannel(0, sfxShoot, 0);
                    }
                }
            }
            
            if(wasPressed(gameInput.bombKey)) {                       
                if (player->player.bombs > 0) {
                    
                    entity *bomb = nextEntity(entities);
                    
                    if (bomb != NULL) {
                        bomb->xForm.pos = player->xForm.pos;
                        bomb->collisionRadius = 0.1f;
                        bomb->xForm.scale = bomb->collisionRadius * 3.0f / (gameState.bombTexture.height * Default_Texel_Scale);
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
        // render
        
        
        if (boss->blinkTime <= 0) {
            drawCircle(boss->xForm, heightOverWidth, color{0.2f, 0.0f, 0.0f, 1.0f} ,true, 32);
        } 
        else {
            color blinkColor = lerp(color{0.2f, 0.0f, 0.0f, 1.0f}, color{0.0f, 0.0f, 0.2f, 1.0f}, boss->blinkTime / boss->blinkDuration); 
            drawCircle(boss->xForm, heightOverWidth, blinkColor, true, 32);
        }
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GEQUAL, 0.1f);
        //draw player and all entities
        drawTexturedQuad(player->xForm, heightOverWidth, playerTexture, White_Color, player->relativeDrawCenter);
        
        for (u32 i = 0; i < entities->entityCount; i++) {
            switch (entities->entries[i].type) {
                
                case Entity_Type_Bullet: {
                    if (entities->entries[i].bullet.damage == 1) {
                        drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, bulletTexture, White_Color, entities->entries[i].relativeDrawCenter);
                    } 
                    else if (entities->entries[i].bullet.damage == 2){
                        drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, bulletPoweredUpTexture, White_Color, entities->entries[i].relativeDrawCenter);  
                    }
                    else {
                        drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, bulletMaxPoweredUpTexture, White_Color, entities->entries[i].relativeDrawCenter);
                    }
                } break;
                
                case Entity_Type_Boss: {
                } break;
                
                case Entity_Type_Fly: {
                    if (entities->entries[i].blinkTime <= 0) {
                        drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, flyTexture, White_Color, entities->entries[i].relativeDrawCenter); 
                    } 
                    else {
                        color blinkColor = lerp(White_Color, color{0.0f, 0.0f, 0.2f, 1.0f}, entities->entries[i].blinkTime / entities->entries[i].blinkDuration); 
                        drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, flyTexture, blinkColor, entities->entries[i].relativeDrawCenter);          
                    }   
                } break;
                
                case Entity_Type_Bomb: {
                    drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, gameState.bombTexture, color{randZeroToOne(), randZeroToOne(), randZeroToOne(), 1.0f}, entities->entries[i].relativeDrawCenter);    
                } break;
                
                case Entity_Type_Powerup: {
                    drawTexturedQuad(entities->entries[i].xForm, heightOverWidth, powerupTexture, White_Color, entities->entries[i].relativeDrawCenter);
                } break;
            }    
        }
        
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        //debug framerate, hitbox and player/boss normalized x, y coordinates
        drawHistogram(frameRateHistogram);
        
        for (u32 i = 0; i < entities->entityCount; i++) {
            transform collisionTransform = entities->entries[i].xForm;
            collisionTransform.scale = 2 * entities->entries[i].collisionRadius;
            drawCircle(collisionTransform, heightOverWidth, color{0.3f, 0.3f, 0.0f, 1.0f}, false);
        }
        drawLine(player->xForm, heightOverWidth, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        drawLine(player->xForm, heightOverWidth, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
        
        drawLine(boss->xForm, heightOverWidth, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        drawLine(boss->xForm, heightOverWidth, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
        
        //GUI
        {
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GEQUAL, 0.1f);
            glDisable(GL_SCISSOR_TEST);


            //bomb count           
            for (int bomb = 0; bomb < player->player.bombs; bomb++){
                uiTexturedRect(&ui, bombCountTexture, 20 + bomb * (bombCountTexture.width + 5), ui.height - 250, bombCountTexture.width, bombCountTexture.height, 0, 0, bombCountTexture.width, bombCountTexture.height, White_Color);
            }

            //boss hp
            auto cursor = uiBeginText(&ui, &defaultFont, 20, ui.height - 90);
            uiWrite(&cursor, "BOSS ");
            uiWrite(&cursor, "hp: %i / %i", boss->hp, boss->maxHp);

            uiBar(&ui, 20, ui.height - 60, ui.width - 40, 40, boss->hp / (f32) boss->maxHp, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});


            //player power
            uiBar(&ui, 20, ui.height - 200, 120,40, (player->player.power % 20) / 20.0f, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
            
            //game over
            if(gameState.isGameover) {   
                auto cursor = uiBeginText(&ui, &defaultFont, ui.width / 2, ui.height / 2, color{1.0f, 0.0f, 0.0f, 1.0f}, 5.0f);
                uiWrite(&cursor, 
                        "Game Over");

                cursor.color = White_Color;
                cursor.scale = 1.0f;
                uiWrite(&cursor, "\n"
                                 "press ");

                cursor.color = color {0.0f, 1.0f, 0.0f, 1.0f};
                uiWrite(&cursor, "Enter ");

                cursor.color = White_Color;
                uiWrite(&cursor, "to continue");
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