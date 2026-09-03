#ifndef AXIOM_SYSTEM_H
#define AXIOM_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <limine.h>

struct system_info {
    char cpu_vendor[13];
    char cpu_brand[49];
    uint64_t memory_bytes;
    uint64_t usable_bytes;
    uint64_t logical_threads;
    uint64_t physical_cores;
    uint32_t threads_per_core;
    uint32_t cpu_family;
    uint32_t cpu_model;
    uint32_t cpu_stepping;
    bool has_apic;
    bool has_x2apic;
    bool has_sse2;
    bool has_avx;
    bool has_long_mode;
};

void system_init(struct limine_memmap_response *memmap, struct limine_mp_response *mp);
const struct system_info *system_get_info(void);
bool system_memory_test(uint64_t *tested_bytes, uint64_t *failure_offset);
bool system_cpu_test(uint64_t *checksum);

#endif

