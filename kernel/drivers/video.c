#include "kernel/video.h"
#include "kernel/io.h"

#define VGA_START 0xB8000
#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_EXTENT (VGA_COLS * VGA_ROWS)

#define STYLE_WB 0x0F

#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_CRTC_CURSOR_HIGH 0x0E
#define VGA_CRTC_CURSOR_LOW 0x0F

#define INPUT_START_ROW 3
#define TAB_WIDTH 4

typedef struct __attribute__((packed)) {
  char character;
  char style;
} vga_char;

static volatile vga_char *TEXT_AREA = (vga_char *)VGA_START;

static unsigned int cursor_row = INPUT_START_ROW;
static unsigned int cursor_col = 0;

static void update_hardware_cursor(void) {
  unsigned short pos = (unsigned short)(cursor_row * VGA_COLS + cursor_col);

  outb(VGA_CRTC_INDEX, VGA_CRTC_CURSOR_LOW);
  outb(VGA_CRTC_DATA, (unsigned char)(pos & 0xFF));
  outb(VGA_CRTC_INDEX, VGA_CRTC_CURSOR_HIGH);
  outb(VGA_CRTC_DATA, (unsigned char)((pos >> 8) & 0xFF));
}

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

static void write_cell(unsigned int row, unsigned int col, char c) {
  unsigned int pos = row * VGA_COLS + col;
  if (pos >= VGA_EXTENT)
    return;

  vga_char temp = {.character = c, .style = STYLE_WB};
  TEXT_AREA[pos] = temp;
}

static void scroll_up(void) {
  for (unsigned int row = INPUT_START_ROW + 1; row < VGA_ROWS; row++) {
    for (unsigned int col = 0; col < VGA_COLS; col++) {
      TEXT_AREA[(row - 1) * VGA_COLS + col] = TEXT_AREA[row * VGA_COLS + col];
    }
  }

  vga_char clear_char = {.character = ' ', .style = STYLE_WB};
  for (unsigned int col = 0; col < VGA_COLS; col++) {
    TEXT_AREA[(VGA_ROWS - 1) * VGA_COLS + col] = clear_char;
  }
}

void video_reset_cursor(void) {
  unsigned long long flags = irq_save();
  cursor_row = INPUT_START_ROW;
  cursor_col = 0;
  update_hardware_cursor();
  irq_restore(flags);
}

void video_set_cursor(unsigned int row, unsigned int col) {
  unsigned long long flags = irq_save();
  if (row < VGA_ROWS)
    cursor_row = row;
  if (col < VGA_COLS)
    cursor_col = col;
  update_hardware_cursor();
  irq_restore(flags);
}

void video_draw_row(unsigned int row, const char *buf, unsigned int len) {
  if (row >= VGA_ROWS)
    return;

  unsigned long long flags = irq_save();

  unsigned int base = row * VGA_COLS;
  for (unsigned int col = 0; col < VGA_COLS; col++) {
    char c = (col < len) ? buf[col] : ' ';
    vga_char temp = {.character = c, .style = STYLE_WB};
    TEXT_AREA[base + col] = temp;
  }

  irq_restore(flags);
}

unsigned int video_cols(void) { return VGA_COLS; }
unsigned int video_rows(void) { return VGA_ROWS; }

void putchar_at_cursor(char c) {
  unsigned long long flags = irq_save();

  if (c == '\n') {
    cursor_row++;
    cursor_col = 0;
  } else if (c == '\b') {
    if (cursor_col > 0) {
      cursor_col--;
      write_cell(cursor_row, cursor_col, ' ');
    } else if (cursor_row > INPUT_START_ROW) {
      cursor_row--;
      cursor_col = VGA_COLS - 1;
      write_cell(cursor_row, cursor_col, ' ');
    }
  } else if (c == '\t') {
    unsigned int next_stop = (cursor_col / TAB_WIDTH + 1) * TAB_WIDTH;
    while (cursor_col < next_stop && cursor_col < VGA_COLS) {
      write_cell(cursor_row, cursor_col, ' ');
      cursor_col++;
    }
    if (cursor_col >= VGA_COLS) {
      cursor_col = 0;
      cursor_row++;
    }
  } else {
    write_cell(cursor_row, cursor_col, c);
    cursor_col++;
    if (cursor_col >= VGA_COLS) {
      cursor_col = 0;
      cursor_row++;
    }
  }

  while (cursor_row >= VGA_ROWS) {
    scroll_up();
    cursor_row--;
  }

  update_hardware_cursor();
  irq_restore(flags);
}
