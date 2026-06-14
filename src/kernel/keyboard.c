#include "keyboard.h"
#include "vga.h"
#include "serial.h"
#include "io.h"

static const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

extern void shell_input(char c);

static void ps2_wait_input_ready() {
    /* Wait until the controller has input ready to read. */
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 1) return;
    }
}

static void ps2_wait_output_ready() {
    /* Wait until the controller can accept another byte. */
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

void keyboard_handler() {
    /* Translate scancodes with a simple US keymap. */
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) {
        // Key release
    } else {
        char c = kbd_us[scancode];
        if (c) {
            shell_input(c);
        }
    }
}

void keyboard_init() {
    /* Enable the first PS/2 port IRQ in the controller command byte. */
    ps2_wait_output_ready();
    outb(0x64, 0x20);
    ps2_wait_input_ready();
    uint8_t status = inb(0x60);
    status |= 0x01;
    ps2_wait_output_ready();
    outb(0x64, 0x60);
    ps2_wait_output_ready();
    outb(0x60, status);

    /* Unmask IRQ1 so the keyboard reaches the IDT. */
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~2);
}
