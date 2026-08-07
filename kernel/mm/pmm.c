#include "kernel/pmm.h"
#include "kernel/e820.h"

extern char _kernel_end[];

#define PMM_MAX_MEMORY 0x100000000ULL
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY / PMM_FRAME_SIZE)
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8)

static unsigned char bitmap[PMM_BITMAP_BYTES];

static unsigned long long total_frames = 0;
static unsigned long long free_frames = 0;
static unsigned long long search_hint =
    0; /* avoids rescanning already-full low memory every call */

static inline unsigned long long frame_of(unsigned long long addr) {
  return addr / PMM_FRAME_SIZE;
}

static void set_used(unsigned long long frame) {
  if (frame >= PMM_MAX_FRAMES)
    return;

  unsigned long long byte = frame / 8;
  unsigned char bit = (unsigned char)(1 << (frame % 8));
  if (!(bitmap[byte] & bit)) {
    bitmap[byte] |= bit;
    if (free_frames > 0)
      free_frames--;
  }
}

static void set_free(unsigned long long frame) {
  if (frame >= PMM_MAX_FRAMES)
    return;

  unsigned long long byte = frame / 8;
  unsigned char bit = (unsigned char)(1 << (frame % 8));
  if (bitmap[byte] & bit) {
    bitmap[byte] &= (unsigned char)~bit;
    free_frames++;
  }
}

static int is_used(unsigned long long frame) {
  if (frame >= PMM_MAX_FRAMES)
    return 1; /* outside tracked range - treat as unavailable */

  unsigned long long byte = frame / 8;
  unsigned char bit = (unsigned char)(1 << (frame % 8));
  return (bitmap[byte] & bit) != 0;
}

static void reserve_range(unsigned long long start, unsigned long long end) {
  unsigned long long first = frame_of(start);
  unsigned long long last = frame_of(end + PMM_FRAME_SIZE - 1);

  for (unsigned long long f = first; f < last; f++) {
    set_used(f);
  }
}

void pmm_init(void) {
  for (unsigned long long i = 0; i < PMM_BITMAP_BYTES; i++) {
    bitmap[i] = 0xFF;
  }
  total_frames = 0;
  free_frames = 0;
  search_hint = 0;

  unsigned int count = e820_entry_count();
  for (unsigned int i = 0; i < count; i++) {
    const e820_entry_t *entry = e820_get_entry(i);
    if (entry == 0 || entry->type != E820_TYPE_USABLE)
      continue;

    unsigned long long start = entry->base;
    unsigned long long end = entry->base + entry->length;
    if (start >= PMM_MAX_MEMORY)
      continue;
    if (end > PMM_MAX_MEMORY)
      end = PMM_MAX_MEMORY;

    unsigned long long first = frame_of(start + PMM_FRAME_SIZE - 1);
    unsigned long long last = frame_of(end);

    for (unsigned long long f = first; f < last; f++) {
      set_free(f);
      total_frames++;
    }
  }

  reserve_range(0x0, 0x8200);

  reserve_range(0x8200, (unsigned long long)_kernel_end);

  reserve_range(0xB8000, 0xB8000 + 4000);
}

unsigned long long pmm_alloc_frame(void) {
  for (unsigned long long i = 0; i < PMM_MAX_FRAMES; i++) {
    unsigned long long f = (search_hint + i) % PMM_MAX_FRAMES;
    if (!is_used(f)) {
      set_used(f);
      search_hint = f + 1;
      return f * PMM_FRAME_SIZE;
    }
  }
  return 0; /* out of memory */
}

void pmm_free_frame(unsigned long long addr) { set_free(frame_of(addr)); }

unsigned long long pmm_total_frames(void) { return total_frames; }

unsigned long long pmm_free_frames(void) { return free_frames; }
