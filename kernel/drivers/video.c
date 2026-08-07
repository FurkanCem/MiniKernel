#include "kernel/video.h"

#define VGA_START 0xB8000
#define VGA_EXTENT 80 * 25
#define VGA_COLS 80

#define STYLE_WB 0x0F

typedef struct __attribute__((packed)) {
  char character;
  char style;
} vga_char;

static volatile vga_char *TEXT_AREA = (vga_char *)VGA_START;

void clearwin(void) {
  vga_char clear_char = {.character = ' ', .style = STYLE_WB};

  for (unsigned int i = 0; i < VGA_EXTENT; i++) {
    TEXT_AREA[i] = clear_char;
  }
}

void putstr(const char *str) {
  for (unsigned int i = 0; str[i] != '\0'; i++) {
    if (i >= VGA_EXTENT)
      break;

    vga_char temp = {.character = str[i], .style = STYLE_WB};

    TEXT_AREA[i] = temp;
  }
}

void putstr_at(const char *str, unsigned int row) {
  unsigned int base = row * VGA_COLS;

  for (unsigned int i = 0; str[i] != '\0'; i++) {
    if (base + i >= VGA_EXTENT)
      break;

    vga_char temp = {.character = str[i], .style = STYLE_WB};

    TEXT_AREA[base + i] = temp;
  }
}

#define INPUT_START_ROW 3
#define VGA_ROWS 25

static unsigned int cursor_row = INPUT_START_ROW;
static unsigned int cursor_col = 0;

static void write_cell(unsigned int row, unsigned int col, char c) {
  unsigned int pos = row * VGA_COLS + col;
  if (pos >= VGA_EXTENT)
    return;

  vga_char temp = {.character = c, .style = STYLE_WB};
  TEXT_AREA[pos] = temp;
}

void video_reset_cursor(void) {
  cursor_row = INPUT_START_ROW;
  cursor_col = 0;
}

void putchar_at_cursor(char c) {
  if (c == '\n') {
    cursor_row++;
    cursor_col = 0;
  } else if (c == '\b') {
    if (cursor_col > 0) {
      cursor_col--;
      write_cell(cursor_row, cursor_col, ' ');
    }
  } else {
    write_cell(cursor_row, cursor_col, c);
    cursor_col++;
    if (cursor_col >= VGA_COLS) {
      cursor_col = 0;
      cursor_row++;
    }
  }

  if (cursor_row >= VGA_ROWS) {
    cursor_row = INPUT_START_ROW;
  }
}
