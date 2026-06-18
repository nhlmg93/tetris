CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Ivendor/stb_me
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

tetris: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

SOURCES := $(wildcard *.c) $(wildcard *.h)

fmt:
	clang-format -i $(SOURCES)

run: tetris
	./tetris

clean:
	rm -f tetris

.PHONY: fmt run clean
