CXX_FLAGS=-lasound -w -Wall -lpthread -lfftw3 -lm
# CXX=~/Downloads/gcc-linaro-7.5.0-2019.12-i686_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
CXX=gcc
TESTING=-DTESTING
# TODO: Add INCLUDE var for the other source files

.PHONY: clean run build all

run:
	./playback

build:
	$(CXX) -o playback playback.c utils.c effects.c $(CXX_FLAGS)

build_testing:
	$(CXX) -o playback playback.c utils.c effects.c $(CXX_FLAGS) $(TESTING)

all: build run

all_testing: build_testing run

clean:
	rm playback
