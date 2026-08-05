CC = gcc
LD = ld
ASM = nasm

INC = -Iinc/
SRC = $(shell find . -type f -name "*.c")
ASM_SRC = $(shell find . -type f -name "*.asm")

OBJ = ${ASM_SRC:.asm=.o} ${SRC:.c=.o} 

OBJCOPY = objcopy

CCFLAGS = -c 
LDFLAGS = -Ttext 0x8200
ASFLAGS = -f elf64

all: kernel kernel.elf

kernel: kernel.elf
	${OBJCOPY} -O binary kernel.elf kernel

kernel.elf: ${OBJ}
	${LD} -o $@ ${LDFLAGS} $^

%.o: %.c
	${CC} ${CCFLAGS} ${INC} -o $@ $^

%.o: %.asm
	${ASM} $< ${ASFLAGS} -o $@

clean:
	rm -f kernel kernel.elf *.o **/*.o
