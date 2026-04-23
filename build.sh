cd out
gcc -g -o clock -I../include ../clock.c ../autil/autil.c ../autil/string.c ../autil/math.c ../autil/containers.c ../autil/draw_opengl.c ../autil/draw_common.c ../autil/window_sdl.c ../autil/platform_unix.c ../libs/tlsf.c ../libs/glad.c -lSDL3 -lGL -lfreetype -lm
cd ..