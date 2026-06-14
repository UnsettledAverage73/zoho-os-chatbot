#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include "graphics.h"

/**
 * @file window.h
 * @brief Window manager and GUI event structures.
 */

#define GUI_EVENT_MOUSE_CLICK 1
#define GUI_EVENT_MOUSE_MOVE  2
#define GUI_EVENT_KEY_PRESS   3

typedef struct {
    uint32_t type;
    uint32_t x, y;
    uint32_t data;
} gui_event_t;

/**
 * Window metadata and backing-buffer state.
 */
typedef struct window {
    uint32_t id;
    uint32_t x, y;
    uint32_t w, h;
    uint32_t title_color;
    uint32_t bg_color;
    uint32_t* buffer;      // Shared buffer (kernel side pointer)
    uint64_t owner_pid;
    uint8_t dirty;
    uint8_t destroyed;
    uint32_t refcount;
    
    gui_event_t event_queue[16];
    uint8_t event_head;
    uint8_t event_tail;

    struct window* next;
} window_t;

#define WINDOW_MAX_DIRTY_RECTS 32

/**
 * Initialize the window manager.
 */
void window_init();

/**
 * Create a new window.
 */
window_t* window_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_color);

/**
 * Find a window by ID.
 */
window_t* window_get_by_id(uint32_t id);

/**
 * Assign a PID owner to a window.
 */
void window_set_owner(window_t* win, uint64_t owner_pid);

/**
 * Attach a framebuffer buffer to a window.
 */
void window_set_buffer(window_t* win, uint32_t* buffer);

/**
 * Pop the next event from a window queue.
 */
int window_pop_event(uint32_t id, gui_event_t* out_event);

/**
 * Destroy all windows owned by a PID.
 */
void window_destroy_by_pid(uint64_t pid);

/**
 * Recompute and redraw the window tree if needed.
 */
void window_update();

/**
 * Draw all windows to the framebuffer.
 */
void window_draw_all();

/**
 * Mark a window as dirty.
 */
void window_mark_dirty(window_t* win);

/**
 * Mark a subregion of a window as dirty.
 */
void window_mark_region_dirty(window_t* win, int32_t x, int32_t y, int32_t w, int32_t h);

/**
 * Report whether any window needs redraw.
 */
int window_needs_redraw();

#endif
