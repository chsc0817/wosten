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

struct camera
{
    f32 HeightOverWidth;
    vec2 WorldPosition;
};

vec2 WorldToCanvasPoint(camera Camera, vec2 WorldPoint){
    vec2 Result = WorldPoint - Camera.WorldPosition;
    Result.x *= Camera.HeightOverWidth; 

    return Result;
}

vec2 CanvasToWorldPoint(camera Camera, vec2 CanvasPoint){
    CanvasPoint.x *= 1.0f / Camera.HeightOverWidth;
    CanvasPoint = CanvasPoint + Camera.WorldPosition;

    return CanvasPoint;
}

struct texture {
    s32 Width, Height;
    GLuint Object;
};

struct textures {
    //doesn't include bomb texture or font
    texture LevelLayer1;
    texture LevelLayer2;
    texture PlayerTexture;
    texture BossTexture;
    texture FlyTexture;
    texture BulletTexture;
    texture BulletPoweredUpTexture;
    texture BulletMaxPoweredUpTexture;
    texture BombCountTexture;
    texture PowerupTexture;
    
    // UI
    texture IdleButtonTexture;
    texture HotButtonTexture;
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

struct ui_context {
    font *CurrentFont;
    s32 width, height;
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

void drawQuad(camera Camera, transform xForm, vec2 center = {0.5f, 0.5f}){
	
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    vec2 v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 0} - center));
    glVertex2f(v.x, v.y);
    
    v= WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{1, 0} - center));
    glVertex2f(v.x, v.y);
    
    v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{1, 1} - center));
    glVertex2f(v.x, v.y);
    
    v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 1} - center));
    glVertex2f(v.x, v.y);
    
    glEnd();
}

const f32 Default_World_Units_Per_Texel = 0.01f;

void drawTexturedQuad(camera Camera, transform xForm, texture fillTexture, color fillColor = White_Color, vec2 relativeCenter = {0.5f, 0.5f}, f32 texelScale = Default_World_Units_Per_Texel, f32 z = 0, f32 doFlip = 0.0f){
    
    vec2 texelSize = vec2{1.0f / fillTexture.Width, 1.0f / fillTexture.Height};
    vec2 quadSize = vec2{(f32) fillTexture.Width, (f32) fillTexture.Height} * texelScale;
    vec2 center = quadSize * relativeCenter;
    
    
    glBindTexture(GL_TEXTURE_2D, fillTexture.Object);
    
    glBegin(GL_QUADS);
    glColor4fv(fillColor.values);
    //assuming our textures are flipped        
    
    glTexCoord2f(0, lerp(0, 1, doFlip));  	
    vec2 v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 0} - center));
    glVertex3f(v.x, v.y, z);    
    
    glTexCoord2f(1, lerp(0, 1, doFlip));
    v= WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{quadSize.x, 0} - center));
    glVertex3f(v.x, v.y, z);
	
    glTexCoord2f(1, lerp(1, 0, doFlip));
    v = WorldToCanvasPoint(Camera, TransformPoint(xForm, quadSize - center));
    glVertex3f(v.x, v.y, z);
	
    glTexCoord2f(0, lerp(1, 0, doFlip));
    v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, quadSize.y} - center));
    glVertex3f(v.x, v.y, z);
    
    glEnd();
}

void drawCircle(camera Camera, transform xForm, color fillColor = {0.7f, 0.0f, 0.0f, 1.0f}, bool isFilled = true, u32 n = 16, f32 z = 0) {
    if (isFilled) {
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        
        vec2 v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 0}));
        glVertex3f(v.x, v.y, z);
        
        for (u32 i = 0; i < n + 1; i++) { 
            xForm.rotation = i * ((2 * PI) / n); 
            v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 0.5f}));
            glVertex3f(v.x, v.y, z);        
        }   
    }
    else {
        glBegin(GL_LINE_LOOP);
        glColor4f(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
        
        for (u32 i = 0; i < n + 1; i++) { 
            xForm.rotation = i * ((2 * PI) / n); 
            auto v = WorldToCanvasPoint(Camera, TransformPoint(xForm, vec2{0, 0.5f}));
            glVertex3f(v.x, v.y, z);        
        }   
    }
    
    glEnd();
}

void drawLine(camera Camera, transform xForm, vec2 from, vec2 to, color lineColor) {
    glBegin(GL_LINES);
    
    glColor4fv(lineColor.values);
    
    vec2 v = WorldToCanvasPoint(Camera, TransformPoint(xForm, from));
    glVertex2f(v.x, v.y);
    
    v = WorldToCanvasPoint(Camera, TransformPoint(xForm, to));
    glVertex2f(v.x, v.y);
    
    glEnd();
}

vec2 UiToCanvasPoint(ui_context *ui, vec2 UiPoint){
    vec2 result = (UiPoint * vec2{ 2.0f / ui->width, 2.0f / ui->height }) + -1.0f;
    
    return result;
}

vec2 UiToCanvasPoint(ui_context *ui, s32 x, s32 y){
    return UiToCanvasPoint(ui, vec2{ (f32)x, (f32)y });
}

vec2 CanvasToUiPoint(ui_context *ui, vec2 CanvasPoint){
    vec2 result = ((CanvasPoint + 1) * vec2{(f32) ui->width, (f32) ui->height}) * 0.5f;
  
    return result;
}

void UiBegin()
{
    glEnable(GL_TEXTURE_2D);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 0.1f);
    
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
}

void UiEnd()
{
    //glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    //glDisable(GL_ALPHA_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
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
    
    vec2 v = UiToCanvasPoint(ui, x, y);
    glVertex3f(v.x, v.y, -0.9f);
    
    v = UiToCanvasPoint(ui, x + width, y);
    glVertex3f(v.x, v.y, -0.9f);
    
    v = UiToCanvasPoint(ui, x + width, y + height);
    glVertex3f(v.x, v.y, -0.9f);
    
    v = UiToCanvasPoint(ui, x, y + height);
    glVertex3f(v.x, v.y, -0.9f);
    
    glEnd();
}

void UiRect(ui_context *Ui, rect Rect, color RectColor, bool IsFilled = true) { 
    uiRect(Ui, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, RectColor, IsFilled);
}

void uiTexturedRect(ui_context *ui, texture texture, s32 x, s32 y, s32 width, s32 height, s32 subTextureX, s32 subTextureY, s32 subTextureWidth, s32 subTextureHeight, color rectColor = White_Color) {
    glBindTexture(GL_TEXTURE_2D, texture.Object);
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glColor4fv(rectColor.values);        
    vec2 uv = {
        subTextureX / (f32) texture.Width,
        subTextureY / (f32) texture.Height
    };
    vec2 uvSize = {
        subTextureWidth / (f32) texture.Width,
        subTextureHeight / (f32) texture.Height
    };
    
    glTexCoord2f(uv.x, uv.y);  	
    vec2 v = UiToCanvasPoint(ui, x, y);
    glVertex3f(v.x, v.y, -0.9f);
    
    glTexCoord2f(uv.x + uvSize.x, uv.y);  	
    v = UiToCanvasPoint(ui, x + width, y);
    glVertex3f(v.x, v.y, -0.9f);
    
    glTexCoord2f(uv.x + uvSize.x, uv.y + uvSize.y);  	
    v = UiToCanvasPoint(ui, x + width, y + height);
    glVertex3f(v.x, v.y, -0.9f);
    
    glTexCoord2f(uv.x, uv.y + uvSize.y);  	
    v = UiToCanvasPoint(ui, x, y + height);
    glVertex3f(v.x, v.y, -0.9f);
	
    glEnd();
}

void UiTexturedRect(ui_context *Ui, texture Texture, rect Rect, rect SubTextureRect, color RectColor = White_Color) {
    uiTexturedRect(Ui, Texture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, SubTextureRect.Left, SubTextureRect.Bottom, SubTextureRect.Right - SubTextureRect.Left, SubTextureRect.Top - SubTextureRect.Bottom, RectColor);
} 

ui_text_cursor uiBeginText(ui_context *Context, font *CurrentFont, s32 X, s32 Y, bool DoRender = true, color Color = White_Color, f32 Scale = 1.0f) {
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

rect UiText(ui_text_cursor *Cursor, const char *Text, u32 TextCount) {

    rect TextRect = MakeEmptyRect();

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
        
        TextRect = Merge(TextRect, GlyphRect);
        
        if (Cursor->DoRender) {
            uiTexturedRect(Cursor->Context, Cursor->Font->Texture, BottomLeft.x, BottomLeft.y, FontGlyph->Width * Cursor->Scale, FontGlyph->Height * Cursor->Scale, FontGlyph->X, FontGlyph->Y, FontGlyph->Width, FontGlyph->Height,  Cursor->Color);
        }
        
        Cursor->CurrentX += FontGlyph->DrawXAdvance * Cursor->Scale;
    }

    if (Cursor->RectIsInitialized) { 
        Cursor->Rect = Merge(Cursor->Rect, TextRect);
    }
    else {
        Cursor->Rect = TextRect;
        Cursor->RectIsInitialized = IsValid(TextRect);
    } 

    return TextRect;
}

rect UiWriteVA(ui_text_cursor *Cursor, const char *Format, va_list Parameters) {
    char Buffer[2048];
    
    u32 ByteCount = vsnprintf(ARRAY_WITH_COUNT(Buffer), Format, Parameters);
    return UiText(Cursor, Buffer, ByteCount);
}

rect UiWrite(ui_text_cursor *Cursor, const char *Format, ...) {
    va_list Parameters;
    va_start(Parameters, Format);
    auto Result = UiWriteVA(Cursor, Format, Parameters);
    va_end(Parameters);

    return Result;
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
    
    glGenTextures(1, &result.Object);
    glBindTexture(GL_TEXTURE_2D, result.Object);
    
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
    
    result.Width = width;
    result.Height = height;
    
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