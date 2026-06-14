#include "serial.h"
#include "io.h"
#include "lock.h"
#include "shell.h"

#define COM1 0x3F8

void serial_init() {
    /* Configure COM1 for 38400 8N1 and enable RX interrupts. */
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (38400 baud)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    /* Enable received-data interrupts. */
    outb(COM1 + 1, 0x01);
    
    /* Unmask IRQ4 so serial input can reach the CPU. */
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~(1 << 4));
}

static int is_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_handler() {
    /* Drain all pending input bytes and feed the shell. */
    while (inb(COM1 + 5) & 0x01) {
        char c = inb(COM1);
        shell_input(c);
    }
}

void serial_poll_input() {
    serial_handler();
}

static spinlock_t serial_lock;

uint64_t serial_lock_all() {
    return spin_lock_irqsave(&serial_lock);
}

void serial_unlock_all(uint64_t flags) {
    spin_unlock_irqrestore(&serial_lock, flags);
}

static void serial_print_locked(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putc(str[i]);
    }
}

void serial_print_no_lock(const char* str) {
    serial_print_locked(str);
}

void serial_print_dec_no_lock(uint64_t val) {
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[20];
    int i = 0;
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (--i >= 0) {
        serial_putc(buf[i]);
    }
}

void serial_print(const char* str) {
    uint64_t flags = spin_lock_irqsave(&serial_lock);
    serial_print_locked(str);
    spin_unlock_irqrestore(&serial_lock, flags);
}

void serial_print_hex(uint64_t val) {
    uint64_t flags = spin_lock_irqsave(&serial_lock);
    char hex_chars[] = "0123456789ABCDEF";
    serial_putc('0');
    serial_putc('x');
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex_chars[(val >> i) & 0xF]);
    }
    spin_unlock_irqrestore(&serial_lock, flags);
}

void serial_print_dec(uint64_t val) {
    uint64_t flags = spin_lock_irqsave(&serial_lock);
    if (val == 0) {
        serial_putc('0');
        spin_unlock_irqrestore(&serial_lock, flags);
        return;
    }
    char buf[20];
    int i = 0;
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (--i >= 0) {
        serial_putc(buf[i]);
    }
    spin_unlock_irqrestore(&serial_lock, flags);
}
