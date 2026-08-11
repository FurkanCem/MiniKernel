#include "kernel/vmm_stack.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"

#define PAGE_4KB_SIZE 4096ULL

typedef struct stack_region {
  vmm_address_space_t space;
  unsigned long long stack_top;
  unsigned long long stack_limit;
  struct stack_region *next;
} stack_region_t;

static stack_region_t *stack_regions = 0;

void vmm_register_growable_stack(vmm_address_space_t space,
                                  unsigned long long stack_top,
                                  unsigned long long max_size) {
  stack_region_t *region = (stack_region_t *)kmalloc(sizeof(stack_region_t));
  if (region == 0)
    return;

  region->space = space;
  region->stack_top = stack_top;
  region->stack_limit = stack_top - max_size;
  region->next = stack_regions;
  stack_regions = region;
}

void vmm_unregister_growable_stack(vmm_address_space_t space) {
  stack_region_t **link = &stack_regions;

  while (*link != 0) {
    if ((*link)->space == space) {
      stack_region_t *dead = *link;
      *link = dead->next;
      kfree(dead);
      return;
    }

    link = &(*link)->next;
  }
}

static stack_region_t *find_region(vmm_address_space_t space,
                                    unsigned long long page) {
  for (stack_region_t *region = stack_regions; region != 0;
       region = region->next) {
    if (region->space != space)
      continue;
    if (page < region->stack_limit || page >= region->stack_top)
      continue;

    return region;
  }

  return 0;
}

int vmm_handle_page_fault(unsigned long long fault_addr, registers_t *regs) {
  (void)regs;

  vmm_address_space_t space = vmm_current_address_space();
  unsigned long long page = fault_addr & ~(PAGE_4KB_SIZE - 1);

  stack_region_t *region = find_region(space, page);
  if (region == 0)
    return 0;

  if (vmm_is_mapped_in(space, page))
    return 0;

  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return 0;

  if (vmm_map_page_in(space, page, frame, VMM_WRITABLE | VMM_USER) != 0) {
    pmm_free_frame(frame);
    return 0;
  }

  return 1;
}
