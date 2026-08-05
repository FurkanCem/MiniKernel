cmake_minimum_required(VERSION 3.20)
project(MiniKernel C ASM_NASM)

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_ASM_NASM_COMPILER_WORKS TRUE)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_custom_command(
    OUTPUT ${KERNEL_ELF}
    COMMAND ld -o ${KERNEL_ELF} -Ttext 0x8200 $<TARGET_OBJECTS:kernel_objs>
    DEPENDS kernel_objs $<TARGET_OBJECTS:kernel_objs>
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Linking kernel.elf"
)

add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND objcopy -O binary ${KERNEL_ELF} ${KERNEL_BIN}
    DEPENDS ${KERNEL_ELF}
    VERBATIM
    COMMENT "Extracting raw kernel binary"
)

add_custom_target(kernel_image ALL DEPENDS ${KERNEL_BIN})

add_custom_command(
    TARGET kernel_image POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${CMAKE_SOURCE_DIR}/kernel
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_ELF} ${CMAKE_SOURCE_DIR}/kernel.elf
    VERBATIM
)
