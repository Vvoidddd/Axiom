#ifndef AXIOM_BLOCK_H
#define AXIOM_BLOCK_H
#include <stdint.h>
#include <stdbool.h>
#define BLOCK_MAX_DEVICES 16
#define BLOCK_MAX_PARTITIONS 32
struct block_device;
typedef int(*block_io_fn)(struct block_device*,uint64_t,uint32_t,void*);
struct block_device{char name[16],model[40];uint64_t sectors;uint32_t sector_size;bool readonly,removable,present;block_io_fn read,write;void*driver;};
struct block_partition{char name[20];struct block_device*device;uint64_t first_lba,sectors;uint8_t type;bool gpt;};
void block_init(void);
int block_register(const struct block_device *device);
uint32_t block_device_count(void);
struct block_device *block_device_at(uint32_t index);
struct block_device *block_find(const char *name);
int block_read(struct block_device *device,uint64_t lba,uint32_t count,void *buffer);
int block_write(struct block_device *device,uint64_t lba,uint32_t count,const void *buffer);
int block_flush(struct block_device *device);
void block_scan_partitions(void);
uint32_t block_partition_count(void);
const struct block_partition *block_partition_at(uint32_t index);
bool storage_init(void);
void storage_probe_hardware(void);
uint32_t storage_ahci_controllers(void);
uint32_t storage_nvme_controllers(void);
bool storage_self_test(void);
bool storage_hardware_self_test(uint32_t *tested);
#endif
