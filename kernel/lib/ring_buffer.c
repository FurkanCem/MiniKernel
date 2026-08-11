#include "kernel/ring_buffer.h"

void ring_buffer_init(ring_buffer_t *rb, volatile char *storage,
                       unsigned int capacity) {
  rb->storage = storage;
  rb->capacity = capacity;
  rb->head = 0;
  rb->tail = 0;
}

int ring_buffer_push(ring_buffer_t *rb, char value) {
  unsigned int next = (rb->head + 1) % rb->capacity;
  if (next == rb->tail)
    return 0;

  rb->storage[rb->head] = value;
  rb->head = next;
  return 1;
}

int ring_buffer_pop(ring_buffer_t *rb, char *out) {
  if (rb->tail == rb->head)
    return 0;

  *out = rb->storage[rb->tail];
  rb->tail = (rb->tail + 1) % rb->capacity;
  return 1;
}
