
CC=gcc
CFLAGS=-Wall -Wextra -Wno-deprecated-declarations -O2 `pkg-config --cflags --libs gtk+-3.0` -lfreeimage
OBJS=tegtkgl.o gifenc.o main.o gifdec.o
OBJS_DEBUG=tegtkgl_debug.o gifenc_debug.o main_debug.o gifdec_debug.o

all: main

debug: tegtkgl_debug.o gifenc_debug.o main_debug.o gifdec_debug.o Makefile
	$(CC) -g -o ../bin/main $(CFLAGS) $(OBJS_DEBUG) -lGLEW -lGL -lX11 -lm -lSDL

main_debug.o: main.c tegtkgl.h Makefile
	$(CC) -g -o main_debug.o -c $(CFLAGS) main.c -lSDL -lGLEW -lGL 

gifenc_debug.o: gif/gifenc.c gif/gifenc.h	
	$(CC) -g -o gifenc_debug.o -c $(CFLAGS) gif/gifenc.c -lSDL -lGLEW

gifdec_debug.o: gif/gifdec.c gif/gifdec.h	
	$(CC) -g -o gifdec_debug.o -c $(CFLAGS) gif/gifdec.c -lSDL -lGLEW

tegtkgl_debug.o: tegtkgl.c tegtkgl.h
	$(CC) -g -o tegtkgl_debug.o -c $(CFLAGS) tegtkgl.c -lSDL 






main: main.o tegtkgl.o gifenc.o gifdec.o Makefile
	$(CC) -o ../bin/main $(CFLAGS) $(OBJS) -lGL -lGLEW -lX11 -lm -lSDL
	
main.o: main.c tegtkgl.h Makefile
	$(CC) -c $(CFLAGS) main.c -lSDL -lGL -lGLEW 

gifenc.o: gif/gifenc.c gif/gifenc.h	
	$(CC) -c $(CFLAGS) gif/gifenc.c -lSDL -lGLEW

gifdec.o: gif/gifdec.c gif/gifdec.h	
	$(CC) -c $(CFLAGS) gif/gifdec.c -lSDL -lGLEW

tegtkgl.o: tegtkgl.c tegtkgl.h
	$(CC) -c $(CFLAGS) tegtkgl.c -lGL -lGLEW -lSDL
	
clean:
	rm -f $(OBJS) ../bin/main

