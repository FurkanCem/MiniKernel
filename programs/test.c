#include "syscall.h"

typedef unsigned long long u64;
typedef unsigned int u32;

/*
 * ------------------------------------------------------------
 * Basic userspace utilities
 * ------------------------------------------------------------
 */

static u64 strlen(const char *str) {
  u64 len = 0;

  while (str[len] != '\0')
    len++;

  return len;
}

static void print(const char *str) { sys_write(STDOUT, str, strlen(str)); }

static void print_char(char c) { sys_write(STDOUT, &c, 1); }

static void print_u64(u64 value) {
  char buffer[32];
  int i = 0;

  if (value == 0) {
    print_char('0');
    return;
  }

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i > 0)
    print_char(buffer[--i]);
}

static void print_hex(u64 value) {
  static const char hex[] = "0123456789abcdef";
  char buffer[18];
  int i;

  buffer[0] = '0';
  buffer[1] = 'x';

  for (i = 0; i < 16; i++) {
    int shift = (15 - i) * 4;
    buffer[2 + i] = hex[(value >> shift) & 0xf];
  }

  sys_write(STDOUT, buffer, sizeof(buffer));
}

/*
 * ------------------------------------------------------------
 * Fibonacci
 * ------------------------------------------------------------
 */

static u64 fibonacci(u64 n) {
  if (n == 0)
    return 0;

  if (n == 1)
    return 1;

  u64 a = 0;
  u64 b = 1;

  for (u64 i = 2; i <= n; i++) {
    u64 next = a + b;
    a = b;
    b = next;
  }

  return b;
}

/*
 * ------------------------------------------------------------
 * Prime number test
 * ------------------------------------------------------------
 */

static int is_prime(u64 n) {
  if (n < 2)
    return 0;

  if (n == 2)
    return 1;

  if ((n & 1) == 0)
    return 0;

  for (u64 i = 3; i * i <= n; i += 2) {
    if (n % i == 0)
      return 0;
  }

  return 1;
}

/*
 * ------------------------------------------------------------
 * Memory test
 * ------------------------------------------------------------
 */

static u64 memory_test(void) {
  /*
   * Deliberately larger than 4 KiB.
   *
   * This tests whether the userspace stack and memory
   * mappings work correctly.
   */
  volatile unsigned char buffer[8192];

  u64 checksum = 0;

  for (u64 i = 0; i < sizeof(buffer); i++) {
    buffer[i] = (unsigned char)(i ^ 0x5a);
  }

  for (u64 i = 0; i < sizeof(buffer); i++) {
    checksum += buffer[i];
  }

  return checksum;
}

/*
 * ------------------------------------------------------------
 * Arithmetic test
 * ------------------------------------------------------------
 */

static u64 arithmetic_test(void) {
  u64 result = 0;

  for (u64 i = 1; i <= 10000; i++) {
    result += i;
  }

  return result;
}

/*
 * ------------------------------------------------------------
 * Main userspace entry
 * ------------------------------------------------------------
 */

__attribute__((section(".text._start"))) void _start(void) {
  /*
   * --------------------------------------------------------
   * Basic syscall test
   * --------------------------------------------------------
   */

  print("\n");
  print("========================================\n");
  print("      MiniKernel userspace test\n");
  print("========================================\n");
  print("\n");

  print("[+] SYS_WRITE works\n");

  /*
   * --------------------------------------------------------
   * Integer conversion test
   * --------------------------------------------------------
   */

  print("[+] Integer test: ");

  print_u64(1234567890);

  print("\n");

  /*
   * --------------------------------------------------------
   * Hexadecimal output
   * --------------------------------------------------------
   */

  print("[+] Hex test: ");

  print_hex(0xDEADBEEFCAFEBABEULL);

  print("\n");

  /*
   * --------------------------------------------------------
   * Fibonacci test
   * --------------------------------------------------------
   */

  print("\n");
  print("[+] Fibonacci test\n");

  for (u64 i = 0; i <= 20; i++) {
    print("    fib(");
    print_u64(i);
    print(") = ");
    print_u64(fibonacci(i));
    print("\n");
  }

  /*
   * --------------------------------------------------------
   * Prime number test
   * --------------------------------------------------------
   */

  print("\n");
  print("[+] Prime numbers below 100:\n");

  for (u64 i = 2; i < 100; i++) {
    if (is_prime(i)) {
      print("    ");
      print_u64(i);
      print("\n");
    }
  }

  /*
   * --------------------------------------------------------
   * Arithmetic test
   * --------------------------------------------------------
   */

  print("\n");
  print("[+] Arithmetic test\n");

  u64 sum = arithmetic_test();

  print("    1 + 2 + ... + 10000 = ");
  print_u64(sum);
  print("\n");

  /*
   * --------------------------------------------------------
   * Userspace memory test
   * --------------------------------------------------------
   */

  print("\n");
  print("[+] Userspace memory test\n");

  u64 checksum = memory_test();

  print("    8192-byte buffer checksum = ");
  print_u64(checksum);
  print("\n");

  /*
   * --------------------------------------------------------
   * Final status
   * --------------------------------------------------------
   */

  print("\n");
  print("[+] All userspace tests completed\n");
  print("[+] Exiting through SYS_EXIT\n");
  print("\n");

  sys_exit(0);

  __builtin_unreachable();
}
