#include <stdint.h>
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAME_BUILD

typedef struct {
  char *chars;
  size_t numchars;
} BoardSide;

typedef struct {
  BoardSide *sides;
  size_t numsides;
  size_t capcity;
} Board;

typedef struct {
  char *chars;
  size_t numchars;
} Word;

typedef struct {
  Word *words;
  size_t numwords;
  size_t capcity;
} Dict;

typedef struct {
  Dict *dict;
  Board *board;
  Word *hist;
  size_t hist_count;
  size_t hist_capcity;
} Game;

#define GROW_CAPACITY(capcity) ((capcity) < 8 ? 8 : (capcity) * 2)
#define GROW_ARRAY(type, pointer, old_count, new_count)                        \
  (type *)reallocate(pointer, sizeof(type) * (old_count),                      \
                     sizeof(type) * new_count)
#define FREE_ARRAY(type, pointer, old_count)                                   \
  reallocate(pointer, sizeof(type) * old_count, 0)

#define GROW_STR_CAPCITY(capcity) ((capcity) < 255 ? 255 : (capcity) * 2)

static void *reallocate(void *pointer, size_t old_size, size_t new_size);

static void board_side_init(BoardSide *board_side);
static void board_side_free(BoardSide *board_side);

static void board_init(Board *board);
static void board_free(Board *board);
static bool board_add_side(Board *board, BoardSide *side);
static bool board_load_from_file(Board *board, const char *board_file_path);
static bool board_check_validity(Board *board);
static void board_get_all_chars(Board *board, char **out, size_t *size);

static void word_init(Word *word);
static bool word_set_n(Word *word, const char *str, size_t len);
static void word_free(Word *word);

static void dict_init(Dict *dict);
static void dict_free(Dict *dict);
static bool dict_append_word(Dict *dict, const char *str);
static bool dict_load_from_file(Dict *dict, const char *dict_file_path);
static bool dict_match_word(Dict *dict, const char *match, size_t *out_index);

static void game_init(Game *game);
static void game_free(Game *game);
static bool game_load_from_files(Game *game, const char *dict_file_path,
                                 const char *board_file_path);
static bool game_check_validity(Game *game);
static bool game_history_append(Game *game, Word *word);
static int game_run(Game *game);
static bool game_check_win(Game *game, char *sorted_board_chars);

bool remove_spaces_from_string(char *str, char **out, size_t *out_size);

int main(int argc, char **argv) {
  if (argc != 3) {
#ifndef LAME_BUILD
    printf("expected: 2 arguments <board-file> <dict-file>, got: %i\n", argc);
#endif
    return 1;
  }

  const char *board_file_path = argv[1];
  const char *dict_file_path = argv[2];
  int return_code = 0;

  Game game;
  game_init(&game);
  if (!game_load_from_files(&game, dict_file_path, board_file_path)) {
    return_code = 1;
    goto RET;
  }

  if (!game_check_validity(&game)) {
    return_code = 1;
    goto RET;
  }

  return_code = game_run(&game);

RET:
  game_free(&game);
  return return_code;
}

bool remove_spaces_and_trilling_newline_from_string(char *str, char **out,
                                                    size_t *out_size) {
  const size_t len = strlen(str);
  char *new_str = malloc(len + 1);
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (str[i] != ' ') {
      new_str[j++] = str[i];
    }
  }
  if (j && new_str[j - 1] == '\n') {
    j--;
  }

  new_str[j] = '\0';
  char *trunc = reallocate(new_str, len + 1, j);
  if (trunc == NULL) {
    free(new_str);
    return false;
  }

  *out = trunc;
  *out_size = j;
  return true;
}

void *reallocate(void *pointer, size_t old_size, size_t new_size) {
  (void)old_size;
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void *result = realloc(pointer, new_size);
  return result;
}

void board_side_init(BoardSide *board_side) {
  board_side->chars = NULL;
  board_side->numchars = 0;
}

void board_side_free(BoardSide *board_side) {
  free(board_side->chars);
  board_side->chars = NULL;
  board_side->numchars = 0;
}

void board_init(Board *board) {
  board->sides = NULL;
  board->capcity = 0;
  board->numsides = 0;
}

void board_free(Board *board) {
  for (size_t i = 0; i < board->numsides; ++i) {
    board_side_free(&board->sides[i]);
  }
  free(board->sides);
  board->sides = NULL;
  board->capcity = 0;
  board->numsides = 0;
}

bool board_add_side(Board *board, BoardSide *side) {
  if (board->capcity < board->numsides + 1) {
    int old_capacity = board->capcity;
    board->capcity = GROW_CAPACITY(old_capacity);
    board->sides =
        GROW_ARRAY(BoardSide, board->sides, old_capacity, board->capcity);
    if (board->sides == NULL) {
      return false;
    }
  }
  board->sides[board->numsides++] = *side;
  return true;
}

bool board_load_from_file(Board *board, const char *board_file_path) {
  FILE *board_file = fopen(board_file_path, "r");
  if (board_file == NULL) {
#ifndef LAME_BUILD
    printf("error opening board file: '%s'.\n", board_file_path);
#endif
    return false;
  }

  bool success = true;
  size_t alloc_sz = 0;
  char *line = NULL;
  while (getline(&line, &alloc_sz, board_file) != -1) {
    BoardSide side;
    board_side_init(&side);
    if (!remove_spaces_and_trilling_newline_from_string(line, &side.chars,
                                                        &side.numchars)) {
#ifndef LAME_BUILD
      printf("failed to truncate spaces from line: %s.\n", line);
#endif
      success = false;
      break;
    }

    if (!board_add_side(board, &side)) {
#ifndef LAME_BUILD
      printf("failed to add side to the board.\n");
#endif
      success = false;
      break;
    }
  }

  free(line);
  fclose(board_file);
  return success;
}

bool board_check_validity(Board *board) {
  if (board->numsides < 3) {
    return false;
  }

  bool seen[256] = {false};

  for (size_t i = 0; i < board->numsides; ++i) {
    BoardSide *side = &board->sides[i];
    for (size_t j = 0; j < side->numchars; ++j) {
      unsigned char c = side->chars[j];
      if (seen[c]) {
        return false;
      }
      seen[c] = true;
    }
  }
  return true;
}

void board_get_all_chars(Board *board, char **out, size_t *size) {
  if (board->numsides == 0) {
    out = NULL;
    size = NULL;
    return;
  }

  for (size_t i = 0; i < board->numsides; i++) {
    *size += board->sides[i].numchars;
  }
  *out = malloc(*size + 1);
  memset(*out, 0, *size + 1);
  for (size_t i = 0; i < board->numsides; ++i) {
    memcpy(*out + i * board->sides[i].numchars, board->sides[i].chars,
           board->sides[i].numchars);
  }
}

void word_init(Word *word) {
  word->chars = NULL;
  word->numchars = 0;
}

void word_free(Word *word) {
  free(word->chars);
  word_init(word);
}

bool word_set_n(Word *word, const char *str, size_t len) {
  word->chars = malloc(len + 1);
  if (memcpy(word->chars, str, len) != NULL) {
    word->numchars = len;
    word->chars[len] = '\0';
    return true;
  }
  return false;
}

void dict_init(Dict *dict) {
  dict->words = NULL;
  dict->numwords = 0;
  dict->capcity = 0;
}

bool dict_append_word(Dict *dict, const char *str) {
  size_t wordnumchars = strlen(str);
  if (dict->capcity < dict->numwords + 1) {
    int old_capacity = dict->capcity;
    dict->capcity = GROW_CAPACITY(old_capacity);
    dict->words = GROW_ARRAY(Word, dict->words, old_capacity, dict->capcity);
    if (dict->words == NULL) {
      return false;
    }
  }

  Word *word = &dict->words[dict->numwords++];
  word_init(word);
  word_set_n(word, str, wordnumchars);

  return true;
}

bool dict_match_word(Dict *dict, const char *match, size_t *out_index) {
  size_t left = 0;
  size_t right = dict->numwords - 1;

  while (left <= right) {
    size_t mid = left + (right - left) / 2;
    if (strcmp(dict->words[mid].chars, match) == 0) {
      *out_index = mid;
      return true;
    }
    if (strcmp(dict->words[mid].chars, match) < 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  return false;
}

void dict_free(Dict *dict) {
  for (size_t i = 0; i < dict->numwords; i++) {
    word_free(&dict->words[i]);
  }
  free(dict->words);
  dict_init(dict);
}

bool dict_load_from_file(Dict *dict, const char *dict_file_path) {
  FILE *file = fopen(dict_file_path, "r");
  if (file == NULL) {
#ifndef LAME_BUILD
    printf("error opening dict file: '%s'.\n", dict_file_path);
#endif
    return false;
  }

  bool success = true;
  char *line = NULL;
  size_t alloc_sz = 0;
  while (getline(&line, &alloc_sz, file) != -1) {
    char *word_chars = NULL;
    size_t sz = 0;
    if (!remove_spaces_and_trilling_newline_from_string(line, &word_chars,
                                                        &sz)) {
#ifndef LAME_BUILD
      printf("failed to truncate spaces from line: %s\n", line);
#endif
      success = false;
      break;
    }
    if (!dict_append_word(dict, word_chars)) {
#ifndef LAME_BUILD
      printf("failed to append word: %s to dict.\n", word_chars);
#endif
      success = false;
      break;
    }
  }
  free(line);
  fclose(file);
  return success;
}

void game_init(Game *game) {
  game->board = malloc(sizeof(Board));
  board_init(game->board);
  game->dict = malloc(sizeof(Dict));
  dict_init(game->dict);
  game->hist_count = 0;
  game->hist_capcity = 0;
  game->hist = NULL;
}

void game_free(Game *game) {
  if (game->board != NULL) {
    board_free(game->board);
    free(game->board);
    game->board = NULL;
  }
  if (game->dict != NULL) {
    dict_free(game->dict);
    free(game->dict);
    game->dict = NULL;
  }

  for (size_t i = 0; i < game->hist_count; i++) {
    word_free(&game->hist[i]);
  }

  free(game->hist);
  game->hist_count = 0;
  game->hist_capcity = 0;
}

bool game_load_from_files(Game *game, const char *dict_file_path,
                          const char *board_file_path) {
  if (!board_load_from_file(game->board, board_file_path)) {
#ifndef LAME_BUILD
    printf("failed to load board from file: %s\n", board_file_path);
#endif
    return false;
  }
  if (!dict_load_from_file(game->dict, dict_file_path)) {
#ifndef LAME_BUILD
    printf("failed to load dict from file: %s\n", dict_file_path);
#endif
    return false;
  }
  return true;
}

bool game_check_validity(Game *game) {
  if (!board_check_validity(game->board)) {
    printf("Invalid board\n");
    return false;
  }
  return true;
}

bool game_history_append(Game *game, Word *word) {
  if (game->hist_capcity < game->hist_count + 1) {

    int old_capacity = game->hist_capcity;
    game->hist_capcity = GROW_CAPACITY(old_capacity);
    game->hist = GROW_ARRAY(Word, game->hist, old_capacity, game->hist_capcity);
    if (game->hist == NULL) {
      return false;
    }
  }
  game->hist[game->hist_count++] = *word;
  return true;
}

int compare_chars(const void *a, const void *b) {
  return (*(char *)a - *(char *)b);
}

bool game_check_win(Game *game, char *sorted_board_chars) {

  bool used[256] = {false};

  for (size_t i = 0; i < game->hist_count; i++) {
    for (size_t j = 0; j < game->hist[i].numchars; j++) {
      used[(unsigned char)game->hist[i].chars[j]] = true;
    }
  }

  for (size_t i = 0; i < strlen(sorted_board_chars); i++) {
    if (!used[(unsigned char)sorted_board_chars[i]]) {
      return false;
    }
  }
  return true;
}

int game_run(Game *game) {

  char *board_chars = NULL;
  size_t board_chars_size = 0;
  board_get_all_chars(game->board, &board_chars, &board_chars_size);
  qsort(board_chars, board_chars_size, sizeof(char), compare_chars);
  bool has_problem = false;
  int return_code = 0;
  while (true) {
    char *line = NULL;
    size_t sz = 0;
    long read = getline(&line, &sz, stdin);
    if (read != -1) {
      if (read == 1 && line[0] == '\n') {
        break;
      }
      if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
        read--;
      }
      for (long i = 0; i < read; ++i) {
        bool in = false;
        for (size_t j = 0; j < game->board->numsides; ++j) {
          for (size_t k = 0; k < game->board->sides[j].numchars; ++k) {
            if (line[i] == game->board->sides[j].chars[k]) {
              in = true;
              if (i > 0) {
                for (size_t c = 0; c < game->board->sides[j].numchars; ++c) {
                  if (line[i - 1] == game->board->sides[j].chars[c]) {
#ifdef LAME_BUILD
                    printf("Same-side letter used consecutively\n");
#else
                    printf("Same-side letter: %c used consecutively\n",
                           line[i - 1]);
#endif
                    has_problem = true;
                    return_code = 0;
                    goto PROBLEM;
                  }
                }
              }
            }
          }
        }
        if (!in) {
#ifdef LAME_BUILD
          printf("Used a letter not present on the board\n");
#else
          printf("Used a letter: %c not present on the board\n", line[i]);

#endif
          has_problem = true;
          return_code = 0;
          goto PROBLEM;
        }
      }

      size_t match = 0;
      if (!dict_match_word(game->dict, line, &match)) {
#ifdef LAME_BUILD
        printf("Word not found in dictionary\n");
#else
        printf("Word: %s not found in dictionary\n", line);
#endif
        has_problem = true;
        return_code = 0;
        goto PROBLEM;
      }

      if (game->hist_count != 0) {
        Word *last_word = &game->hist[game->hist_count - 1];
        if (line[0] != last_word->chars[last_word->numchars - 1]) {
#ifdef LAME_BUILD
          printf("First letter of word does not match last letter of previous "
                 "word\n");
#else
          printf("First letter of word: (%s, %c) does not match last letter of "
                 "previous "
                 "word: (%s, %c)\n",
                 line, line[0], last_word->chars,
                 last_word->chars[last_word->numchars - 1]);
#endif
          has_problem = true;
          return_code = 0;
          goto PROBLEM;
        }
      }

      Word word;
      word_init(&word);
      word_set_n(&word, line, read);
      game_history_append(game, &word); // move word ownership to game

    } else {
      break;
    }
  PROBLEM:
    free(line);
    if (has_problem) {
      return return_code;
    }
  }
  if (game_check_win(game, board_chars)) {
    printf("Correct\n");
    return 0;
  } else {
    printf("Not all letters used\n");
    return 0;
  }
}