#ifndef AXIOM_MEMORY_H
#define AXIOM_MEMORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limine.h>
struct vmm_space{uint64_t root_physical;};
void memory_manager_init(struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc(void);
void pmm_free(uint64_t physical);
uint64_t pmm_free_pages(void);
uint64_t pmm_used_pages(void);
bool pmm_test_pages(uint64_t pages, uint64_t *tested, uint64_t *failed_page);
bool vmm_map(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
bool vmm_map_user(uint64_t virtual_address, uint64_t physical_address, bool writable, bool executable);
bool vmm_user_range_valid(uint64_t address,size_t bytes,bool writable);
bool vmm_space_create(struct vmm_space *space);
bool vmm_space_map_user(struct vmm_space *space,uint64_t virtual_address,uint64_t physical_address,bool writable,bool executable);
bool vmm_space_user_range_valid(const struct vmm_space *space,uint64_t address,size_t bytes,bool writable);
void *vmm_guarded_stack(size_t usable_pages);
void *vmm_map_mmio(uint64_t physical_address,size_t bytes);
void *kmalloc(size_t size);
void kfree(void *pointer);
uint64_t heap_bytes_used(void);
void *pmm_direct_map(uint64_t physical);
uint64_t pmm_physical_address(const void *virtual_address);
#endif
