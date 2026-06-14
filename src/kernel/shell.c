#include "shell.h"
#include "vga.h"
#include "serial.h"
#include "pmm.h"
#include "pit.h"
#include "vfs.h"
#include "task.h"
#include "kmalloc.h"
#include "ata.h"
#include "string.h"
#include "graphics.h"
#include <stddef.h>

#include "net.h"
#include "e1000.h"

#include "tcp.h"
#include "syscall.h"

/*
 * Interactive shell implementation.
 *
 * The shell renders text into the terminal window, routes commands to the
 * kernel subsystems, and mirrors output to VGA/serial for debugging.
 */

#define MAX_BUFFER 256
#define SHELL_CHAR_W 8
#define SHELL_CHAR_H 8
#define SHELL_BG_COLOR 0x00000000
#define SHELL_FG_COLOR 0xFFFFFFFF

static char input_buffer[MAX_BUFFER];
static int buffer_idx = 0;

static window_t* shell_window = NULL;
static uint32_t term_row = 0;
static uint32_t term_col = 0;
static uint32_t term_cols = 0;
static uint32_t term_rows = 0;
static char* term_cells = NULL;

static void shell_putchar(char c);

static void shell_print_hex(uint64_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        if (nibble > 0 || started || i == 0) {
            shell_putchar(hex_chars[nibble]);
            started = 1;
        }
    }
}

static void shell_print_dec(uint64_t value) {
    char buf[21];
    int idx = 0;

    if (value == 0) {
        shell_putchar('0');
        return;
    }

    while (value > 0 && idx < (int)sizeof(buf)) {
        buf[idx++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (idx > 0) {
        shell_putchar(buf[--idx]);
    }
}

static void shell_clear_cell_pixels(uint32_t row, uint32_t col) {
    if (!shell_window || !shell_window->buffer) return;

    uint32_t px = col * SHELL_CHAR_W;
    uint32_t py = row * SHELL_CHAR_H;
    for (uint32_t y = 0; y < SHELL_CHAR_H; y++) {
        uint32_t* dst = shell_window->buffer + (uint64_t)(py + y) * shell_window->w + px;
        for (uint32_t x = 0; x < SHELL_CHAR_W; x++) {
            dst[x] = SHELL_BG_COLOR;
        }
    }
}

static void shell_render_cell(uint32_t row, uint32_t col) {
    if (!shell_window || !shell_window->buffer || !term_cells) return;
    if (row >= term_rows || col >= term_cols) return;

    uint32_t px = col * SHELL_CHAR_W;
    uint32_t py = row * SHELL_CHAR_H;
    shell_clear_cell_pixels(row, col);
    graphics_draw_char(shell_window->buffer, shell_window->w, px, py, term_cells[row * term_cols + col], SHELL_FG_COLOR);
    window_mark_region_dirty(shell_window, (int32_t)px, (int32_t)py, SHELL_CHAR_W, SHELL_CHAR_H);
}

static void shell_render_all() {
    if (!shell_window || !shell_window->buffer || !term_cells) return;

    memset(shell_window->buffer, 0, shell_window->w * (shell_window->h - 25) * sizeof(uint32_t));
    for (uint32_t row = 0; row < term_rows; row++) {
        for (uint32_t col = 0; col < term_cols; col++) {
            char c = term_cells[row * term_cols + col];
            if (c == '\0' || c == ' ') continue;
            graphics_draw_char(shell_window->buffer,
                               shell_window->w,
                               col * SHELL_CHAR_W,
                               row * SHELL_CHAR_H,
                               c,
                               SHELL_FG_COLOR);
        }
    }
    window_mark_region_dirty(shell_window, 0, 0, (int32_t)shell_window->w, (int32_t)(shell_window->h - 25));
}

static void shell_scroll_up() {
    if (!term_cells || term_rows == 0 || !shell_window || !shell_window->buffer) return;

    memmove(term_cells, term_cells + term_cols, (term_rows - 1) * term_cols);
    memset(term_cells + (term_rows - 1) * term_cols, ' ', term_cols);

    uint32_t client_h = shell_window->h - 25;
    uint32_t scroll_px = SHELL_CHAR_H;
    uint32_t preserved_rows = client_h - scroll_px;

    memmove(shell_window->buffer,
            shell_window->buffer + (uint64_t)scroll_px * shell_window->w,
            (uint64_t)preserved_rows * shell_window->w * sizeof(uint32_t));

    for (uint32_t y = preserved_rows; y < client_h; y++) {
        uint32_t* dst = shell_window->buffer + (uint64_t)y * shell_window->w;
        for (uint32_t x = 0; x < shell_window->w; x++) {
            dst[x] = SHELL_BG_COLOR;
        }
    }

    window_mark_region_dirty(shell_window, 0, 0, (int32_t)shell_window->w, (int32_t)client_h);
}

static void shell_advance_line() {
    if (term_rows == 0) return;
    term_col = 0;
    term_row++;
    if (term_row >= term_rows) {
        term_row = term_rows - 1;
        shell_scroll_up();
    }
}

void shell_set_window(window_t* win) {
    shell_window = win;
    term_row = 0;
    term_col = 0;

    if (!shell_window) return;

    term_cols = shell_window->w / SHELL_CHAR_W;
    term_rows = (shell_window->h - 25) / SHELL_CHAR_H;

    if (term_cells) {
        kfree(term_cells);
    }
    term_cells = kmalloc(term_rows * term_cols);
    memset(term_cells, ' ', term_rows * term_cols);

    shell_render_all();
}

static void shell_putchar(char c) {
    if (c == '\n') {
        vga_print("\n");
        serial_print("\n");
        shell_advance_line();
        return;
    }

    if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            if (term_cells) {
                term_cells[term_row * term_cols + term_col] = ' ';
                shell_render_cell(term_row, term_col);
            }
        }
        return;
    }

    char s[2] = {c, '\0'};
    vga_print(s);
    serial_print(s);

    if (term_cells) {
        term_cells[term_row * term_cols + term_col] = c;
        shell_render_cell(term_row, term_col);
    }

    term_col++;
    if (term_cols != 0 && term_col >= term_cols) {
        shell_advance_line();
    }
}

static void shell_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        shell_putchar(str[i]);
    }
}

static void shell_print_lock_stats() {
    /* Summarize the lock subsystem statistics. */
    uint32_t count = lock_get_registered_count();
    lock_stats_t stats;

    for (uint32_t i = 0; i < count; i++) {
        if (!lock_get_stats(i, &stats)) continue;

        shell_print(stats.name ? stats.name : "lock");
        shell_print(": acq=");
        shell_print_dec(stats.acquisitions);
        shell_print(" cont=");
        shell_print_dec(stats.contentions);
        shell_print(" spins=");
        shell_print_dec(stats.spin_loops);
        shell_print("\n");
    }
}

static void shell_execute(char* cmd) {
    /* Parse and run a single shell command line. */
    if (strcmp(cmd, "help") == 0) {
        shell_print("Available commands: help, ls, cat <file>, write <f> <t>, exec <f>, free, heap, locks, clear, ticks, gui, net, ping <ip>, udp <msg>, tcp <ip> <port>, stress_proc, stress_disk, monitor, top, dashboard\n");
    } else if (strcmp(cmd, "dashboard") == 0) {
        extern void dashboard_run();
        task_create(dashboard_run);
        shell_print("Dashboard started.\n");
    } else if (strcmp(cmd, "monitor") == 0) {
        sys_info_t info;
        sys_get_sys_info(&info);
        
        shell_print("--- System Monitor ---\n");
        shell_print("CPU Cores: "); shell_print_dec(info.cpu_count); shell_print("\n");
        for (uint32_t i = 0; i < info.cpu_count; i++) {
            shell_print(" CPU "); shell_print_dec(i);
            shell_print(": Usage ");
            if (info.cpus[i].total_ticks > 0) {
                uint64_t busy = info.cpus[i].total_ticks - info.cpus[i].idle_ticks;
                uint64_t usage = (busy * 100) / info.cpus[i].total_ticks;
                shell_print_dec(usage);
                shell_print("%");
            } else {
                shell_print("0%");
            }
            shell_print("\n");
        }
        
        uint64_t total_mem = info.total_frames * 4096 / 1024;
        uint64_t free_mem = info.free_frames * 4096 / 1024;
        shell_print("Memory: "); shell_print_dec((total_mem - free_mem) / 1024);
        shell_print("MB / "); shell_print_dec(total_mem / 1024); shell_print("MB used\n");
        shell_print("Tasks: "); shell_print_dec(info.task_count); shell_print("\n");
    } else if (strcmp(cmd, "top") == 0) {
        task_info_t tasks[32];
        int count = sys_get_tasks(tasks, 32);
        
        shell_print("ID  CPU  TICKS  STATE\n");
        for (int i = 0; i < count; i++) {
            shell_print_dec(tasks[i].id); shell_print("   ");
            shell_print_dec(tasks[i].cpu_id); shell_print("    ");
            shell_print_dec(tasks[i].total_ticks); shell_print("    ");
            switch(tasks[i].state) {
                case 0: shell_print("READY"); break;
                case 1: shell_print("RUNNING"); break;
                case 2: shell_print("EXITED"); break;
                default: shell_print("UNKNOWN"); break;
            }
            shell_print("\n");
        }
    } else if (strcmp(cmd, "fork") == 0) {
        shell_print("Forking...\n");
        uint64_t pid = sys_fork();
        if (pid == 0) {
            // Child
            serial_print("Child process started\n");
            for (volatile int i = 0; i < 10000000; i++);
            serial_print("Child process exiting\n");
            sys_exit(0);
        } else {
            // Parent
            shell_print("Parent: Child PID is ");
            shell_print_dec(pid);
            shell_print("\n");
        }
    } else if (memcmp(cmd, "tcp ", 4) == 0) {
        char* rest = cmd + 4;
        uint8_t ip[4] = {0, 0, 0, 0};
        int part = 0;
        char* p = rest;
        while (*p && part < 4) {
            if (*p >= '0' && *p <= '9') {
                ip[part] = ip[part] * 10 + (*p - '0');
            } else if (*p == '.') {
                part++;
            } else if (*p == ' ') {
                p++;
                break;
            }
            p++;
        }
        uint16_t port = 0;
        while (*p && *p >= '0' && *p <= '9') {
            port = port * 10 + (*p - '0');
            p++;
        }
        if (part == 3 && port > 0) {
            tcp_connect(ip, port);
        } else {
            shell_print("Usage: tcp <ip> <port>\n");
        }
    } else if (memcmp(cmd, "udp ", 4) == 0) {
        char* msg = cmd + 4;
        uint8_t bcast[4] = {255, 255, 255, 255};
        net_send_udp(bcast, 12345, 12345, msg, strlen(msg));
        shell_print("UDP broadcast sent.\n");
    } else if (memcmp(cmd, "ping ", 5) == 0) {
        char* ip_str = cmd + 5;
        uint8_t ip[4] = {0, 0, 0, 0};
        int part = 0;
        char* p = ip_str;
        while (*p && part < 4) {
            if (*p >= '0' && *p <= '9') {
                ip[part] = ip[part] * 10 + (*p - '0');
            } else if (*p == '.') {
                part++;
            }
            p++;
        }
        if (part == 3) {
            net_ping(ip);
        } else {
            shell_print("Invalid IP address.\n");
        }
    } else if (strcmp(cmd, "net") == 0) {
        uint8_t mac[6];
        uint8_t ip[4];
        e1000_get_mac(mac);
        net_get_ip(ip);
        shell_print("E1000 MAC: ");
        for (int i = 0; i < 6; i++) {
            shell_print_hex(mac[i]);
            if (i < 5) shell_putchar(':');
        }
        shell_print("\nIP Address: ");
        for (int i = 0; i < 4; i++) {
            shell_print_dec(ip[i]);
            if (i < 3) shell_putchar('.');
        }
        shell_print("\nRX Packets: ");
        shell_print_dec(net_get_rx_count());
        shell_print("\nTX Packets: ");
        shell_print_dec(net_get_tx_count());
        shell_print("\n");
    } else if (strcmp(cmd, "ls") == 0) {
        char name[100];
        int i = 0;
        while (vfs_readdir(i++, name) == 0) {
            shell_print(name);
            shell_print("\n");
        }
    } else if (memcmp(cmd, "cat ", 4) == 0) {
        char* filename = cmd + 4;
        int fd = vfs_open(filename);
        if (fd < 0) {
            shell_print("File not found.\n");
        } else {
            uint32_t sz = vfs_size(fd);
            char* buf = (char*)kmalloc(sz + 1);
            vfs_read(fd, buf, sz);
            buf[sz] = '\0';
            shell_print(buf);
            shell_print("\n");
            kfree(buf);
            vfs_close(fd);
        }
    } else if (memcmp(cmd, "exec ", 5) == 0) {
        char* filename = cmd + 5;
        if (task_exec_file(filename)) {
            shell_print("Process started.\n");
        } else {
            shell_print("Failed to start process.\n");
        }
    } else if (strcmp(cmd, "gui") == 0) {
        extern void user_gui_app();
        task_create_user(user_gui_app);
        shell_print("GUI Process started.\n");
    } else if (memcmp(cmd, "write ", 6) == 0) {
        char* rest = cmd + 6;
        char* filename = rest;
        char* data = NULL;
        for (int i = 0; rest[i] != '\0'; i++) {
            if (rest[i] == ' ') {
                rest[i] = '\0';
                data = rest + i + 1;
                break;
            }
        }
        if (data) {
            int fd = vfs_open(filename);
            if (fd < 0) {
                shell_print("File not found.\n");
            } else {
                vfs_write(fd, data, strlen(data));
                vfs_close(fd);
                shell_print("File written.\n");
            }
        } else {
            shell_print("Usage: write <file> <text>\n");
        }
    } else if (strcmp(cmd, "free") == 0) {
        shell_print("Free frames: ");
        serial_print_hex(pmm_get_free_count());
        shell_print("\n");
    } else if (strcmp(cmd, "heap") == 0) {
        kmalloc_stats_t stats;
        kmalloc_get_stats(&stats);

        shell_print("Heap used bytes: ");
        shell_print_dec(stats.bytes_used);
        shell_print("\n");
        shell_print("Heap peak bytes: ");
        shell_print_dec(stats.peak_bytes_used);
        shell_print("\n");
        shell_print("Heap free bytes: ");
        shell_print_dec(stats.bytes_free);
        shell_print("\n");
        shell_print("Largest free block: ");
        shell_print_dec(stats.largest_free_block);
        shell_print("\n");
        shell_print("Live allocations: ");
        shell_print_dec(stats.live_allocations);
        shell_print("\n");
        shell_print("Alloc calls: ");
        shell_print_dec(stats.alloc_count);
        shell_print("\n");
        shell_print("Free calls: ");
        shell_print_dec(stats.free_count);
        shell_print("\n");
        shell_print("Failed allocs: ");
        shell_print_dec(stats.failed_allocations);
        shell_print("\n");
    } else if (strcmp(cmd, "locks") == 0) {
        shell_print_lock_stats();
    } else if (strcmp(cmd, "stress_proc") == 0) {
        shell_print("Starting Process Storm (50 cycles)...\n");
        for (int i = 0; i < 50; i++) {
            extern void user_gui_app();
            task_create_user(user_gui_app);
            for (volatile int j = 0; j < 5000000; j++);
        }
        shell_print("Stress test complete.\n");
    } else if (strcmp(cmd, "stress_disk") == 0) {
        shell_print("Starting Disk Hammer...\n");
        uint8_t buf[512];
        for (int i = 0; i < 512; i++) {
            memset(buf, (uint8_t)i, 512);
            ata_write_sector(5000 + i, buf);
            memset(buf, 0, 512);
            ata_read_sector(5000 + i, buf);
            if (buf[0] != (uint8_t)i) {
                shell_print("DISK CORRUPTION DETECTED!\n");
                break;
            }
        }
        shell_print("Disk stress test complete.\n");
    } else if (strcmp(cmd, "ticks") == 0) {
        shell_print("System Ticks: ");
        serial_print_hex(pit_get_ticks());
        shell_print("\n");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
        if (term_cells) {
            memset(term_cells, ' ', term_rows * term_cols);
            shell_render_all();
        }
        term_row = 0;
        term_col = 0;
    } else if (cmd[0] != '\0') {
        if (task_exec_file(cmd)) {
             shell_print("Process started.\n");
        } else {
            shell_print("Unknown command or file: ");
            shell_print(cmd);
            shell_print("\n");
        }
    }
    shell_print("zoho> ");
}

void shell_init() {
    /* Print the initial banner and prompt. */
    shell_print("\nZoho OS Interactive Shell\n");
    shell_print("zoho> ");
}

void shell_input(char c) {
    /* Update the input buffer and execute on Enter. */
    if (c == '\n' || c == '\r') {
        shell_putchar('\n');
        input_buffer[buffer_idx] = '\0';
        if (buffer_idx > 0) {
            shell_execute(input_buffer);
        } else {
            shell_print("zoho> ");
        }
        buffer_idx = 0;
    } else if (c == '\b') {
        if (buffer_idx > 0) {
            buffer_idx--;
            shell_putchar('\b');
        }
    } else {
        if (buffer_idx < MAX_BUFFER - 1) {
            input_buffer[buffer_idx++] = c;
            shell_putchar(c);
        }
    }
}
