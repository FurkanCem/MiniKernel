#include "kernel/ata.h"
#include "kernel/io.h"

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA_LO 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HI 0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

#define ATA_CMD_READ_SECTORS 0x20

static int ata_wait_ready(void) {
  for (int i = 0; i < 1000000; i++) {
    unsigned char status = inb(ATA_STATUS);
    if (status & ATA_SR_ERR)
      return -1;
    if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
      return 0;
  }
  return -1;
}

static int ata_wait_not_busy(void) {
  for (int i = 0; i < 1000000; i++) {
    if (!(inb(ATA_STATUS) & ATA_SR_BSY))
      return 0;
  }
  return -1;
}

int ata_read_sectors(unsigned long long lba, unsigned int count, void *buf) {
  if (count == 0 || count > 256)
    return -1;

  unsigned short *dst = (unsigned short *)buf;

  if (ata_wait_not_busy() != 0)
    return -1;

  outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_SECCOUNT, (unsigned char)(count == 256 ? 0 : count));
  outb(ATA_LBA_LO, (unsigned char)(lba & 0xFF));
  outb(ATA_LBA_MID, (unsigned char)((lba >> 8) & 0xFF));
  outb(ATA_LBA_HI, (unsigned char)((lba >> 16) & 0xFF));
  outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

  for (unsigned int s = 0; s < count; s++) {
    if (ata_wait_ready() != 0)
      return -1;

    for (int i = 0; i < 256; i++) {
      dst[s * 256 + i] = inw(ATA_DATA);
    }
  }

  return 0;
}
