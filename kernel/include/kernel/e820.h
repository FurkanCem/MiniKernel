#ifndef KERNEL_E820_H
#define KERNEL_E820_H

typedef struct __attribute__((packed)) {
  unsigned long long base;
  unsigned long long length;
  unsigned int type;
  unsigned int extended_attributes;
} e820_entry_t;

#define E820_TYPE_USABLE 1
#define E820_TYPE_RESERVED 2
#define E820_TYPE_ACPI_RECLAIMABLE 3
#define E820_TYPE_ACPI_NVS 4
#define E820_TYPE_BAD 5

void e820_init(void);

unsigned int e820_entry_count(void);

const e820_entry_t *e820_get_entry(unsigned int index);

unsigned long long e820_total_usable_bytes(void);

const char *e820_type_name(unsigned int type);

#endif
