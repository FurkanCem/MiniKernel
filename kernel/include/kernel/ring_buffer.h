#ifndef KERNEL_RING_BUFFER_H
#define KERNEL_RING_BUFFER_H

typedef struct {
  volatile char *storage;
  unsigned int capacity;
  volatile unsigned int head;
  volatile unsigned int tail;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb, volatile char *storage,
                       unsigned int capacity);
int ring_buffer_push(ring_buffer_t *rb, char value);
int ring_buffer_pop(ring_buffer_t *rb, char *out);

#endif
