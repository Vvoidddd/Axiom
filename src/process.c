#include "process.h"
#include "hardware.h"

struct process_slot {
    struct process_info info;
    bool occupied;
};

static struct process_slot table[PROCESS_MAX];
static uint32_t current_slot;
static uint32_t next_pid;

static void copy_name(char *destination, const char *source) {
    size_t i = 0;
    while (source[i] && i + 1 < 32) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = 0;
}

static int create_process(const char *name, uint32_t parent, uint32_t uid,
                          uint32_t gid) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (table[i].occupied) continue;
        table[i].occupied = true;
        table[i].info = (struct process_info){
            .pid = next_pid++, .parent_pid = parent, .uid = uid, .gid = gid,
            .state = PROCESS_READY
        };
        copy_name(table[i].info.name, name);
        return (int)i;
    }
    return -1;
}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) table[i].occupied = false;
    next_pid = 1;
    int init = create_process("kernel-init", 0, 0, 0);
    int shell = create_process("kernel-shell", 1, 0, 0);
    current_slot = shell >= 0 ? (uint32_t)shell : (uint32_t)init;
    table[current_slot].info.state = PROCESS_RUNNING;
}

static void schedule(void) {
    uint32_t previous = current_slot;
    if (table[previous].occupied && table[previous].info.state == PROCESS_RUNNING)
        table[previous].info.state = PROCESS_READY;
    for (uint32_t step = 1; step <= PROCESS_MAX; step++) {
        uint32_t candidate = (previous + step) % PROCESS_MAX;
        if (table[candidate].occupied && table[candidate].info.state == PROCESS_READY) {
            current_slot = candidate;
            table[candidate].info.state = PROCESS_RUNNING;
            return;
        }
    }
    if (table[previous].occupied && table[previous].info.state == PROCESS_READY)
        table[previous].info.state = PROCESS_RUNNING;
}

void process_timer_tick(uint64_t now_ms) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++)
        if (table[i].occupied && table[i].info.state == PROCESS_SLEEPING &&
            now_ms >= table[i].info.wake_tick)
            table[i].info.state = PROCESS_READY;
    if (table[current_slot].occupied && table[current_slot].info.state == PROCESS_RUNNING)
        table[current_slot].info.runtime_ticks++;
    if ((now_ms % 10u) == 0) schedule();
}

long process_syscall(uint64_t number, uint64_t arg0, uint64_t arg1,
                     uint64_t arg2, uint64_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    if (number >= SYS_COUNT || !table[current_slot].occupied) return -38;
    struct process_info *current = &table[current_slot].info;
    switch ((enum axiom_syscall)number) {
        case SYS_ABI_VERSION: return AXIOM_SYSCALL_ABI_VERSION;
        case SYS_GETPID: return current->pid;
        case SYS_GETUID: return current->uid;
        case SYS_YIELD: schedule(); return 0;
        case SYS_SLEEP:
            if (arg0 > 86400000u) return -22;
            current->wake_tick = hardware_uptime_ms() + arg0;
            current->state = PROCESS_SLEEPING;
            schedule();
            return 0;
        case SYS_EXIT:
            current->exit_status = (int)arg0;
            current->state = PROCESS_ZOMBIE;
            schedule();
            return 0;
        default: return -38;
    }
}

uint32_t process_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < PROCESS_MAX; i++) if (table[i].occupied) count++;
    return count;
}

bool process_info_at(uint32_t index, struct process_info *out) {
    uint32_t seen = 0;
    if (!out) return false;
    for (uint32_t i = 0; i < PROCESS_MAX; i++) if (table[i].occupied) {
        if (seen++ == index) { *out = table[i].info; return true; }
    }
    return false;
}

const char *process_state_name(enum process_state state) {
    static const char *names[] = {"unused", "ready", "running", "sleeping", "blocked", "zombie"};
    return state <= PROCESS_ZOMBIE ? names[state] : "invalid";
}

bool process_self_test(void) {
    if (process_count() != 2 || process_syscall(SYS_ABI_VERSION, 0, 0, 0, 0) != 1)
        return false;
    long pid = process_syscall(SYS_GETPID, 0, 0, 0, 0);
    if (pid <= 0 || process_syscall(SYS_GETUID, 0, 0, 0, 0) != 0) return false;
    if (process_syscall(SYS_COUNT, 0, 0, 0, 0) != -38) return false;
    if (process_syscall(SYS_SLEEP, 86400001u, 0, 0, 0) != -22) return false;
    return process_syscall(SYS_YIELD, 0, 0, 0, 0) == 0 && process_count() == 2;
}
