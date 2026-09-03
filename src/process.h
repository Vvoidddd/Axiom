#ifndef AXIOM_PROCESS_H
#define AXIOM_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AXIOM_SYSCALL_ABI_VERSION 1u
#define PROCESS_MAX 32u

enum process_state {
    PROCESS_UNUSED,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
};

enum axiom_syscall {
    SYS_ABI_VERSION,
    SYS_GETPID,
    SYS_GETUID,
    SYS_YIELD,
    SYS_SLEEP,
    SYS_EXIT,
    SYS_COUNT
};

struct process_info {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t uid;
    uint32_t gid;
    enum process_state state;
    uint64_t runtime_ticks;
    uint64_t wake_tick;
    int exit_status;
    char name[32];
};

void process_init(void);
void process_timer_tick(uint64_t now_ms);
long process_syscall(uint64_t number, uint64_t arg0, uint64_t arg1,
                     uint64_t arg2, uint64_t arg3);
uint32_t process_count(void);
bool process_info_at(uint32_t index, struct process_info *out);
const char *process_state_name(enum process_state state);
bool process_self_test(void);

#endif
