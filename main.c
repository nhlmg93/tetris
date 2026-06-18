#include "json.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define BOARD_COLS 10
#define BOARD_ROWS 20
#define CELL_SIZE 25
#define BOARD_WIDTH (BOARD_COLS * CELL_SIZE)
#define BOARD_HEIGHT (BOARD_ROWS * CELL_SIZE)
#define BOARD_X 80
#define BOARD_Y ((SCREEN_HEIGHT - BOARD_HEIGHT) / 2)

#define MAX_PIECE_SIZE 4
#define ROTATION_COUNT 4
#define MAX_ENTITY 8
#define E_NULL 0

#define LINE_SCORES 100, 300, 500, 800
#define LINES_PER_LEVEL 10
#define BASE_FALL_TIME 0.8f
#define MIN_FALL_TIME 0.05f

typedef enum { E_none, E_piece } Type;

typedef enum {
    PIECE_I,
    PIECE_O,
    PIECE_T,
    PIECE_S,
    PIECE_Z,
    PIECE_J,
    PIECE_L,
    PIECE_COUNT,
} PieceType;

typedef unsigned int EntityId;

typedef struct Entity {
    Type type;
    PieceType piece_type;
    int rotation;
    int x;
    int y;
} Entity;

EntityId alloc_entity(void);
void free_entity(EntityId id);

Entity entities[MAX_ENTITY];

EntityId alloc_entity(void)
{
    for (int i = 1; i < MAX_ENTITY; i++) {
        if (entities[i].type == E_none)
            return i;
    }
    return E_NULL;
}

void free_entity(EntityId id) { entities[id].type = E_none; }

static int piece_size = MAX_PIECE_SIZE;
static char shapes[PIECE_COUNT][ROTATION_COUNT][MAX_PIECE_SIZE][MAX_PIECE_SIZE];

int board[BOARD_ROWS][BOARD_COLS];

EntityId activePieceId;
PieceType next_piece_type;

int score;
int lines_cleared;
int level;
float fall_timer;
float fall_interval;
bool game_over;

PieceType bag[PIECE_COUNT];
int bag_index;

void reset_game(void);
bool load_shapes(void);
void shuffle_bag(void);
PieceType draw_from_bag(void);
bool piece_fits(PieceType type, int rotation, int x, int y);
bool spawn_piece(void);
void lock_piece(void);
void clear_lines(void);
void update_piece(float dt);
void try_move(int dx, int dy);
void try_rotate(int direction);
void hard_drop(void);
void draw_board(void);
void draw_piece(EntityId id);
void draw_preview(PieceType type, int px, int py);
void draw_ui(void);
void render(void);

static int board_pixel_x(int col) { return BOARD_X + col * CELL_SIZE; }

static int board_pixel_y(int row) { return BOARD_Y + row * CELL_SIZE; }

static void draw_cell(int col, int row, Color color)
{
    Rectangle rect = {
        (float)board_pixel_x(col) + 1.0f,
        (float)board_pixel_y(row) + 1.0f,
        (float)CELL_SIZE - 2.0f,
        (float)CELL_SIZE - 2.0f,
    };
    DrawRectangleRec(rect, color);
}

static void draw_piece_cells(PieceType type, int rotation, int x, int y, Color color)
{
    for (int r = 0; r < piece_size; r++) {
        for (int c = 0; c < piece_size; c++) {
            if (!shapes[type][rotation][r][c])
                continue;
            int board_col = x + c;
            int board_row = y + r;
            if (board_row < 0)
                continue;
            draw_cell(board_col, board_row, color);
        }
    }
}

static bool json_string_eq(const struct json_string_s *str, const char *key)
{
    size_t key_len = strlen(key);
    return str && str->string_size == key_len && strncmp(str->string, key, key_len) == 0;
}

static const struct json_value_s *json_object_get(const struct json_object_s *obj, const char *key)
{
    if (!obj)
        return NULL;

    for (struct json_object_element_s *el = obj->start; el; el = el->next) {
        if (json_string_eq(el->name, key))
            return el->value;
    }

    return NULL;
}

static const struct json_value_s *json_array_at(const struct json_array_s *arr, size_t index)
{
    if (!arr || index >= arr->length)
        return NULL;

    struct json_array_element_s *el = arr->start;
    for (size_t i = 0; el && i < index; i++)
        el = el->next;

    return el ? el->value : NULL;
}

static int json_value_int(const struct json_value_s *value)
{
    struct json_number_s *num = json_value_as_number((struct json_value_s *)value);
    if (!num)
        return 0;
    return atoi(num->number);
}

static bool parse_shape_grid(const struct json_value_s *grid, int piece_index, int rotation)
{
    struct json_array_s *array = json_value_as_array((struct json_value_s *)grid);
    if (!array || array->length != (size_t)piece_size)
        return false;

    for (int row = 0; row < piece_size; row++) {
        const struct json_value_s *line_value = json_array_at(array, (size_t)row);
        struct json_array_s *line = json_value_as_array((struct json_value_s *)line_value);
        if (!line || line->length != (size_t)piece_size)
            return false;

        for (int col = 0; col < piece_size; col++) {
            const struct json_value_s *cell = json_array_at(line, (size_t)col);
            if (!cell || cell->type != json_type_number)
                return false;
            shapes[piece_index][rotation][row][col] = (char)json_value_int(cell);
        }
    }

    return true;
}

static bool parse_piece_object(const struct json_value_s *piece, int piece_index)
{
    if (!piece || piece->type != json_type_object)
        return false;

    const struct json_value_s *rotations_value =
        json_object_get(json_value_as_object((struct json_value_s *)piece), "rotations");
    struct json_array_s *rotations = json_value_as_array((struct json_value_s *)rotations_value);
    if (!rotations || rotations->length != ROTATION_COUNT)
        return false;

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        const struct json_value_s *grid = json_array_at(rotations, (size_t)rotation);
        if (!parse_shape_grid(grid, piece_index, rotation))
            return false;
    }

    return true;
}

bool load_shapes(void)
{
    char *json_text = LoadFileText("assets/shapes.json");
    if (!json_text) {
        TraceLog(LOG_ERROR, "Failed to load assets/shapes.json");
        return false;
    }

    struct json_value_s *root = json_parse(json_text, strlen(json_text));
    UnloadFileText(json_text);
    if (!root) {
        TraceLog(LOG_ERROR, "Failed to parse assets/shapes.json");
        return false;
    }

    bool ok = false;
    if (root->type != json_type_object) {
        TraceLog(LOG_ERROR, "Expected object in assets/shapes.json");
        goto cleanup;
    }

    struct json_object_s *object = json_value_as_object(root);
    const struct json_value_s *size_value = json_object_get(object, "piece_size");
    if (!size_value || size_value->type != json_type_number) {
        TraceLog(LOG_ERROR, "Invalid piece_size in assets/shapes.json");
        goto cleanup;
    }

    piece_size = json_value_int(size_value);
    if (piece_size <= 0 || piece_size > MAX_PIECE_SIZE) {
        TraceLog(LOG_ERROR, "Invalid piece_size in assets/shapes.json");
        goto cleanup;
    }

    const struct json_value_s *pieces_value = json_object_get(object, "pieces");
    struct json_array_s *pieces = json_value_as_array((struct json_value_s *)pieces_value);
    if (!pieces || pieces->length != PIECE_COUNT) {
        TraceLog(LOG_ERROR, "Expected %d pieces in assets/shapes.json", PIECE_COUNT);
        goto cleanup;
    }

    for (int i = 0; i < PIECE_COUNT; i++) {
        const struct json_value_s *piece = json_array_at(pieces, (size_t)i);
        if (!parse_piece_object(piece, i)) {
            TraceLog(LOG_ERROR, "Failed to parse piece at index %d", i);
            goto cleanup;
        }
    }

    ok = true;

cleanup:
    free(root);
    return ok;
}

void shuffle_bag(void)
{
    for (int i = 0; i < PIECE_COUNT; i++)
        bag[i] = (PieceType)i;

    for (int i = PIECE_COUNT - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        PieceType tmp = bag[i];
        bag[i] = bag[j];
        bag[j] = tmp;
    }

    bag_index = 0;
}

PieceType draw_from_bag(void)
{
    if (bag_index >= PIECE_COUNT)
        shuffle_bag();
    return bag[bag_index++];
}

bool piece_fits(PieceType type, int rotation, int x, int y)
{
    for (int r = 0; r < piece_size; r++) {
        for (int c = 0; c < piece_size; c++) {
            if (!shapes[type][rotation][r][c])
                continue;

            int board_col = x + c;
            int board_row = y + r;

            if (board_col < 0 || board_col >= BOARD_COLS || board_row >= BOARD_ROWS)
                return false;
            if (board_row >= 0 && board[board_row][board_col] != 0)
                return false;
        }
    }
    return true;
}

bool spawn_piece(void)
{
    activePieceId = alloc_entity();
    assert(activePieceId != E_NULL);
    if (activePieceId == E_NULL)
        return false;

    Entity *piece = &entities[activePieceId];
    piece->type = E_piece;
    piece->piece_type = next_piece_type;
    piece->rotation = 0;
    piece->x = BOARD_COLS / 2 - piece_size / 2;
    piece->y = -1;
    next_piece_type = draw_from_bag();

    if (!piece_fits(piece->piece_type, piece->rotation, piece->x, piece->y)) {
        free_entity(activePieceId);
        activePieceId = E_NULL;
        return false;
    }

    return true;
}

void lock_piece(void)
{
    Entity *piece = &entities[activePieceId];

    for (int r = 0; r < piece_size; r++) {
        for (int c = 0; c < piece_size; c++) {
            if (!shapes[piece->piece_type][piece->rotation][r][c])
                continue;

            int board_col = piece->x + c;
            int board_row = piece->y + r;
            if (board_row < 0)
                continue;
            board[board_row][board_col] = (int)piece->piece_type + 1;
        }
    }

    free_entity(activePieceId);
    activePieceId = E_NULL;
    clear_lines();

    if (!spawn_piece())
        game_over = true;
}

void clear_lines(void)
{
    static const int line_scores[] = {LINE_SCORES};
    int cleared = 0;

    for (int row = BOARD_ROWS - 1; row >= 0; row--) {
        bool full = true;
        for (int col = 0; col < BOARD_COLS; col++) {
            if (board[row][col] == 0) {
                full = false;
                break;
            }
        }
        if (!full)
            continue;

        cleared++;
        for (int shift_row = row; shift_row > 0; shift_row--)
            memcpy(board[shift_row], board[shift_row - 1], sizeof(board[0]));
        memset(board[0], 0, sizeof(board[0]));
        row++;
    }

    if (cleared == 0)
        return;

    lines_cleared += cleared;
    score += line_scores[cleared - 1] * level;
    level = lines_cleared / LINES_PER_LEVEL + 1;
    fall_interval = fmaxf(MIN_FALL_TIME, BASE_FALL_TIME - (level - 1) * 0.07f);
}

void try_move(int dx, int dy)
{
    if (activePieceId == E_NULL || game_over)
        return;

    Entity *piece = &entities[activePieceId];
    int new_x = piece->x + dx;
    int new_y = piece->y + dy;

    if (!piece_fits(piece->piece_type, piece->rotation, new_x, new_y))
        return;

    piece->x = new_x;
    piece->y = new_y;
}

void try_rotate(int direction)
{
    if (activePieceId == E_NULL || game_over)
        return;

    Entity *piece = &entities[activePieceId];
    int new_rotation = (piece->rotation + direction + 4) % 4;
    static const int kicks[] = {0, -1, 1, -2, 2};

    for (int i = 0; i < 5; i++) {
        int test_x = piece->x + kicks[i];
        if (!piece_fits(piece->piece_type, new_rotation, test_x, piece->y))
            continue;
        piece->rotation = new_rotation;
        piece->x = test_x;
        return;
    }
}

void hard_drop(void)
{
    if (activePieceId == E_NULL || game_over)
        return;

    Entity *piece = &entities[activePieceId];
    while (piece_fits(piece->piece_type, piece->rotation, piece->x, piece->y + 1))
        piece->y++;
    score += 2;
    lock_piece();
    fall_timer = 0.0f;
}

void update_piece(float dt)
{
    if (activePieceId == E_NULL || game_over)
        return;

    fall_timer += dt;
    if (fall_timer < fall_interval)
        return;

    fall_timer = 0.0f;
    Entity *piece = &entities[activePieceId];
    if (piece_fits(piece->piece_type, piece->rotation, piece->x, piece->y + 1)) {
        piece->y++;
        return;
    }

    lock_piece();
}

void draw_board(void)
{
    DrawRectangleLines(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, WHITE);

    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLS; col++) {
            if (board[row][col] == 0)
                continue;
            draw_cell(col, row, WHITE);
        }
    }
}

void draw_piece(EntityId id)
{
    if (id == E_NULL)
        return;
    Entity *piece = &entities[id];
    draw_piece_cells(piece->piece_type, piece->rotation, piece->x, piece->y, WHITE);
}

void draw_preview(PieceType type, int px, int py)
{
    for (int r = 0; r < piece_size; r++) {
        for (int c = 0; c < piece_size; c++) {
            if (!shapes[type][0][r][c])
                continue;
            Rectangle rect = {
                (float)px + c * CELL_SIZE + 1.0f,
                (float)py + r * CELL_SIZE + 1.0f,
                (float)CELL_SIZE - 2.0f,
                (float)CELL_SIZE - 2.0f,
            };
            DrawRectangleRec(rect, WHITE);
        }
    }
}

void draw_ui(void)
{
    int panel_x = BOARD_X + BOARD_WIDTH + 40;

    DrawText("NEXT", panel_x, BOARD_Y, 20, WHITE);
    draw_preview(next_piece_type, panel_x, BOARD_Y + 30);

    DrawText(TextFormat("SCORE %d", score), panel_x, BOARD_Y + 150, 20, WHITE);
    DrawText(TextFormat("LINES %d", lines_cleared), panel_x, BOARD_Y + 180, 20, WHITE);
    DrawText(TextFormat("LEVEL %d", level), panel_x, BOARD_Y + 210, 20, WHITE);

    DrawText("LEFT/RIGHT  move", panel_x, BOARD_Y + 280, 16, WHITE);
    DrawText("UP  rotate", panel_x, BOARD_Y + 302, 16, WHITE);
    DrawText("DOWN  soft drop", panel_x, BOARD_Y + 324, 16, WHITE);
    DrawText("SPACE  hard drop", panel_x, BOARD_Y + 346, 16, WHITE);
    DrawText("R  restart", panel_x, BOARD_Y + 368, 16, WHITE);

    if (game_over) {
        const char *msg = "GAME OVER";
        int width = MeasureText(msg, 40);
        DrawText(msg, (SCREEN_WIDTH - width) / 2, SCREEN_HEIGHT / 2 - 20, 40, WHITE);
    }
}

void render(void)
{
    BeginDrawing();
    ClearBackground(BLACK);
    draw_board();
    draw_piece(activePieceId);
    draw_ui();
    DrawText("Tetris", SCREEN_WIDTH - MeasureText("Tetris", 20) - 20, 20, 20, WHITE);
    DrawFPS(10, 10);
    EndDrawing();
}

void reset_game(void)
{
    for (int i = 1; i < MAX_ENTITY; i++)
        entities[i].type = E_none;

    memset(board, 0, sizeof(board));

    score = 0;
    lines_cleared = 0;
    level = 1;
    fall_timer = 0.0f;
    fall_interval = BASE_FALL_TIME;
    game_over = false;
    activePieceId = E_NULL;

    shuffle_bag();
    next_piece_type = draw_from_bag();
    spawn_piece();
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Tetris");
    SetTargetFPS(60);

    if (!load_shapes()) {
        CloseWindow();
        return 1;
    }

    reset_game();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_R))
            reset_game();

        if (!game_over) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
                try_move(-1, 0);
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                try_move(1, 0);
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                try_move(0, 1);
                score += 1;
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
                try_rotate(1);
            if (IsKeyPressed(KEY_SPACE))
                hard_drop();
        }

        update_piece(dt);
        render();
    }

    CloseWindow();
    return 0;
}
