#if !defined RENDER_H
#define RENDER_H

#include "defines.h"
#include "SDL_opengl.h"
#include <stdarg.h>


//color
union color {
    struct {
        f32 r, g, b, a;
    };
    f32 values[4];
    
};

const color White_Color = {1.0f, 1.0f, 1.0f, 1.0f};

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

struct glyph {
    u32 code;
    s32 drawXAdvance;
    s32 drawXOffset, drawYOffset;
    s32 x,y,width,height;
};

struct font {
    texture texture;
    glyph glyphs[256];
    s32 maxGlyphHeight, maxGlyphWidth;
    s32 baselineYOffset;
};

struct ui_text_cursor {
    ui_context *context;
    font *font;
    f32 scale;
    s32 startX, startY;
    s32 currentX, currentY;
    color color;
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

const f32 Default_World_Units_Per_Texel = 0.01f;

void drawTexturedQuad(transform xForm, f32 heightOverWidth, texture fillTexture, color fillColor = White_Color, vec2 relativeCenter = {0.5f, 0.5f}, f32 texelScale = Default_World_Units_Per_Texel, f32 doFlip = 0.0f){
    
    vec2 texelSize = vec2{1.0f / fillTexture.width, 1.0f / fillTexture.height};
    vec2 quadSize = vec2{(f32) fillTexture.width, (f32) fillTexture.height} * texelScale;
    vec2 center = quadSize * relativeCenter;
    
    
    glBindTexture(GL_TEXTURE_2D, fillTexture.object);
    
    glBegin(GL_QUADS);
    glColor4fv(fillColor.values);
    //assuming our textures are flipped        
    
    glTexCoord2f(0, lerp(0, 1, doFlip));  	
    vec2 v = transformPoint(xForm, vec2{0, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);    
    
    glTexCoord2f(1, lerp(0, 1, doFlip));
    v= transformPoint(xForm, vec2{quadSize.x, 0} - center, heightOverWidth);
    glVertex2f(v.x, v.y);
	
    glTexCoord2f(1, lerp(1, 0, doFlip));
    v = transformPoint(xForm, quadSize - center, heightOverWidth);
    glVertex2f(v.x, v.y);
	
    glTexCoord2f(0, lerp(1, 0, doFlip));
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
	
    glDisable(GL_TEXTURE_2D);
    
    if (isFilled) {
        glBegin(GL_QUADS);
    }
    else {
        glBegin(GL_LINE_LOOP);
    }
    
    glColor4fv(rectColor.values);        

    vec2 v = uiToGl(ui, x, y);
    glVertex2f(v.x, v.y);

    v = uiToGl(ui, x + width, y);
    glVertex2f(v.x, v.y);

    v = uiToGl(ui, x + width, y + height);
    glVertex2f(v.x, v.y);

    v = uiToGl(ui, x, y + height);
    glVertex2f(v.x, v.y);

    glEnd();
    
}


void uiTexturedRect(ui_context *ui, texture texture, s32 x, s32 y, s32 width, s32 height, s32 subTextureX, s32 subTextureY, s32 subTextureWidth, s32 subTextureHeight, color rectColor)
{
    glBindTexture(GL_TEXTURE_2D, texture.object);
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glColor4fv(rectColor.values);        
    vec2 uv = {
        subTextureX / (f32) texture.width,
        subTextureY / (f32) texture.height
    };
    vec2 uvSize = {
        subTextureWidth / (f32) texture.width,
        subTextureHeight / (f32) texture.height
    };
    
    glTexCoord2f(uv.x, uv.y);  	
    vec2 v = uiToGl(ui, x, y);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(uv.x + uvSize.x, uv.y);  	
    v = uiToGl(ui, x + width, y);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(uv.x + uvSize.x, uv.y + uvSize.y);  	
    v = uiToGl(ui, x + width, y + height);
    glVertex2f(v.x, v.y);
    
    glTexCoord2f(uv.x, uv.y + uvSize.y);  	
    v = uiToGl(ui, x, y + height);
    glVertex2f(v.x, v.y);
	
    glEnd();
}

ui_text_cursor uiBeginText(ui_context *ui, font *currentFont,  s32 x, s32 y, color color = White_Color, f32 scale = 1.0f) {
    ui_text_cursor result;
    result.context = ui;
    result.startX = x;
    result.startY = y;
    result.currentX = x;
    result.currentY = y;
    result.font = currentFont;
    result.scale = scale;
    result.color = color;
    
    return result;
}

void uiText(ui_text_cursor *cursor, const char *text, u32 textCount) {
    for (u32 i = 0; i < textCount; i++) {
        if(text[i] == '\n') {
            cursor->currentX = cursor->startX;
            cursor->currentY -= (cursor->font->maxGlyphHeight + 1) *cursor->scale;
        }

        glyph *fontGlyph = cursor->font->glyphs + text[i];
        if (fontGlyph->code == 0) {
            continue;
        }
        
        uiTexturedRect(cursor->context, cursor->font->texture, cursor->currentX + fontGlyph->drawXOffset * cursor->scale, cursor->currentY + fontGlyph->drawYOffset * cursor->scale, fontGlyph->width * cursor->scale, fontGlyph->height * cursor->scale, fontGlyph->x, fontGlyph->y, fontGlyph->width, fontGlyph->height,  cursor->color);
        cursor->currentX += fontGlyph->drawXAdvance * cursor->scale;
    }
}

void uiWrite(ui_text_cursor *cursor, const char *format, ...) {
    va_list parameters;
    va_start(parameters, format);
    char buffer[2048];
    
    u32 byteCount = vsnprintf(ARRAY_WITH_COUNT(buffer), format, parameters);
    uiText(cursor, buffer, byteCount);
    
    va_end(parameters);
}

void uiBar(ui_context *ui, s32 x, s32 y, s32 width, s32 height, f32 percentage, color emptyColor, color fullColor) {
    
    uiRect(ui, x, y, width, height, emptyColor, false);
    uiRect(ui, x, y, width * CLAMP(percentage, 0, 1), height, fullColor);
    
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


texture loadTexture(u8 *data, s32 width, s32 height, u8 bytesPerPixel, GLenum filter = GL_LINEAR, bool flipY = true)
{
    if (flipY)
    {
        u32 rowByteCount = width * bytesPerPixel;
        u8 *tmp = new u8[rowByteCount];
        
        for (u32 y = 0; y < height / 2; y++)
        {
            memcpy(tmp, data + y * rowByteCount, rowByteCount);
            memcpy(data + y * rowByteCount, data + (height - 1 - y) * rowByteCount, rowByteCount);
            memcpy(data + (height - 1 - y) * rowByteCount, tmp, rowByteCount);
        }
        
        delete[] tmp;
    }
    
    texture result;
    
    glGenTextures(1, &result.object);
    glBindTexture(GL_TEXTURE_2D, result.object);
    
    switch(bytesPerPixel) {
        case 1: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
            GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);           
        } break;
        
        case 4: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        } break;
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    
    result.width = width;
    result.height = height;
    
    return result;
}

texture loadTexture(const char *path, GLenum filter = GL_LINEAR){
    SDL_Surface *textureSurface = IMG_Load(path);
    
    if (textureSurface->format->BytesPerPixel == 1){
        for (s32 i = 0; i < textureSurface->w * textureSurface->h; i++) {
            ((u8*)textureSurface->pixels)[i] = 255 - ((u8*)textureSurface->pixels)[i];
        }
    }
    
    texture result = loadTexture((u8*) textureSurface->pixels, textureSurface->w, textureSurface->h, textureSurface->format->BytesPerPixel, filter);    
    SDL_FreeSurface(textureSurface);
    return result;
}

#endif // RENDER_H