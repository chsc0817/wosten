#if !defined RENDER_H
#define RENDER_H

#include "defines.h"
#include "SDL_opengl.h"
#include <stdarg.h>

enum Text_Align
{
    Align_Left,
    Align_Center,
    Align_Right
};

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

struct histogram {
    f32 values[10 * 60];
    u32 currentIndex;
};

struct ui_context {
    s32 width, height;
};

struct texture {
    s32 width, height;
    GLuint object;
};

struct glyph {
    u32 Code;
    s32 DrawXAdvance;
    s32 DrawXOffset, DrawYOffset;
    s32 X, Y, Width, Height;
};

struct font {
    texture Texture;
    glyph Glyphs[256];
    s32 MaxGlyphHeight, MaxGlyphWidth;
    s32 BaselineYOffset;
};

struct ui_text_cursor {
    ui_context *Context;
    font *Font;
    f32 Scale;
    s32 StartX, StartY;
    s32 CurrentX, CurrentY;
    rect Rect;
    bool RectIsInitialized;
    bool DoRender;
    color Color;
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

void drawTexturedQuad(transform xForm, f32 heightOverWidth, texture fillTexture, color fillColor = White_Color, vec2 relativeCenter = {0.5f, 0.5f}, f32 texelScale = Default_World_Units_Per_Texel, f32 z = 0, f32 doFlip = 0.0f){
    
    vec2 texelSize = vec2{1.0f / fillTexture.width, 1.0f / fillTexture.height};
    vec2 quadSize = vec2{(f32) fillTexture.width, (f32) fillTexture.height} * texelScale;
    vec2 center = quadSize * relativeCenter;
    
    
    glBindTexture(GL_TEXTURE_2D, fillTexture.object);
    
    glBegin(GL_QUADS);
    glColor4fv(fillColor.values);
    //assuming our textures are flipped        
    
    glTexCoord2f(0, lerp(0, 1, doFlip));  	
    vec2 v = transformPoint(xForm, vec2{0, 0} - center, heightOverWidth);
    glVertex3f(v.x, v.y, z);    
    
    glTexCoord2f(1, lerp(0, 1, doFlip));
    v= transformPoint(xForm, vec2{quadSize.x, 0} - center, heightOverWidth);
    glVertex3f(v.x, v.y, z);
	
    glTexCoord2f(1, lerp(1, 0, doFlip));
    v = transformPoint(xForm, quadSize - center, heightOverWidth);
    glVertex3f(v.x, v.y, z);
	
    glTexCoord2f(0, lerp(1, 0, doFlip));
    v = transformPoint(xForm, vec2{0, quadSize.y} - center, heightOverWidth);
    glVertex3f(v.x, v.y, z);
    
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

ui_text_cursor uiBeginText(ui_context *Context, font *CurrentFont,  s32 X, s32 Y, bool DoRender = true, color Color = White_Color, f32 Scale = 1.0f) {
    ui_text_cursor Result;
    Result.Context = Context;
    Result.StartX = X;
    Result.StartY = Y;
    Result.CurrentX = X;
    Result.CurrentY = Y;
    Result.Font = CurrentFont;
    Result.Scale = Scale;
    Result.Color = Color;
    Result.DoRender = DoRender;
    Result.Rect = MakeRect(X, Y, X, Y);
    Result.RectIsInitialized = false;
    
    return Result;
}

void UiText(ui_text_cursor *Cursor, const char *Text, u32 TextCount) {
    for (u32 i = 0; i < TextCount; i++) {
        if(Text[i] == '\n') {
            Cursor->CurrentX = Cursor->StartX;
            Cursor->CurrentY -= (Cursor->Font->MaxGlyphHeight + 1) *Cursor->Scale;
        }
        
        glyph *FontGlyph = Cursor->Font->Glyphs + Text[i];
        if (FontGlyph->Code == 0) {
            continue;
        }
        
        vec2 BottomLeft = {
            Cursor->CurrentX + FontGlyph->DrawXOffset * Cursor->Scale,
            Cursor->CurrentY + FontGlyph->DrawYOffset * Cursor->Scale};
        
        vec2 TopRight = {
            BottomLeft.x + FontGlyph->Width * Cursor->Scale,
            BottomLeft.y + FontGlyph->Height * Cursor->Scale};
        
        rect GlyphRect = MakeRect(BottomLeft, TopRight);
        
        if (Cursor->RectIsInitialized) { 
            Cursor->Rect = Merge(GlyphRect, Cursor->Rect);
        }
        else {
            Cursor->Rect = GlyphRect;
            Cursor->RectIsInitialized = true;
        } 
        
        if (Cursor->DoRender) {
            uiTexturedRect(Cursor->Context, Cursor->Font->Texture, BottomLeft.x, BottomLeft.y, FontGlyph->Width * Cursor->Scale, FontGlyph->Height * Cursor->Scale, FontGlyph->X, FontGlyph->Y, FontGlyph->Width, FontGlyph->Height,  Cursor->Color);
        }
        
        Cursor->CurrentX += FontGlyph->DrawXAdvance * Cursor->Scale;
    }
}

void UiWriteVA(ui_text_cursor *Cursor, const char *Format, va_list Parameters) {
    char Buffer[2048];
    
    u32 ByteCount = vsnprintf(ARRAY_WITH_COUNT(Buffer), Format, Parameters);
    UiText(Cursor, Buffer, ByteCount);
}

void UiWrite(ui_text_cursor *Cursor, const char *Format, ...) {
    va_list Parameters;
    va_start(Parameters, Format);
    UiWriteVA(Cursor, Format, Parameters);
    va_end(Parameters);
}

rect UiAlignedWrite(ui_text_cursor Cursor, vec2 Alignment, const char *Format, ...) {
    
    auto DummyCursor = uiBeginText(Cursor.Context, Cursor.Font, Cursor.CurrentX, Cursor.CurrentY, false, White_Color, Cursor.Scale);
    
    va_list Parameters;
    va_start(Parameters, Format);
    
    UiWriteVA(&DummyCursor, Format, Parameters); 
    
    
    vec2 DummySize = DummyCursor.Rect.TopRight - DummyCursor.Rect.BottomLeft;
    vec2 Offset = vec2{(f32) Cursor.CurrentX, (f32)Cursor.CurrentY} - DummyCursor.Rect.BottomLeft - DummySize * Alignment;
    
    DummyCursor.Rect.BottomLeft = DummyCursor.Rect.BottomLeft + Offset;
    DummyCursor.Rect.TopRight = DummyCursor.Rect.TopRight + Offset;
    
    Cursor = uiBeginText(Cursor.Context, Cursor.Font, Cursor.CurrentX + Offset.x, Cursor.CurrentY + Offset.y, Cursor.DoRender, Cursor.Color, Cursor.Scale);
    UiWriteVA(&Cursor, Format, Parameters); 
    
    va_end(Parameters);
    
    return DummyCursor.Rect;
}
/*
void UiTextWithBorder(ui_text_cursor *cursor,Rect Border, const char *text, u32 TextCount, Text_Align Alignment) {
    const char *CurrentLine = "";
    f32 maxLineLength = Border.BottomRight.x - Border.TopLeft.x;
    f32 CurrentLineLength = 0.0f;
    f32 maxLines = Border.TopLeft.y - Border.BottomRight.y;
    
    for (u32 i = 0; i < TextCount; i++) {
        glyph *fontGlyph = cursor->font->glyphs + text[i];
        
        if((text[i] == '\n') || (CurrentLineLength + fontGlyph->drawXAdvance * cursor->scale) >= maxLineLength) {
            cursor->currentY -= (cursor->font->maxGlyphHeight + 1) *cursor->scale;
            
            maxLines -= (cursor->font->maxGlyphHeight + 1) *cursor->scale;
            if (maxLines <= 0)
                return;
                
            cursor->currentX = cursor->startX;
            CurrentLine = "";
            CurrentLineLength = 0.0f;
        }
        
        uiTexturedRect(cursor->context, cursor->font->texture, cursor->currentX + fontGlyph->drawXOffset * cursor->scale, cursor->currentY + fontGlyph->drawYOffset * cursor->scale, fontGlyph->width * cursor->scale, fontGlyph->height * cursor->scale, fontGlyph->x, fontGlyph->y, fontGlyph->width, fontGlyph->height,  cursor->color);
        cursor->currentX += fontGlyph->drawXAdvance * cursor->scale;
        CurrentLineLength += fontGlyph->drawXAdvance * cursor->scale; 
    }    
}

void uiWriteWithBorder(ui_text_cursor *cursor, Rect Border, Text_Align Alignment, const char *format,  ...){
    va_list parameters;
    va_start(parameters, format);
    char buffer[2048];
    
    u32 byteCount = vsnprintf(ARRAY_WITH_COUNT(buffer), format, parameters);
    UiTextWithBorder(cursor, Border, buffer, byteCount, Alignment);
    
    va_end(parameters);
}
*/
void uiBar(ui_context *ui, s32 x, s32 y, s32 width, s32 height, f32 percentage, color emptyColor, color fullColor) {
    
    uiRect(ui, x, y, width, height, emptyColor, false);
    uiRect(ui, x, y, width * CLAMP(percentage, 0, 1), height, fullColor);
    
}

void drawCircle(transform xForm, f32 heightOverWidth, color fillColor = {0.7f, 0.0f, 0.0f, 1.0f}, bool isFilled = true, u32 n = 16, f32 z = 0) {
    if (isFilled) {
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        
        vec2 v = transformPoint(xForm, vec2{0, 0}, heightOverWidth);
        glVertex3f(v.x, v.y, z);
        
        for (u32 i = 0; i < n + 1; i++) { 
            xForm.rotation = i * ((2 * PI) / n); 
            v = transformPoint(xForm, vec2{0, 0.5f}, heightOverWidth);
            glVertex3f(v.x, v.y, z);		
        }	
    }
    else {
        glBegin(GL_LINE_LOOP);
        glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        
        for (u32 i = 0; i < n + 1; i++) { 
            xForm.rotation = i * ((2 * PI) / n); 
            auto v = transformPoint(xForm, vec2{0, 0.5f}, heightOverWidth);
            glVertex3f(v.x, v.y, z);		
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


#endif // RENDER_H