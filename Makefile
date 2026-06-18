CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Ivendor
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

JSON_H_URL = https://raw.githubusercontent.com/nhlmg93/stb/master/json.h
JSON_H = vendor/json.h

tetris: main.c $(JSON_H)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

$(JSON_H):
	@mkdir -p vendor
	curl -fsSL $(JSON_H_URL) -o $(JSON_H)

SOURCES := $(wildcard *.c) $(wildcard *.h)

fmt:
	clang-format -i $(SOURCES)

run: tetris
	./tetris

clean:
	rm -f tetris

.PHONY: fmt run clean
