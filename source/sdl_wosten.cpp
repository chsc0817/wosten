#include "SDL.h"
#include <stdio.h>
#include "SDL_opengl.h"
#include "SDL_image.h"
#include <stdint.h>

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
    Entity_Type_Boss,
    Entity_Type_Player,
    Entity_Type_Bullet,
    Entity_Type_Fly,
    
    Entity_Type_Count
};

struct entity {
	transform xForm;
	f32 collisionRadius;
    u32 hp, maxHp;
    entity_type type;
    bool markedForDeletion;
    u32 collisionTypeMask;
    f32 blinkTime, blinkDuration;
};

entity* nextEntity(entity *entities, u32 entityCapacity, u32 *entityCount){
    if (*entityCount < entityCapacity) {
        auto result = entities + ((*entityCount)++);
        *result = {};
        
        return result;
    }
    
    return NULL;
}

void update(entity *entities, u32 *entityCount, f32 deltaSeconds){
    
    
    for(u32 i = 0; i < *entityCount; i++) {
        
        auto e = entities + i;
        
        switch(e->type) {
            case Entity_Type_Bullet: {
                
                vec2 dir = normalizeOrZero(transformPoint(e->xForm, {0, 1}, 1) - e->xForm.pos);
                
                //e->xForm.pos.y += speed * deltaTime;
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
                            entities[j].hp--;
                        
                    }
                }
                e->markedForDeletion = doDelete;
                
            } break;
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
    
    GLuint playerTexture;
    {
        SDL_Surface *playerTextureSurface = IMG_Load("data/kenney_animalpackredux/PNG/Round/giraffe.png");
        
        glGenTextures(1, &playerTexture);
        glBindTexture(GL_TEXTURE_2D, playerTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, playerTextureSurface->w, playerTextureSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, playerTextureSurface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        
        SDL_FreeSurface(playerTextureSurface);
    }
    
    GLuint bulletTexture;
    {
        SDL_Surface *surface = IMG_Load("data/heart.png");
        
        glGenTextures(1, &bulletTexture);
        glBindTexture(GL_TEXTURE_2D, bulletTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        SDL_FreeSurface(surface);
    }
    
    entity entities[100];
    u32 entityCount = 0;
    
    
    
    entity *player = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount); 
    player->xForm = TRANSFORM_IDENTITY;
    player->xForm.scale = 0.1f;
    player->collisionRadius = player->xForm.scale * 0.5;
    player->maxHp = 1;
    player->hp = player->maxHp;
    player->type = Entity_Type_Player;
    player->collisionTypeMask = FLAG(Entity_Type_Boss) | FLAG(Entity_Type_Bullet) | FLAG(Entity_Type_Fly);
	
    
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
    
    
    //timer init
    
    for (u32 i = 0; i < entityCount; i++)
        entities[i].blinkTime = 0.0f;
    f32 bulletSpawnCooldown = 0;
	
    
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
        double deltaTime = (double)(currentTime - lastTime) / (double)ticks;
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
        
        
        frameRateHistogram.values[frameRateHistogram.currentIndex] = 1 / deltaTime;
        frameRateHistogram.currentIndex++;
        if (frameRateHistogram.currentIndex == ARRAY_COUNT(frameRateHistogram.values)) {
            frameRateHistogram.currentIndex = 0;
        }
        
        //bullet movement        
        if (bulletSpawnCooldown > 0) bulletSpawnCooldown -= deltaTime;
        if (boss->blinkTime > 0) boss->blinkTime -= deltaTime;
        
        vec2 direction = {};
        f32 speed = 1.0f;
        
        update(entities, &entityCount, deltaTime);
        
        //player movement
        if (gameInput.leftKey.isPressed) {
            direction.x -= 1;
            //player->xForm.rotation -= 0.5f * PI * deltaTime;
        }
        
        if (gameInput.rightKey.isPressed) {
            direction.x += 1; 
            //player->xForm.rotation += 0.5f * PI * deltaTime;
        }
        
        if (gameInput.upKey.isPressed) {
            direction.y += 1; 
        }
        
        if (gameInput.downKey.isPressed) {
            direction.y -= 1; 
        }
        direction = normalizeOrZero(direction);
        direction = direction * (speed * deltaTime);
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
        
        
        if(gameInput.fireKey.isPressed) {
            if ((bulletSpawnCooldown <= 0)) {
                entity *bullet = nextEntity(ARRAY_WITH_COUNT(entities), &entityCount);
                
                if (bullet != NULL) {
                    bullet->xForm.pos = player->xForm.pos;
                    bullet->xForm.scale = 0.05f;
                    bullet->xForm.rotation = player->xForm.rotation;
                    bullet->collisionRadius = bullet->xForm.scale * 0.5;
                    bullet->type = Entity_Type_Bullet;
                    bullet->collisionTypeMask = (1 << Entity_Type_Boss) | (1 << Entity_Type_Fly);
                    
                    bulletSpawnCooldown += 0.05f;
                }
            }
        }
        
        // render
        
        
		if (boss->blinkTime <= 0) {
			drawCircle(boss->xForm, heightOverWidth, color{0.2f, 0.0f, 0.0f, 1.0f} ,32);
		} 
		else {
			color blinkColor = lerp(color{0.2f, 0.0f, 0.0f, 1.0f}, color{0.0f, 0.0f, 0.2f, 1.0f}, boss->blinkTime / boss->blinkDuration); 
			drawCircle(boss->xForm, heightOverWidth, blinkColor, 32);
		}
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindTexture(GL_TEXTURE_2D, playerTexture);
        drawQuad(player->xForm, heightOverWidth);
        
        glBindTexture(GL_TEXTURE_2D, bulletTexture);
        for (u32 i = 0; i < entityCount; i++) {
            //drawCircle(bullets[i].xForm, heightOverWidth);
            if (entities[i].type == Entity_Type_Bullet)
                drawQuad(entities[i].xForm, heightOverWidth);
        }
        
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        
        drawHistogram(frameRateHistogram);
        
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