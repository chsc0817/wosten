#if !defined UI_H

#include "render.h"

struct ui_rectangle
{
    s32 X, Y, Width, Height;
};

enum
{
    Ui_Draw_Command_Textured_Rectangle,
    Ui_Draw_Command_Rectangle,
};

struct ui_draw_command
{
    u32 Kind;
    
    union
    {
        struct
        {
            color Color;
            ui_rectangle DrawRectangle;
            bool IsFilled;
        } Rectangle;
        
        struct
        {
            color Color;
            ui_rectangle DrawRectangle, TextureSubRectangle;
            texture Texture;
        } TexturedRectangle;
    };
};

#define template_array_name      ui_draw_commands
#define template_array_data_type ui_draw_command
#define template_array_is_buffer 
#include "template_array.h"

struct ui_context
{
    ui_draw_commands DrawCommands;
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

void Init(ui_context *Context, u32 DrawCommandCount = 4096)
{
    *Context = {};
    Context->DrawCommands = { new ui_draw_command[DrawCommandCount], DrawCommandCount };
}

vec2 UiToCanvasPoint(ui_context *Context, vec2 UiPoint){
    vec2 Result = (UiPoint * vec2{ 2.0f / Context->Width, 2.0f / Context->Height }) + -1.0f;
    return Result;
}

vec2 UiToCanvasPoint(ui_context *Context, s32 X, s32 Y){
    return UiToCanvasPoint(Context, vec2{ (f32)X, (f32)Y });
}

vec2 CanvasToUiPoint(ui_context *Context, vec2 CanvasPoint){
    vec2 Result = ((CanvasPoint + 1) * vec2{(f32) Context->Width, (f32) Context->Height}) * 0.5f;
    return Result;
}

void
UiTexturedRectangle(ui_context *Context, texture Texture, s32 X, s32 Y, s32 Width, s32 Height, s32 SubTextureX, s32 SubTextureY, s32 SubTextureWidth, s32 SubTextureHeight, color Color = White_Color)
{
    auto command = Push(&Context->DrawCommands);
    if (!command)
        return;
    
    command->Kind = Ui_Draw_Command_Textured_Rectangle;
    auto TexturedRectangle = &command->TexturedRectangle;
    
    TexturedRectangle->Color = Color;
    TexturedRectangle->DrawRectangle = { X, Y, Width, Height };
    
    TexturedRectangle->Texture = Texture;
    TexturedRectangle->TextureSubRectangle = {  SubTextureX, SubTextureY, SubTextureWidth, SubTextureHeight };
}

void UiTexturedRectangle(ui_context *Context, texture Texture, rect Rect, rect SubTextureRect, color Color = White_Color) {
    UiTexturedRectangle(Context, Texture, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, SubTextureRect.Left, SubTextureRect.Bottom, SubTextureRect.Right - SubTextureRect.Left, SubTextureRect.Top - SubTextureRect.Bottom, Color);
} 
void
UiRectangle(ui_context *Context, s32 X, s32 Y, s32 Width, s32 Height, color Color = White_Color, bool IsFilled = true)
{
    auto command = Push(&Context->DrawCommands);
    if (!command)
        return;
    
    command->Kind = Ui_Draw_Command_Rectangle;
    auto Rectangle = &command->Rectangle;
    
    Rectangle->Color = Color;
    Rectangle->DrawRectangle = { X, Y, Width, Height };
    Rectangle->IsFilled = IsFilled;
}

void
UiRectangle(ui_context *Context, rect Rect, color Color = White_Color, bool IsFilled = true) { 
    UiRectangle(Context, Rect.Left, Rect.Bottom, Rect.Right - Rect.Left, Rect.Top - Rect.Bottom, Color, IsFilled);
}

void
UiRenderCommands(ui_context *Context)
{
    glDisable(GL_DEPTH_TEST);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 0.1f);
    
    for (u32 i = 0; i < Context->DrawCommands.Count; i++)
    {
        switch (Context->DrawCommands[i].Kind)
        {
            case Ui_Draw_Command_Textured_Rectangle:
            {
                auto TexturedRectangle = &Context->DrawCommands[i].TexturedRectangle;
                
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, TexturedRectangle->Texture.Object);
                
                glColor4fv(TexturedRectangle->Color.Values);
                
                glBegin(GL_QUADS);
                
                vec2 UV = {
                    TexturedRectangle->TextureSubRectangle.X / (f32) TexturedRectangle->Texture.Width,
                    TexturedRectangle->TextureSubRectangle.Y / (f32) TexturedRectangle->Texture.Height,
                };
                
                vec2 UVSize = {
                    TexturedRectangle->TextureSubRectangle.Width / (f32) TexturedRectangle->Texture.Width,
                    TexturedRectangle->TextureSubRectangle.Height / (f32) TexturedRectangle->Texture.Height,
                };
                
                glTexCoord2f(UV.X, UV.Y);  	
                vec2 v = UiToCanvasPoint(Context, TexturedRectangle->DrawRectangle.X, TexturedRectangle->DrawRectangle.Y);
                glVertex3f(v.X, v.Y, -0.9f);
                
                glTexCoord2f(UV.X + UVSize.X, UV.Y);  	
                v = UiToCanvasPoint(Context, TexturedRectangle->DrawRectangle.X + TexturedRectangle->DrawRectangle.Width, TexturedRectangle->DrawRectangle.Y);
                glVertex3f(v.X, v.Y, -0.9f);
                
                glTexCoord2f(UV.X + UVSize.X, UV.Y + UVSize.Y);  	
                v = UiToCanvasPoint(Context, TexturedRectangle->DrawRectangle.X + TexturedRectangle->DrawRectangle.Width, TexturedRectangle->DrawRectangle.Y + TexturedRectangle->DrawRectangle.Height);
                glVertex3f(v.X, v.Y, -0.9f);
                
                glTexCoord2f(UV.X, UV.Y + UVSize.Y);  	
                v = UiToCanvasPoint(Context, TexturedRectangle->DrawRectangle.X, TexturedRectangle->DrawRectangle.Y + TexturedRectangle->DrawRectangle.Height);
                glVertex3f(v.X, v.Y, -0.9f);
                
                glEnd();
            } break;
            
            case Ui_Draw_Command_Rectangle:
            {
                auto Rectangle = &Context->DrawCommands[i].Rectangle;
                
                glDisable(GL_TEXTURE_2D);
                
                glColor4fv(Rectangle->Color.Values);
                
                if (Rectangle->IsFilled)
                    glBegin(GL_QUADS);
                else
                    glBegin(GL_LINE_LOOP);
                
                vec2 v = UiToCanvasPoint(Context, Rectangle->DrawRectangle.X, Rectangle->DrawRectangle.Y);
                glVertex3f(v.X, v.Y, -0.9f);
                
                v = UiToCanvasPoint(Context, Rectangle->DrawRectangle.X + Rectangle->DrawRectangle.Width, Rectangle->DrawRectangle.Y);
                glVertex3f(v.X, v.Y, -0.9f);
                
                v = UiToCanvasPoint(Context, Rectangle->DrawRectangle.X + Rectangle->DrawRectangle.Width, Rectangle->DrawRectangle.Y + Rectangle->DrawRectangle.Height);
                glVertex3f(v.X, v.Y, -0.9f);
                
                v = UiToCanvasPoint(Context, Rectangle->DrawRectangle.X, Rectangle->DrawRectangle.Y + Rectangle->DrawRectangle.Height);
                glVertex3f(v.X, v.Y, -0.9f);
                
                glEnd();
            } break;
            
            default:
            assert(0);
        }
    }
    
    Context->DrawCommands.Count = 0;
}

ui_text_cursor UiBeginText(ui_context *Context, font *Font, s32 X, s32 Y, bool DoRender = true, color Color = White_Color, f32 Scale = 1.0f) {
    ui_text_cursor Result;
    Result.Context = Context;
    Result.StartX = X;
    Result.StartY = Y;
    Result.CurrentX = X;
    Result.CurrentY = Y;
    Result.Font = Font;
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
            UiTexturedRectangle(Cursor->Context, Cursor->Font->Texture, BottomLeft.X, BottomLeft.Y, FontGlyph->Width * Cursor->Scale, FontGlyph->Height * Cursor->Scale, FontGlyph->X, FontGlyph->Y, FontGlyph->Width, FontGlyph->Height,  Cursor->Color);
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

void UiBar(ui_context *Context, s32 X, s32 Y, s32 Width, s32 Height, f32 Percentage, color EmptyColor, color FullColor) {
    
    UiRectangle(Context, X, Y, Width, Height, EmptyColor, false);
    UiRectangle(Context, X, Y, Width * CLAMP(Percentage, 0, 1), Height, FullColor);
    
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

#endif // UI_H