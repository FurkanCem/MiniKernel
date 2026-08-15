import struct
import sys
import os

FS_BASE_LBA = 2048
SECTOR = 512
MAX_ENTRIES = 16

# Must mirror the constants in kernel/core/ufs.c exactly - this is the
# persistent, writable region a running kernel can create/save files
# into (as opposed to the read-only MFS1 region built above, which only
# ever holds binaries baked in at build time).
UFS_BASE_LBA = 8192
UFS_DIR_SECTORS = 4
UFS_DATA_LBA = UFS_BASE_LBA + 1 + UFS_DIR_SECTORS
UFS_DATA_RESERVED_SECTORS = 8192

def build(boot_path, kernel_path, files, out_path):
    with open(boot_path, 'rb') as f:
        boot = f.read()
    with open(kernel_path, 'rb') as f:
        kernel = f.read()

    img = bytearray(boot + kernel)
    pad_to = FS_BASE_LBA * SECTOR
    if len(img) < pad_to:
        img += bytes(pad_to - len(img))

    if len(files) > MAX_ENTRIES:
        raise ValueError('too many files, max is %d' % MAX_ENTRIES)

    superblock = bytearray(SECTOR)
    superblock[0:4] = b'MFS1'
    superblock[4:8] = struct.pack('<I', len(files))

    directory = bytearray(SECTOR)
    file_start_lba = FS_BASE_LBA + 16
    blobs = []

    for i, (name, path) in enumerate(files):
        if len(name) >= 20:
            raise ValueError('name too long: %s' % name)
        with open(path, 'rb') as f:
            data = f.read()

        entry = bytearray(32)
        entry[0:len(name)] = name.encode('ascii')
        entry[20:24] = struct.pack('<I', file_start_lba)
        entry[24:28] = struct.pack('<I', len(data))
        directory[i * 32:(i + 1) * 32] = entry

        blobs.append(data)
        sectors = (len(data) + SECTOR - 1) // SECTOR
        file_start_lba += sectors

    img += superblock
    img += directory

    for i, ((name, path), data) in enumerate(zip(files, blobs)):
        current_lba = len(img) // SECTOR
        target_lba = FS_BASE_LBA + 16 + sum(
            (len(b) + SECTOR - 1) // SECTOR for b in blobs[:i]
        )
        if current_lba < target_lba:
            img += bytes((target_lba - current_lba) * SECTOR)
        img += data
        remainder = len(img) % SECTOR
        if remainder != 0:
            img += bytes(SECTOR - remainder)

    with open(out_path, 'wb') as f:
        f.write(img)

    print('wrote', out_path, len(img), 'bytes,', len(img) // SECTOR, 'sectors')


def format_ufs_region(out_path):
    """Appends a freshly-formatted, empty UFS (persistent user filesystem)
    region after the existing image, so the kernel's ufs_init() finds a
    valid, correctly-sized filesystem on first boot instead of reading
    past the end of the image file."""
    current_len = os.path.getsize(out_path)
    pad_to = UFS_BASE_LBA * SECTOR
    if current_len > pad_to:
        raise ValueError(
            'system region (%d bytes) overruns UFS_BASE_LBA=%d; '
            'raise UFS_BASE_LBA in both mkdisk.py and kernel/core/ufs.c'
            % (current_len, UFS_BASE_LBA))

    with open(out_path, 'ab') as f:
        if current_len < pad_to:
            f.write(bytes(pad_to - current_len))

        superblock = bytearray(SECTOR)
        superblock[0:4] = b'UFS1'
        superblock[4:8] = struct.pack('<I', 0)          # file_count
        superblock[8:12] = struct.pack('<I', UFS_DATA_LBA)  # next_free_lba
        f.write(superblock)

        f.write(bytes(UFS_DIR_SECTORS * SECTOR))  # empty directory

        f.write(bytes(UFS_DATA_RESERVED_SECTORS * SECTOR))  # reserved data

    total_sectors = (UFS_BASE_LBA + 1 + UFS_DIR_SECTORS +
                      UFS_DATA_RESERVED_SECTORS)
    print('formatted UFS region:', total_sectors, 'sectors total,',
          UFS_DATA_RESERVED_SECTORS, 'sectors of user data space')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('usage: mkdisk.py out.img [name:path ...]')
        print('example: mkdisk.py os.img hello:build/hello.bin')
        sys.exit(1)

    out_path = sys.argv[1]
    files = []
    for arg in sys.argv[2:]:
        name, path = arg.split(':', 1)
        files.append((name, path))

    build('bootloader/boot', 'kernel/kernel', files, out_path)
    format_ufs_region(out_path)
