#ifndef AXIOM_DEVICE_H
#define AXIOM_DEVICE_H
#include <stdbool.h>
#include <stdint.h>

#define DEVICE_MANAGER_MAX 96
#define DEVICE_EVENT_MAX 64
enum device_event_type { DEVICE_ADDED, DEVICE_CHANGED, DEVICE_REMOVED };
struct managed_device {
    char stable_name[48], path[96], kind[16], description[96];
    uint32_t uid, gid, mode;
    uint64_t generation;
    bool present;
};
struct device_event {
    uint64_t sequence, time_ms;
    enum device_event_type type;
    char stable_name[48];
};

bool device_manager_init(void);
bool device_manager_scan(void);
uint32_t device_manager_count(void);
const struct managed_device *device_manager_at(uint32_t index);
uint32_t device_event_count(void);
const struct device_event *device_event_at(uint32_t index);
const char *device_event_type_name(enum device_event_type type);
bool device_manager_self_test(void);
#endif
