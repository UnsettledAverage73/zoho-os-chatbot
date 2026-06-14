#ifndef APIC_H
#define APIC_H

#include <stdint.h>

/**
 * @file apic.h
 * @brief Local APIC interface.
 */

/**
 * Initialize the local APIC and enable it for the current CPU.
 */
void lapic_init();

/**
 * Read the local APIC ID of the current CPU.
 *
 * @return APIC ID byte.
 */
uint8_t lapic_get_id();

/**
 * Signal end-of-interrupt to the local APIC.
 */
void lapic_eoi();

/**
 * Program the local APIC timer.
 *
 * @param hz Desired timer frequency.
 */
void lapic_timer_init(uint32_t hz);

/**
 * Send an inter-processor interrupt to another APIC.
 *
 * @param apic_id Target APIC ID.
 * @param vector Interrupt vector / command value.
 */
void lapic_send_ipi(uint8_t apic_id, uint32_t vector);

#endif
