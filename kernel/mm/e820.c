#include "kernel/e820.h"

#define E820_BUFFER_ADDR 0x5000

typedef struct __attribute__((packed)) {
  unsigned int count;
  e820_entry_t entries[];
} e820_buffer_t;

static const e820_buffer_t *e820_buffer =
    (const e820_buffer_t *)E820_BUFFER_ADDR;

void e820_init(void) {
  /* Nothing to do - see the header comment. This exists so the
     boot sequence in core/main.c has an explicit, documented step
     for "the memory map is now available", rather than every
     caller silently assuming a magic address is already valid. */
}

unsigned int e820_entry_count(void) { return e820_buffer->count; }

const e820_entry_t *e820_get_entry(unsigned int index) {
  if (index >= e820_buffer->count)
    return 0;
  return &e820_buffer->entries[index];
}

unsigned long long e820_total_usable_bytes(void) {
  unsigned long long total = 0;

  for (unsigned int i = 0; i < e820_buffer->count; i++) {
    if (e820_buffer->entries[i].type == E820_TYPE_USABLE)
      total += e820_buffer->entries[i].length;
  }

  return total;
}

const char *e820_type_name(unsigned int type) {
  switch (type) {
  case E820_TYPE_USABLE:
    return "usable";
  case E820_TYPE_RESERVED:
    return "reserved";
  case E820_TYPE_ACPI_RECLAIMABLE:
    return "ACPI reclaimable";
  case E820_TYPE_ACPI_NVS:
    return "ACPI NVS";
  case E820_TYPE_BAD:
    return "bad";
  default:
    return "unknown";
  }
}
