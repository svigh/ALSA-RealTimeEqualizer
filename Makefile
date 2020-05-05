CXX_FLAGS=-lasound -w -Wall -lpthread -lfftw3 -lm
CXX=gcc
# TODO: Add INCLUDE var for the other source files

.PHONY: clean run build all

run:
	./playback

build:
	$(CXX) -o playback playback.c utils.c effects.c $(CXX_FLAGS)

all: build run

clean:
	rm playback
