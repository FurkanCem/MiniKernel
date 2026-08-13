#include "kernel/elf.h"
#include "kernel/pmm.h"

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
#define ELF_MACHINE_X86_64 0x3E
#define ELF_CLASS_64 2
#define ELF_TYPE_EXEC 2
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

static int load_segment(vmm_address_space_t space, const unsigned char *data,
                         unsigned long long size, const elf64_phdr_t *ph,
                         unsigned long long load_bias) {
  if (ph->p_filesz > ph->p_memsz || ph->p_offset > size ||
      ph->p_filesz > size - ph->p_offset)
    return -1;

  if (ph->p_vaddr + load_bias < ph->p_vaddr)
    return -1;

  unsigned long long segment_vaddr = ph->p_vaddr + load_bias;

  if (ph->p_memsz == 0)
    return 0;

  unsigned long long vaddr_start = segment_vaddr & ~(PAGE_SIZE_4K - 1);
  unsigned long long vaddr_end =
      (segment_vaddr + ph->p_memsz + PAGE_SIZE_4K - 1) &
      ~(PAGE_SIZE_4K - 1);

  if (vaddr_end < segment_vaddr)
    return -1;

  for (unsigned long long va = vaddr_start; va < vaddr_end; va += PAGE_SIZE_4K) {
    unsigned long long frame = pmm_alloc_frame();
    if (frame == 0)
      return -1;

    unsigned char *kview = (unsigned char *)frame;
    for (unsigned int i = 0; i < PAGE_SIZE_4K; i++) {
      kview[i] = 0;
    }

    unsigned long long flags = VMM_USER;
    if (ph->p_flags & PF_W)
      flags |= VMM_WRITABLE;
    if (vmm_map_page_in(space, va, frame, flags) != 0)
      return -1;

    unsigned long long page_vstart = va;
    unsigned long long page_vend = va + PAGE_SIZE_4K;
    unsigned long long seg_data_start = segment_vaddr;
    unsigned long long seg_data_end = segment_vaddr + ph->p_filesz;

    unsigned long long copy_start =
        page_vstart > seg_data_start ? page_vstart : seg_data_start;
    unsigned long long copy_end =
        page_vend < seg_data_end ? page_vend : seg_data_end;

    if (copy_end > copy_start) {
      unsigned long long file_off = ph->p_offset + (copy_start - segment_vaddr);
      unsigned long long page_off = copy_start - page_vstart;
      unsigned long long n = copy_end - copy_start;
      for (unsigned long long k = 0; k < n; k++) {
        kview[page_off + k] = data[file_off + k];
      }
    }
  }

  return 0;
}

static int entry_is_executable(const elf64_ehdr_t *eh,
                               const unsigned char *data) {
  for (unsigned short i = 0; i < eh->e_phnum; i++) {
    const elf64_phdr_t *ph = (const elf64_phdr_t *)(data + eh->e_phoff +
                                                     i * eh->e_phentsize);
    if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_X) || ph->p_memsz == 0)
      continue;
    if (ph->p_vaddr + ph->p_memsz < ph->p_vaddr)
      return 0;
    if (eh->e_entry >= ph->p_vaddr &&
        eh->e_entry < ph->p_vaddr + ph->p_memsz)
      return 1;
  }
  return 0;
}

int elf_load(vmm_address_space_t space, const unsigned char *data,
             unsigned long long size, unsigned long long load_bias,
             unsigned long long *out_entry) {
  if (size < sizeof(elf64_ehdr_t))
    return -1;

  const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;

  if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
      eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
    return -1;
  if (eh->e_ident[4] != ELF_CLASS_64)
    return -1;
  if (eh->e_machine != ELF_MACHINE_X86_64)
    return -1;
  if (eh->e_type != ELF_TYPE_EXEC)
    return -1;
  if (eh->e_phentsize != sizeof(elf64_phdr_t))
    return -1;

  if (eh->e_phoff > size ||
      (unsigned long long)eh->e_phnum >
          (size - eh->e_phoff) / eh->e_phentsize)
    return -1;

  if (!entry_is_executable(eh, data))
    return -1;

  for (unsigned short i = 0; i < eh->e_phnum; i++) {
    const elf64_phdr_t *ph = (const elf64_phdr_t *)(data + eh->e_phoff +
                                                     i * eh->e_phentsize);
    if (ph->p_type != PT_LOAD)
      continue;

    if (load_segment(space, data, size, ph, load_bias) != 0)
      return -1;
  }

  if (eh->e_entry + load_bias < eh->e_entry)
    return -1;

  *out_entry = eh->e_entry + load_bias;
  return 0;
}
