#include "process.h"
#include "hardware.h"
#include "memory.h"
#include "elf.h"
#include "vfs.h"
#include "ipc.h"

struct process_slot {
    struct process_info info;
    bool occupied;
};

static struct process_slot table[PROCESS_MAX];
static uint32_t current_slot;
static uint32_t next_pid;
static bool user_mode_ok;
extern uint64_t arch_enter_user(uint64_t entry,uint64_t stack,uint64_t address_space);

static const uint8_t user_probe[] = {
    0x48,0x31,0xc0,                         /* xor rax,rax: ABI version */
    0xcd,0x80,                              /* int 0x80 */
    0x48,0x83,0xf8,0x01,                   /* cmp rax,1 */
    0x75,0x17,                              /* jne fail */
    0x48,0xc7,0xc0,0x01,0x00,0x00,0x00,   /* mov rax,SYS_GETPID */
    0xcd,0x80,                              /* int 0x80 */
    0x48,0x85,0xc0,                         /* test rax,rax */
    0x74,0x09,                              /* je fail */
    0x48,0xc7,0xc7,0x01,0x00,0x00,0x00,   /* mov rdi,1 */
    0xeb,0x07,                              /* jmp return */
    0x48,0x31,0xff,                         /* fail: xor rdi,rdi */
    0x90,0x90,0x90,0x90,                   /* padding */
    0x48,0xc7,0xc0,0x10,0xa7,0x00,0x00,   /* return: mov rax,0xa710 */
    0xcd,0x80,                              /* int 0x80 */
    0xf4                                    /* hlt if kernel fails to return */
};

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
    ipc_init();
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

bool process_user_mode_self_test(void){
    struct test_elf{uint8_t ident[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;uint32_t ptype,pflags;uint64_t offset,vaddr,paddr,filesz,memsz,align;uint8_t padding[0x1000-120];uint8_t code[sizeof(user_probe)];}__attribute__((packed));
    struct test_elf file={.ident={0x7f,'E','L','F',2,1,1},.type=2,.machine=0x3e,.version=1,.entry=0x400000,.phoff=64,.ehsize=64,.phentsize=56,.phnum=1,.ptype=1,.pflags=5,.offset=0x1000,.vaddr=0x400000,.filesz=sizeof(user_probe),.memsz=sizeof(user_probe),.align=4096};for(size_t i=0;i<sizeof(user_probe);i++)file.code[i]=user_probe[i];
    int directory=vfs_mkdir("/system/bin",0755);if(directory&&directory!=VFS_EEXIST)return false;
    int fd=vfs_open("/system/bin/ring3-test",VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0||vfs_write(fd,&file,sizeof(file))!=(long)sizeof(file)||vfs_close(fd))return false;
    struct user_image image;if(elf_load_vfs("/system/bin/ring3-test",&image)||vmm_user_range_valid(0x400000,1,false))return false;
    user_mode_ok=arch_enter_user(image.entry,image.stack_top,image.space.root_physical)==1&&ipc_self_test();
    return user_mode_ok;
}
bool process_user_mode_ready(void){return user_mode_ok;}
