#include "kernel/shell.h"
#include "kernel/e820.h"
#include "kernel/keyboard.h"
#include "kernel/klog.h"
#include "kernel/video.h"

#define LINE_BUFFER_SIZE 128
static char line_buffer[LINE_BUFFER_SIZE];
static unsigned int line_length = 0;

static int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static int str_starts_with(const char *str, const char *prefix) {
  unsigned int i = 0;
  while (prefix[i] != '\0') {
    if (str[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

static void print_str(const char *str) {
  for (unsigned int i = 0; str[i] != '\0'; i++) {
    putchar_at_cursor(str[i]);
  }
}

static void print_hex(unsigned long long value) {
  static const char digits[] = "0123456789ABCDEF";
  char buf[19]; /* "0x" + 16 hex digits + NUL */

  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 16; i++) {
    buf[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
  }
  buf[18] = '\0';

  print_str(buf);
}

static void print_meminfo(void) {
  unsigned int count = e820_entry_count();

  print_str("\nBIOS memory map (");
  print_hex(count);
  print_str(" regions):");

  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0)
      break;

    print_str("\n  ");
    print_hex(entry->base);
    print_str(" - ");
    print_hex(entry->base + entry->length);
    print_str("  ");
    print_str(e820_type_name(entry->type));
  }

  print_str("\ntotal usable: ");
  print_hex(e820_total_usable_bytes());
  print_str(" bytes");
}

static void print_prompt(void) { print_str("\n> "); }

static void run_command(const char *line) {
  klog_write("shell: command '");
  klog_write(line);
  klog_write("'\n");

  if (line[0] == '\0') {
    /* empty line - nothing to do, just reprint the prompt below */
  } else if (str_eq(line, "help")) {
    print_str("\ncommands: help, clear, echo <text>, meminfo");
  } else if (str_eq(line, "meminfo")) {
    print_meminfo();
  } else if (str_eq(line, "clear")) {
    clearwin();
    video_reset_cursor();
    return; /* fresh screen already has the cursor in the right spot */
  } else if (str_starts_with(line, "echo ")) {
    print_str("\n");
    print_str(line + 5);
  } else {
    print_str("\nunknown command: ");
    print_str(line);
  }

  print_prompt();
}

__attribute__((noreturn)) void shell_run(void) {
  print_str("Type 'help' for a list of commands.");
  print_prompt();

  for (;;) {
    char c;

    if (!kbd_read_char(&c)) {
      __asm__ volatile("hlt"); /* nothing to do - wake on the next interrupt */
      continue;
    }

    if (c == '\n') {
      line_buffer[line_length] = '\0';
      run_command(line_buffer);
      line_length = 0;
    } else if (c == '\b') {
      if (line_length > 0) {
        line_length--;
        putchar_at_cursor('\b');
      }
    } else if (line_length < LINE_BUFFER_SIZE - 1) {
      line_buffer[line_length++] = c;
      putchar_at_cursor(c);
    }
    /* else: line full - drop further characters until Enter/Backspace */
  }
}
