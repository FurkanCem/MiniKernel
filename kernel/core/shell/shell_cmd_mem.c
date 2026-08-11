#include "shell_internal.h"
#include "kernel/e820.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "kernel/video.h"

void cmd_meminfo(void) {
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

  print_str("\n\nframe allocator: ");
  print_hex(pmm_total_frames());
  print_str(" total frames, ");
  print_hex(pmm_free_frames());
  print_str(" free (");
  print_hex(pmm_total_frames() - pmm_free_frames());
  print_str(" used)");
}

void cmd_alloctest(void) {
  print_str("\nallocating 3 frames:");

  for (int i = 0; i < 3; i++) {
    unsigned long long addr = pmm_alloc_frame();
    print_str("\n  frame ");
    print_hex((unsigned long long)i);
    print_str(": ");
    if (addr == 0) {
      print_str("out of memory");
    } else {
      print_hex(addr);
    }
  }

  print_str("\nfree frames remaining: ");
  print_hex(pmm_free_frames());
}

void cmd_vmmtest(void) {
  print_str("\nallocating frames until one lands past the 2MB mark...");

  unsigned long long addr = 0;
  unsigned long long attempts = 0;
  const unsigned long long TWO_MB = 0x200000ULL;
  const unsigned long long MAX_ATTEMPTS = 100000ULL;

  while (attempts < MAX_ATTEMPTS) {
    addr = pmm_alloc_frame();
    attempts++;
    if (addr == 0) {
      print_str("\nout of memory before reaching 2MB");
      return;
    }
    if (addr >= TWO_MB)
      break;
  }

  print_str("\ngot frame ");
  print_hex(addr);
  print_str(" after ");
  print_hex(attempts);
  print_str(" allocations");

  volatile unsigned char *ptr = (volatile unsigned char *)addr;
  unsigned char pattern = 0xA5;
  *ptr = pattern;
  unsigned char readback = *ptr;

  print_str("\nwrote 0xA5 to it, read back ");
  print_hex((unsigned long long)readback);
  if (readback == pattern) {
    print_str(" -> OK, paging extension works");
  } else {
    print_str(" -> MISMATCH");
  }
}

void cmd_heaptest(void) {
  print_str("\nallocating three blocks and writing through them:");

  char *a = (char *)kmalloc(32);
  char *b = (char *)kmalloc(64);
  char *c = (char *)kmalloc(16);

  if (a == 0 || b == 0 || c == 0) {
    print_str("\nkmalloc returned NULL - out of memory");
    return;
  }

  for (int i = 0; i < 31; i++)
    a[i] = 'A';
  a[31] = '\0';
  for (int i = 0; i < 63; i++)
    b[i] = 'B';
  b[63] = '\0';
  for (int i = 0; i < 15; i++)
    c[i] = 'C';
  c[15] = '\0';

  print_str("\n  a=");
  print_hex((unsigned long long)a);
  print_str("\n  b=");
  print_hex((unsigned long long)b);
  print_str("\n  c=");
  print_hex((unsigned long long)c);

  print_str("\nfreeing b, then allocating a similar-sized block:");
  kfree(b);
  char *d = (char *)kmalloc(40);
  print_str("\n  d=");
  print_hex((unsigned long long)d);
  if (d == b) {
    print_str(" (same address as freed b - free list reused it)");
  } else {
    print_str(" (different address - grew instead of reusing b this time)");
  }

  print_str("\ndata still intact after all that: a[0]=");
  putchar_at_cursor(a[0]);
  print_str(" c[0]=");
  putchar_at_cursor(c[0]);
}
