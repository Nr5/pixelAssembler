
CC=gcc
CFLAGS=-O2 `pkg-config --cflags --libs gtk+-3.0` -lfreeimage
OBJS=tegtkgl.o gifenc.o main.o

all: main

main: main.o tegtkgl.o gifenc.o
	$(CC) -g -o ../bin/main $(CFLAGS) $(OBJS) -lGL -lGLEW -lX11 -lm -lSDL
	
main.o: main.c tegtkgl.h
	$(CC) -g -c $(CFLAGS) main.c -lSDL -lGLEW

gifenc.o: gifenc.c gifenc.h	
	$(CC) -g -c $(CFLAGS) gifenc.c -lSDL -lGLEW

tegtkgl.o: tegtkgl.c tegtkgl.h
	$(CC) -g -c $(CFLAGS) tegtkgl.c -lSDL
	
clean:
	rm -f $(OBJS) ../bin/main

