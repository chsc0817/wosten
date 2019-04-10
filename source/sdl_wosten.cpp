#include "SDL.h"
#include <stdio.h>
#include "SDL_opengl.h"
#include "SDL_image.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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

int main(int argc, char* argv[]) {
    srand (time(NULL));
    
    SDL_Window *window;                    // Declare a pointer
    
    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
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
    
    //init
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
        f32 worldWidthHalf = 1/ heightOverWidth;
        
        glViewport(0, 0, width, height);
        ui.width = width;
        ui.height = height;
        
        debugHeightOrWidth = heightOverWidth;
        
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
        
        //bomb count
        glBindTexture(GL_TEXTURE_2D, bombCountTexture.object);        
        for (int bomb = 0; bomb < player->player.bombs; bomb++){
            uiRect(&ui, 20 + bomb * (bombCountTexture.width + 5), ui.height - 250, bombCountTexture.width, bombCountTexture.height, White_Color);
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
        
        
        //boss hp
        uiBar(&ui, 20, ui.height - 60, ui.width - 40,40, boss->hp / (f32) boss->maxHp, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
       
        //player power
        uiBar(&ui, 20, ui.height - 200, 120,40, (player->player.power % 20) / 20.0f, color{1.0f, 0.0f, 0.0f, 1.0f}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
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