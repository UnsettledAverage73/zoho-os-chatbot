#include "xhci.h"
#include "klog.h"
#include "pci.h"

void xhci_init(void) {
    klog(LOG_INFO, "XHCI", "Initializing XHCI (USB 3.0) controller...");
    
    /* This is a stub for future PCI discovery and controller bring-up. */
    
    klog(LOG_INFO, "XHCI", "XHCI subsystem initialized (stub).");
}
