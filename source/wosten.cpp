#include <stdio.h>
#include <windows.h>

#define WINDOW_CALLBACK_DEC(name) LRESULT CALLBACK name(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)



WINDOW_CALLBACK_DEC(windowCallback) {
    switch (message) {
        case WM_DESTROY:{
            PostQuitMessage(0);
        } break;
        
        default:{
            return DefWindowProc(windowHandle, message, wParam, lParam);
        }
    } 
    return 0;
}

#if 1

int WinMain(

HINSTANCE hInstance,
HINSTANCE hPrevInstance,
LPSTR     lpCmdLine,
int       nShowCmd
)

#else

int main(int argc, char **args) 

#endif

{
    WNDCLASS windowClass = {};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowCallback;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.lpszClassName = "wostenClass";
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    
    if (RegisterClass(&windowClass) == 0) {
        printf("registerclass failed with error: %d", GetLastError());
        return -1;
    }
    
    HWND windowHandle = CreateWindow( 
        windowClass.lpszClassName, 
        "wostenWindow", 
        (WS_OVERLAPPEDWINDOW | WS_VISIBLE), 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        640,
        480, 
        NULL, 
        NULL, 
        windowClass.hInstance, 
        0);
    
    if(windowHandle == 0) {
        printf("createWindow failed; invalid handle: %d", GetLastError());
        return -1;
    } 
    
    ShowWindow(windowHandle, SW_SHOWNORMAL);
    
    bool doContinue = true;
    while(doContinue) {
        MSG msg;
        
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            
            switch (msg.message) {
                case WM_QUIT: {
                    doContinue = false;
                } break;
                
                default:{
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }
    printf("hello world");
    return 0;
}