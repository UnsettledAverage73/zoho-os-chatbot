#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/**
 * @file keyboard.h
 * @brief PS/2 keyboard input helpers.
 */

/**
 * Initialize the keyboard controller.
 */
void keyboard_init();

/**
 * Keyboard interrupt handler.
 */
void keyboard_handler();

#endif
