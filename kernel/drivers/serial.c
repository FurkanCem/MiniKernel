#include "kernel/serial.h"
#include "kernel/io.h"

#define COM1 0x3F8

void serial_init(void) {
  outb(COM1 + 1, 0x00); /* disable UART interrupts - we poll instead */
  outb(COM1 + 3, 0x80); /* enable DLAB to set the baud rate divisor */
  outb(COM1 + 0, 0x03); /* divisor low byte  -> 38400 baud */
  outb(COM1 + 1, 0x00); /* divisor high byte */
  outb(COM1 + 3, 0x03); /* 8 bits, no parity, 1 stop bit; also clears DLAB */
  outb(COM1 + 2, 0xC7); /* enable + clear FIFOs, 14-byte trigger level */
  outb(COM1 + 4, 0x0B); /* IRQs off, RTS/DSR set (needed for the chip to
                           actually transmit) */
}

static int serial_tx_ready(void) { return inb(COM1 + 5) & 0x20; }

static void serial_putchar(char c) {
  while (!serial_tx_ready())
    ;
  outb(COM1, (unsigned char)c);
}

void serial_write(const char *str) {
  for (unsigned int i = 0; str[i] != '\0'; i++) {
    if (str[i] == '\n')
      serial_putchar('\r'); /* terminals want CRLF, not bare LF */
    serial_putchar(str[i]);
  }
}

void serial_write_n(const char *data, unsigned long long len) {
  for (unsigned long long i = 0; i < len; i++) {
    if (data[i] == '\n')
      serial_putchar('\r');
    serial_putchar(data[i]);
  }
}

void serial_write_hex(unsigned long long value) {
  static const char digits[] = "0123456789ABCDEF";
  serial_write("0x");
  for (int shift = 60; shift >= 0; shift -= 4) {
    serial_putchar(digits[(value >> shift) & 0xF]);
  }
}
