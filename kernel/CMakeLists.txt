cmake_minimum_required(VERSION 3.20)
project(MiniKernel C ASM_NASM)

# --- Bare-metal target: never let CMake try to link/run a hosted test
# program (its usual compiler sanity check), since a freestanding
# kernel object can't produce a runnable host binary.
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_ASM_NASM_COMPILER_WORKS TRUE)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# --- This is the actual fix for editor "not declared" errors: with
# this on, CMake writes build/compile_commands.json, listing the
# exact command (including -Iinclude and the freestanding flags)
# used for every .c file. clangd/ccls read that file automatically
# and stop guessing your include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Symlink it into the project root, since clangd searches upward from
# the file you're editing and won't find it sitting in build/ alone.
# This is a dangling symlink until the first configure finishes, which
# is fine - it resolves as soon as CMake writes the real file below.
file(CREATE_LINK
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
)

add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
)
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE KERNEL_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/arch/*.c"
    "${CMAKE_SOURCE_DIR}/drivers/*.c"
    "${CMAKE_SOURCE_DIR}/core/*.c"
)
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/arch/*.asm"
)

add_library(kernel_objs OBJECT ${KERNEL_C_SOURCES} ${KERNEL_ASM_SOURCES})

set(KERNEL_ELF ${CMAKE_BINARY_DIR}/kernel.elf)
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/kernel)

# Link the ELF ourselves with `ld` directly (not add_executable) so
# CMake never tries to add a hosted CRT/libc via the compiler driver -
# this has to stay a bare -Ttext link against only our own objects,
# exactly like the old Makefile did.
add_custom_command(
    OUTPUT ${KERNEL_ELF}
    COMMAND ld -o ${KERNEL_ELF} -T ${CMAKE_SOURCE_DIR}/linker.ld $<TARGET_OBJECTS:kernel_objs>
    DEPENDS kernel_objs $<TARGET_OBJECTS:kernel_objs> ${CMAKE_SOURCE_DIR}/linker.ld
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Linking kernel.elf"
)

# Raw flat binary the bootloader actually loads - via objcopy, not
# `ld --oformat binary` directly (that was observed to misjudge the
# image span on this toolchain and pad the output to megabytes).
add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND objcopy -O binary ${KERNEL_ELF} ${KERNEL_BIN}
    DEPENDS ${KERNEL_ELF}
    VERBATIM
    COMMENT "Extracting raw kernel binary"
)

add_custom_target(kernel_image ALL DEPENDS ${KERNEL_BIN})

# make.sh (and anything else outside this build) expects to find
# kernel/kernel and kernel/kernel.elf sitting next to this CMakeLists,
# so mirror the build outputs there after every build.
add_custom_command(
    TARGET kernel_image POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${CMAKE_SOURCE_DIR}/kernel
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_ELF} ${CMAKE_SOURCE_DIR}/kernel.elf
    VERBATIM
)
