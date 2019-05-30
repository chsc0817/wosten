#if !defined UI_CONTROL_H
#define UI_CONTROL_H

#include "defines.h"

#define UI_FILE_ID ((u64)0)
#define UI_ID(index) ((UI_FILE_ID << 48) | (((u64)__LINE__) << 32) | ((u64)(index))) 
#define UI_ID0 UI_ID(0)

enum {
	UiInvalidId = 0,
};

struct ui_control
{
	vec2 Cursor;
	bool CursorWasPressed, CursorWasReleased;
    
	u64 HotId;
	u64 ActiveId;
    
	u64 NextHotId;
	u32 NextHotIdPriority;
    
	vec2 DragCursorStart;
};

void UiFrameStart(ui_control *Control, vec2 Cursor, bool CursorWasPressed, bool CursorWasReleased)
{
	Control->Cursor = Cursor;
	Control->CursorWasPressed = CursorWasPressed;
	Control->CursorWasReleased = CursorWasReleased;
    
	if (Control->ActiveId == UiInvalidId)
	{
		Control->HotId = Control->NextHotId;
	}
	else
	{
		Control->HotId = UiInvalidId;
	}
    
	Control->NextHotId = UiInvalidId;
	Control->NextHotIdPriority = -1;
}

bool UiButton(ui_control *Control, u64 Id, rect Rect, u32 Priority = 0)
{
	assert(Id != UiInvalidId);
    
	bool IsHot = Contains(Rect, Control->Cursor);
    
	if (IsHot && (Priority < Control->NextHotIdPriority)) {
		Control->NextHotId = Id;
		Control->NextHotIdPriority = Priority;
	}
    
	if (Control->ActiveId == Id) {
		if (Control->CursorWasReleased) {
			Control->ActiveId = UiInvalidId;
            
			if (IsHot)
				return true;
		}
	} else {
		if ((Control->HotId == Id) && Control->CursorWasPressed) {
			Control->ActiveId = Id;
		}
	}
    
	return false;
}

bool UiDragable(ui_control *Control, u64 Id, rect Rect, vec2 *DeltaPosition, u32 Priority = 0)
{
	assert(Id != UiInvalidId);
    
	bool IsHot = Contains(Rect, Control->Cursor);
    
	if (IsHot && (Priority < Control->NextHotIdPriority)) {
		Control->NextHotId = Id;
		Control->NextHotIdPriority = Priority;
	}
    
	if (Control->ActiveId == Id) {
		*DeltaPosition = Control->Cursor - Control->DragCursorStart;
		Control->DragCursorStart = Control->Cursor;
        
		if (Control->CursorWasReleased) {
			Control->ActiveId = UiInvalidId;
		}
	} else {
		*DeltaPosition = {};
        
		if ((Control->HotId == Id) && Control->CursorWasPressed) {
			Control->ActiveId = Id;
			Control->DragCursorStart = Control->Cursor;			
		}
	}
    
	return (Control->ActiveId == Id);
}

// qt:

// onClick() { ,.,. }

// FloatSlider.GetValue();


// imGUI:

// if (ui_click(ui, cursor)) { ... }

// ui_float_slider(ui, &value);

#undef UI_FILE_ID
#endif // UI_CONTROL_H