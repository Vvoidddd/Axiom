#include "system.h"
#include <stddef.h>

#define TEST_WORDS (256u * 1024u / sizeof(uint64_t))

static struct system_info info;
static volatile uint64_t memory_test_area[TEST_WORDS];

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *a, uint32_t *b,
                  uint32_t *c, uint32_t *d) {
    __asm__ volatile ("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf), "c"(subleaf));
}

static void copy_u32(char *destination, uint32_t value) {
    destination[0] = (char)value;
    destination[1] = (char)(value >> 8);
    destination[2] = (char)(value >> 16);
    destination[3] = (char)(value >> 24);
}

static void clean_brand(char *brand) {
    char *source = brand;
    while (*source == ' ') source++;
    char *destination = brand;
    bool previous_space = false;
    while (*source) {
        if (*source == ' ') {
            if (!previous_space) *destination++ = ' ';
            previous_space = true;
        } else {
            *destination++ = *source;
            previous_space = false;
        }
        source++;
    }
    while (destination > brand && destination[-1] == ' ') destination--;
    *destination = 0;
}

void system_init(struct limine_memmap_response *memmap, struct limine_mp_response *mp) {
    uint32_t a, b, c, d;
    cpuid(0, 0, &a, &b, &c, &d);
    uint32_t max_basic = a;
    copy_u32(&info.cpu_vendor[0], b);
    copy_u32(&info.cpu_vendor[4], d);
    copy_u32(&info.cpu_vendor[8], c);
    info.cpu_vendor[12] = 0;

    cpuid(0x80000000u, 0, &a, &b, &c, &d);
    uint32_t max_extended = a;
    if (max_extended >= 0x80000004u) {
        char *part = info.cpu_brand;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
            cpuid(leaf, 0, &a, &b, &c, &d);
            copy_u32(part, a); copy_u32(part + 4, b);
            copy_u32(part + 8, c); copy_u32(part + 12, d);
            part += 16;
        }
        info.cpu_brand[48] = 0;
        clean_brand(info.cpu_brand);
    } else {
        const char fallback[] = "Unknown x86-64 processor";
        for (size_t i = 0; i < sizeof(fallback); i++) info.cpu_brand[i] = fallback[i];
    }

    cpuid(1, 0, &a, &b, &c, &d);
    uint32_t legacy_logical_threads = (b >> 16) & 255u;
    uint32_t base_family = (a >> 8) & 15u, base_model = (a >> 4) & 15u;
    uint32_t ext_family = (a >> 20) & 255u, ext_model = (a >> 16) & 15u;
    info.cpu_family = base_family == 15 ? base_family + ext_family : base_family;
    info.cpu_model = (base_family == 6 || base_family == 15) ? (ext_model << 4) | base_model : base_model;
    info.cpu_stepping = a & 15u;
    info.has_apic = (d & (1u << 9)) != 0;
    info.has_sse2 = (d & (1u << 26)) != 0;
    info.has_x2apic = (c & (1u << 21)) != 0;
    info.has_avx = (c & (1u << 28)) != 0;
    if (max_extended >= 0x80000001u) {
        cpuid(0x80000001u, 0, &a, &b, &c, &d);
        info.has_long_mode = (d & (1u << 29)) != 0;
    }

    info.logical_threads = mp && mp->cpu_count ? mp->cpu_count : legacy_logical_threads;
    if (!info.logical_threads) info.logical_threads = 1;
    info.threads_per_core = 1;
    uint32_t topology_leaf = max_basic >= 0x1fu ? 0x1fu : (max_basic >= 0x0bu ? 0x0bu : 0);
    if (topology_leaf) {
        for (uint32_t level = 0; level < 8; level++) {
            cpuid(topology_leaf, level, &a, &b, &c, &d);
            if (!b) break;
            if (((c >> 8) & 255u) == 1u) info.threads_per_core = b & 0xffffu;
        }
    }
    if (!info.threads_per_core) info.threads_per_core = 1;
    info.physical_cores = (info.logical_threads + info.threads_per_core - 1) / info.threads_per_core;

    if (memmap) {
        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE) info.usable_bytes += entry->length;
            if (entry->type == LIMINE_MEMMAP_USABLE ||
                entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
                entry->type == LIMINE_MEMMAP_ACPI_NVS ||
                entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
                entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES)
                info.memory_bytes += entry->length;
        }
    }
}

const struct system_info *system_get_info(void) { return &info; }

bool system_memory_test(uint64_t *tested_bytes, uint64_t *failure_offset) {
    static const uint64_t patterns[] = {
        0x0000000000000000ull, 0xffffffffffffffffull,
        0xaaaaaaaaaaaaaaaaull, 0x5555555555555555ull,
        0x0123456789abcdefull
    };
    for (size_t pattern = 0; pattern < sizeof(patterns)/sizeof(patterns[0]); pattern++) {
        for (size_t i = 0; i < TEST_WORDS; i++) memory_test_area[i] = patterns[pattern] ^ (uint64_t)i;
        for (size_t i = 0; i < TEST_WORDS; i++) {
            if (memory_test_area[i] != (patterns[pattern] ^ (uint64_t)i)) {
                if (tested_bytes) *tested_bytes = sizeof(memory_test_area);
                if (failure_offset) *failure_offset = i * sizeof(uint64_t);
                return false;
            }
        }
    }
    if (tested_bytes) *tested_bytes = sizeof(memory_test_area);
    if (failure_offset) *failure_offset = 0;
    return true;
}

bool system_cpu_test(uint64_t *checksum) {
    volatile uint64_t value = 0x9e3779b97f4a7c15ull;
    for (uint64_t i = 1; i <= 1000000; i++) {
        value ^= value << 7;
        value ^= value >> 9;
        value += i * 0x100000001b3ull;
        value = (value << 13) | (value >> 51);
    }
    uint32_t a, b, c, d;
    cpuid(0, 0, &a, &b, &c, &d);
    value ^= ((uint64_t)a << 32) | b;
    if (checksum) *checksum = value;
    return value != 0 && info.has_long_mode;
}
