#include "kernel/fs.h"
#include "kernel/ata.h"
#include "kernel/klog.h"

#define FS_MAGIC "MFS1"
#define FS_BASE_LBA 2048
#define FS_SUPERBLOCK_LBA (FS_BASE_LBA + 0)
#define FS_DIR_LBA (FS_BASE_LBA + 1)
#define FS_MAX_ENTRIES 16

static unsigned char superblock[512];
static unsigned char dir_sector[512];
static int fs_ready = 0;
static unsigned int cached_file_count = 0;

static int str_eq(const char *a, const char *b) {
  unsigned int i = 0;
  for (; a[i] != '\0' || b[i] != '\0'; i++) {
    if (a[i] != b[i])
      return 0;
  }
  return 1;
}

void fs_init(void) {
  fs_ready = 0;
  cached_file_count = 0;

  if (ata_read_sectors(FS_SUPERBLOCK_LBA, 1, superblock) != 0) {
    klog_write("fs: failed to read superblock\n");
    return;
  }

  if (superblock[0] != 'M' || superblock[1] != 'F' || superblock[2] != 'S' ||
      superblock[3] != '1') {
    klog_write("fs: no filesystem found (bad magic)\n");
    return;
  }

  if (ata_read_sectors(FS_DIR_LBA, 1, dir_sector) != 0) {
    klog_write("fs: failed to read directory\n");
    return;
  }

  cached_file_count = ((unsigned int)superblock[4]) |
                       ((unsigned int)superblock[5] << 8) |
                       ((unsigned int)superblock[6] << 16) |
                       ((unsigned int)superblock[7] << 24);
  if (cached_file_count > FS_MAX_ENTRIES)
    cached_file_count = FS_MAX_ENTRIES;

  fs_ready = 1;

  klog_write("fs: mounted, ");
  klog_write_hex(cached_file_count);
  klog_write(" file(s)\n");
}

unsigned int fs_file_count(void) { return fs_ready ? cached_file_count : 0; }

const fs_dirent_t *fs_entry(unsigned int index) {
  static fs_dirent_t entry;

  if (!fs_ready || index >= cached_file_count)
    return 0;

  const unsigned char *raw = dir_sector + index * 32;

  for (int i = 0; i < 20; i++) {
    entry.name[i] = (char)raw[i];
  }
  entry.start_lba = ((unsigned int)raw[20]) | ((unsigned int)raw[21] << 8) |
                     ((unsigned int)raw[22] << 16) |
                     ((unsigned int)raw[23] << 24);
  entry.size_bytes = ((unsigned int)raw[24]) | ((unsigned int)raw[25] << 8) |
                      ((unsigned int)raw[26] << 16) |
                      ((unsigned int)raw[27] << 24);

  return &entry;
}

const fs_dirent_t *fs_find(const char *name) {
  for (unsigned int i = 0; i < fs_file_count(); i++) {
    const fs_dirent_t *e = fs_entry(i);
    if (e != 0 && str_eq(e->name, name))
      return e;
  }
  return 0;
}

int fs_read_file(const fs_dirent_t *entry, void *buf) {
  if (entry == 0)
    return -1;

  unsigned int sectors = (entry->size_bytes + 511) / 512;
  if (sectors == 0)
    sectors = 1;

  unsigned char *dst = (unsigned char *)buf;
  while (sectors > 0) {
    unsigned int chunk = sectors > 256 ? 256 : sectors;
    if (ata_read_sectors(entry->start_lba +
                              ((dst - (unsigned char *)buf) / 512),
                          chunk, dst) != 0)
      return -1;
    dst += chunk * 512;
    sectors -= chunk;
  }

  return 0;
}
