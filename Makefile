CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
CFLAGS  += $(shell pkg-config --cflags sdl3)
LDLIBS  := $(shell pkg-config --libs sdl3) -lm
GLSLANG ?= glslangValidator

all: stuntracer game.vert.spv game.frag.spv shadow.frag.spv

stuntracer: stuntracer.c sr_main.h car_model.h scenery.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

%.spv: shaders/%
	$(GLSLANG) -V $< -o $@

clean:
	rm -f stuntracer *.spv
