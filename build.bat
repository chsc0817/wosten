rem /LIBPATH:"3rdparty/SDL2-2.0.9/lib/x64"
set mode=debug
rem set mode=release
set options= 
if %mode%==debug (
	set options=%options% /Od
) else (
	set options=%options% /O2
)


cl sdl_wosten.cpp /Zi /nologo /EHsc %options% /I "3rdparty/SDL2-2.0.9/include" /DSDL_MAIN_HANDLED  /link   User32.lib "3rdparty/SDL2-2.0.9/lib/x64/SDL2.lib" "3rdparty/SDL2-2.0.9/lib/x64/SDL2main.lib" opengl32.lib
