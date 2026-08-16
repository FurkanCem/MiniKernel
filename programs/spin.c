#include "syscall.h"

/* Prints a heartbeat forever. Used to exercise `bg`/`kill`: start this
 * in the background, note its pid, then kill it and confirm the
 * heartbeats actually stop. */
__attribute__((section(".text._start"))) void _start(void) {
  const char msg[] = "spin: heartbeat\n";

  while (1) {
    sys_write(STDOUT, msg, sizeof(msg) - 1);
    for (volatile unsigned long i = 0; i < 30000000UL; i++) {
    }
  }
}
