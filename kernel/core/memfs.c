#include "kernel/memfs.h"
#include "kernel/heap.h"

#define MEMFS_INITIAL_CAPACITY 4
#define MEMFS_NAME_MAX 32
#define MEMFS_INITIAL_BUFFER_SIZE 256

typedef struct {
  char name[MEMFS_NAME_MAX];
  unsigned char *data;
  unsigned int size;
  unsigned int capacity;
  int in_use;
} memfs_entry_t;

static memfs_entry_t *entries = 0;
static int entry_capacity = 0;

static void init_entry_slot(memfs_entry_t *e) {
  e->name[0] = '\0';
  e->data = 0;
  e->size = 0;
  e->capacity = 0;
  e->in_use = 0;
}

static void copy_entry_slot(memfs_entry_t *dst, const memfs_entry_t *src) {
  for (int i = 0; i < MEMFS_NAME_MAX; i++) {
    dst->name[i] = src->name[i];
  }
  dst->data = src->data;
  dst->size = src->size;
  dst->capacity = src->capacity;
  dst->in_use = src->in_use;
}

static int grow_table(void) {
  int new_capacity =
      entry_capacity == 0 ? MEMFS_INITIAL_CAPACITY : entry_capacity * 2;

  memfs_entry_t *new_table = (memfs_entry_t *)kmalloc(
      sizeof(memfs_entry_t) * (unsigned long long)new_capacity);
  if (new_table == 0)
    return -1;

  for (int i = 0; i < entry_capacity; i++) {
    copy_entry_slot(&new_table[i], &entries[i]);
  }
  for (int i = entry_capacity; i < new_capacity; i++) {
    init_entry_slot(&new_table[i]);
  }

  memfs_entry_t *old_table = entries;
  entries = new_table;
  entry_capacity = new_capacity;

  if (old_table != 0)
    kfree(old_table);

  return 0;
}

static int str_eq(const char *a, const char *b) {
  int i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i])
      return 0;
    i++;
  }
  return a[i] == '\0' && b[i] == '\0';
}

static void copy_name(char *dst, const char *src) {
  int i = 0;
  while (src[i] != '\0' && i < MEMFS_NAME_MAX - 1) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
}

static int find_slot(const char *name) {
  for (int i = 0; i < entry_capacity; i++) {
    if (entries[i].in_use && str_eq(entries[i].name, name))
      return i;
  }
  return -1;
}

static int find_free_slot(void) {
  for (int i = 0; i < entry_capacity; i++) {
    if (!entries[i].in_use)
      return i;
  }
  return -1;
}

int memfs_create(const char *name) {
  if (find_slot(name) >= 0)
    return -1;

  int slot = find_free_slot();
  if (slot == -1) {
    int previous_capacity = entry_capacity;
    if (grow_table() != 0)
      return -1;
    slot = previous_capacity;
  }

  unsigned char *buf = (unsigned char *)kmalloc(MEMFS_INITIAL_BUFFER_SIZE);
  if (buf == 0)
    return -1;

  copy_name(entries[slot].name, name);
  entries[slot].data = buf;
  entries[slot].size = 0;
  entries[slot].capacity = MEMFS_INITIAL_BUFFER_SIZE;
  entries[slot].in_use = 1;

  return slot;
}

int memfs_find(const char *name) { return find_slot(name); }

int memfs_delete(const char *name) {
  int slot = find_slot(name);
  if (slot == -1)
    return -1;

  kfree(entries[slot].data);
  init_entry_slot(&entries[slot]);
  return 0;
}

unsigned int memfs_size(int handle) {
  if (handle < 0 || handle >= entry_capacity || !entries[handle].in_use)
    return 0;

  return entries[handle].size;
}

static int ensure_capacity(int handle, unsigned int needed) {
  if (needed <= entries[handle].capacity)
    return 0;

  unsigned int new_capacity = entries[handle].capacity * 2;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  unsigned char *new_buf = (unsigned char *)kmalloc(new_capacity);
  if (new_buf == 0)
    return -1;

  for (unsigned int i = 0; i < entries[handle].size; i++) {
    new_buf[i] = entries[handle].data[i];
  }

  kfree(entries[handle].data);
  entries[handle].data = new_buf;
  entries[handle].capacity = new_capacity;
  return 0;
}

int memfs_write_at(int handle, unsigned int offset, const void *buf,
                    unsigned int len) {
  if (handle < 0 || handle >= entry_capacity || !entries[handle].in_use)
    return -1;

  unsigned int needed = offset + len;
  if (needed < offset)
    return -1;

  if (ensure_capacity(handle, needed) != 0)
    return -1;

  const unsigned char *src = (const unsigned char *)buf;
  for (unsigned int i = 0; i < len; i++) {
    entries[handle].data[offset + i] = src[i];
  }

  if (needed > entries[handle].size)
    entries[handle].size = needed;

  return (int)len;
}

int memfs_read_at(int handle, unsigned int offset, void *buf,
                   unsigned int len) {
  if (handle < 0 || handle >= entry_capacity || !entries[handle].in_use)
    return -1;

  if (offset >= entries[handle].size)
    return 0;

  unsigned int available = entries[handle].size - offset;
  if (len > available)
    len = available;

  unsigned char *dst = (unsigned char *)buf;
  for (unsigned int i = 0; i < len; i++) {
    dst[i] = entries[handle].data[offset + i];
  }

  return (int)len;
}

int memfs_first(void) {
  for (int i = 0; i < entry_capacity; i++) {
    if (entries[i].in_use)
      return i;
  }
  return -1;
}

int memfs_next(int handle) {
  for (int i = handle + 1; i < entry_capacity; i++) {
    if (entries[i].in_use)
      return i;
  }
  return -1;
}

const char *memfs_name(int handle) {
  if (handle < 0 || handle >= entry_capacity || !entries[handle].in_use)
    return "";

  return entries[handle].name;
}
