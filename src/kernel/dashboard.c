#include "window.h"
#include "graphics.h"
#include "syscall.h"
#include "kmalloc.h"
#include "task.h"
#include "string.h"
#include <stddef.h>

static void dashboard_itoa(uint64_t n, char* s) {
    int i = 0;
    if (n == 0) {
        s[i++] = '0';
    } else {
        while (n > 0) {
            s[i++] = (char)((n % 10) + '0');
            n /= 10;
        }
    }
    s[i] = '\0';
    // reverse
    for (int j = 0; j < i / 2; j++) {
        char c = s[j];
        s[j] = s[i - j - 1];
        s[i - j - 1] = c;
    }
}

void dashboard_run() {
    window_t* win = window_create(700, 50, 250, 400, 0xFF222222);
    uint32_t client_h = win->h - 25;
    win->buffer = kmalloc(win->w * client_h * 4);
    memset(win->buffer, 0, win->w * client_h * 4);

    while(1) {
        sys_info_t info;
        sys_get_sys_info(&info);

        // Clear client area
        graphics_fill_rect_buffer(win->buffer, win->w, 0, 0, win->w, client_h, 0xFF222222);

        graphics_draw_string(win->buffer, win->w, 10, 10, "SYSTEM MONITOR", 0xFFFFFFFF);
        
        // CPU stats
        for (uint32_t i = 0; i < info.cpu_count && i < 8; i++) {
            char label[16] = "CPU  : ";
            label[4] = '0' + i;
            graphics_draw_string(win->buffer, win->w, 10, 40 + i * 30, label, 0xFFFFFFFF);

            // Draw bar background
            graphics_fill_rect_buffer(win->buffer, win->w, 60, 40 + i * 30, 150, 10, 0xFF444444);
            
            if (info.cpus[i].total_ticks > 0) {
                uint64_t busy = info.cpus[i].total_ticks - info.cpus[i].idle_ticks;
                uint32_t bar_w = (uint32_t)((busy * 150) / info.cpus[i].total_ticks);
                if (bar_w > 150) bar_w = 150;
                graphics_fill_rect_buffer(win->buffer, win->w, 60, 40 + i * 30, bar_w, 10, 0xFF00FF00);
            }
        }

        // Memory stats
        graphics_draw_string(win->buffer, win->w, 10, 300, "MEM:", 0xFFFFFFFF);
        graphics_fill_rect_buffer(win->buffer, win->w, 60, 300, 150, 10, 0xFF444444);
        if (info.total_frames > 0) {
            uint64_t used = info.total_frames - info.free_frames;
            uint32_t bar_w = (uint32_t)((used * 150) / info.total_frames);
            if (bar_w > 150) bar_w = 150;
            graphics_fill_rect_buffer(win->buffer, win->w, 60, 300, bar_w, 10, 0xFF00AAFF);
        }

        char task_str[32] = "TASKS: ";
        dashboard_itoa(info.task_count, task_str + 7);
        graphics_draw_string(win->buffer, win->w, 10, 330, task_str, 0xFFFFFFFF);

        window_mark_dirty(win);
        
        // Sleep a bit (approximate)
        for (volatile int i = 0; i < 5000000; i++);
        task_yield();
    }
}
