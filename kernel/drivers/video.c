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
