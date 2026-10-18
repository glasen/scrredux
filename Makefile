CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
CFLAGS  += $(shell pkg-config --cflags sdl3)
LDLIBS  := $(shell pkg-config --libs sdl3) -lm
GLSLANG ?= glslangValidator

all: stuntracer game.vert.spv game.frag.spv shadow.frag.spv hud.frag.spv

stuntracer: stuntracer.c font_vec.h sr_main.h car_model.h scenery.h scr_tracks.h audio.h trackdef.h league.h replay.h menu.h editor.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

%.spv: shaders/%
	$(GLSLANG) -V $< -o $@

clean:
	rm -f stuntracer *.spv
