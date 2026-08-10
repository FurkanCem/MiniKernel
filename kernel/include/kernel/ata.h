#ifndef KERNEL_ATA_H
#define KERNEL_ATA_H

int ata_read_sectors(unsigned long long lba, unsigned int count, void *buf);

#endif
