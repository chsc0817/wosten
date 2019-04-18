rem /LIBPATH:"3rdparty/SDL2-2.0.9/lib/x64"
set mode=debug
rem set mode=release
set options= 
if %mode%==debug (
	set options=%options% /Od
) else (
	set options=%options% /O2
)


cl %cd%/source/sdl_wosten.cpp /Zi /nologo /EHsc %options% /I "3rdparty" /I "3rdparty/SDL2-2.0.9/include" /I "3rdparty/SDL2_mixer-2.0.4/include" /I "3rdparty/SDL2_image-2.0.4/include" /DSDL_MAIN_HANDLED  /link   User32.lib "3rdparty/SDL2-2.0.9/lib/x64/SDL2.lib" "3rdparty/SDL2_mixer-2.0.4/lib/x64/SDL2_mixer.lib" "3rdparty/SDL2-2.0.9/lib/x64/SDL2main.lib" "3rdparty/SDL2_image-2.0.4/lib/x64/SDL2_image.lib" opengl32.lib
