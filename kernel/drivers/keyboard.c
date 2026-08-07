#include "kernel/keyboard.h"
#include "kernel/io.h"
#include "kernel/irq.h"
#include "kernel/klog.h"

#define KEYBOARD_DATA_PORT 0x60

static const char *scancode_to_str[128] = {
    [0x01] = "\x1b", /* Esc */
    [0x02] = "1",    [0x03] = "2", [0x04] = "3",
    [0x05] = "4",    [0x06] = "5", [0x07] = "6",
    [0x08] = "7",    [0x09] = "8", [0x0A] = "9",
    [0x0B] = "0",    [0x0C] = "*", /* OEM_8 */
    [0x0D] = "-",                  /* OEM_MINUS */
    [0x0E] = "\b",                 /* Backspace */
    [0x0F] = "\t",                 /* Tab */
    [0x10] = "q",    [0x11] = "w", [0x12] = "e",
    [0x13] = "r",    [0x14] = "t", [0x15] = "y",
    [0x16] = "u",    [0x17] = "ı",               /* dotless i */
    [0x18] = "o",    [0x19] = "p", [0x1A] = "ğ", /* OEM_4 */
    [0x1B] = "ü",                                /* OEM_6 */
    [0x1C] = "\n",                               /* Enter */
    [0x1E] = "a",    [0x1F] = "s", [0x20] = "d",
    [0x21] = "f",    [0x22] = "g", [0x23] = "h",
    [0x24] = "j",    [0x25] = "k", [0x26] = "l",
    [0x27] = "ş",  /* OEM_1 */
    [0x28] = "i",  /* OEM_7, plain ASCII dotted i */
    [0x29] = "\"", /* OEM_3, backtick position */
    [0x2B] = ",",  /* OEM_COMMA, ISO extra key */
    [0x2C] = "z",    [0x2D] = "x", [0x2E] = "c",
    [0x2F] = "v",    [0x30] = "b", [0x31] = "n",
    [0x32] = "m",    [0x33] = "ö", /* OEM_2, US comma position */
    [0x34] = "ç",                  /* OEM_5, US period position */
    [0x35] = ".",                  /* OEM_PERIOD, US slash position */
    [0x37] = "*",                  /* keypad */
    [0x39] = " ",                  /* Space */
    [0x4A] = "-",                  /* keypad */
    [0x4E] = "+",                  /* keypad */
    [0x56] = "<",                  /* OEM_102, ISO extra key */
};

static const char *scancode_to_str_shifted[128] = {
    [0x01] = "\x1b", [0x02] = "!", [0x03] = "'", [0x04] = "^",  [0x05] = "+",
    [0x06] = "%",    [0x07] = "&", [0x08] = "/", [0x09] = "(",  [0x0A] = ")",
    [0x0B] = "=",    [0x0C] = "?", [0x0D] = "_", [0x0E] = "\b", [0x0F] = "\t",
    [0x10] = "Q",    [0x11] = "W", [0x12] = "E", [0x13] = "R",  [0x14] = "T",
    [0x15] = "Y",    [0x16] = "U", [0x17] = "I", /* plain ASCII - dotless
                                                    capital */
    [0x18] = "O",    [0x19] = "P", [0x1A] = "Ğ", [0x1B] = "Ü",  [0x1C] = "\n",
    [0x1E] = "A",    [0x1F] = "S", [0x20] = "D", [0x21] = "F",  [0x22] = "G",
    [0x23] = "H",    [0x24] = "J", [0x25] = "K", [0x26] = "L",  [0x27] = "Ş",
    [0x28] = "İ", /* dotted capital - NOT ASCII */
    [0x29] = "é",    [0x2B] = ";", [0x2C] = "Z", [0x2D] = "X",  [0x2E] = "C",
    [0x2F] = "V",    [0x30] = "B", [0x31] = "N", [0x32] = "M",  [0x33] = "Ö",
    [0x34] = "Ç",    [0x35] = ":", [0x37] = "*", [0x39] = " ",  [0x4A] = "-",
    [0x4E] = "+",    [0x56] = ">",
};

#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_LSHIFT_UP (SCANCODE_LSHIFT | 0x80)
#define SCANCODE_RSHIFT_UP (SCANCODE_RSHIFT | 0x80)
#define SCANCODE_RELEASE_BIT 0x80

static int shift_held = 0;

#define KBD_BUFFER_SIZE 256
static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

static void kbd_push(char c) {
  unsigned int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
  if (next == kbd_tail)
    return; /* buffer full - drop rather than overwrite unread bytes */

  kbd_buffer[kbd_head] = c;
  kbd_head = next;
}

int kbd_read_char(char *out) {
  if (kbd_tail == kbd_head)
    return 0; /* empty */

  *out = kbd_buffer[kbd_tail];
  kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
  return 1;
}

static void keyboard_irq_handler(registers_t *regs) {
  (void)regs;

  unsigned char scancode = inb(KEYBOARD_DATA_PORT);

  if (scancode == SCANCODE_LSHIFT || scancode == SCANCODE_RSHIFT) {
    shift_held = 1;
    return;
  }
  if (scancode == SCANCODE_LSHIFT_UP || scancode == SCANCODE_RSHIFT_UP) {
    shift_held = 0;
    return;
  }
  if (scancode & SCANCODE_RELEASE_BIT) {
    return; /* some other key's release code - nothing to do with it yet */
  }

  const char *seq = shift_held ? scancode_to_str_shifted[scancode]
                               : scancode_to_str[scancode];
  if (seq == 0)
    return; /* unmapped key (function keys, arrows, ...) - ignore for now */

  for (unsigned int i = 0; seq[i] != '\0'; i++) {
    kbd_push(seq[i]);
  }

  klog_write("key: '");
  klog_write(seq);
  klog_write("'\n");
}

void keyboard_driver_init(void) {
  irq_register_handler(1, keyboard_irq_handler);
}
