#ifndef AXIOM_PCI_H
#define AXIOM_PCI_H
#include <stdint.h>
#include <stdbool.h>
#define PCI_MAX_DEVICES 256
struct pci_device {uint8_t bus,slot,function,class_code,subclass,prog_if,revision,irq;uint16_t vendor_id,device_id;uint32_t bar[6];};
void pci_init(void);
uint32_t pci_device_count(void);
uint32_t pci_storage_count(void);
const struct pci_device *pci_device_at(uint32_t index);
uint32_t pci_config_read32(const struct pci_device *device,uint8_t offset);
void pci_config_write32(const struct pci_device *device,uint8_t offset,uint32_t value);
uint64_t pci_bar_address(const struct pci_device *device,unsigned bar);
void pci_enable_bus_master(const struct pci_device *device);
void pci_list_devices(bool storage_only);
#endif
