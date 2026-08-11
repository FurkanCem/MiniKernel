#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "== assembling bootloader =="
(cd "$ROOT_DIR/bootloader" && nasm -o boot boot.asm)

echo "== building kernel =="
if [ ! -d "$ROOT_DIR/kernel/build" ]; then
  cmake -S "$ROOT_DIR/kernel" -B "$ROOT_DIR/kernel/build"
fi
cmake --build "$ROOT_DIR/kernel/build"

echo "== building programs =="
"$SCRIPT_DIR/build_programs.sh"

echo "== patching kernel sector count into boot sector =="
KERNEL_BIN="$ROOT_DIR/kernel/kernel"
BOOT_BIN="$ROOT_DIR/bootloader/boot"

kernel_size=$(wc -c < "$KERNEL_BIN")
kernel_sectors=$(( (kernel_size + 511) / 512 ))
if [ "$kernel_sectors" -gt 255 ]; then
  echo "error: kernel is $kernel_sectors sectors, but the boot sector's" >&2
  echo "kernel_size field is a single byte (max 255 sectors / ~127KB)." >&2
  exit 1
fi
printf "\\x$(printf '%02x' "$kernel_sectors")" | dd of="$BOOT_BIN" bs=1 seek=2 count=1 conv=notrunc status=none

echo "== assembling disk image =="
args=("$ROOT_DIR/os.img")
for elf in "$ROOT_DIR/programs/build"/*.elf; do
  [ -e "$elf" ] || continue
  name="$(basename "$elf" .elf)"
  args+=("$name:$elf")
done

(cd "$ROOT_DIR" && python3 mkdisk.py "${args[@]}")

echo "build finished successfully: $ROOT_DIR/os.img"
