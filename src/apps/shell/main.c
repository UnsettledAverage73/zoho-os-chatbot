#include <stdint.h>

/*
 * Tiny ring-3 shell demo.
 *
 * It prints a prompt through the syscall interface and then idles.
 */
void _start() {
    const char* msg = "Zoho OS User Shell v1.0\nzoho> ";
    uint32_t len = 0;
    while (msg[len]) len++;

    /* sys_write(fd=1, buf=msg, len) */
    __asm__ volatile (
        "mov $6, %%rax\n"
        "mov $1, %%rdi\n"
        "mov %0, %%rsi\n"
        "mov %1, %%rdx\n"
        "syscall"
        :
        : "r"(msg), "r"((uint64_t)len)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11"
    );

    while (1) {
        __asm__ volatile ("pause");
    }
}
