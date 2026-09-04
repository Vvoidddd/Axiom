#ifndef AXIOM_PROCESS_H
#define AXIOM_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <axiom/syscall.h>
#define PROCESS_MAX 32u

enum process_state {
    PROCESS_UNUSED,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE
};

struct process_info {
    uint32_t pid;
    uint32_t tid;
    uint32_t parent_pid;
    uint32_t real_uid;
    uint32_t effective_uid;
    uint32_t saved_uid;
    uint32_t real_gid;
    uint32_t effective_gid;
    uint32_t saved_gid;
    enum process_state state;
    uint64_t runtime_ticks;
    uint64_t wake_tick;
    int exit_status;
    char name[32];
};

void process_init(void);
void process_timer_tick(uint64_t now_ms);
void process_timer_interrupt(uint64_t *frame,uint64_t now_ms);
bool process_syscall_interrupt(uint64_t *frame);
long process_syscall(uint64_t number, uint64_t arg0, uint64_t arg1,
                     uint64_t arg2, uint64_t arg3);
uint32_t process_count(void);
bool process_info_at(uint32_t index, struct process_info *out);
const char *process_state_name(enum process_state state);
bool process_self_test(void);
bool process_user_mode_self_test(void);
bool process_user_mode_ready(void);
bool process_scheduler_self_test(void);
/* Runs one authenticated user session and returns its shell exit status. */
int process_launch_init(void);
bool process_runtime_ready(void);

#endif
