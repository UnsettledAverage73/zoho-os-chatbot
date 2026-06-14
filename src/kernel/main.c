#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "shell.h"
#include "klog.h"
#include "pit.h"
#include "kmalloc.h"
#include "task.h"
#include "syscall.h"
#include "ata.h"
#include "ext2.h"
#include "vfs.h"
#include "tar.h"
#include "acpi.h"
#include "apic.h"
#include "smp.h"
#include "cpu.h"
#include "graphics.h"
#include "multiboot2.h"
#include "mouse.h"
#include "keyboard.h"
#include "window.h"
#include "string.h"
#include "pci.h"
#include "net.h"
#include "e1000.h"
#include "dhcp.h"
#include "tcp.h"
#include "ktrace.h"
#include "kstats.h"
#include "tty.h"
#include "xhci.h"

/*
 * Kernel main entry point.
 *
 * This function runs after the bootstrap code has already entered long mode,
 * loaded the temporary page tables, and transferred Multiboot2 information
 * through the boot registers.
 */
void kmain(unsigned long magic, unsigned long addr) {
    (void)magic;
    struct multiboot_info* mb_info = (struct multiboot_info*)addr;

    /* Bring up logging and the earliest console output first. */
    klog_set_level(LOG_DEBUG);
    cpu_early_init();
    serial_init();
    vga_clear();
    tty_init();
    klog(LOG_INFO, "KERNEL", "Zoho OS Booting...");
    
    pmm_init(mb_info);
    vmm_init();
    kmalloc_init();

    /* Debugging and tracing are safe after the basic allocators exist. */
    kstats_init();
    ktrace_init();

    /* ACPI gives us the MADT and LAPIC base address. */
    acpi_init();

    // Map LAPIC area
    uint32_t lapic_phys = acpi_get_lapic_addr();
    if (lapic_phys == 0) lapic_phys = 0xFEE00000; // Fallback
    
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    vmm_map((void*)cr3, lapic_phys, lapic_phys, PAGE_WRITABLE | (1 << 3) | (1 << 4)); 
    // (1<<3) = PWT, (1<<4) = PCD for MMIO

    /* Core CPU setup: GDT, IDT, APIC, and timers. */
    gdt_init();
    idt_init_global();
    idt_init_per_cpu();

    lapic_init();
    pit_init(100);
    lapic_timer_init(100);

    __asm__ volatile ("sti");
    tsc_calibrate();

    task_init_global();
    task_init_per_cpu();
    syscall_init();
    
    /* Device discovery and I/O subsystems. */
    pci_init();
    xhci_init();
    smp_init();

    if (ata_init() == 0) {
        ext2_init();
    }

    vfs_init();
    tar_init_disk(2048);

    /* Pull framebuffer details from Multiboot2 and initialize graphics. */
    struct multiboot_tag* tag;
    for (tag = (struct multiboot_tag*)(mb_info->tags);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            graphics_init((struct multiboot_tag_framebuffer*)tag);
        }
    }

    mouse_init();
    keyboard_init();
    window_init();

    // Create a terminal window for the shell
    /* Create a terminal window for the shell UI. */
    window_t* shell_win = window_create(50, 50, 600, 400, 0xFF333333); 
    shell_win->buffer = kmalloc(600 * 375 * 4); // 400 - 25 = 375
    memset(shell_win->buffer, 0, 600 * 375 * 4);
    shell_set_window(shell_win);
    net_init();
    dhcp_init();
    tcp_init();
    shell_init();
    
    /* Kick off DHCP discovery once the network stack is ready. */
    dhcp_discover();
    
    klog(LOG_INFO, "KERNEL", "System stabilized on LAPIC Timer. Starting scheduler...");

    /* Main event loop: poll devices and redraw the desktop. */
    while(1) {
        serial_poll_input();
        e1000_poll();
        window_update();
        window_draw_all();
    }
}
