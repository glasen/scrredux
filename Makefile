CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
CFLAGS  += -Isrc $(shell pkg-config --cflags sdl3)
LDLIBS  := $(shell pkg-config --libs sdl3) -lm
GLSLANG ?= glslangValidator

# The game is one translation unit: src/main.c includes every module under src/.
SOURCES := $(shell find src -name '*.c' -o -name '*.h')

all: scrredux game.vert.spv game.frag.spv shadow.frag.spv hud.frag.spv

scrredux: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LDLIBS)

%.spv: shaders/%
	$(GLSLANG) -V $< -o $@

clean:
	rm -f scrredux *.spv
