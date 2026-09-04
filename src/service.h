#ifndef AXIOM_SERVICE_H
#define AXIOM_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define SERVICE_MAX 8
enum service_state {SERVICE_STOPPED,SERVICE_STARTING,SERVICE_RUNNING,SERVICE_FAILED};
struct service_info {char name[32],executable[96],account[32];enum service_state state;uint32_t pid,restarts;uint64_t last_heartbeat_ms;int last_status;bool enabled;};

void service_init(void);
bool service_ensure_started(void);
int service_start(const char *name);
int service_stop(const char *name);
int service_restart(const char *name);
void service_heartbeat(uint32_t pid,uint64_t now_ms);
void service_process_exited(uint32_t pid,int status);
const struct service_info *service_at(uint32_t index);
uint32_t service_count(void);
const char *service_state_name(enum service_state state);
bool service_self_test(void);

#endif
