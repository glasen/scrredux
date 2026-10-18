CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
CFLAGS  += $(shell pkg-config --cflags sdl3)
LDLIBS  := $(shell pkg-config --libs sdl3) -lm
GLSLANG ?= glslangValidator

all: scrredux game.vert.spv game.frag.spv shadow.frag.spv hud.frag.spv

scrredux: scrredux.c $(wildcard include/*.h)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

%.spv: shaders/%
	$(GLSLANG) -V $< -o $@

clean:
	rm -f scrredux *.spv
