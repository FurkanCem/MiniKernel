#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#define PMM_FRAME_SIZE 4096

void pmm_init(void);

unsigned long long pmm_alloc_frame(void);

unsigned long long pmm_alloc_contiguous(unsigned long long count);

void pmm_free_frame(unsigned long long addr);

unsigned long long pmm_total_frames(void);
unsigned long long pmm_free_frames(void);

#endif
