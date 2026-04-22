@echo off
cd out
cl /MDd ..\clock.c ..\autil\containers.c ..\autil\math.c ..\autil\string.c ..\autil\draw_opengl.c ..\autil\draw_common.c ..\autil\window_sdl.c ..\autil\autil.c ..\autil\platform_win.c ..\libs\tlsf.c ..\libs\glad.c /I..\include freetyped.lib SDL3-static.lib
cd ..
