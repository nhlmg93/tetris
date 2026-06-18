CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Ivendor/stb
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

STB_REPO = https://raw.githubusercontent.com/nhlmg93/stb
STB_REF = master
VENDOR_DIR = vendor/stb
JSON_H = $(VENDOR_DIR)/json.h

tetris: main.c $(JSON_H)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

$(JSON_H):
	@mkdir -p $(VENDOR_DIR)
	curl -fsSL $(STB_REPO)/$(STB_REF)/json.h -o $(JSON_H)

SOURCES := $(wildcard *.c) $(wildcard *.h)

fmt:
	clang-format -i $(SOURCES)

run: tetris
	./tetris

clean:
	rm -f tetris

.PHONY: fmt run clean
