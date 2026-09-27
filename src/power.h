#ifndef AXIOM_POWER_H
#define AXIOM_POWER_H
#include <stdbool.h>
#include <stdint.h>

enum power_action { POWER_ACTION_NONE, POWER_ACTION_REBOOT, POWER_ACTION_SHUTDOWN };
enum power_state { POWER_RUNNING, POWER_PREPARING, POWER_READY, POWER_FAILED };
struct power_status {
    enum power_state state;
    enum power_action action;
    uint32_t quiesced_processes;
    bool accounts_saved, services_stopped, filesystem_checked, caches_flushed;
};

void power_init(void);
bool power_prepare(enum power_action action);
const struct power_status *power_get_status(void);
const char *power_state_name(enum power_state state);
bool power_self_test(void);
#endif
