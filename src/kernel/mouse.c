#include "mouse.h"
#include "io.h"
#include "klog.h"

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];
static int32_t mouse_x = 400, mouse_y = 300;

static void mouse_wait(uint8_t type) {
    /* Wait for PS/2 controller readiness. */
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t data) {
    /* Send one byte to the auxiliary mouse device. */
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

static uint8_t mouse_read() {
    /* Read one byte from the mouse data port. */
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    klog(LOG_INFO, "MOUSE", "Initializing PS/2 Mouse...");

    uint8_t status;

    /* Enable the auxiliary mouse device. */
    mouse_wait(1);
    outb(0x64, 0xA8);

    /* Enable mouse IRQ delivery through the controller. */
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    /* Switch to default packet settings. */
    mouse_write(0xF6);
    mouse_read();

    /* Begin streaming movement packets. */
    mouse_write(0xF4);
    mouse_read();

    /* Unmask IRQ12 on the PIC. */
    uint8_t master_mask = inb(0x21);
    outb(0x21, master_mask & ~(1 << 2));
    uint8_t mask = inb(0xA1);
    outb(0xA1, mask & ~(1 << 4));

    klog(LOG_INFO, "MOUSE", "Mouse initialized.");
}

void mouse_handler() {
    /* Accumulate three-byte PS/2 mouse packets. */
    uint8_t status = inb(0x64);
    if (!(status & 1) || !(status & 0x20)) return;

    mouse_byte[mouse_cycle++] = inb(0x60);

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        if (mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40) return;

        int32_t dx = mouse_byte[1];
        int32_t dy = mouse_byte[2];

        if (mouse_byte[0] & 0x10) dx -= 256;
        if (mouse_byte[0] & 0x20) dy -= 256;

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1024) mouse_x = 1024;
        if (mouse_y > 768) mouse_y = 768;
        
        /* Periodic debug hook to avoid log spam. */
        static int count = 0;
        if (++count % 10 == 0) {
             // klog(LOG_DEBUG, "MOUSE", "Pos updated");
        }
    }
}

int32_t mouse_get_x() { return mouse_x; }
int32_t mouse_get_y() { return mouse_y; }
uint8_t mouse_get_buttons() { return mouse_byte[0] & 0x07; }
