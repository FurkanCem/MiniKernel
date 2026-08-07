#include "kernel/vmm.h"
#include "kernel/e820.h"
#include "kernel/pmm.h"

#define PAGE_PRESENT 0x1ULL
#define PAGE_WRITABLE 0x2ULL
#define PAGE_SIZE_2MB 0x80ULL /* PS bit - only meaningful at the PD level */
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define ENTRIES_PER_TABLE 512
#define PAGE_2MB_SIZE (2ULL * 1024 * 1024)

static inline unsigned int pml4_index(unsigned long long addr) {
  return (unsigned int)((addr >> 39) & 0x1FF);
}
static inline unsigned int pdpt_index(unsigned long long addr) {
  return (unsigned int)((addr >> 30) & 0x1FF);
}
static inline unsigned int pd_index(unsigned long long addr) {
  return (unsigned int)((addr >> 21) & 0x1FF);
}

static unsigned long long *pml4;

static unsigned long long *get_or_create_table(unsigned long long *table,
                                               unsigned int index) {
  if (!(table[index] & PAGE_PRESENT)) {
    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return 0; /* out of memory - caller must handle */

    unsigned long long *new_table = (unsigned long long *)frame;
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
      new_table[i] = 0;
    }

    table[index] = frame | PAGE_PRESENT | PAGE_WRITABLE;
  }

  return (unsigned long long *)(table[index] & PAGE_ADDR_MASK);
}

static void map_2mb_identity(unsigned long long phys_addr) {
  unsigned long long *pdpt = get_or_create_table(pml4, pml4_index(phys_addr));
  if (pdpt == 0)
    return;

  unsigned long long *pd = get_or_create_table(pdpt, pdpt_index(phys_addr));
  if (pd == 0)
    return;

  unsigned int pdi = pd_index(phys_addr);
  if (!(pd[pdi] & PAGE_PRESENT)) {
    pd[pdi] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2MB;
  }
}

void vmm_init(void) {
  unsigned long long cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  pml4 = (unsigned long long *)(cr3 & PAGE_ADDR_MASK);

  unsigned int count = e820_entry_count();
  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0 || entry->type != E820_TYPE_USABLE)
      continue;

    unsigned long long start = entry->base & ~(PAGE_2MB_SIZE - 1);
    unsigned long long end = entry->base + entry->length;

    for (unsigned long long addr = start; addr < end; addr += PAGE_2MB_SIZE) {
      map_2mb_identity(addr);
    }
  }

  __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}
