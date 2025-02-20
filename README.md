# MiniKernel
- I'm developing an OS and this is its bootloader and kernel 
- Right now its mostly empty and non-optimized but i'll fix them later
# Requirements
- Qemu
- Gcc compiler
- Nasm
- For Arch
```
pacman -Syu
pacman -S nasm gcc qemu
```
- For Debian
```
apt update
apt install nasm gcc qemu
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
