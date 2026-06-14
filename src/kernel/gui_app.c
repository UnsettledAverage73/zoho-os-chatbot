#include "graphics.h"
#include "window.h"
#include "syscall.h"

/*
 * Minimal user-space GUI demo.
 *
 * It creates a window, draws into the shared buffer, and polls for events
 * through the syscall interface.
 */
void user_gui_app() {
    uint32_t win_id;
    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov $200, %%rdi\n"
        "mov $200, %%rsi\n"
        "mov $300, %%rdx\n"
        "mov $225, %%r10\n"
        "syscall\n"
        : "=a"(win_id)
        :
        : "rdi", "rsi", "rdx", "r10", "rcx", "r11"
    );

    /* The kernel maps each window buffer at a predictable user-space address. */
    uint32_t* buf = (uint32_t*)(0xA0000000 + (uint64_t)win_id * 0x1000000);
    uint32_t color = 0xFFFF0000; // Red
    uint32_t frames = 0;

    while(1) {
        for(uint32_t i = 0; i < 300 * 200; i++) buf[i] = color;

        gui_event_t ev;
        int ret;
        __asm__ volatile (
            "mov $2, %%rax\n"
            "mov %1, %%rdi\n"
            "mov %2, %%rsi\n"
            "syscall\n"
            : "=a"(ret)
            : "r"((uint64_t)win_id), "r"((uint64_t)&ev)
            : "rdi", "rsi", "rcx", "r11"
        );

        if (ret == 1 && ev.type == GUI_EVENT_MOUSE_CLICK) {
            color = 0xFFFFFF00; // Yellow
        }

        if (++frames > 2000) {
            __asm__ volatile (
                "mov $3, %%rax\n"
                "mov $0, %%rdi\n"
                "syscall\n"
                :
                :
                : "rax", "rdi", "rcx", "r11"
            );
        }

        for(volatile int i=0; i<100000; i++);
    }
}
