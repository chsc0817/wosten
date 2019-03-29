#include "SDL.h"
#include <stdio.h>
#include "SDL_opengl.h"
#include "SDL_image.h"
#include <stdint.h>

#define u32 uint32_t
#define u64 uint64_t
#define f32 float
#define s32 int32_t 

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

#define PI 3.14159265359
#define TRANSFORM_IDENTITY {{}, 0.0f, 1.0f}


struct histogram {
    f32 values[10 * 60];
    u32 currentIndex;
};

struct vec2 {
    f32 x, y;
};

struct transform {
    vec2 pos;
    f32 rotation;
    f32 scale;
};

struct entity {
	transform xForm;
	f32 collisionRadius;
};


//vector operations
vec2 operator* (vec2 a, f32 scale) {
    vec2 result;
    result.x = a.x * scale;
    result.y = a.y * scale;
    
    return result;
}

vec2 operator+ (vec2 a, vec2 b) {
    vec2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    
    return result;
}

vec2 operator- (vec2 a, vec2 b) {
    vec2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    
    return result;
}

f32 dotProduct(vec2 a, vec2 b) {
    return (a.x *b.x + a.y * b.y);
}


f32 lengthSquared(vec2 a) {
	return dotProduct(a, a);
}


f32 length(vec2 a) {
    return sqrt(lengthSquared(a));
}


void normalize(vec2 *a) {
    if ((a->x == 0) && (a->y == 0))
        return;
    a->x = a->x * 1/(length(*a));
    a->y = a->y * 1/(length(*a));
}

//geometry
struct circle{
	vec2 pos;
	f32 radius;
};

bool areIntersecting(circle a, circle b) {
		f32 combinedRadius = a.radius + b.radius;
		vec2 distance = a.pos - b.pos;
		
		return (combinedRadius * combinedRadius >= lengthSquared(distance));
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

vec2 transformPoint(transform t, vec2 point, f32 heightOverWidth) {
    vec2 result;
    f32 cosRotation = cos(t.rotation);
    f32 sinRotation = sin(t.rotation);
    
    
    result.x = (cosRotation * point.x - sinRotation * point.y) * t.scale + t.pos.x;
    result.y = (sinRotation * point.x + cosRotation * point.y) * t.scale + t.pos.y;
    
    result.x *= heightOverWidth;
    
    return result;
}

f32 lerp(f32 a, f32 b, f32 t) {
    f32 result = (a * (1 - t)) + (b * t);
    return result;
}

//color
struct color {
	f32 r, g, b, a;
};

color lerp(color a, color b, f32 t) {
	color result;
	result.r = (a.r * (1 - t)) + (b.r * t);
	result.g = (a.g * (1 - t)) + (b.g * t);
	result.b = (a.b * (1 - t)) + (b.b * t);
	result.a = (a.a * (1 - t)) + (b.a * t);	

	return result;
}
void drawQuad(transform xForm, f32 heightOverWidth, f32 doFlip = 0.0f, vec2 center = {0.5f, 0.5f}){
	
   	glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
//assuming our textures are flipped        
        
        glTexCoord2f(0, lerp(1, 0, doFlip));  	
	vec2 v = transformPoint(xForm, vec2{0, 0} - center, heightOverWidth);
	glVertex2f(v.x, v.y);
	
        glTexCoord2f(1, lerp(1, 0, doFlip));
	v= transformPoint(xForm, vec2{1, 0} - center, heightOverWidth);
	glVertex2f(v.x, v.y);
	
        glTexCoord2f(1, lerp(0, 1, doFlip));
	v = transformPoint(xForm, vec2{1, 1} - center, heightOverWidth);
	glVertex2f(v.x, v.y);
	
	glTexCoord2f(0, lerp(0, 1, doFlip));
	v = transformPoint(xForm, vec2{0, 1} - center, heightOverWidth);
	glVertex2f(v.x, v.y);
		
	glEnd();
}

void drawCircle(transform xForm, f32 heightOverWidth, color fillColor = {0.7f, 0.0f, 0.0f, 1.0f}, u32 n = 16) {
	
	glBegin(GL_TRIANGLE_FAN);
	glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
	
	vec2 v = transformPoint(xForm, vec2{0, 0}, heightOverWidth);
	glVertex2f(v.x, v.y);
	
	for (u32 i = 0; i < n + 1; i++) { 
		xForm.rotation = i * ((2 * PI) / n); 
		v = transformPoint(xForm, vec2{0, 0.5f}, heightOverWidth);
		glVertex2f(v.x, v.y);		
	}	
	
	glEnd();
}


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
    entity player; 
    player.xForm = TRANSFORM_IDENTITY;
    player.xForm.scale = 0.1f;
    player.collisionRadius = player.xForm.scale * 0.5;
	
    entity enemy;
    enemy.xForm.pos = vec2{0.0f, 0.5f};  
    enemy.xForm.rotation = 0.0f;
    enemy.xForm.scale = 0.7f;
    enemy.collisionRadius = enemy.xForm.scale * 0.5;
    
    const f32 blinkDuration = 0.1f;
    f32 blinkTime = 0;
    
    entity bullets[100];
    u32 bulletCount = 0;
    f32 bulletSpawnCooldown = 0;
	
    
    input gameInput = {};
    
    f32 lagTime = 0;
    
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
        
        frameRateHistogram.values[frameRateHistogram.currentIndex] = 1 / deltaTime;
        frameRateHistogram.currentIndex++;
        if (frameRateHistogram.currentIndex == ARRAY_COUNT(frameRateHistogram.values)) {
            frameRateHistogram.currentIndex = 0;
        }
       
//bullet movement        
        if (bulletSpawnCooldown > 0) bulletSpawnCooldown -= deltaTime;
        if (blinkTime > 0) blinkTime -= deltaTime;
        
        vec2 direction = {};
        f32 speed = 1.0f;
        
        {
            u32 i = 0;
            while(i < bulletCount) {
                bullets[i].xForm.pos.y += speed * deltaTime;
                
				bool doDelete = false;
				
                if (bullets[i].xForm.pos.y >= 1) {
					doDelete = true;
				}
				
				if (areIntersecting(circle{bullets[i].xForm.pos, bullets[i].collisionRadius}, circle{enemy.xForm.pos, enemy.collisionRadius})) {
					doDelete = true;
					blinkTime += blinkDuration;					
				}
				
				if (doDelete) {					
                    bullets[i] = bullets[bulletCount - 1];
                    bulletCount--;
                }				
                else {
                    i++;
                }
            }
        } 
//player movement
        if (gameInput.leftKey.isPressed) {
            direction.x -= 1;
            player.xForm.rotation -= 0.5f * PI * deltaTime;
        }
        
        if (gameInput.rightKey.isPressed) {
            direction.x += 1; 
            player.xForm.rotation += 0.5f * PI * deltaTime;
        }
        
        if (gameInput.upKey.isPressed) {
            direction.y += 1; 
        }
        
        if (gameInput.downKey.isPressed) {
            direction.y -= 1; 
        }
        normalize(&direction);
        
        direction = direction * (speed * deltaTime);
        player.xForm.pos = player.xForm.pos + direction;
        
        
        if(gameInput.fireKey.isPressed) {
            if (bulletCount < 100 && (bulletSpawnCooldown <= 0)) {
                entity *bullet = bullets + bulletCount;
                bulletCount++;
                
                bullet->xForm.pos = player.xForm.pos;
                bullet->xForm.scale = 0.05f;
                bullet->xForm.rotation = 0;
                bullet->collisionRadius = bullet->xForm.scale * 0.5;
                
                bulletSpawnCooldown += 0.05f;
                
            }
        }
        
//draw entities 
        f32 heightOverWidth = 480.0f / 640.0f; 
        glViewport(0, 0, 640, 480);
        
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
		
       
		if (blinkTime <= 0) {
			drawCircle(enemy.xForm, heightOverWidth, color{0.2f, 0.0f, 0.0f, 1.0f} ,32);
		} 
		else {
			color blinkColor = lerp(color{0.2f, 0.0f, 0.0f, 1.0f}, color{0.0f, 0.0f, 0.2f, 1.0f}, blinkTime / blinkDuration); 
			drawCircle(enemy.xForm, heightOverWidth, blinkColor, 32);
		}
        
        for (u32 i = 0; i < bulletCount; i++) {
           drawCircle(bullets[i].xForm, heightOverWidth);
        }
        
        drawHistogram(frameRateHistogram);
        
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawQuad(player.xForm, heightOverWidth);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        
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