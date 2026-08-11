#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROGRAMS_DIR="$ROOT_DIR/programs"
BUILD_DIR="$PROGRAMS_DIR/build"

mkdir -p "$BUILD_DIR"

CFLAGS=(-m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -c)
LDFLAGS=(-T "$PROGRAMS_DIR/linker.ld" -nostdlib -static)

built_any=0

for src in "$PROGRAMS_DIR"/*.c; do
  [ -e "$src" ] || continue

  name="$(basename "$src" .c)"
  obj="$BUILD_DIR/$name.o"
  elf="$BUILD_DIR/$name.elf"

  echo "building $name..."
  gcc "${CFLAGS[@]}" "$src" -o "$obj"
  ld "${LDFLAGS[@]}" -o "$elf" "$obj"

  built_any=1
done

if [ "$built_any" = "0" ]; then
  echo "no programs found in $PROGRAMS_DIR"
  exit 1
fi

echo "programs built in $BUILD_DIR"
