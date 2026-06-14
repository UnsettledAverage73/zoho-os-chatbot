#include "apic.h"
#include "acpi.h"
#include "vmm.h"
#include "klog.h"
#include "io.h"

#define LAPIC_EOI        0x00B0
#define LAPIC_SVR        0x00F0
#define LAPIC_LVT_TMR    0x0320
#define LAPIC_TMRINIT    0x0380
#define LAPIC_TMRCUR     0x0390
#define LAPIC_TMRDIV     0x03E0

static uint64_t lapic_base = 0;

static uint32_t lapic_read(uint32_t reg) {
    /* LAPIC registers are memory-mapped I/O. */
    return *(volatile uint32_t*)(lapic_base + (uint64_t)reg);
}

static void lapic_write(uint32_t reg, uint32_t data) {
    /* Write one LAPIC register through the mapped MMIO window. */
    *(volatile uint32_t*)(lapic_base + (uint64_t)reg) = data;
}

#define MSR_APIC_BASE 0x1B

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

void lapic_init() {
    /* Enable the LAPIC through the APIC base MSR. */
    klog(LOG_INFO, "APIC", "Step 1: MSR");
    uint64_t base_msr = rdmsr(MSR_APIC_BASE);
    wrmsr(MSR_APIC_BASE, base_msr | (1 << 11)); 

    /* Discover the physical LAPIC base from ACPI. */
    klog(LOG_INFO, "APIC", "Step 2: Mapping");
    uint32_t phys_addr = acpi_get_lapic_addr();
    if (phys_addr == 0) phys_addr = 0xFEE00000;
    lapic_base = (uint64_t)phys_addr;

    /* Enable the spurious interrupt vector and APIC. */
    klog(LOG_INFO, "APIC", "Step 3: SVR");
    lapic_write(LAPIC_SVR, 0x100 | 0xFF);
    
    klog(LOG_INFO, "APIC", "DONE");
}

uint8_t lapic_get_id() {
    return (uint8_t)(lapic_read(0x0020) >> 24);
}

void lapic_eoi() {
    lapic_write(LAPIC_EOI, 0);
}

void lapic_timer_init(uint32_t hz) {
    /* Divide the LAPIC timer clock to a manageable rate. */
    lapic_write(LAPIC_TMRDIV, 0x03); // Divide by 16
    
    /* Use a simple baseline calibration for virtualized environments. */
    uint32_t count = 10000000 / hz; 

    /* Program periodic timer mode on vector 32. */
    lapic_write(LAPIC_LVT_TMR, 32 | (1 << 17)); // Vector 32, Periodic mode
    lapic_write(LAPIC_TMRINIT, count);
}

void lapic_send_ipi(uint8_t apic_id, uint32_t vector) {
    /* Send an IPI by writing the destination and command registers. */
    *(volatile uint32_t*)(lapic_base + 0x310) = (uint32_t)apic_id << 24;
    *(volatile uint32_t*)(lapic_base + 0x300) = vector;
}
