# Tetris

A minimal Tetris clone written in C11 with [raylib](https://www.raylib.com/). The entire game lives in a single `main.c` file.

## Requirements

- GCC
- [raylib](https://github.com/raysan5/raylib)

On Arch Linux:

```bash
sudo pacman -S raylib
```

## Build

```bash
git clone --recurse-submodules https://github.com/nhlmg93/tetris.git
cd tetris
make
```

If you already cloned without submodules: `git submodule update --init`

Run the game from the project root so shape assets can be loaded:

```bash
make run
```

Format the source:

```bash
make fmt
```

## Assets

All tetromino shapes live in `assets/shapes.json` and are loaded once at startup. JSON is parsed with [stb_me](https://github.com/nhlmg93/stb_me) `json.h` (git submodule at `vendor/stb_me`; define `JSON_IMPLEMENTATION` in one source file).

## Controls

| Key | Action |
|-----|--------|
| Left / A | Move left |
| Right / D | Move right |
| Up / W | Rotate |
| Down / S | Soft drop |
| Space | Hard drop |
| R | Restart |

## License

MIT
