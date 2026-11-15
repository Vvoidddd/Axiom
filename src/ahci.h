#ifndef AXIOM_AHCI_H
#define AXIOM_AHCI_H
#include "pci.h"
#include <stdint.h>
uint32_t ahci_attach(const struct pci_device *device);
struct block_device;
bool ahci_enable_checkpoint_writes(struct block_device *device);
#endif
