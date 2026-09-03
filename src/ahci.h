#ifndef AXIOM_AHCI_H
#define AXIOM_AHCI_H
#include "pci.h"
#include <stdint.h>
uint32_t ahci_attach(const struct pci_device *device);
#endif
