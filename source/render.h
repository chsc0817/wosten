
#if !defined RENDER_H
#define RENDER_H

#include "defines.h"
#include "SDL_opengl.h"

//color
union color {
    struct {
        f32 r, g, b, a;
    };
    f32 values[4];
   
};

const color whiteColor = {1.0f, 1.0f, 1.0f, 1.0f};

color lerp(color a, color b, f32 t) {
	color result;
	result.r = (a.r * (1 - t)) + (b.r * t);
	result.g = (a.g * (1 - t)) + (b.g * t);
	result.b = (a.b * (1 - t)) + (b.b * t);
	result.a = (a.a * (1 - t)) + (b.a * t);	
    
	return result;
}

struct ui_context {
    s32 width, height;
};

struct texture {
    s32 width, height;
    GLuint object;
};


void drawQuad(transform xForm, f32 heightOverWidth, vec2 center = {0.5f, 0.5f}){
	
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    vec2 v = transformPoint(xForm, vec2{0, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);

    v= transformPoint(xForm, vec2{1, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);

    v = transformPoint(xForm, vec2{1, 1} - center, heightOverWidth);
    glVertex2f(v.x, v.y);

    v = transformPoint(xForm, vec2{0, 1} - center, heightOverWidth);
    glVertex2f(v.x, v.y);

    glEnd();
}

void drawTexturedQuad(transform xForm, f32 heightOverWidth, texture fillTexture, color fillColor = whiteColor, vec2 relativeCenter = {0.5f, 0.5f}, f32 texelScale = 0.01f, f32 doFlip = 0.0f){
    
    vec2 texelSize = vec2{1.0f / fillTexture.width, 1.0f / fillTexture.height};
    vec2 quadSize = vec2{(f32) fillTexture.width, (f32) fillTexture.height} * texelScale;
    vec2 center = quadSize * relativeCenter;
    
    
    glBindTexture(GL_TEXTURE_2D, fillTexture.object);
    
    glBegin(GL_QUADS);
    glColor4fv(fillColor.values);
    //assuming our textures are flipped        
    
    glTexCoord2f(0, lerp(1, 0, doFlip));  	
    vec2 v = transformPoint(xForm, vec2{0, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);    
    
    glTexCoord2f(1, lerp(1, 0, doFlip));
    v= transformPoint(xForm, vec2{quadSize.x, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);
	
    glTexCoord2f(1, lerp(0, 1, doFlip));
    v = transformPoint(xForm, quadSize - center, heightOverWidth);
    glVertex2f(v.x, v.y);
	
    glTexCoord2f(0, lerp(0, 1, doFlip));
    v = transformPoint(xForm, vec2{0, quadSize.y} - center, heightOverWidth);
    glVertex2f(v.x, v.y);
    
    glEnd();
}

vec2 uiToGl (ui_context *ui, s32 x, s32 y){
    vec2 result;
    
    result.x = (2.0f * x / ui->width) - 1.0f;
    result.y = (2.0f * y / ui->height) - 1.0f;
    
    return result;
}

void uiRect(ui_context *ui, s32 x, s32 y, s32 width, s32 height, color rectColor, bool isFilled = true){
	
    glBegin(GL_QUADS);
    glColor4fv(rectColor.values);        
    
    glTexCoord2f(0, 1);  	
	vec2 v = uiToGl(ui, x, y);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(1, 1);  	
    v = uiToGl(ui, x + width, y);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(1, 0);  	
    v = uiToGl(ui, x + width, y + height);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(0, 0);  	
    v = uiToGl(ui, x, y + height);
    glVertex2f(v.x, v.y);
	
	glEnd();
}

void drawCircle(transform xForm, f32 heightOverWidth, color fillColor = {0.7f, 0.0f, 0.0f, 1.0f}, bool isFilled = true, u32 n = 16) {
    if (isFilled) {
	glBegin(GL_TRIANGLE_FAN);
	glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
	
	vec2 v = transformPoint(xForm, vec2{0, 0}, heightOverWidth);
	glVertex2f(v.x, v.y);
	
	for (u32 i = 0; i < n + 1; i++) { 
		xForm.rotation = i * ((2 * PI) / n); 
		v = transformPoint(xForm, vec2{0, 0.5f}, heightOverWidth);
		glVertex2f(v.x, v.y);		
	}	
    }
    else {
        glBegin(GL_LINE_LOOP);
	glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
	
        for (u32 i = 0; i < n + 1; i++) { 
		xForm.rotation = i * ((2 * PI) / n); 
		auto v = transformPoint(xForm, vec2{0, 0.5f}, heightOverWidth);
		glVertex2f(v.x, v.y);		
	}	
    }
	
	glEnd();
}

void drawLine(transform xForm, f32 heightOverWidth, vec2 from, vec2 to, color lineColor) {
    glBegin(GL_LINES);
    
    glColor4fv(lineColor.values);
    
	vec2 v = transformPoint(xForm, from, heightOverWidth);
	glVertex2f(v.x, v.y);
    
    v = transformPoint(xForm, to, heightOverWidth);
	glVertex2f(v.x, v.y);
    
    glEnd();
}


texture loadTexture(const char *path){
    SDL_Surface *textureSurface = IMG_Load(path);
    texture result;
    
    glGenTextures(1, &result.object);
    glBindTexture(GL_TEXTURE_2D, result.object);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSurface->w, textureSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureSurface->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    result.width = textureSurface->w;
    result.height = textureSurface->h;
    
    SDL_FreeSurface(textureSurface);
    return result;
}

#endif // RENDER_H