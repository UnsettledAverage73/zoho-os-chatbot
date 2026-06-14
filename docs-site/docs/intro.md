---
id: intro
title: Zoho OS Documentation
slug: /
---

# Zoho OS Documentation

Zoho OS is a teaching-oriented x86_64 kernel. This site explains the project in a simple order:

1. what the system is
2. how it is built
3. how it boots from GRUB into 64-bit mode
4. how the kernel initializes memory, drivers, tasks, and user space

<div className="landingHero">
  <div className="landingHero__grid">
    <div>
      <div className="eyebrow">Documentation home</div>
      <h2>Start with the architecture, then follow the boot path.</h2>
      <p className="landingLead">
        The documentation is organized to match the codebase. The top-level pages explain the project
        at a high level, and the boot sequence pages walk through the exact assembly and C handoff that
        brings the kernel up.
      </p>

      <div className="landingActions">
        <a className="landingButton landingButton--primary" href="/overview">
          Read the overview
        </a>
        <a className="landingButton" href="/boot/00-overview">
          Start boot sequence
        </a>
        <a className="landingButton" href="/build-and-run">
          Build and run
        </a>
      </div>
    </div>

    <div className="statusPanel">
      <div className="statusPanel__title">What the site covers</div>
      <div className="statusRow">
        <span>System type</span>
        <strong>x86_64 research kernel</strong>
      </div>
      <div className="statusRow">
        <span>Boot chain</span>
        <strong>BIOS / UEFI → GRUB → kernel</strong>
      </div>
      <div className="statusRow">
        <span>Primary entry</span>
        <strong><code>src/boot/main.asm</code></strong>
      </div>
      <div className="statusRow">
        <span>Kernel entry</span>
        <strong><code>src/kernel/core/main.c</code></strong>
      </div>
    </div>
  </div>
</div>

<div className="quickGrid">
  <div className="docCard">
    <div className="docCard__kicker">Overview</div>
    <h3>What the project contains</h3>
    <p>
      A readable summary of the kernel, its boot flow, memory managers, drivers, scheduler, shell, and
      user-space entry points.
    </p>
    <p>
      <a href="/overview">Open the overview</a>
    </p>
  </div>

  <div className="docCard">
    <div className="docCard__kicker">Build</div>
    <h3>How to build and boot it</h3>
    <p>
      The exact prerequisites and commands needed to produce the ISO, generate the boot media, and run
      it in QEMU.
    </p>
    <p>
      <a href="/build-and-run">Open build guide</a>
    </p>
  </div>

  <div className="docCard">
    <div className="docCard__kicker">Boot</div>
    <h3>GRUB to 64-bit kernel</h3>
    <p>
      A step-by-step explanation of the Multiboot2 header, paging setup, long mode transition, and
      the first `kmain()` call.
    </p>
    <p>
      <a href="/boot/00-overview">Open boot sequence</a>
    </p>
  </div>
</div>

## Reading order

If you want the shortest path through the docs, read them in this order:

1. [Project overview](/overview)
2. [Build and run](/build-and-run)
3. [Boot sequence overview](/boot/00-overview)
4. [64-bit entry and `kmain`](/boot/09-64-bit-entry)
5. [CPU feature checks](/boot/12-cpu-checks)
6. [Architecture guide](/architecture)
7. [GDT and TSS](/core/00-gdt-tss)
8. [IDT and interrupts](/core/01-idt-interrupts)
9. [PMM and VMM](/core/02-pmm)
10. [Tasking and SMP](/core/04-tasking-smp)
11. [Syscalls and user space](/core/05-syscalls-userspace)
12. [Filesystems and shell](/core/06-filesystems-shell)
13. [Graphics and window manager](/core/07-graphics-window)
14. [PCI and storage](/core/08-pci-storage)
15. [Networking](/core/09-networking)
16. [Shell internals](/core/10-shell-internals)

## Source map

<table className="summaryTable">
  <thead>
    <tr>
      <th>Area</th>
      <th>Primary files</th>
      <th>What they do</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>Boot</td>
      <td><code>src/boot/*.asm</code></td>
      <td>GRUB handoff, stack setup, paging, long mode, and entry into C.</td>
    </tr>
    <tr>
      <td>Kernel core</td>
      <td><code>src/kernel/core/*.c</code></td>
      <td>Initialization, logging, tasking, syscalls, and shell launch.</td>
    </tr>
    <tr>
      <td>Memory</td>
      <td><code>src/kernel/mm/*.c</code></td>
      <td>Physical frames, virtual mappings, and heap setup.</td>
    </tr>
    <tr>
      <td>Drivers</td>
      <td><code>src/kernel/drivers/*.c</code></td>
      <td>Serial, VGA, keyboard, mouse, PCI, ATA, USB, and network devices.</td>
    </tr>
    <tr>
      <td>User space</td>
      <td><code>src/apps/shell/main.c</code></td>
      <td>Interactive shell that calls kernel syscalls.</td>
    </tr>
  </tbody>
</table>
