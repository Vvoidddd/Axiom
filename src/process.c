#include "process.h"
#include "hardware.h"
#include "memory.h"
#include "elf.h"
#include "vfs.h"
#include "ipc.h"
#include "console.h"
#include "log.h"
#include "account.h"

struct process_slot {
    struct process_info info;
    bool occupied;
    bool user_task;
    uint64_t frame[22];
    struct user_image image;
};

static struct process_slot table[PROCESS_MAX];
static uint32_t current_slot;
static uint32_t next_pid;
static uint32_t next_tid;
static bool user_mode_ok;
static bool scheduler_active;
static uint32_t scheduler_preemptions;
static uint32_t scheduler_marks[2];
static bool scheduler_ok;
static uint32_t scheduler_sleeps;
static bool session_launch;
extern uint64_t arch_enter_user(uint64_t entry,uint64_t stack,uint64_t address_space);
extern void arch_return_from_user(uint64_t result) __attribute__((noreturn));
extern void kernel_execute_user_command(char *line);

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
struct test_elf{uint8_t ident[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;uint32_t ptype,pflags;uint64_t offset,vaddr,paddr,filesz,memsz,align;uint8_t padding[0x1000-120];uint8_t code[64];}__attribute__((packed));
static bool write_test_elf(const char*path,const uint8_t*code,size_t size){if(size>64)return false;struct test_elf file={.ident={0x7f,'E','L','F',2,1,1},.type=2,.machine=0x3e,.version=1,.entry=0x400000,.phoff=64,.ehsize=64,.phentsize=56,.phnum=1,.ptype=1,.pflags=5,.offset=0x1000,.vaddr=0x400000,.filesz=size,.memsz=size,.align=4096};for(size_t i=0;i<size;i++)file.code[i]=code[i];int fd=vfs_open(path,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(fd<0)return false;bool ok=vfs_write(fd,&file,0x1000+size)==(long)(0x1000+size);return vfs_close(fd)==0&&ok;}

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
            .pid = next_pid++, .tid = next_tid++, .parent_pid = parent,
            .real_uid = uid, .effective_uid = uid, .saved_uid = uid,
            .real_gid = gid, .effective_gid = gid, .saved_gid = gid,
            .state = PROCESS_READY
        };
        copy_name(table[i].info.name, name);
        return (int)i;
    }
    return -1;
}

static int create_user_process(const char*name,const struct user_image*image,uint32_t uid,uint32_t gid){int slot=create_process(name,1,uid,gid);if(slot<0)return slot;table[slot].user_task=true;table[slot].image=*image;uint64_t*f=table[slot].frame;for(unsigned i=0;i<22;i++)f[i]=0;f[17]=image->entry;f[18]=0x33;f[19]=0x202;f[20]=image->stack_top;f[21]=0x2b;return slot;}
static int create_user_thread(int owner,const char*name,uint64_t stack_address){if(owner<0||(uint32_t)owner>=PROCESS_MAX||!table[owner].occupied||!table[owner].user_task)return-1;uint64_t physical=pmm_alloc();if(!physical||!vmm_space_map_user(&table[owner].image.space,stack_address-4096,physical,true,false))return-1;int slot=create_process(name,table[owner].info.parent_pid,table[owner].info.real_uid,table[owner].info.real_gid);if(slot<0)return slot;table[slot].user_task=true;table[slot].info.pid=table[owner].info.pid;table[slot].info.effective_uid=table[owner].info.effective_uid;table[slot].info.saved_uid=table[owner].info.saved_uid;table[slot].info.effective_gid=table[owner].info.effective_gid;table[slot].info.saved_gid=table[owner].info.saved_gid;table[slot].image=table[owner].image;table[slot].image.stack_top=stack_address;uint64_t*f=table[slot].frame;for(unsigned i=0;i<22;i++)f[i]=0;f[17]=table[owner].image.entry;f[18]=0x33;f[19]=0x202;f[20]=stack_address;f[21]=0x2b;return slot;}
static void load_cr3(uint64_t root){__asm__ volatile("mov %0,%%cr3"::"r"(root):"memory");}
static bool choose_user(uint32_t previous,uint32_t*out){for(uint32_t step=1;step<=PROCESS_MAX;step++){uint32_t candidate=(previous+step)%PROCESS_MAX;if(table[candidate].occupied&&table[candidate].user_task&&table[candidate].info.state==PROCESS_READY){*out=candidate;return true;}}return false;}
static bool any_live_user(void){for(unsigned i=0;i<PROCESS_MAX;i++)if(table[i].occupied&&table[i].user_task&&table[i].info.state!=PROCESS_ZOMBIE)return true;return false;}
static bool copy_user_string(uint64_t address,char*out,size_t capacity){if(!address||!out||capacity<2||!table[current_slot].user_task)return false;for(size_t i=0;i<capacity;i++){if(!vmm_space_user_range_valid(&table[current_slot].image.space,address+i,1,false))return false;char c=*(const char*)(uintptr_t)(address+i);out[i]=c;if(!c)return true;}out[capacity-1]=0;return false;}
static void switch_user_frame(uint64_t*frame,bool save_current){uint32_t previous=current_slot;if(save_current)for(unsigned i=0;i<22;i++)table[previous].frame[i]=frame[i];if(table[previous].info.state==PROCESS_RUNNING)table[previous].info.state=PROCESS_READY;uint32_t next;if(!choose_user(previous,&next)){/* There is no kernel idle task yet. Never return to a zombie frame or let
       a lone sleeper continue while still marked sleeping: wake one sleeper
       early and make the state/frame transition explicit. */for(uint32_t step=1;step<=PROCESS_MAX;step++){uint32_t candidate=(previous+step)%PROCESS_MAX;if(table[candidate].occupied&&table[candidate].user_task&&table[candidate].info.state==PROCESS_SLEEPING){table[candidate].info.state=PROCESS_READY;table[candidate].info.wake_tick=0;break;}}if(!choose_user(previous,&next)){if(table[previous].occupied&&table[previous].user_task&&table[previous].info.state==PROCESS_SLEEPING){table[previous].info.state=PROCESS_RUNNING;table[previous].info.wake_tick=0;return;}if(!any_live_user()){uint64_t result=session_launch?(0x100u+((uint32_t)table[previous].info.exit_status&0xffu)):1u;vfs_set_credentials(0,0);arch_return_from_user(result);}arch_return_from_user(2);}}current_slot=next;table[next].info.state=PROCESS_RUNNING;vfs_set_credentials(table[next].info.effective_uid,table[next].info.effective_gid);for(unsigned i=0;i<22;i++)frame[i]=table[next].frame[i];load_cr3(table[next].image.space.root_physical);}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) table[i].occupied = false;
    next_pid = 1;
    next_tid = 1;
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
void process_timer_interrupt(uint64_t*frame,uint64_t now_ms){
    if(!scheduler_active||(frame[18]&3)!=3){process_timer_tick(now_ms);return;}
    for(uint32_t i=0;i<PROCESS_MAX;i++)if(table[i].occupied&&table[i].user_task&&table[i].info.state==PROCESS_SLEEPING&&now_ms>=table[i].info.wake_tick)table[i].info.state=PROCESS_READY;
    table[current_slot].info.runtime_ticks++;scheduler_preemptions++;switch_user_frame(frame,true);
}
bool process_syscall_interrupt(uint64_t*f){
    if(!scheduler_active||(f[18]&3)!=3)return false;
    uint64_t number=f[14];
    for(unsigned i=0;i<22;i++)table[current_slot].frame[i]=f[i];
    if(number==0xa711){uint64_t id=f[9];if(id>=1&&id<=2)scheduler_marks[id-1]++;f[14]=0;return true;}
    if(number==SYS_EXIT){table[current_slot].info.exit_status=(int)f[9];table[current_slot].info.state=PROCESS_ZOMBIE;switch_user_frame(f,false);return true;}
    if(number==SYS_SLEEP){if(f[9]>10000){f[14]=(uint64_t)-22;return true;}f[14]=0;table[current_slot].frame[14]=0;table[current_slot].info.wake_tick=hardware_uptime_ms()+f[9];table[current_slot].info.state=PROCESS_SLEEPING;scheduler_sleeps++;switch_user_frame(f,false);return true;}
    if(number==SYS_YIELD){f[14]=0;switch_user_frame(f,true);return true;}
    if(number==SYS_WRITE){size_t size=(size_t)f[8];if(size>4096||!vmm_space_user_range_valid(&table[current_slot].image.space,f[9],size,false)){f[14]=(uint64_t)-14;return true;}const char*text=(const char*)(uintptr_t)f[9];for(size_t i=0;i<size;i++)console_putc(text[i]);f[14]=size;return true;}
    if(number==SYS_KEY_POLL){struct key_event event;f[14]=keyboard_poll_event(&event)&&event.pressed&&event.character?(uint8_t)event.character:0;return true;}
    if(number==SYS_COMMAND){char line[128];if(!copy_user_string(f[9],line,sizeof(line))){f[14]=(uint64_t)-14;return true;}kernel_execute_user_command(line);f[14]=0;return true;}
    if(number==SYS_SPAWN){char path[128];if(!copy_user_string(f[9],path,sizeof(path))){f[14]=(uint64_t)-14;return true;}struct user_image image;if(elf_load_vfs(path,&image)){f[14]=(uint64_t)-8;return true;}struct process_info*parent=&table[current_slot].info;int child=create_user_process(path,&image,parent->real_uid,parent->real_gid);if(child<0){f[14]=(uint64_t)child;return true;}table[child].info.parent_pid=parent->pid;table[child].info.effective_uid=parent->effective_uid;table[child].info.saved_uid=parent->saved_uid;table[child].info.effective_gid=parent->effective_gid;table[child].info.saved_gid=parent->saved_gid;f[14]=table[child].info.pid;log_write("SPAWN",path);return true;}
    f[14]=(uint64_t)process_syscall(number,f[9],f[8],f[11],f[5]);return true;
}

long process_syscall(uint64_t number, uint64_t arg0, uint64_t arg1,
                     uint64_t arg2, uint64_t arg3) {
    (void)arg1; (void)arg2; (void)arg3;
    if (number >= SYS_COUNT || !table[current_slot].occupied) return -38;
    struct process_info *current = &table[current_slot].info;
    switch ((enum axiom_syscall)number) {
        case SYS_ABI_VERSION: return AXIOM_SYSCALL_ABI_VERSION;
        case SYS_GETPID: return current->pid;
        case SYS_GETUID: return current->real_uid;
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
    int directory=vfs_mkdir("/system/bin",0755);if(directory&&directory!=VFS_EEXIST)return false;
    if(!write_test_elf("/system/bin/ring3-test",user_probe,sizeof(user_probe)))return false;
    struct user_image image;if(elf_load_vfs("/system/bin/ring3-test",&image)||vmm_user_range_valid(0x400000,1,false))return false;
    user_mode_ok=arch_enter_user(image.entry,image.stack_top,image.space.root_physical)==1&&ipc_self_test();
    return user_mode_ok;
}
bool process_user_mode_ready(void){return user_mode_ok;}
bool process_scheduler_self_test(void){
    uint8_t code1[]={0x48,0xc7,0xc0,0x11,0xa7,0,0,0x48,0xc7,0xc7,1,0,0,0,0xcd,0x80,0x48,0xc7,0xc1,0,0,0x20,0,0x48,0xff,0xc9,0x75,0xfb,0x48,0xc7,0xc0,5,0,0,0,0x48,0xc7,0xc7,1,0,0,0,0xcd,0x80,0xf4};
    uint8_t code2[]={0x48,0xc7,0xc0,4,0,0,0,0x48,0xc7,0xc7,5,0,0,0,0xcd,0x80,0x48,0xc7,0xc0,0x11,0xa7,0,0,0x48,0xc7,0xc7,2,0,0,0,0xcd,0x80,0x48,0xc7,0xc1,0,0,0x20,0,0x48,0xff,0xc9,0x75,0xfb,0x48,0xc7,0xc0,5,0,0,0,0x48,0xc7,0xc7,2,0,0,0,0xcd,0x80,0xf4};
    if(!write_test_elf("/system/bin/sched-a",code1,sizeof(code1))||!write_test_elf("/system/bin/sched-b",code2,sizeof(code2)))return false;
    struct user_image a,b;if(elf_load_vfs("/system/bin/sched-a",&a)||elf_load_vfs("/system/bin/sched-b",&b))return false;
    int first=create_user_process("sched-a",&a,1000,1000),second=create_user_process("sched-b",&b,1000,1000);if(first<0||second<0)return false;int thread=create_user_thread(first,"sched-a-worker",0x7fffffffd000ull);if(thread<0)return false;
    uint32_t old=current_slot;scheduler_marks[0]=scheduler_marks[1]=scheduler_preemptions=scheduler_sleeps=0;scheduler_active=true;current_slot=(uint32_t)first;table[first].info.state=PROCESS_RUNNING;
    uint64_t result=arch_enter_user(a.entry,a.stack_top,a.space.root_physical);scheduler_active=false;current_slot=old;table[old].info.state=PROCESS_RUNNING;
    scheduler_ok=result==1&&scheduler_preemptions>0&&scheduler_sleeps==1&&scheduler_marks[0]==2&&scheduler_marks[1]==1&&table[first].info.state==PROCESS_ZOMBIE&&table[second].info.state==PROCESS_ZOMBIE&&table[thread].info.state==PROCESS_ZOMBIE&&table[first].info.pid==table[thread].info.pid&&table[first].info.tid!=table[thread].info.tid&&table[first].image.space.root_physical==table[thread].image.space.root_physical&&table[first].info.exit_status==1&&table[second].info.exit_status==2;if(!scheduler_ok){LOG_ERROR("scheduler result/preemptions/sleeps/marks/states");log_hex(result);log_hex(scheduler_preemptions);log_hex(scheduler_sleeps);log_hex(scheduler_marks[0]);log_hex(scheduler_marks[1]);log_hex(table[first].info.state);log_hex(table[second].info.state);log_hex(table[thread].info.state);}return scheduler_ok;
}
int process_launch_init(void){const struct account_info*user=account_current();if(!user)return-1;struct user_image image;if(elf_load_vfs("/system/bin/init",&image))return-1;int slot=create_user_process("init",&image,user->uid,user->gid);if(slot<0)return-1;uint32_t old=current_slot;session_launch=true;scheduler_active=true;current_slot=(uint32_t)slot;table[slot].info.state=PROCESS_RUNNING;vfs_set_credentials(user->uid,user->gid);uint64_t result=arch_enter_user(image.entry,image.stack_top,image.space.root_physical);scheduler_active=false;session_launch=false;current_slot=old;table[old].info.state=PROCESS_RUNNING;for(uint32_t i=0;i<PROCESS_MAX;i++)if(table[i].occupied&&table[i].user_task)table[i].occupied=false;vfs_set_credentials(0,0);return result>=0x100u&&result<0x200u?(int)(result-0x100u):-1;}
bool process_runtime_ready(void){return user_mode_ok&&scheduler_ok;}
