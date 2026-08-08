#include "kernel/heap.h"
#include "kernel/io.h"
#include "kernel/pmm.h"

typedef struct heap_block {
  unsigned long long size; /* usable size, not including this header */
  int free;
  struct heap_block *next;
} heap_block_t;

#define HEAP_ALIGN 16
#define HEAP_HEADER_SIZE (sizeof(heap_block_t))

static heap_block_t *heap_head = 0;

static unsigned long long align_up(unsigned long long value,
                                   unsigned long long align) {
  return (value + align - 1) & ~(align - 1);
}

static heap_block_t *grow_heap(void) {
  unsigned long long frame = pmm_alloc_frame();
  if (frame == 0)
    return 0; /* out of physical memory entirely */

  heap_block_t *block = (heap_block_t *)frame;
  block->size = PMM_FRAME_SIZE - HEAP_HEADER_SIZE;
  block->free = 1;
  block->next = heap_head;
  heap_head = block;
  return block;
}

void heap_init(void) { heap_head = 0; }

void *kmalloc(unsigned long long size) {
  if (size == 0)
    return 0;

  size = align_up(size, HEAP_ALIGN);

  unsigned long long flags = irq_save();

  heap_block_t *block = heap_head;
  while (block != 0) {
    if (block->free && block->size >= size)
      break;
    block = block->next;
  }

  if (block == 0) {
    block = grow_heap();
    if (block == 0 || block->size < size) {
      irq_restore(flags);
      return 0;
    }
  }

  if (block->size >= size + HEAP_HEADER_SIZE + HEAP_ALIGN) {
    unsigned char *raw = (unsigned char *)block;
    heap_block_t *remainder = (heap_block_t *)(raw + HEAP_HEADER_SIZE + size);
    remainder->size = block->size - size - HEAP_HEADER_SIZE;
    remainder->free = 1;
    remainder->next = block->next;

    block->size = size;
    block->next = remainder;
  }

  block->free = 0;
  irq_restore(flags);
  return (void *)((unsigned char *)block + HEAP_HEADER_SIZE);
}

void kfree(void *ptr) {
  if (ptr == 0)
    return;

  unsigned long long flags = irq_save();
  heap_block_t *block =
      (heap_block_t *)((unsigned char *)ptr - HEAP_HEADER_SIZE);
  block->free = 1;
  irq_restore(flags);
}
