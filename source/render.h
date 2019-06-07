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

struct ui_context {
    font *CurrentFont;
    s32 Width, Height;
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

vec2 UiToCanvasPoint(ui_context *Ui, vec2 UiPoint){
    vec2 Result = (UiPoint * vec2{ 2.0f / Ui->Width, 2.0f / Ui->Height }) + -1.0f;
    
    return Result;
}

vec2 UiToCanvasPoint(ui_context *Ui, s32 X, s32 Y){
    return UiToCanvasPoint(Ui, vec2{ (f32)X, (f32)Y });
}

vec2 CanvasToUiPoint(ui_context *Ui, vec2 CanvasPoint){
    vec2 Result = ((CanvasPoint + 1) * vec2{(f32) Ui->Width, (f32) Ui->Height}) * 0.5f;
    
    return Result;
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

void UiLine(ui_context *Ui, s32 X0, s32 Y0, s32 X1, s32 Y1,  color Color){
	
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    
    glColor4fv(Color.Values);        
    
    vec2 V = UiToCanvasPoint(Ui, X0, Y0);
    glVertex3f(V.X, V.Y, -0.9f);
    
    V = UiToCanvasPoint(Ui, X1, Y1);
    glVertex3f(V.X, V.Y, -0.9f);
    
    glEnd();
}

void UiRect(ui_context *Ui, s32 X, s32 Y, s32 Width, s32 Height, color RectColor, bool IsFilled = true){
	
    glDisable(GL_TEXTURE_2D);
    
    if (IsFilled) {
        glBegin(GL_QUADS);
    }
    else {
        glBegin(GL_LINE_LOOP);
    }
    
    glColor4fv(RectColor.Values);        
    
    vec2 V = UiToCanvasPoint(Ui, X, Y);
    glVertex3f(V.X, V.Y, -0.9f);
    
    V = UiToCanvasPoint(Ui, X + Width, Y);
    glVertex3f(V.X, V.Y, -0.9f);
    
    V = UiToCanvasPoint(Ui, X + Width, Y + Height);
    glVertex3f(V.X, V.Y, -0.9f);
    
    V = UiToCanvasPoint(Ui, X, Y + Height);
    glVertex3f(V.X, V.Y, -0.9f);
    
    glEnd();
}

void UiRect(ui_context *Ui, rect Rect, color RectColor, bool IsFilled = true) { 
    UiRect(Ui, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, RectColor, IsFilled);
}

void UiTexturedRect(ui_context *Ui, texture Texture, s32 X, s32 Y, s32 Width, s32 Height, s32 SubTextureX, s32 SubTextureY, s32 SubTextureWidth, s32 SubTextureHeight, color RectColor = White_Color) {
    glBindTexture(GL_TEXTURE_2D, Texture.Object);
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glColor4fv(RectColor.Values);        
    vec2 UV = {
        SubTextureX / (f32) Texture.Width,
        SubTextureY / (f32) Texture.Height
    };
    vec2 UVSize = {
        SubTextureWidth / (f32) Texture.Width,
        SubTextureHeight / (f32) Texture.Height
    };
    
    glTexCoord2f(UV.X, UV.Y);  	
    vec2 v = UiToCanvasPoint(Ui, X, Y);
    glVertex3f(v.X, v.Y, -0.9f);
    
    glTexCoord2f(UV.X + UVSize.X, UV.Y);  	
    v = UiToCanvasPoint(Ui, X + Width, Y);
    glVertex3f(v.X, v.Y, -0.9f);
    
    glTexCoord2f(UV.X + UVSize.X, UV.Y + UVSize.Y);  	
    v = UiToCanvasPoint(Ui, X + Width, Y + Height);
    glVertex3f(v.X, v.Y, -0.9f);
    
    glTexCoord2f(UV.X, UV.Y + UVSize.Y);  	
    v = UiToCanvasPoint(Ui, X, Y + Height);
    glVertex3f(v.X, v.Y, -0.9f);
	
    glEnd();
}

void UiTexturedRect(ui_context *Ui, texture Texture, rect Rect, rect SubTextureRect, color RectColor = White_Color) {
    UiTexturedRect(Ui, Texture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, SubTextureRect.Left, SubTextureRect.Bottom, SubTextureRect.Right - SubTextureRect.Left, SubTextureRect.Top - SubTextureRect.Bottom, RectColor);
} 

ui_text_cursor UiBeginText(ui_context *Context, font *CurrentFont, s32 X, s32 Y, bool DoRender = true, color Color = White_Color, f32 Scale = 1.0f) {
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
            BottomLeft.X + FontGlyph->Width * Cursor->Scale,
            BottomLeft.Y + FontGlyph->Height * Cursor->Scale};
        
        rect GlyphRect = MakeRect(BottomLeft, TopRight);
        
        TextRect = Merge(TextRect, GlyphRect);
        
        if (Cursor->DoRender) {
            UiTexturedRect(Cursor->Context, Cursor->Font->Texture, BottomLeft.X, BottomLeft.Y, FontGlyph->Width * Cursor->Scale, FontGlyph->Height * Cursor->Scale, FontGlyph->X, FontGlyph->Y, FontGlyph->Width, FontGlyph->Height,  Cursor->Color);
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
    
    auto DummyCursor = UiBeginText(Cursor.Context, Cursor.Font, Cursor.CurrentX, Cursor.CurrentY, false, White_Color, Cursor.Scale);
    
    va_list Parameters;
    va_start(Parameters, Format);
    
    UiWriteVA(&DummyCursor, Format, Parameters); 
    
    
    vec2 DummySize = DummyCursor.Rect.TopRight - DummyCursor.Rect.BottomLeft;
    vec2 Offset = vec2{(f32) Cursor.CurrentX, (f32)Cursor.CurrentY} - DummyCursor.Rect.BottomLeft - DummySize * Alignment;
    
    DummyCursor.Rect.BottomLeft = DummyCursor.Rect.BottomLeft + Offset;
    DummyCursor.Rect.TopRight = DummyCursor.Rect.TopRight + Offset;
    
    Cursor = UiBeginText(Cursor.Context, Cursor.Font, Cursor.CurrentX + Offset.X, Cursor.CurrentY + Offset.Y, Cursor.DoRender, Cursor.Color, Cursor.Scale);
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
void UiBar(ui_context *Ui, s32 X, s32 Y, s32 Width, s32 Height, f32 Percentage, color EmptyColor, color FullColor) {
    
    UiRect(Ui, X, Y, Width, Height, EmptyColor, false);
    UiRect(Ui, X, Y, Width * CLAMP(Percentage, 0, 1), Height, FullColor);
    
}

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