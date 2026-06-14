#ifndef SMP_H
#define SMP_H

/**
 * @file smp.h
 * @brief Symmetric multiprocessing bring-up helpers.
 */

/**
 * Bring secondary CPUs online using ACPI and APIC startup IPIs.
 */
void smp_init();

#endif
