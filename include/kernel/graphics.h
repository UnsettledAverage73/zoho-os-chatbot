#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "multiboot2.h"

/**
 * @file graphics.h
 * @brief Framebuffer graphics and backbuffer blitting.
 */

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

/**
 * Initialize graphics from the Multiboot2 framebuffer tag.
 *
 * @param tag Framebuffer tag returned by GRUB.
 */
void graphics_init(struct multiboot_tag_framebuffer* tag);

/**
 * Copy the full backbuffer to the framebuffer.
 */
void graphics_swap();

/**
 * Copy a list of dirty rectangles to the framebuffer.
 *
 * @param rects Rectangles to copy.
 * @param count Number of rectangles.
 */
void graphics_swap_rects(const rect_t* rects, uint32_t count);

/**
 * Draw one pixel into the backbuffer.
 */
void graphics_put_pixel(uint32_t x, uint32_t y, uint32_t color);

/**
 * Clear the entire backbuffer.
 */
void graphics_clear(uint32_t color);

/**
 * Clear a rectangular region of the backbuffer.
 */
void graphics_clear_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/**
 * Fill a rectangle directly into the backbuffer.
 */
void graphics_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/**
 * Fill a rectangle into an arbitrary buffer.
 */
void graphics_fill_rect_buffer(uint32_t* buf, uint32_t buf_w, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/**
 * Copy a horizontal line from a buffer into the backbuffer.
 */
void graphics_draw_line_buffer(uint32_t x, uint32_t y, uint32_t w, uint32_t* buffer);

/**
 * Draw a cursor shape into the backbuffer.
 */
void graphics_draw_cursor(uint32_t x, uint32_t y);

/**
 * Draw a character into a buffer.
 */
void graphics_draw_char(uint32_t* buf, uint32_t w, uint32_t x, uint32_t y, char c, uint32_t color);

/**
 * Draw a string into a buffer.
 */
void graphics_draw_string(uint32_t* buf, uint32_t w, uint32_t x, uint32_t y, const char* str, uint32_t color);

/**
 * Get the framebuffer width.
 */
uint32_t graphics_get_width();

/**
 * Get the framebuffer height.
 */
uint32_t graphics_get_height();

/**
 * Get the physical framebuffer address.
 */
uint64_t graphics_get_fb_addr();

#endif
