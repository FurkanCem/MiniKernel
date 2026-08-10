#include "kernel/elf.h"
#include "kernel/pmm.h"

const unsigned char tiny_elf_binary[] = {
127,69,76,70,2,1,1,0,0,0,0,0,0,0,0,0,2,0,62,0,1,0,0,0,120,0,0,0,128,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64,0,56,0,1,0,0,0,0,0,0,0,1,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,128,0,0,0,220,0,0,0,0,0,0,0,220,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,72,184,3,0,0,0,0,0,0,0,72,191,1,0,0,0,0,0,0,0,72,190,176,0,0,0,128,0,0,0,72,186,44,0,0,0,0,0,0,0,205,128,72,184,2,0,0,0,0,0,0,0,205,128,235,254,104,101,108,108,111,32,102,114,111,109,32,97,32,114,101,97,108,32,101,108,102,32,115,121,115,99,97,108,108,32,118,105,97,32,115,121,115,95,119,114,105,116,101,10
};
const unsigned long long tiny_elf_binary_size = sizeof(tiny_elf_binary);

typedef struct __attribute__((packed)) {
  unsigned char e_ident[16];
  unsigned short e_type;
  unsigned short e_machine;
  unsigned int e_version;
  unsigned long long e_entry;
  unsigned long long e_phoff;
  unsigned long long e_shoff;
  unsigned int e_flags;
  unsigned short e_ehsize;
  unsigned short e_phentsize;
  unsigned short e_phnum;
  unsigned short e_shentsize;
  unsigned short e_shnum;
  unsigned short e_shstrndx;
} elf64_ehdr_t;

typedef struct __attribute__((packed)) {
  unsigned int p_type;
  unsigned int p_flags;
  unsigned long long p_offset;
  unsigned long long p_vaddr;
  unsigned long long p_paddr;
  unsigned long long p_filesz;
  unsigned long long p_memsz;
  unsigned long long p_align;
} elf64_phdr_t;

#define PT_LOAD 1
#define PAGE_SIZE_4K 4096ULL

static int load_segment(vmm_address_space_t space, const unsigned char *data,
                         unsigned long long size, const elf64_phdr_t *ph) {
  if (ph->p_offset + ph->p_filesz > size)
    return -1;

  unsigned long long vaddr_start = ph->p_vaddr & ~(PAGE_SIZE_4K - 1);
  unsigned long long vaddr_end =
      (ph->p_vaddr + ph->p_memsz + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

  for (unsigned long long va = vaddr_start; va < vaddr_end; va += PAGE_SIZE_4K) {
    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return -1;

    unsigned char *kview = (unsigned char *)frame;
    for (unsigned int i = 0; i < PAGE_SIZE_4K; i++) {
      kview[i] = 0;
    }

    if (vmm_map_page_in(space, va, frame, VMM_WRITABLE | VMM_USER) != 0)
      return -1;

    unsigned long long page_vstart = va;
    unsigned long long page_vend = va + PAGE_SIZE_4K;
    unsigned long long seg_data_start = ph->p_vaddr;
    unsigned long long seg_data_end = ph->p_vaddr + ph->p_filesz;

    unsigned long long copy_start =
        page_vstart > seg_data_start ? page_vstart : seg_data_start;
    unsigned long long copy_end =
        page_vend < seg_data_end ? page_vend : seg_data_end;

    if (copy_end > copy_start) {
      unsigned long long file_off = ph->p_offset + (copy_start - ph->p_vaddr);
      unsigned long long page_off = copy_start - page_vstart;
      unsigned long long n = copy_end - copy_start;
      for (unsigned long long k = 0; k < n; k++) {
        kview[page_off + k] = data[file_off + k];
      }
    }
  }

  return 0;
}

int elf_load(vmm_address_space_t space, const unsigned char *data,
             unsigned long long size, unsigned long long *out_entry) {
  if (size < sizeof(elf64_ehdr_t))
    return -1;

  const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;

  if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
      eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
    return -1;
  if (eh->e_ident[4] != 2)
    return -1;
  if (eh->e_machine != 0x3E)
    return -1;

  if (eh->e_phoff + (unsigned long long)eh->e_phnum * eh->e_phentsize > size)
    return -1;

  for (unsigned short i = 0; i < eh->e_phnum; i++) {
    const elf64_phdr_t *ph = (const elf64_phdr_t *)(data + eh->e_phoff +
                                                     i * eh->e_phentsize);
    if (ph->p_type != PT_LOAD)
      continue;

    if (load_segment(space, data, size, ph) != 0)
      return -1;
  }

  *out_entry = eh->e_entry;
  return 0;
}
