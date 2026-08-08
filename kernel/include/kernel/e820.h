#ifndef KERNEL_E820_H
#define KERNEL_E820_H

typedef struct __attribute__((packed)) {
    unsigned long long base;
    unsigned long long length;
    unsigned int type;
    unsigned int extended_attributes;
} e820_entry_t;

#define E820_TYPE_USABLE           1
#define E820_TYPE_RESERVED         2
#define E820_TYPE_ACPI_RECLAIMABLE 3
#define E820_TYPE_ACPI_NVS         4
#define E820_TYPE_BAD              5

/* Explicit boot step, even though the real work already happened in
   the bootloader (bootloader/real/e820.asm) - BIOS calls aren't
   available once we're past real mode, so by the time the kernel
   runs, the memory map is already sitting at a fixed physical
   address, just waiting to be read. */
void e820_init(void);

unsigned int e820_entry_count(void);

/* Returns 0 if index is out of range. */
const e820_entry_t *e820_get_entry(unsigned int index);

/* Sum of the length of every USABLE region, in bytes. */
unsigned long long e820_total_usable_bytes(void);

const char *e820_type_name(unsigned int type);

#endif
