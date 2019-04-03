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
    Entity_Type_Powerup,

    
    Entity_Type_Count
};

struct entity {
    transform xForm;
    f32 collisionRadius;
    u32 hp, maxHp;
    u32 power;
    entity_type type;
    bool markedForDeletion;
    u32 collisionTypeMask;
    f32 blinkTime, blinkDuration;
    vec2 relativeDrawCenter;
    
    union {
        struct {
           f32 flipCountdown, flipInterval;
           vec2 velocity;
        } fly;
    };
    
};

entity* nextEntity(entity *entities, u32 entityCapacity, u32 *entityCount){
    if (*entityCount < entityCapacity) {
        auto result = entities + ((*entityCount)++);
        *result = {};
        
        return result;
    }
    
    return NULL;
}

f32 randZeroToOne(){
    return ((rand()  %  RAND_MAX) / (f32) RAND_MAX);
}

f32 randMinusOneToOne(){
    return (randZeroToOne() * 2 - 1.0f);
}

void update(entity *entities, u32 *entityCount, f32 deltaSeconds){    
    for(u32 i = 0; i < *entityCount; i++) {
        
        auto e = entities + i;
        
        switch(e->type) {
            case Entity_Type_Bullet: {
                
                vec2 dir = normalizeOrZero(transformPoint(e->xForm, {0, 1}, 1) - e->xForm.pos);
                
                //e->xForm.pos.y += speed * deltaSeconds;
                f32 speed = 1.0f; 
                e->xForm.pos = e->xForm.pos + dir * (speed * deltaSeconds);
                
                bool doDelete = false;
                
                if (e->xForm.pos.y >= 1) {
                    doDelete = true;
                }
                
                for (u32 j = 0; j < *entityCount; j++){
                    
                    if (i == j)
                        continue;
                    
                    if (!(e->collisionTypeMask & FLAG(entities[j].type)))
                        continue;
                    
                    if (!(entities[j].collisionTypeMask & FLAG(e->type)))
                        continue;
                    
                    if (areIntersecting(circle{e->xForm.pos, e->collisionRadius}, circle{entities[j].xForm.pos, entities[j].collisionRadius})) 
                    {
                        doDelete = true;
                        entities[j].blinkTime = entities[j].blinkDuration;	
                        
                        if (entities[j].hp > 0)
                            entities[j].hp -= e->power;;
                        
                    }
                }
                e->markedForDeletion = doDelete;
                
            } break;
            
            case Entity_Type_Fly:{
                if (e->hp == 0) {
                    e->markedForDeletion = true;
//TODO: figure out why ARRAY_WITH_COUNT(entities) returns 0
                    entity *powerup = nextEntity(entities,100, entityCount);
                    powerup->xForm.pos = e->xForm.pos;  
                    powerup->xForm.rotation = 0.0f;
                    powerup->xForm.scale = 0.02f;
                    powerup->collisionRadius = powerup->xForm.scale * 1.3f;
                    powerup->type = Entity_Type_Powerup;
                    powerup->collisionTypeMask = FLAG(Entity_Type_Player); 
                    powerup->relativeDrawCenter = vec2 {0.5f, 0.5f};
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
                
                if (e->blinkTime > 0) e->blinkTime -= deltaSeconds;
            } break;
            
            case Entity_Type_Boss:{
                if (e->blinkTime > 0) e->blinkTime -= deltaSeconds;
            } break;
            
            case Entity_Type_Powerup: {
                
                f32 fallSpeed = 0.8f; 
                e->xForm.pos = e->xForm.pos + vec2{0, -1} * (fallSpeed * deltaSeconds);
                
                for (u32 j = 0; j < *entityCount; j++) {
                    if (i == j)
                        continue;
                    
                    if (!(e->collisionTypeMask & FLAG(entities[j].type)))
                        continue;
                    
                    if (!(entities[j].collisionTypeMask & FLAG(e->type)))
                        continue;
                    
                    if (areIntersecting(circle{e->xForm.pos, e->collisionRadius}, circle{entities[j].xForm.pos, entities[j].collisionRadius})) {
                        e->markedForDeletion = true;
                        entities[j].power++;     
                    }
                }                   
            }
        }
    }
    
    u32 i = 0;
    while (i < *entityCount) {
        if (entities[i].markedForDeletion) {
            entities[i] = entities[(*entityCount) - 1];
            (*entityCount)--;
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
        key keys[5];
        
        struct {
            key upKey;
            key downKey;
            key leftKey;
            key rightKey;
            key fireKey;
        };
    };
};

int main(int argc, char* argv[]) {
    srand (time(NULL));
    
    SDL_Window *window;                    // Declare a pointer
    
    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL2
    
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
    texture powerupTexture = loadTexture("data/Kenney/Letter Tiles/letter_P.png");
  
    entity entities[100];
    u32 entityCount = 0;        
    
    entity *player = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount); 
    player->xForm = TRANSFORM_IDENTITY;
    player->xForm.scale = 0.1f;
    player->collisionRadius = player->xForm.scale * 0.5;
    player->maxHp = 1;
    player->hp = player->maxHp;
    player->power = 1;
    player->type = Entity_Type_Player;
    player->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly) | FLAG(Entity_Type_Powerup);
    player->relativeDrawCenter = vec2 {0.5f, 0.4f};	
    
    auto boss  = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);    
    boss->xForm.pos = vec2{0.0f, 0.5f};  
    boss->xForm.rotation = 0.0f;
    boss->xForm.scale = 0.7f;
    boss->collisionRadius = boss->xForm.scale * 0.5;
    boss->maxHp = 500;
    boss->hp = boss->maxHp;
    boss->type = Entity_Type_Boss;
    boss->collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    boss->blinkDuration = 0.1f;
    
    auto fly = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);
    fly->xForm.pos = vec2{-0.7f, 0.3f};  
    fly->xForm.rotation = 0.0f;
    fly->xForm.scale = 0.09f;
    fly->collisionRadius = fly->xForm.scale * 0.65;
    fly->maxHp = 20;
    fly->hp = fly->maxHp;
    fly->type = Entity_Type_Fly;
    fly->collisionTypeMask = FLAG(Entity_Type_Player) | FLAG(Entity_Type_Bullet); 
    fly->fly.flipInterval = 1.5f;
    fly->fly.flipCountdown = fly->fly.flipInterval;
    fly->fly.velocity = vec2{1.0f, 0.1f};    
    fly->relativeDrawCenter = vec2 {0.5f, 0.44f};
    fly->blinkDuration = 0.1f;
        
//    auto powerup = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);
//    powerup->xForm.pos = vec2{-0.5f, 0.3f};  
//    powerup->xForm.rotation = 0.0f;
//    powerup->xForm.scale = 0.02f;
//    powerup->collisionRadius = powerup->xForm.scale * 1.3f;
//    powerup->type = Entity_Type_Powerup;
//    powerup->collisionTypeMask = FLAG(Entity_Type_Player); 
//    powerup->relativeDrawCenter = vec2 {0.5f, 0.5f};
    //timer init
    
    for (u32 i = 0; i < entityCount; i++)
        entities[i].blinkTime = 0.0f;
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
        
        //bullet movement        
        if (bulletSpawnCooldown > 0) bulletSpawnCooldown -= deltaSeconds;
              
        vec2 direction = {};
        f32 speed = 1.0f;
        
        update(entities, &entityCount, deltaSeconds);
        
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
        direction = normalizeOrZero(direction);
        direction = direction * (speed * deltaSeconds);
        player->xForm.pos = player->xForm.pos + direction;
        
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
            entity *chicken = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);

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
                chicken->xForm.pos = vec2{chicken->fly.velocity.x * chicken->fly.flipInterval * -0.5f, randMinusOneToOne()} + chicken->fly.velocity * (chicken->fly.flipInterval - chicken->fly.flipCountdown);   
                chicken->relativeDrawCenter = vec2 {0.5f, 0.44f};
                chicken->blinkDuration = 0.1f;
            }        
            chickenSpawnCooldown += 1.0f;
        }
    }
        
        if(gameInput.fireKey.isPressed) {
            if ((bulletSpawnCooldown <= 0)) {
                entity *bullet = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);
                
                if (bullet != NULL) {
                    bullet->xForm.pos = player->xForm.pos;
                    bullet->xForm.scale = 0.2f;
                    bullet->xForm.rotation = 0;
                    bullet->collisionRadius = bullet->xForm.scale * 0.2;
                    bullet->type = Entity_Type_Bullet;
                    bullet->collisionTypeMask = (1 << Entity_Type_Boss) | (1 << Entity_Type_Fly);
                    bullet->relativeDrawCenter = vec2 {0.5f, 0.5f};
                    
                    bullet->power = (player->power / 20) + 1;
                    bullet->power = MIN(bullet->power, 3);
                    
                    bulletSpawnCooldown += 0.05f;
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
        
        drawTexturedQuad(player->xForm, heightOverWidth, playerTexture, whiteColor, player->relativeDrawCenter);
        
        for (u32 i = 0; i < entityCount; i++) {
            switch (entities[i].type) {
                
                case Entity_Type_Bullet: {
                    if (entities[i].power == 1) {
                        drawTexturedQuad(entities[i].xForm, heightOverWidth, bulletTexture, whiteColor, entities[i].relativeDrawCenter);
                    } 
                    else if (entities[i].power == 2){
                        drawTexturedQuad(entities[i].xForm, heightOverWidth, bulletPoweredUpTexture, whiteColor, entities[i].relativeDrawCenter);  
                    }
                    else {
                        drawTexturedQuad(entities[i].xForm, heightOverWidth, bulletMaxPoweredUpTexture, whiteColor, entities[i].relativeDrawCenter);
                    }
                } break;
                
                case Entity_Type_Boss: {
                } break;
                
                case Entity_Type_Fly: {
                    if (entities[i].blinkTime <= 0) {
			drawTexturedQuad(entities[i].xForm, heightOverWidth, flyTexture, whiteColor, entities[i].relativeDrawCenter); 
                    } 
                    else {
			color blinkColor = lerp(whiteColor, color{0.0f, 0.0f, 0.2f, 1.0f}, entities[i].blinkTime / entities[i].blinkDuration); 
			drawTexturedQuad(entities[i].xForm, heightOverWidth, flyTexture, blinkColor, entities[i].relativeDrawCenter);          
                    }   
                } break;
                
                case Entity_Type_Powerup: {
                    drawTexturedQuad(entities[i].xForm, heightOverWidth, powerupTexture, whiteColor, entities[i].relativeDrawCenter);
                } break;
            }    
        }
        
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        
        drawHistogram(frameRateHistogram);
        
        for (u32 i = 0; i < entityCount; i++) {
            transform collisionTransform = entities[i].xForm;
            collisionTransform.scale = 2 * entities[i].collisionRadius;
            drawCircle(collisionTransform, heightOverWidth, color{0.3f, 0.3f, 0.0f, 1.0f}, false);
        }
        
        drawLine(player->xForm, heightOverWidth, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        drawLine(player->xForm, heightOverWidth, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
        drawLine(boss->xForm, heightOverWidth, vec2{0, 0}, vec2{1, 0}, color{1.0f, 0.0f, 0.0f, 1.0f});
        drawLine(boss->xForm, heightOverWidth, vec2{0, 0}, vec2{0, 1}, color{0.0f, 1.0f, 0.0f, 1.0f});
        
        uiRect(&ui, 20, ui.height - 80, (ui.width - 40), 40, color{1.0f, 0.0f, 0.0f, 1.0f});
        uiRect(&ui, 20, ui.height - 80, (ui.width - 40) * boss->hp / boss->maxHp, 40, color{0.0f, 1.0f, 0.0f, 1.0f});
        
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