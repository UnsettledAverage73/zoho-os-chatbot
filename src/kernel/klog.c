#include "klog.h"
#include "vga.h"
#include "serial.h"
#include "lock.h"

static const char* level_strings[] = {
    "[DEBUG]",
    "[INFO ]",
    "[WARN ]",
    "[ERROR]"
};

static spinlock_t klog_lock;
static int klog_init_done = 0;
static log_level_t klog_min_level = KLOG_MIN_LEVEL;

static void klog_ensure_init() {
    if (!klog_init_done) {
        spin_init(&klog_lock);
        klog_init_done = 1;
    }
}

void klog_set_level(log_level_t level) {
    klog_ensure_init();
    uint64_t flags = spin_lock_irqsave(&klog_lock);
    klog_min_level = level;
    spin_unlock_irqrestore(&klog_lock, flags);
}

log_level_t klog_get_level() {
    return klog_min_level;
}

void klog(log_level_t level, const char* module, const char* fmt, ...) {
    if (level < klog_min_level) return;

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    klog_ensure_init();

    uint64_t flags = spin_lock_irqsave(&klog_lock);

    // Print to VGA
    vga_print(level_strings[level]);
    vga_print(" ");
    vga_print(module);
    vga_print(": ");
    vga_print(buf);
    vga_print("\n");

    // Print to Serial
    serial_print(level_strings[level]);
    serial_print(" ");
    serial_print(module);
    serial_print(": ");
    serial_print(buf);
    serial_print("\n");

    spin_unlock_irqrestore(&klog_lock, flags);
}

