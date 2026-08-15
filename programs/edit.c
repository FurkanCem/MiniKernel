#include "syscall.h"

/* A small line-numbered text editor for MiniKernel.
 *
 * The keyboard driver in this kernel doesn't decode arrow keys or other
 * extended scancodes (see kernel/drivers/keyboard/keyboard_layout_tr.c),
 * so there's no way to move a cursor left/right/up/down within existing
 * text. Rather than fake a visual editor that can't actually navigate,
 * this is modeled on `ed`/`edlin`: the whole file is shown on screen
 * with line numbers, and edits are line-numbered commands typed on the
 * bottom row. Full-screen viewing + line commands turns out to cover
 * "write and edit a file" quite well despite the missing arrow keys.
 */

#define MAX_LINES 200
#define MAX_LINE_LEN 120
#define CMD_MAX 128

#define TITLE_ROW 0
#define HINT_ROW 1
#define TEXT_TOP 2

static char lines[MAX_LINES][MAX_LINE_LEN + 1];
static int line_count = 0;
static int modified = 0;
static char filename[64];

static unsigned long screen_cols = 80;
static unsigned long screen_rows = 25;
static unsigned long status_row;
static unsigned long cmd_row;
static unsigned long text_rows_visible;
static int top_line = 0; /* first line (0-indexed) currently shown */

static unsigned long str_len(const char *s) {
  unsigned long n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

static int str_eq(const char *a, const char *b) {
  unsigned long i = 0;
  for (; a[i] != '\0' || b[i] != '\0'; i++) {
    if (a[i] != b[i])
      return 0;
  }
  return 1;
}

static int str_starts_with(const char *s, const char *prefix) {
  unsigned long i = 0;
  while (prefix[i] != '\0') {
    if (s[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

static void print(const char *s) { sys_write(STDOUT, s, str_len(s)); }

/* Parses a non-negative integer starting at s, skipping leading spaces.
 * Returns -1 if there isn't one. Advances *endptr past the digits. */
static long parse_uint(const char *s, const char **endptr) {
  while (*s == ' ')
    s++;

  if (*s < '0' || *s > '9') {
    if (endptr)
      *endptr = s;
    return -1;
  }

  long value = 0;
  while (*s >= '0' && *s <= '9') {
    value = value * 10 + (*s - '0');
    s++;
  }
  if (endptr)
    *endptr = s;
  return value;
}

static void utoa(unsigned long value, char *out, int min_digits) {
  char digits[20];
  int n = 0;
  if (value == 0)
    digits[n++] = '0';
  while (value > 0) {
    digits[n++] = (char)('0' + (value % 10));
    value /= 10;
  }
  int i = 0;
  while (n < min_digits) {
    out[i++] = '0';
    min_digits--;
  }
  while (n > 0)
    out[i++] = digits[--n];
  out[i] = '\0';
}

static long read_line(char *buf, unsigned long max_len) {
  unsigned long len = 0;
  while (1) {
    char c;
    sys_read(STDIN, &c, 1);

    if (c == '\n') {
      buf[len] = '\0';
      sys_write(STDOUT, "\n", 1);
      return (long)len;
    }

    if (c == '\b') {
      if (len > 0) {
        len--;
        sys_write(STDOUT, "\b", 1);
      }
      continue;
    }

    if (len + 1 < max_len) {
      buf[len++] = c;
      sys_write(STDOUT, &c, 1);
    }
  }
}

static void status(const char *msg) {
  sys_draw_row(status_row, msg, str_len(msg));
}

/* Draws the whole screen: title, hint, the visible window of text
 * lines (with numbers), whatever's on the status row, and finally
 * parks the cursor at the command prompt. */
static void redraw(void) {
  char title[96];
  unsigned long i = 0;
  const char *t = "MiniKernel editor - ";
  while (t[i] != '\0') {
    title[i] = t[i];
    i++;
  }
  unsigned long j = 0;
  while (filename[j] != '\0' && i < sizeof(title) - 12) {
    title[i++] = filename[j++];
  }
  if (modified) {
    const char *m = " [modified]";
    unsigned long k = 0;
    while (m[k] != '\0') {
      title[i++] = m[k++];
    }
  }
  title[i] = '\0';
  sys_draw_row(TITLE_ROW, title, str_len(title));

  const char *hint = "a=append  i<n> r<n> d<n>=insert/replace/delete line  "
                      "g<n>=goto  w=save  q=quit  wq  q!=discard";
  sys_draw_row(HINT_ROW, hint, str_len(hint));

  for (unsigned long row = 0; row < text_rows_visible; row++) {
    int line_index = top_line + (int)row;
    char buf[MAX_LINE_LEN + 8];

    if (line_index < line_count) {
      char num[8];
      utoa((unsigned long)(line_index + 1), num, 3);
      unsigned long p = 0;
      unsigned long ni = 0;
      while (num[ni] != '\0')
        buf[p++] = num[ni++];
      buf[p++] = '|';
      buf[p++] = ' ';
      unsigned long li = 0;
      while (lines[line_index][li] != '\0')
        buf[p++] = lines[line_index][li++];
      buf[p] = '\0';
      sys_draw_row(TEXT_TOP + row, buf, p);
    } else {
      sys_draw_row(TEXT_TOP + row, "~", 1);
    }
  }
}

/* Keeps `line` (0-indexed) inside the visible window, re-centering if
 * it just scrolled off either edge. */
static void ensure_visible(int line) {
  if (line < top_line) {
    top_line = line;
  } else if (line >= top_line + (int)text_rows_visible) {
    top_line = line - (int)text_rows_visible + 1;
    if (top_line < 0)
      top_line = 0;
  }
}

static void load_file(void) {
  long fd = sys_open(filename, O_PERSIST);
  if (fd < 0) {
    line_count = 0;
    return;
  }

  static char content[MAX_LINES * (MAX_LINE_LEN + 1)];
  unsigned long total = 0;
  long n;
  while (total + 512 <= sizeof(content) &&
         (n = sys_read((int)fd, content + total, 512)) > 0) {
    total += (unsigned long)n;
  }
  sys_close((int)fd);

  line_count = 0;
  unsigned long col = 0;
  for (unsigned long i = 0; i < total && line_count < MAX_LINES; i++) {
    if (content[i] == '\n') {
      lines[line_count][col] = '\0';
      line_count++;
      col = 0;
    } else if (col < MAX_LINE_LEN) {
      lines[line_count][col++] = content[i];
    }
  }
  if (col > 0 && line_count < MAX_LINES) {
    lines[line_count][col] = '\0';
    line_count++;
  }
}

static void save_file(void) {
  long fd = sys_open(filename, O_CREATE | O_PERSIST | O_TRUNC);
  if (fd < 0) {
    status("save failed: could not open file");
    return;
  }

  unsigned long total = 0;
  for (int i = 0; i < line_count; i++) {
    unsigned long len = str_len(lines[i]);
    sys_write((int)fd, lines[i], len);
    sys_write((int)fd, "\n", 1);
    total += len + 1;
  }
  sys_close((int)fd);

  modified = 0;

  char msg[64];
  const char *pre = "saved (";
  unsigned long p = 0;
  while (pre[p] != '\0') {
    msg[p] = pre[p];
    p++;
  }
  char num[20];
  utoa(total, num, 0);
  unsigned long ni = 0;
  while (num[ni] != '\0')
    msg[p++] = num[ni++];
  const char *post = " bytes)";
  unsigned long qi = 0;
  while (post[qi] != '\0')
    msg[p++] = post[qi++];
  msg[p] = '\0';
  status(msg);
}

static void cmd_append(void) {
  status("append mode: type lines, blank line to stop");
  redraw();
  while (1) {
    sys_set_cursor(cmd_row, 0);
    print("+ ");
    char buf[MAX_LINE_LEN + 1];
    long len = read_line(buf, sizeof(buf));
    if (len <= 0)
      break;
    if (line_count >= MAX_LINES) {
      status("file full, can't append more lines");
      break;
    }
    unsigned long i = 0;
    while (buf[i] != '\0') {
      lines[line_count][i] = buf[i];
      i++;
    }
    lines[line_count][i] = '\0';
    line_count++;
    modified = 1;
    ensure_visible(line_count - 1);
    redraw();
  }
  status("append finished");
}

static void cmd_insert(int at) {
  if (at < 0 || at > line_count || line_count >= MAX_LINES) {
    status("bad line number");
    return;
  }
  sys_set_cursor(cmd_row, 0);
  print("text> ");
  char buf[MAX_LINE_LEN + 1];
  read_line(buf, sizeof(buf));

  for (int i = line_count; i > at; i--) {
    unsigned long k = 0;
    while (lines[i - 1][k] != '\0') {
      lines[i][k] = lines[i - 1][k];
      k++;
    }
    lines[i][k] = '\0';
  }
  unsigned long i = 0;
  while (buf[i] != '\0') {
    lines[at][i] = buf[i];
    i++;
  }
  lines[at][i] = '\0';
  line_count++;
  modified = 1;
  ensure_visible(at);
  status("inserted");
}

static void cmd_replace(int at) {
  if (at < 0 || at >= line_count) {
    status("bad line number");
    return;
  }
  sys_set_cursor(cmd_row, 0);
  print("text> ");
  char buf[MAX_LINE_LEN + 1];
  read_line(buf, sizeof(buf));

  unsigned long i = 0;
  while (buf[i] != '\0') {
    lines[at][i] = buf[i];
    i++;
  }
  lines[at][i] = '\0';
  modified = 1;
  ensure_visible(at);
  status("replaced");
}

static void cmd_delete(int at) {
  if (at < 0 || at >= line_count) {
    status("bad line number");
    return;
  }
  for (int i = at; i < line_count - 1; i++) {
    unsigned long k = 0;
    while (lines[i + 1][k] != '\0') {
      lines[i][k] = lines[i + 1][k];
      k++;
    }
    lines[i][k] = '\0';
  }
  line_count--;
  modified = 1;
  status("deleted");
}

__attribute__((section(".text._start"))) void _start(unsigned long long argv,
                                                       unsigned long long argv_len) {
  const char *arg = (const char *)argv;
  unsigned long i = 0;
  while (i < argv_len && i < sizeof(filename) - 1 && arg[i] != '\0' &&
         arg[i] != ' ') {
    filename[i] = arg[i];
    i++;
  }
  filename[i] = '\0';

  if (filename[0] == '\0') {
    print("usage: edit <filename>\n");
    sys_exit(1);
  }

  unsigned long cols, rows;
  if (sys_screen_info(&cols, &rows) == 0) {
    screen_cols = cols;
    screen_rows = rows;
  }
  status_row = screen_rows - 2;
  cmd_row = screen_rows - 1;
  text_rows_visible = status_row - TEXT_TOP;
  (void)screen_cols;

  load_file();
  sys_clear();
  status("loaded");
  redraw();

  while (1) {
    sys_set_cursor(cmd_row, 0);
    print("cmd> ");
    char cmd[CMD_MAX];
    read_line(cmd, sizeof(cmd));

    if (str_eq(cmd, "q")) {
      if (modified) {
        status("unsaved changes - use 'wq' to save, or 'q!' to discard");
        continue;
      }
      break;
    } else if (str_eq(cmd, "q!")) {
      break;
    } else if (str_eq(cmd, "wq")) {
      save_file();
      break;
    } else if (str_eq(cmd, "w")) {
      save_file();
    } else if (str_eq(cmd, "a")) {
      cmd_append();
    } else if (str_starts_with(cmd, "i")) {
      const char *end;
      long n = parse_uint(cmd + 1, &end);
      if (n < 0)
        status("usage: i<line number>");
      else
        cmd_insert((int)n - 1);
    } else if (str_starts_with(cmd, "r")) {
      const char *end;
      long n = parse_uint(cmd + 1, &end);
      if (n < 0)
        status("usage: r<line number>");
      else
        cmd_replace((int)n - 1);
    } else if (str_starts_with(cmd, "d")) {
      const char *end;
      long n = parse_uint(cmd + 1, &end);
      if (n < 0)
        status("usage: d<line number>");
      else
        cmd_delete((int)n - 1);
    } else if (str_starts_with(cmd, "g")) {
      const char *end;
      long n = parse_uint(cmd + 1, &end);
      if (n < 0) {
        status("usage: g<line number>");
      } else {
        ensure_visible((int)n - 1);
      }
    } else if (cmd[0] == '\0') {
      /* just redraw */
    } else {
      status("unknown command (see hint row)");
    }

    redraw();
  }

  sys_clear();
  sys_set_cursor(0, 0);
  sys_exit(0);
}
