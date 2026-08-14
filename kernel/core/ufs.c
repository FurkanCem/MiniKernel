#include "kernel/ufs.h"
#include "kernel/ata.h"
#include "kernel/klog.h"

#define SECTOR_SIZE 512

#define UFS_BASE_LBA 8192
#define UFS_SB_LBA (UFS_BASE_LBA + 0)
#define UFS_DIR_SECTORS 4 /* 2048 bytes / 32 bytes per entry = 64 files */
#define UFS_DIR_LBA (UFS_BASE_LBA + 1)
#define UFS_DATA_LBA (UFS_DIR_LBA + UFS_DIR_SECTORS)
#define UFS_DATA_RESERVED_SECTORS 8192 /* 4 MiB of user data */
#define UFS_DATA_END_LBA (UFS_DATA_LBA + UFS_DATA_RESERVED_SECTORS)

#define UFS_ENTRY_SIZE 32
#define UFS_MAX_ENTRIES ((UFS_DIR_SECTORS * SECTOR_SIZE) / UFS_ENTRY_SIZE)
#define UFS_NAME_MAX 20

static unsigned char superblock[SECTOR_SIZE];
static unsigned char directory[UFS_DIR_SECTORS * SECTOR_SIZE];
static int ufs_ready = 0;
static unsigned int cached_file_count = 0;
static unsigned int next_free_lba = UFS_DATA_LBA;

static void write_u32(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)(v & 0xFF);
  p[1] = (unsigned char)((v >> 8) & 0xFF);
  p[2] = (unsigned char)((v >> 16) & 0xFF);
  p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned int read_u32(const unsigned char *p) {
  return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8) |
         ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned char *entry_at(unsigned int index) {
  return directory + index * UFS_ENTRY_SIZE;
}

static int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  for (; a[i] != '\0' || b[i] != '\0'; i++) {
    if (a[i] != b[i])
      return 0;
  }
  return 1;
}

static unsigned int str_len(const char *s) {
  unsigned int n = 0;
  while (s[n] != '\0')
    n++;
  return n;
}

static void set_name(unsigned char *entry, const char *name) {
  unsigned int i = 0;
  for (; i < UFS_NAME_MAX - 1 && name[i] != '\0'; i++) {
    entry[i] = (unsigned char)name[i];
  }
  for (; i < UFS_NAME_MAX; i++) {
    entry[i] = 0;
  }
}

static int persist_superblock(void) {
  superblock[0] = 'U';
  superblock[1] = 'F';
  superblock[2] = 'S';
  superblock[3] = '1';
  write_u32(superblock + 4, cached_file_count);
  write_u32(superblock + 8, next_free_lba);
  for (unsigned int i = 12; i < SECTOR_SIZE; i++)
    superblock[i] = 0;

  return ata_write_sectors(UFS_SB_LBA, 1, superblock);
}

static int persist_directory(void) {
  return ata_write_sectors(UFS_DIR_LBA, UFS_DIR_SECTORS, directory);
}

static unsigned int count_used_entries(void) {
  unsigned int count = 0;
  for (unsigned int i = 0; i < UFS_MAX_ENTRIES; i++) {
    if (entry_at(i)[0] != 0)
      count++;
  }
  return count;
}

static void format_fresh(void) {
  for (unsigned int i = 0; i < sizeof(directory); i++)
    directory[i] = 0;

  cached_file_count = 0;
  next_free_lba = UFS_DATA_LBA;

  persist_directory();
  persist_superblock();
  ufs_ready = 1;
}

void ufs_init(void) {
  ufs_ready = 0;

  if (ata_read_sectors(UFS_SB_LBA, 1, superblock) != 0) {
    klog_write("ufs: failed to read superblock\n");
    return;
  }

  if (superblock[0] != 'U' || superblock[1] != 'F' || superblock[2] != 'S' ||
      superblock[3] != '1') {
    klog_write("ufs: no filesystem found, formatting\n");
    format_fresh();
    klog_write("ufs: formatted and mounted, 0 file(s)\n");
    return;
  }

  if (ata_read_sectors(UFS_DIR_LBA, UFS_DIR_SECTORS, directory) != 0) {
    klog_write("ufs: failed to read directory\n");
    return;
  }

  next_free_lba = read_u32(superblock + 8);
  if (next_free_lba < UFS_DATA_LBA || next_free_lba > UFS_DATA_END_LBA)
    next_free_lba = UFS_DATA_LBA;

  cached_file_count = count_used_entries();
  ufs_ready = 1;

  klog_write("ufs: mounted, ");
  klog_write_hex(cached_file_count);
  klog_write(" file(s)\n");

  for (unsigned int i = 0; i < UFS_MAX_ENTRIES; i++) {
    unsigned char *e = entry_at(i);
    if (e[0] == 0)
      continue;
    klog_write("ufs:   slot=");
    klog_write_hex(i);
    klog_write(" name='");
    klog_write((const char *)e);
    klog_write("' start_lba=");
    klog_write_hex(read_u32(e + 20));
    klog_write(" size=");
    klog_write_hex(read_u32(e + 24));
    klog_write(" cap_sectors=");
    klog_write_hex(read_u32(e + 28));
    klog_write("\n");
  }
}

unsigned int ufs_file_count(void) { return ufs_ready ? cached_file_count : 0; }

const ufs_dirent_t *ufs_entry(unsigned int index) {
  static ufs_dirent_t out;

  if (!ufs_ready)
    return 0;

  unsigned int seen = 0;
  for (unsigned int i = 0; i < UFS_MAX_ENTRIES; i++) {
    unsigned char *e = entry_at(i);
    if (e[0] == 0)
      continue;
    if (seen == index) {
      for (unsigned int j = 0; j < UFS_NAME_MAX; j++)
        out.name[j] = (char)e[j];
      out.size_bytes = read_u32(e + 24);
      return &out;
    }
    seen++;
  }

  return 0;
}

int ufs_find(const char *name) {
  if (!ufs_ready)
    return -1;

  for (unsigned int i = 0; i < UFS_MAX_ENTRIES; i++) {
    unsigned char *e = entry_at(i);
    if (e[0] != 0 && str_eq((const char *)e, name))
      return (int)i;
  }

  return -1;
}

int ufs_create(const char *name) {
  if (!ufs_ready)
    return -1;

  if (str_len(name) == 0 || str_len(name) >= UFS_NAME_MAX)
    return -1;

  if (ufs_find(name) >= 0)
    return -1;

  int slot = -1;
  for (unsigned int i = 0; i < UFS_MAX_ENTRIES; i++) {
    if (entry_at(i)[0] == 0) {
      slot = (int)i;
      break;
    }
  }
  if (slot < 0)
    return -1; /* directory full */

  unsigned char *e = entry_at((unsigned int)slot);
  set_name(e, name);
  write_u32(e + 20, 0); /* start_lba, allocated lazily on first write */
  write_u32(e + 24, 0); /* size_bytes */
  write_u32(e + 28, 0); /* capacity_sectors */

  cached_file_count++;

  if (persist_directory() != 0 || persist_superblock() != 0)
    return -1;

  return slot;
}

int ufs_delete(const char *name) {
  if (!ufs_ready)
    return -1;

  int slot = ufs_find(name);
  if (slot < 0)
    return -1;

  entry_at((unsigned int)slot)[0] = 0;
  cached_file_count--;

  if (persist_directory() != 0 || persist_superblock() != 0)
    return -1;

  return 0;
}

static int handle_valid(int handle) {
  return ufs_ready && handle >= 0 && (unsigned int)handle < UFS_MAX_ENTRIES &&
         entry_at((unsigned int)handle)[0] != 0;
}

unsigned int ufs_size(int handle) {
  if (!handle_valid(handle))
    return 0;

  return read_u32(entry_at((unsigned int)handle) + 24);
}

int ufs_truncate(int handle, unsigned int new_size) {
  if (!handle_valid(handle))
    return -1;

  unsigned char *e = entry_at((unsigned int)handle);
  unsigned int capacity_bytes = read_u32(e + 28) * SECTOR_SIZE;
  if (new_size > capacity_bytes)
    return -1;

  write_u32(e + 24, new_size);
  if (persist_directory() != 0 || persist_superblock() != 0)
    return -1;

  return 0;
}

static unsigned int sectors_for_bytes(unsigned int bytes) {
  return (bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

static int ensure_capacity(int handle, unsigned int needed_bytes) {
  unsigned char *e = entry_at((unsigned int)handle);
  unsigned int cur_capacity_sectors = read_u32(e + 28);

  if (needed_bytes <= cur_capacity_sectors * SECTOR_SIZE)
    return 0;

  unsigned int needed_sectors = sectors_for_bytes(needed_bytes);
  unsigned int new_capacity_sectors =
      cur_capacity_sectors == 0 ? 1 : cur_capacity_sectors;
  while (new_capacity_sectors < needed_sectors)
    new_capacity_sectors *= 2;

  if (next_free_lba + new_capacity_sectors > UFS_DATA_END_LBA) {
    klog_write("ufs: out of disk space\n");
    return -1;
  }

  unsigned int new_start = next_free_lba;
  unsigned int old_start = read_u32(e + 20);
  unsigned int old_size = read_u32(e + 24);

  if (old_size > 0) {
    static unsigned char move_buf[8 * SECTOR_SIZE];
    unsigned int old_sectors = sectors_for_bytes(old_size);
    unsigned int done = 0;
    while (done < old_sectors) {
      unsigned int chunk = old_sectors - done;
      if (chunk > 8)
        chunk = 8;
      if (ata_read_sectors(old_start + done, chunk, move_buf) != 0)
        return -1;
      if (ata_write_sectors(new_start + done, chunk, move_buf) != 0)
        return -1;
      done += chunk;
    }
  }

  write_u32(e + 20, new_start);
  write_u32(e + 28, new_capacity_sectors);
  next_free_lba += new_capacity_sectors;

  if (persist_directory() != 0 || persist_superblock() != 0)
    return -1;

  return 0;
}

int ufs_write_at(int handle, unsigned int offset, const void *buf,
                 unsigned int len) {
  if (!handle_valid(handle))
    return -1;

  unsigned int needed = offset + len;
  if (needed < offset) /* overflow */
    return -1;

  if (len > 0 && ensure_capacity(handle, needed) != 0)
    return -1;

  unsigned char *e = entry_at((unsigned int)handle);
  unsigned int start_lba = read_u32(e + 20);

  const unsigned char *src = (const unsigned char *)buf;
  unsigned int remaining = len;
  unsigned int cur = offset;
  static unsigned char scratch[SECTOR_SIZE];

  while (remaining > 0) {
    unsigned int sector_index = cur / SECTOR_SIZE;
    unsigned int byte_in_sector = cur % SECTOR_SIZE;
    unsigned int chunk = SECTOR_SIZE - byte_in_sector;
    if (chunk > remaining)
      chunk = remaining;

    unsigned long long lba = start_lba + sector_index;

    if (ata_read_sectors(lba, 1, scratch) != 0)
      return -1;

    for (unsigned int i = 0; i < chunk; i++)
      scratch[byte_in_sector + i] = src[i];

    if (ata_write_sectors(lba, 1, scratch) != 0)
      return -1;

    src += chunk;
    cur += chunk;
    remaining -= chunk;
  }

  if (needed > read_u32(e + 24)) {
    write_u32(e + 24, needed);
    if (persist_directory() != 0 || persist_superblock() != 0)
      return -1;
  }

  return (int)len;
}

int ufs_read_at(int handle, unsigned int offset, void *buf, unsigned int len) {
  if (!handle_valid(handle))
    return -1;

  unsigned char *e = entry_at((unsigned int)handle);
  unsigned int size = read_u32(e + 24);

  if (offset >= size)
    return 0;

  unsigned int available = size - offset;
  if (len > available)
    len = available;

  unsigned int start_lba = read_u32(e + 20);
  unsigned char *dst = (unsigned char *)buf;
  unsigned int remaining = len;
  unsigned int cur = offset;
  static unsigned char scratch[SECTOR_SIZE];

  while (remaining > 0) {
    unsigned int sector_index = cur / SECTOR_SIZE;
    unsigned int byte_in_sector = cur % SECTOR_SIZE;
    unsigned int chunk = SECTOR_SIZE - byte_in_sector;
    if (chunk > remaining)
      chunk = remaining;

    unsigned long long lba = start_lba + sector_index;

    if (ata_read_sectors(lba, 1, scratch) != 0)
      return -1;

    for (unsigned int i = 0; i < chunk; i++)
      dst[i] = scratch[byte_in_sector + i];

    dst += chunk;
    cur += chunk;
    remaining -= chunk;
  }

  return (int)len;
}
