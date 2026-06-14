#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/**
 * @file pit.h
 * @brief Programmable Interval Timer and TSC timing helpers.
 */

/**
 * Program the PIT to a fixed frequency.
 */
void pit_init(uint32_t frequency);

/**
 * Get the current PIT tick count.
 */
uint64_t pit_get_ticks();

/**
 * Busy-wait for a number of milliseconds.
 */
void delay_ms(uint32_t ms);

/**
 * Busy-wait for a number of microseconds.
 */
void delay_us(uint32_t us);

/**
 * Calibrate the TSC against PIT ticks.
 */
void tsc_calibrate();

/**
 * Read the timestamp counter.
 */
uint64_t rdtsc();

#endif
