#include "kernel/shell.h"
#include "shell_internal.h"
#include "kernel/keyboard.h"
#include "kernel/klog.h"
#include "kernel/video.h"

#define LINE_BUFFER_SIZE 128
static char line_buffer[LINE_BUFFER_SIZE];
static unsigned int line_length = 0;

static unsigned int str_len(const char *s) {
  unsigned int n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

static void cmd_help(void) {
  print_str("\ncommands: help, clear, echo <text>, meminfo, alloctest, "
            "vmmtest, heaptest, threadtest, largethreadtest, usermodetest, "
            "elftest, ls, run <name>");
}

static void cmd_clear(void) {
  clearwin();
  video_reset_cursor();
}

static void cmd_echo(const char *arg) {
  print_str("\n");
  print_str(arg);
}

typedef void (*exact_handler_fn)(void);
typedef struct {
  const char *name;
  exact_handler_fn handler;
  int suppress_prompt;
} exact_command_t;

typedef void (*arg_handler_fn)(const char *arg);
typedef struct {
  const char *prefix;
  arg_handler_fn handler;
} prefix_command_t;

static const exact_command_t exact_commands[] = {
    {"help", cmd_help, 0},
    {"meminfo", cmd_meminfo, 0},
    {"alloctest", cmd_alloctest, 0},
    {"vmmtest", cmd_vmmtest, 0},
    {"heaptest", cmd_heaptest, 0},
    {"threadtest", cmd_threadtest, 0},
    {"largethreadtest", cmd_largethreadtest, 0},
    {"usermodetest", cmd_usermodetest, 0},
    {"elftest", cmd_elftest, 0},
    {"ls", cmd_ls, 0},
    {"clear", cmd_clear, 1},
};
#define EXACT_COMMAND_COUNT (sizeof(exact_commands) / sizeof(exact_commands[0]))

static const prefix_command_t prefix_commands[] = {
    {"run ", cmd_run},
    {"echo ", cmd_echo},
};
#define PREFIX_COMMAND_COUNT (sizeof(prefix_commands) / sizeof(prefix_commands[0]))

static void print_prompt(void) { print_str("\n> "); }

static void run_command(const char *line) {
  klog_write("shell: command '");
  klog_write(line);
  klog_write("'\n");

  if (line[0] == '\0') {
    print_prompt();
    return;
  }

  for (unsigned int i = 0; i < EXACT_COMMAND_COUNT; i++) {
    if (!str_eq(line, exact_commands[i].name))
      continue;

    exact_commands[i].handler();
    if (!exact_commands[i].suppress_prompt)
      print_prompt();
    return;
  }

  for (unsigned int i = 0; i < PREFIX_COMMAND_COUNT; i++) {
    if (!str_starts_with(line, prefix_commands[i].prefix))
      continue;

    prefix_commands[i].handler(line + str_len(prefix_commands[i].prefix));
    print_prompt();
    return;
  }

  print_str("\nunknown command: ");
  print_str(line);
  print_prompt();
}

__attribute__((noreturn)) void shell_run(void) {
  print_str("Type 'help' for a list of commands.");
  print_prompt();

  for (;;) {
    char c = kbd_getchar();

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
  }
}
