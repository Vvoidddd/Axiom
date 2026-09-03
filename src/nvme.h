#ifndef AXIOM_NVME_H
#define AXIOM_NVME_H
#include "pci.h"
#include <stdint.h>
uint32_t nvme_attach(const struct pci_device *device);
#endif
