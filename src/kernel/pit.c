#include "pit.h"
#include "klog.h"
#include "io.h"
#include "task.h"

static uint64_t ticks = 0;

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;

    /* Program channel 0 in rate-generator mode. */
    outb(0x43, 0x36);

    /* The divisor is written as two bytes. */
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

    /* Load the frequency divisor. */
    outb(0x40, l);
    outb(0x40, h);

    /* Unmask IRQ0. */
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~1);

    klog(LOG_INFO, "PIT", "Timer initialized at 100Hz");
}

void pit_handler() {
    /* Update the global tick counter and task accounting. */
    ticks++;
    task_timer_tick();
}

uint64_t pit_get_ticks() {
    return ticks;
}

static uint64_t tsc_freq_mhz = 0;

uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void tsc_calibrate() {
    uint64_t start_ticks = ticks;
    /* Wait for a tick boundary before measuring. */
    while (ticks == start_ticks);
    
    uint64_t start_tsc = rdtsc();
    start_ticks = ticks;
    /* Measure over one PIT tick. */
    while (ticks == start_ticks);
    uint64_t end_tsc = rdtsc();
    
    /* Convert 10ms cycle count to MHz. */
    tsc_freq_mhz = (end_tsc - start_tsc) / 10000;
    if (tsc_freq_mhz == 0) tsc_freq_mhz = 2000; // Fallback to 2GHz
}

void delay_us(uint32_t us) {
    if (tsc_freq_mhz == 0) {
        /* Fallback path before TSC calibration. */
        for (volatile uint32_t i = 0; i < us * 1000; i++) {
            __asm__ volatile ("pause");
        }
        return;
    }
    uint64_t start = rdtsc();
    uint64_t target = start + (uint64_t)us * tsc_freq_mhz;
    while (rdtsc() < target) {
        __asm__ volatile ("pause");
    }
}

void delay_ms(uint32_t ms) {
    delay_us(ms * 1000);
}
