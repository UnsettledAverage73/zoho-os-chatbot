const sidebars = {
  tutorialSidebar: [
    "intro",
    {
      type: "category",
      label: "Core Docs",
      items: [
        "overview",
        "build-and-run",
        "architecture"
      ]
    },
    {
      type: "category",
      label: "Boot Sequence",
      items: [
        "boot/boot-00-overview",
        "boot/boot-01-grub-handoff",
        "boot/boot-02-multiboot-header",
        "boot/boot-03-entry-stack",
        "boot/boot-04-enable-pae",
        "boot/boot-05-page-tables-cr3",
        "boot/boot-06-enable-long-mode",
        "boot/boot-07-enable-paging",
        "boot/boot-08-gdt-far-jump",
        "boot/boot-09-64-bit-entry",
        "boot/boot-10-common-failures",
        "boot/boot-11-chat-notes",
        "boot/boot-12-cpu-checks"
      ]
    },
    {
      type: "category",
      label: "Kernel Systems",
      items: [
        "core/core-00-gdt-tss",
        "core/core-01-idt-interrupts",
        "core/core-02-pmm",
        "core/core-03-vmm",
        "core/core-04-tasking-smp",
        "core/core-05-syscalls-userspace",
        "core/core-06-filesystems-shell",
        "core/core-07-graphics-window",
        "core/core-08-pci-storage",
        "core/core-09-networking",
        "core/core-10-shell-internals"
      ]
    }
  ]
};

module.exports = sidebars;
