#include "kernel/irq.h"
#include "kernel/keyboard.h"
#include "kernel/keyboard_layout.h"
#include "kernel/klog.h"
#include "kernel/io.h"
#include "kernel/ring_buffer.h"
#include "kernel/thread.h"

#define KEYBOARD_DATA_PORT 0x60

#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_LSHIFT_UP (SCANCODE_LSHIFT | 0x80)
#define SCANCODE_RSHIFT_UP (SCANCODE_RSHIFT | 0x80)
#define SCANCODE_RELEASE_BIT 0x80

static int shift_held = 0;

#define KBD_BUFFER_SIZE 256
static volatile char kbd_storage[KBD_BUFFER_SIZE];
static ring_buffer_t kbd_buffer;
static int kbd_wait_channel;

int kbd_read_char(char *out) { return ring_buffer_pop(&kbd_buffer, out); }

char kbd_getchar(void) {
  char c;
  while (!ring_buffer_pop(&kbd_buffer, &c)) {
    sched_sleep(&kbd_wait_channel);
  }
  return c;
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
    return;
  }

  const char *seq = keyboard_layout_translate(scancode, shift_held);
  if (seq == 0)
    return;

  for (unsigned int i = 0; seq[i] != '\0'; i++) {
    ring_buffer_push(&kbd_buffer, seq[i]);
  }

  sched_wakeup(&kbd_wait_channel);

  klog_write("key: '");
  klog_write(seq);
  klog_write("'\n");
}

void keyboard_driver_init(void) {
  ring_buffer_init(&kbd_buffer, kbd_storage, KBD_BUFFER_SIZE);
  irq_register_handler(1, keyboard_irq_handler);
}
