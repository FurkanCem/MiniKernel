#include "kernel/gdt.h"

typedef struct __attribute__((packed)) {
  unsigned short limit_low;
  unsigned short base_low;
  unsigned char base_mid;
  unsigned char access;
  unsigned char
      granularity; /* high nibble = flags, low nibble = limit[19:16] */
  unsigned char base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
  unsigned short limit_low;
  unsigned short base_low;
  unsigned char base_mid;
  unsigned char access;
  unsigned char granularity;
  unsigned char base_high;
  unsigned int base_upper;
  unsigned int reserved;
} tss_descriptor_t;

typedef struct __attribute__((packed)) {
  unsigned int reserved0;
  unsigned long long rsp0;
  unsigned long long rsp1;
  unsigned long long rsp2;
  unsigned long long reserved1;
  unsigned long long ist1;
  unsigned long long ist2;
  unsigned long long ist3;
  unsigned long long ist4;
  unsigned long long ist5;
  unsigned long long ist6;
  unsigned long long ist7;
  unsigned long long reserved2;
  unsigned short reserved3;
  unsigned short iopb_offset;
} tss_t;

typedef struct __attribute__((packed)) {
  unsigned short limit;
  unsigned long long base;
} gdt_ptr_t;

static struct __attribute__((packed)) {
  gdt_entry_t null_entry;
  gdt_entry_t kernel_code;
  gdt_entry_t kernel_data;
  gdt_entry_t user_code;
  gdt_entry_t user_data;
  tss_descriptor_t tss;
} gdt;

static gdt_ptr_t gdt_ptr;
static tss_t tss;

static void set_entry(gdt_entry_t *e, unsigned char access,
                      unsigned char granularity) {
  e->limit_low = 0;
  e->base_low = 0;
  e->base_mid = 0;
  e->access = access;
  e->granularity = granularity;
  e->base_high = 0;
}

static void set_tss_descriptor(tss_descriptor_t *d, unsigned long long base,
                               unsigned int limit) {
  d->limit_low = (unsigned short)(limit & 0xFFFF);
  d->base_low = (unsigned short)(base & 0xFFFF);
  d->base_mid = (unsigned char)((base >> 16) & 0xFF);
  d->access = 0x89; /* present, DPL0, type=1001 (64-bit TSS, available) */
  d->granularity = (unsigned char)((limit >> 16) & 0x0F);
  d->base_high = (unsigned char)((base >> 24) & 0xFF);
  d->base_upper = (unsigned int)((base >> 32) & 0xFFFFFFFF);
  d->reserved = 0;
}

void tss_set_kernel_stack(unsigned long long rsp0) { tss.rsp0 = rsp0; }

void gdt_init(void) {
  set_entry(&gdt.null_entry, 0x00, 0x00);

  set_entry(&gdt.kernel_code, 0x9A, 0xAF);
  set_entry(&gdt.kernel_data, 0x92, 0xA0);

  set_entry(&gdt.user_code, 0xFA, 0xAF);
  set_entry(&gdt.user_data, 0xF2, 0xA0);

  for (unsigned int i = 0; i < sizeof(tss); i++) {
    ((unsigned char *)&tss)[i] = 0;
  }
  tss.iopb_offset = sizeof(tss); /* no I/O permission bitmap */
  set_tss_descriptor(&gdt.tss, (unsigned long long)&tss, sizeof(tss) - 1);

  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (unsigned long long)&gdt;

  __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));

  __asm__ volatile("push %0\n\t"
                   "lea 1f(%%rip), %%rax\n\t"
                   "push %%rax\n\t"
                   "lretq\n\t"
                   "1:\n\t"
                   :
                   : "i"(GDT_KERNEL_CODE)
                   : "rax", "memory");

  __asm__ volatile("mov %0, %%ax\n\t"
                   "mov %%ax, %%ds\n\t"
                   "mov %%ax, %%es\n\t"
                   "mov %%ax, %%fs\n\t"
                   "mov %%ax, %%gs\n\t"
                   "mov %%ax, %%ss\n\t"
                   :
                   : "i"(GDT_KERNEL_DATA)
                   : "ax");

  __asm__ volatile("ltr %0" : : "r"((unsigned short)GDT_TSS));
}
