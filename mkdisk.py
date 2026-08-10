import struct
import sys
import os

FS_BASE_LBA = 2048
SECTOR = 512
MAX_ENTRIES = 16

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
