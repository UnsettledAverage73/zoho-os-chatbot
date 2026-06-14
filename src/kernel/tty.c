#include "tty.h"
#include "klog.h"
#include "vga.h"
#include "serial.h"

void tty_init(void) {
    klog(LOG_INFO, "TTY", "Initializing TTY subsystem...");
    // Future TTY initialization logic (e.g., buffer setup)
    klog(LOG_INFO, "TTY", "TTY subsystem initialized.");
}

void tty_write(const char* data, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        tty_putchar(data[i]);
    }
}

void tty_putchar(char c) {
    vga_print_char(c);
    serial_putc(c);
}
