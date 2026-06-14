#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/**
 * @file serial.h
 * @brief Serial console and input helpers.
 */

/**
 * Initialize COM1 and enable RX interrupts.
 */
void serial_init();

/**
 * Lock all serial output paths.
 */
uint64_t serial_lock_all();

/**
 * Restore serial lock/interrupt state.
 */
void serial_unlock_all(uint64_t flags);

/**
 * Write one character to COM1.
 */
void serial_putc(char c);

/**
 * Print a string with locking.
 */
void serial_print(const char* str);

/**
 * Print a string without taking the serial lock.
 */
void serial_print_no_lock(const char* str);

/**
 * Print a hexadecimal value.
 */
void serial_print_hex(uint64_t val);

/**
 * Print a decimal value.
 */
void serial_print_dec(uint64_t val);

/**
 * Print a decimal value without taking the serial lock.
 */
void serial_print_dec_no_lock(uint64_t val);

/**
 * Serial IRQ handler.
 */
void serial_handler();

/**
 * Poll for serial input bytes.
 */
void serial_poll_input();

#endif
