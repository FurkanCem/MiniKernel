#include "kernel/pic.h"
#include "kernel/io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define ICW1_ICW4    0x01   /* an ICW4 will be sent */
#define ICW1_INIT    0x10   /* start initialization sequence */
#define ICW4_8086    0x01   /* 8086/88 (MCS-80/85) mode */

void pic_remap(int offset1, int offset2){
    /* ICW1: start the init sequence on both PICs */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();

    /* ICW2: vector offset each PIC's IRQ0 maps to */
    outb(PIC1_DATA, offset1); io_wait();
    outb(PIC2_DATA, offset2); io_wait();

    /* ICW3: tell master there's a slave PIC at IRQ2 (0000 0100),
       tell slave its own cascade identity (0000 0010) */
    outb(PIC1_DATA, 4); io_wait();
    outb(PIC2_DATA, 2); io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask everything by default - the caller unmasks only the IRQs
       it has an actual handler for, rather than inheriting whatever
       state the BIOS happened to leave behind. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(unsigned char irq){
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_unmask_irq(unsigned char irq){
    unsigned short port;

    if (irq < 8){
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    outb(port, inb(port) & ~(1 << irq));
}

void pic_mask_irq(unsigned char irq){
    unsigned short port;

    if (irq < 8){
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    outb(port, inb(port) | (1 << irq));
}
