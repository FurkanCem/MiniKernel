# MiniKernel

**MiniKernel** is a small x86-64 operating system and kernel written from scratch in **C and x86-64 Assembly**.

This is an ongoing systems-programming project focused on understanding how an operating system interacts directly with the hardware, from the boot process and interrupt handling to memory management, context switching, and scheduling.

> **Status:** Active development — many components are intentionally minimal and will be expanded over time.

## Current Features

### Boot & Architecture

* Custom x86 bootloader
* x86-64 long mode
* Custom linker script
* C and x86-64 Assembly kernel code
* Bare-metal kernel build using CMake

### Interrupts & Exceptions

* Interrupt Descriptor Table (IDT)
* Interrupt Service Routines (ISRs)
* IRQ handling
* PIC remapping
* PIT timer interrupts
* CPU exception handling
* General Protection Fault handling
* Page Fault handling
* Kernel panic reporting with exception information and register state

### Memory Management

* E820 BIOS memory map detection
* Physical Memory Manager (PMM)
* 2 MiB page mapping
* Virtual Memory Manager (VMM)
* Kernel heap allocator

### Drivers & I/O

* VGA text-mode video output
* PS/2 keyboard driver
* PIT timer
* Serial output
* Basic kernel logging

### Scheduling & Threads

* Kernel threads
* Per-thread kernel stacks
* Thread creation and termination
* Thread states
* x86-64 context switching
* Cooperative scheduling
* Timer-driven preemptive scheduling
* Basic thread lifecycle management

## Current Architecture

The project is currently structured around a small monolithic kernel:

```text
MiniKernel
├── bootloader/
├── kernel/
│   ├── arch/x86_64/
│   │   ├── boot/startup
│   │   ├── interrupt & exception handling
│   │   └── context switching
│   │
│   ├── core/
│   │   ├── kernel entry
│   │   ├── shell
│   │   ├── logging
│   │   └── panic handling
│   │
│   ├── drivers/
│   │   ├── keyboard
│   │   ├── PIT/timer
│   │   ├── PIC
│   │   ├── serial
│   │   └── video
│   │
│   ├── mm/
│   │   ├── E820 memory detection
│   │   ├── physical memory management
│   │   ├── virtual memory
│   │   └── heap
│   │
│   └── sched/
│       └── threading & scheduling
```

## Building

### Requirements

* GCC
* NASM
* CMake
* QEMU
* `xxd`
* `make`

### Arch Linux

```bash
sudo pacman -Syu
sudo pacman -S gcc nasm cmake qemu xxd make
```

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install gcc nasm cmake qemu-system-x86 xxd make
```

### Build and Run

Clone the repository:

```bash
git clone https://github.com/FurkanCem/MiniKernel.git
cd MiniKernel
```

Build the bootloader and kernel:

```bash
./make.sh
```

Run the resulting image with QEMU:

```bash
qemu-system-x86_64 os.img
```

## Development Roadmap

MiniKernel is still under active development. Planned areas include:

* Improving thread lifecycle management
* More robust preemptive scheduling
* Synchronization primitives such as spinlocks
* More complete virtual memory management
* User/kernel privilege separation
* System calls
* Process management
* Multicore / SMP support
* Additional device drivers
* A more capable kernel shell

## Why?

The primary goal of this project is to learn systems programming by implementing the abstractions normally hidden by a modern operating system.

Rather than starting with an existing kernel framework, MiniKernel is built from the bottom up to explore:

* CPU initialization
* Interrupts and exceptions
* Memory management
* Hardware I/O
* Context switching
* Thread scheduling
* Kernel architecture

The project is intentionally small and experimental, and the architecture will evolve as new concepts are implemented.
