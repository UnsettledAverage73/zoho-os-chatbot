#include "vga.h"

static const size_t NUM_COLS = 80;
static const size_t NUM_ROWS = 25;

struct vga_char {
    uint8_t character;
    uint8_t color;
};

static struct vga_char* const VGA_BUFFER = (struct vga_char*) 0xb8000;

static size_t col = 0;
static size_t row = 0;
static uint8_t color = 0x07; // Light gray on black

void vga_clear() {
    for (size_t i = 0; i < NUM_COLS * NUM_ROWS; i++) {
        VGA_BUFFER[i] = (struct vga_char) {
            .character = ' ',
            .color = color,
        };
    }
    col = 0;
    row = 0;
}

static void print_newline() {
    col = 0;
    if (row < NUM_ROWS - 1) {
        row++;
    } else {
        // Scroll (minimal)
        for (size_t r = 1; r < NUM_ROWS; r++) {
            for (size_t c = 0; c < NUM_COLS; c++) {
                VGA_BUFFER[(r-1) * NUM_COLS + c] = VGA_BUFFER[r * NUM_COLS + c];
            }
        }
        for (size_t c = 0; c < NUM_COLS; c++) {
            VGA_BUFFER[(NUM_ROWS-1) * NUM_COLS + c] = (struct vga_char) {
                .character = ' ',
                .color = color,
            };
        }
    }
}

static void print_char(char c) {
    if (c == '\n') {
        print_newline();
        return;
    }

    if (col >= NUM_COLS) {
        print_newline();
    }

    VGA_BUFFER[row * NUM_COLS + col] = (struct vga_char) {
        .character = (uint8_t) c,
        .color = color,
    };
    col++;
}

void vga_print_char(char c) {
    print_char(c);
}

void vga_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

void vga_print_hex(uint64_t val) {
    char hex_chars[] = "0123456789ABCDEF";
    vga_print("0x");
    for (int i = 60; i >= 0; i -= 4) {
        print_char(hex_chars[(val >> i) & 0xF]);
    }
}
