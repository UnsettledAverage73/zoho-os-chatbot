#ifndef TTY_H
#define TTY_H

#include <stdint.h>

void tty_init(void);
void tty_write(const char* data, uint64_t size);
void tty_putchar(char c);

#endif
