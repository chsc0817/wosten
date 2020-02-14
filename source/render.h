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
        f32 R, G, B, A;
    };
    f32 Values[4];
    
};

const color White_Color = {1.0f, 1.0f, 1.0f, 1.0f};
const color Black_Color = {0.0f, 0.0f, 0.0f, 1.0f};
const color Red_Color = {1.0f, 0.0f, 0.0f, 1.0f};
const color Green_Color = {0.0f, 1.0f, 0.0f, 1.0f};
const color Blue_Color = {0.0f, 0.0f, 1.0f, 1.0f};
const color Orange_Color ={1.0f, 0.5f, 0.0f, 1.0f};

color lerp(color A, color B, f32 T) {
	color Result;
	Result.R = (A.R * (1 - T)) + (B.R * T);
	Result.G = (A.G * (1 - T)) + (B.G * T);
	Result.B = (A.B * (1 - T)) + (B.B * T);
	Result.A = (A.A * (1 - T)) + (B.A * T);	
    
	return Result;
}

struct histogram {
    f32 Values[10 * 60];
    u32 CurrentIndex;
};

struct camera
{
    f32 HeightOverWidth;
    vec2 WorldPosition;
};

vec2 WorldToCanvasPoint(camera Camera, vec2 WorldPoint){
    vec2 Result = WorldPoint - Camera.WorldPosition;
    Result.X *= Camera.HeightOverWidth; 
    
    return Result;
}

vec2 CanvasToWorldPoint(camera Camera, vec2 CanvasPoint){
    CanvasPoint.X *= 1.0f / Camera.HeightOverWidth;
    CanvasPoint = CanvasPoint + Camera.WorldPosition;
    
    return CanvasPoint;
}

struct texture {
    s32 Width, Height;
    GLuint Object;
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
    s32 BaselineTopOffset;
    s32 BaselineBottomOffset;
};

void DrawQuad(camera Camera, transform XForm, vec2 Center = {0.5f, 0.5f}){
	
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    vec2 V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 0} - Center));
    glVertex2f(V.X, V.Y);
    
    V= WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{1, 0} - Center));
    glVertex2f(V.X, V.Y);
    
    V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{1, 1} - Center));
    glVertex2f(V.X, V.Y);
    
    V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 1} - Center));
    glVertex2f(V.X, V.Y);
    
    glEnd();
}

const f32 Default_World_Units_Per_Texel = 0.01f;

void DrawTexturedQuad(camera Camera, transform XForm, texture FillTexture, color FillColor = White_Color, vec2 RelativeCenter = {0.5f, 0.5f}, f32 TexelScale = Default_World_Units_Per_Texel, f32 Z = 0, f32 DoFlip = 0.0f){
    
    //vec2 texelSize = vec2{1.0f / FillTexture.Width, 1.0f / FillTexture.Height};
    vec2 QuadSize = vec2{(f32) FillTexture.Width, (f32) FillTexture.Height} * TexelScale;
    vec2 Center = QuadSize * RelativeCenter;
    
    
    glBindTexture(GL_TEXTURE_2D, FillTexture.Object);
    
    glBegin(GL_QUADS);
    glColor4fv(FillColor.Values);
    //assuming our textures are flipped        
    
    glTexCoord2f(0, lerp(0, 1, DoFlip));  	
    vec2 V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 0} - Center));
    glVertex3f(V.X, V.Y, Z);    
    
    glTexCoord2f(1, lerp(0, 1, DoFlip));
    V= WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{QuadSize.X, 0} - Center));
    glVertex3f(V.X, V.Y, Z);
	
    glTexCoord2f(1, lerp(1, 0, DoFlip));
    V = WorldToCanvasPoint(Camera, TransformPoint(XForm, QuadSize - Center));
    glVertex3f(V.X, V.Y, Z);
	
    glTexCoord2f(0, lerp(1, 0, DoFlip));
    V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, QuadSize.Y} - Center));
    glVertex3f(V.X, V.Y, Z);
    
    glEnd();
}

void DrawCircle(camera Camera, transform XForm, color FillColor = {0.7f, 0.0f, 0.0f, 1.0f}, bool IsFilled = true, u32 N = 16, f32 Z = 0) {
    if (IsFilled) {
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A);
        
        vec2 V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 0}));
        glVertex3f(V.X, V.Y, Z);
        
        for (u32 i = 0; i < N + 1; i++) { 
            XForm.Rotation = i * ((2 * PI) / N); 
            V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 0.5f}));
            glVertex3f(V.X, V.Y, Z);        
        }   
    }
    else {
        glBegin(GL_LINE_LOOP);
        glColor4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A);
        
        for (u32 i = 0; i < N + 1; i++) { 
            XForm.Rotation = i * ((2 * PI) / N); 
            auto V = WorldToCanvasPoint(Camera, TransformPoint(XForm, vec2{0, 0.5f}));
            glVertex3f(V.X, V.Y, Z);        
        }   
    }
    
    glEnd();
}

void DrawLine(camera Camera, transform XForm, vec2 From, vec2 To, color LineColor) {
    glBegin(GL_LINES);
    
    glColor4fv(LineColor.Values);
    
    vec2 V = WorldToCanvasPoint(Camera, TransformPoint(XForm, From));
    glVertex2f(V.X, V.Y);
    
    V = WorldToCanvasPoint(Camera, TransformPoint(XForm, To));
    glVertex2f(V.X, V.Y);
    
    glEnd();
}

SDL_Rect RectToSDLRect(rect Rect) {
    SDL_Rect Result;
    
    Result.x = Rect.Left;
    Result.y = Rect.Top;
    Result.w = Rect.Right - Rect.Left;
    Result.h = Rect.Top - Rect.Bottom;
    
    return Result;
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

texture LoadTexture(u8 *Data, s32 Width, s32 Height, u8 BytesPerPixel, GLenum Filter = GL_LINEAR, bool FlipY = true)
{
    if (FlipY)
    {
        u32 RowByteCount = Width * BytesPerPixel;
        u8 *tmp = new u8[RowByteCount];
        
        for (u32 y = 0; y < Height / 2; y++)
        {
            memcpy(tmp, Data + y * RowByteCount, RowByteCount);
            memcpy(Data + y * RowByteCount, Data + (Height - 1 - y) * RowByteCount, RowByteCount);
            memcpy(Data + (Height - 1 - y) * RowByteCount, tmp, RowByteCount);
        }
        
        delete[] tmp;
    }
    
    texture Result;
    
    glGenTextures(1, &Result.Object);
    glBindTexture(GL_TEXTURE_2D, Result.Object);
    
    switch(BytesPerPixel) {
        case 1: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RED, GL_UNSIGNED_BYTE, Data);
            GLint SwizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, SwizzleMask);           
        } break;
        
        case 4: {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data);
        } break;
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Filter);
    
    Result.Width = Width;
    Result.Height = Height;
    
    return Result;
}

texture LoadTexture(const char *Path, GLenum filter = GL_LINEAR){
    SDL_Surface *TextureSurface = IMG_Load(Path);
    
    if (TextureSurface->format->BytesPerPixel == 1){
        for (s32 i = 0; i < TextureSurface->w * TextureSurface->h; i++) {
            ((u8*)TextureSurface->pixels)[i] = 255 - ((u8*)TextureSurface->pixels)[i];
        }
    }
    
    texture Result = LoadTexture((u8*) TextureSurface->pixels, TextureSurface->w, TextureSurface->h, TextureSurface->format->BytesPerPixel, filter);    
    SDL_FreeSurface(TextureSurface);
    return Result;
}

void DrawHistogram(histogram H) {
    glBegin(GL_LINES);
    
    f32 MaxValue = 0;
    
    for(u32 i = 0; i < (ARRAY_COUNT(H.Values)); i++) {
        if (H.Values[i] > MaxValue) {
            MaxValue = H.Values[i]; 
        }
    }
    
    f32 Scale = 0.5 / 60;
    
    for(u32 i = 0; i < (ARRAY_COUNT(H.Values) - 1); i++) {
        glVertex2f( i * (2 / (f32) ARRAY_COUNT(H.Values)) - 1, H.Values[i] * Scale);
        glVertex2f((i + 1) * (2 / (f32) ARRAY_COUNT(H.Values)) - 1, H.Values[i + 1] * Scale);
    } 
    
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glVertex2f(-1, Scale * 60);
    glVertex2f(1, Scale * 60);
    
    glEnd();
}


#endif // RENDER_H