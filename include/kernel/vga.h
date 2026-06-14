#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

void vga_clear();
void vga_print(const char* str);
void vga_print_char(char c);
void vga_print_hex(uint64_t val);

#endif
