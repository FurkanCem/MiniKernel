#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

void pic_remap(int offset1, int offset2);

void pic_send_eoi(unsigned char irq);

void pic_unmask_irq(unsigned char irq);
void pic_mask_irq(unsigned char irq);

#endif
