#include "syscall.h"

/* A tiny interpreted language for MiniKernel, so scripts written with
 * `edit` can actually be run from the shell.
 *
 * This is an INTERPRETER, not a compiler: it re-parses and executes
 * each line directly rather than translating the program to machine
 * code first. A real compiler (parsing to an AST/bytecode, then
 * emitting native x86-64 and handing it to process_spawn_from_file)
 * is a substantially bigger project - this is the "write it, run it"
 * step working end to end first.
 *
 * Language (one statement per line):
 *   let NAME = EXPR
 *   print EXPR            (EXPR, or a "quoted string")
 *   input NAME             reads an integer from the keyboard into NAME
 *   if EXPR OP EXPR goto N     OP is one of == != < > <= >=
 *   goto N
 *   end
 *   # comment / blank lines are ignored
 *
 * EXPR is a standard +,-,*,/ integer expression over numbers,
 * variables, and parens, e.g. "(x + 1) * 2". N in goto/if is a 1-based
 * line number into the script.
 */

#define MAX_LINES 200
#define MAX_LINE_LEN 120
#define MAX_VARS 32
#define VAR_NAME_LEN 16
#define STEP_LIMIT 2000000L

static char lines[MAX_LINES][MAX_LINE_LEN + 1];
static int line_count = 0;

typedef struct {
  char name[VAR_NAME_LEN];
  long long value;
} var_t;

static var_t vars[MAX_VARS];
static int var_count = 0;

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

static void print_int(long long value) {
  char digits[24];
  int n = 0;
  int neg = value < 0;
  unsigned long long v = neg ? (unsigned long long)(-value) : (unsigned long long)value;

  if (v == 0)
    digits[n++] = '0';
  while (v > 0) {
    digits[n++] = (char)('0' + (v % 10));
    v /= 10;
  }
  if (neg)
    sys_write(STDOUT, "-", 1);
  while (n > 0) {
    n--;
    sys_write(STDOUT, &digits[n], 1);
  }
}

static long long *var_ref(const char *name, int create) {
  for (int i = 0; i < var_count; i++) {
    if (str_eq(vars[i].name, name))
      return &vars[i].value;
  }
  if (!create || var_count >= MAX_VARS)
    return 0;

  unsigned long i = 0;
  while (name[i] != '\0' && i < VAR_NAME_LEN - 1) {
    vars[var_count].name[i] = name[i];
    i++;
  }
  vars[var_count].name[i] = '\0';
  vars[var_count].value = 0;
  return &vars[var_count++].value;
}

static void skip_ws(const char **s) {
  while (**s == ' ' || **s == '\t')
    (*s)++;
}

static int is_ident_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

/* Reads an identifier into `out` (up to VAR_NAME_LEN-1 chars) and
 * advances *s past it. Returns 0 if there wasn't one. */
static int read_ident(const char **s, char *out) {
  unsigned long n = 0;
  while (is_ident_char(**s)) {
    if (n < VAR_NAME_LEN - 1)
      out[n++] = **s;
    (*s)++;
  }
  out[n] = '\0';
  return n > 0;
}

static long long parse_expr(const char **s);

static long long parse_factor(const char **s) {
  skip_ws(s);

  if (**s == '(') {
    (*s)++;
    long long v = parse_expr(s);
    skip_ws(s);
    if (**s == ')')
      (*s)++;
    return v;
  }

  if (**s == '-') {
    (*s)++;
    return -parse_factor(s);
  }

  if (**s >= '0' && **s <= '9') {
    long long v = 0;
    while (**s >= '0' && **s <= '9') {
      v = v * 10 + (**s - '0');
      (*s)++;
    }
    return v;
  }

  char name[VAR_NAME_LEN];
  if (read_ident(s, name)) {
    long long *ref = var_ref(name, 0);
    return ref ? *ref : 0;
  }

  return 0;
}

static long long parse_term(const char **s) {
  long long v = parse_factor(s);
  skip_ws(s);
  while (**s == '*' || **s == '/') {
    char op = **s;
    (*s)++;
    long long rhs = parse_factor(s);
    if (op == '*') {
      v *= rhs;
    } else {
      v = (rhs != 0) ? v / rhs : 0;
    }
    skip_ws(s);
  }
  return v;
}

static long long parse_expr(const char **s) {
  long long v = parse_term(s);
  skip_ws(s);
  while (**s == '+' || **s == '-') {
    char op = **s;
    (*s)++;
    long long rhs = parse_term(s);
    v = (op == '+') ? v + rhs : v - rhs;
    skip_ws(s);
  }
  return v;
}

/* Reads a two-char comparison operator (==, !=, <=, >=) or a single-char
 * one (<, >) at *s, advances past it, and returns it packed into an int
 * (first char, or first char + second<<8). Returns 0 if none found. */
static int read_cmp_op(const char **s) {
  skip_ws(s);
  char a = **s;
  char b = *(*s + 1);

  if ((a == '=' && b == '=') || (a == '!' && b == '=') ||
      (a == '<' && b == '=') || (a == '>' && b == '=')) {
    *s += 2;
    return (int)a | ((int)b << 8);
  }
  if (a == '<' || a == '>') {
    (*s)++;
    return (int)a;
  }
  return 0;
}

static int eval_cmp(long long lhs, int op, long long rhs) {
  switch (op) {
  case '=' | ('=' << 8):
    return lhs == rhs;
  case '!' | ('=' << 8):
    return lhs != rhs;
  case '<' | ('=' << 8):
    return lhs <= rhs;
  case '>' | ('=' << 8):
    return lhs >= rhs;
  case '<':
    return lhs < rhs;
  case '>':
    return lhs > rhs;
  default:
    return 0;
  }
}

static long read_line_stdin(char *buf, unsigned long max_len) {
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

static long long parse_line_number(const char *s) {
  long long v = 0;
  int any = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    s++;
    any = 1;
  }
  return any ? v : -1;
}

/* Executes one line. Sets jumped/target if it branched, and clears
 * running if it was an `end`. */
static void exec_line(const char *line, int line_no, int *jumped,
                       long long *target, int *running) {
  const char *s = line;
  skip_ws(&s);

  if (*s == '\0' || *s == '#')
    return;

  if (str_starts_with(s, "let ")) {
    s += 4;
    skip_ws(&s);
    char name[VAR_NAME_LEN];
    if (!read_ident(&s, name)) {
      print("syntax error (bad name) on line ");
      print_int(line_no);
      print("\n");
      return;
    }
    skip_ws(&s);
    if (*s != '=') {
      print("syntax error (expected '=') on line ");
      print_int(line_no);
      print("\n");
      return;
    }
    s++;
    long long value = parse_expr(&s);
    *var_ref(name, 1) = value;
    return;
  }

  if (str_starts_with(s, "print ")) {
    s += 6;
    skip_ws(&s);
    if (*s == '"') {
      s++;
      while (*s != '\0' && *s != '"') {
        sys_write(STDOUT, s, 1);
        s++;
      }
      print("\n");
    } else {
      long long v = parse_expr(&s);
      print_int(v);
      print("\n");
    }
    return;
  }

  if (str_starts_with(s, "input ")) {
    s += 6;
    skip_ws(&s);
    char name[VAR_NAME_LEN];
    if (!read_ident(&s, name)) {
      print("syntax error (bad name) on line ");
      print_int(line_no);
      print("\n");
      return;
    }
    print("? ");
    char buf[32];
    read_line_stdin(buf, sizeof(buf));
    const char *p = buf;
    skip_ws(&p);
    long long value = parse_expr(&p);
    *var_ref(name, 1) = value;
    return;
  }

  if (str_starts_with(s, "if ")) {
    s += 3;
    long long lhs = parse_expr(&s);
    int op = read_cmp_op(&s);
    long long rhs = parse_expr(&s);
    skip_ws(&s);
    if (!str_starts_with(s, "goto ")) {
      print("syntax error (expected 'goto') on line ");
      print_int(line_no);
      print("\n");
      return;
    }
    s += 5;
    skip_ws(&s);
    long long tgt = parse_line_number(s);
    if (eval_cmp(lhs, op, rhs) && tgt >= 1) {
      *jumped = 1;
      *target = tgt;
    }
    return;
  }

  if (str_starts_with(s, "goto ")) {
    s += 5;
    skip_ws(&s);
    long long tgt = parse_line_number(s);
    if (tgt >= 1) {
      *jumped = 1;
      *target = tgt;
    }
    return;
  }

  if (str_eq(s, "end")) {
    *running = 0;
    return;
  }

  print("syntax error (unknown statement) on line ");
  print_int(line_no);
  print("\n");
}

static void load_script(const char *filename) {
  long fd = sys_open(filename, O_PERSIST);
  if (fd < 0) {
    print("lang: no such file: ");
    print(filename);
    print("\n");
    sys_exit(1);
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

static void run_script(void) {
  int pc = 0;
  int running = 1;
  long steps = 0;

  while (running && pc >= 0 && pc < line_count) {
    steps++;
    if (steps > STEP_LIMIT) {
      print("lang: step limit exceeded (possible infinite loop)\n");
      break;
    }

    int jumped = 0;
    long long target = -1;
    exec_line(lines[pc], pc + 1, &jumped, &target, &running);

    if (jumped) {
      pc = (int)target - 1;
    } else {
      pc++;
    }
  }
}

__attribute__((section(".text._start"))) void _start(unsigned long long argv,
                                                       unsigned long long argv_len) {
  char filename[64];
  const char *arg = (const char *)argv;
  unsigned long i = 0;
  while (i < argv_len && i < sizeof(filename) - 1 && arg[i] != '\0' &&
         arg[i] != ' ') {
    filename[i] = arg[i];
    i++;
  }
  filename[i] = '\0';

  if (filename[0] == '\0') {
    print("usage: lang <script filename>\n");
    sys_exit(1);
  }

  load_script(filename);
  run_script();

  sys_exit(0);
}
