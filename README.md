# MiniKernel
- I'm developing an OS and this is its bootloader and kernel 
- Right now its mostly empty and non-optimized but i'll fix them later
# Requirements
- Qemu
- Gcc compiler
- Nasm
- CMake
- xxd
- For Arch
```
pacman -Syu
pacman -S nasm gcc cmake qemu xxd
```
- For Debian
```
apt update
apt install nasm gcc cmake qemu xxd
```
Should Work

# Installation
- Download the code from github and compile
```
git clone https://github.com/FurkanCem/MiniKernel.git
cd MiniKernel
sh make.sh
qemu-system-x86_64 os.img
```
Should work
