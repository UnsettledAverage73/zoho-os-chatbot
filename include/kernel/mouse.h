#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

/**
 * @file mouse.h
 * @brief PS/2 mouse input helpers.
 */

/**
 * Initialize the PS/2 mouse device.
 */
void mouse_init();

/**
 * Mouse interrupt handler.
 */
void mouse_handler();

/**
 * Get the current cursor X coordinate.
 */
int32_t mouse_get_x();

/**
 * Get the current cursor Y coordinate.
 */
int32_t mouse_get_y();

/**
 * Get the current mouse button state.
 */
uint8_t mouse_get_buttons();

#endif
