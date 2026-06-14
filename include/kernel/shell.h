#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "window.h"

/**
 * @file shell.h
 * @brief Interactive terminal shell entrypoints.
 */

/**
 * Initialize the shell UI and prompt.
 */
void shell_init();

/**
 * Feed one character into the shell input stream.
 */
void shell_input(char c);

/**
 * Attach the shell to a window buffer.
 */
void shell_set_window(window_t* win);

#endif
