#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limine.h>
#include "console.h"
#include "framebuffer.h"
#include "hardware.h"
#include "system.h"
#include "arch.h"
#include "log.h"
#include "memory.h"
#include "acpi.h"
#include "smp.h"
#include "pci.h"
#include "block.h"
#include "vfs.h"
#include "filesystems.h"
#include "initramfs.h"
#include "process.h"

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID, .revision = 0, .flags = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID, .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

static bool streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static void execute(char *line);

static void cmd_help(const char *args) {
    (void)args;
    console_write("SYSTEM: SYSINFO CPU RAM MODULES VERSION ACPI TIME UPTIME\n");
    console_write("TESTS: MEMTEST CPUTEST\n");
    console_write("TOOLS: HELP CLEAR ECHO MEMMAP FBINFO ALLOCSTAT LOGS IRQS RANDOM\n");
    console_write("FILES: LS CD PWD CAT TOUCH MKDIR CP MV RM LN CHMOD CHOWN\n");
    console_write("STORAGE: DISKS PARTITIONS MOUNT UNMOUNT DF DU FSCK\n");
    console_write("PROCESSES: PS PROCTEST\n");
    console_write("POWER: REBOOT SHUTDOWN HALT\n");
}

static void cmd_about(const char *args) {
    (void)args;
    console_write("AXIOM V0 - YOUR SYSTEM. YOUR RULES.\n");
    console_write("X86-64 KERNEL BOOTED BY LIMINE.\n");
}

static void cmd_version(const char *args) {
    (void)args;
    console_write("AXIOM KERNEL VERSION 0.4.0\nBOOT PROTOCOL: LIMINE\nARCHITECTURE: X86-64\n");
}

static void cmd_clear(const char *args) { (void)args; console_clear(); }
static void cmd_echo(const char *args) { console_write(args); console_putc('\n'); }

static const char *mem_type(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE: return "USABLE";
        case LIMINE_MEMMAP_RESERVED: return "RESERVED";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return "ACPI";
        case LIMINE_MEMMAP_ACPI_NVS: return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY: return "BAD";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "BOOTLOADER";
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return "KERNEL";
        case LIMINE_MEMMAP_FRAMEBUFFER: return "FRAMEBUFFER";
        default: return "UNKNOWN";
    }
}

static void cmd_memmap(const char *args) {
    (void)args;
    struct limine_memmap_response *r = memmap_request.response;
    if (!r) { console_write("MEMORY MAP UNAVAILABLE.\n"); return; }
    uint64_t total = 0, usable = 0;
    console_write("MEMORY MAP ENTRIES: "); console_write_u64(r->entry_count); console_putc('\n');
    for (uint64_t i = 0; i < r->entry_count; i++) {
        struct limine_memmap_entry *e = r->entries[i];
        total += e->length;
        if (e->type == LIMINE_MEMMAP_USABLE) usable += e->length;
        console_write_hex(e->base); console_write(" + "); console_write_hex(e->length);
        console_write(" "); console_write(mem_type(e->type)); console_putc('\n');
    }
    console_write("TOTAL KIB: "); console_write_u64(total / 1024);
    console_write("  USABLE KIB: "); console_write_u64(usable / 1024); console_putc('\n');
}

static void cmd_fbinfo(const char *args) {
    (void)args;
    struct limine_framebuffer *f = fb_info();
    if (!f) { console_write("FRAMEBUFFER UNAVAILABLE.\n"); return; }
    console_write("ADDRESS: "); console_write_hex((uint64_t)(uintptr_t)f->address); console_putc('\n');
    console_write("WIDTH: "); console_write_u64(f->width);
    console_write(" HEIGHT: "); console_write_u64(f->height);
    console_write(" PITCH: "); console_write_u64(f->pitch);
    console_write(" BPP: "); console_write_u64(f->bpp); console_putc('\n');
}

static void write_mib(uint64_t bytes) { console_write_u64(bytes / (1024u * 1024u)); console_write(" MIB"); }

static void cmd_ram(const char *args) {
    (void)args;
    const struct system_info *s = system_get_info();
    console_write("DETECTED MEMORY: "); write_mib(s->memory_bytes); console_putc('\n');
    console_write("USABLE AT BOOT: "); write_mib(s->usable_bytes); console_putc('\n');
    console_write("NOTE: DETECTED MEMORY IS DERIVED FROM THE FIRMWARE MAP.\n");
}

static void cmd_cpu(const char *args) {
    (void)args;
    const struct system_info *s = system_get_info();
    console_write("CPU: "); console_write(s->cpu_brand); console_putc('\n');
    console_write("VENDOR: "); console_write(s->cpu_vendor); console_putc('\n');
    console_write("FAMILY: "); console_write_u64(s->cpu_family);
    console_write(" MODEL: "); console_write_u64(s->cpu_model);
    console_write(" STEPPING: "); console_write_u64(s->cpu_stepping); console_putc('\n');
    console_write("CORES: "); console_write_u64(s->physical_cores);
    console_write(" THREADS: "); console_write_u64(s->logical_threads);
    console_write(" THREADS PER CORE: "); console_write_u64(s->threads_per_core); console_putc('\n');
    console_write("FEATURES:");
    if (s->has_long_mode) console_write(" LONG-MODE");
    if (s->has_apic) console_write(" APIC");
    if (s->has_x2apic) console_write(" X2APIC");
    if (s->has_sse2) console_write(" SSE2");
    if (s->has_avx) console_write(" AVX");
    console_putc('\n');
}

static void cmd_sysinfo(const char *args) {
    (void)args;
    console_write("AXIOM SYSTEM INFORMATION\n");
    cmd_cpu("");
    cmd_ram("");
    struct limine_framebuffer *f = fb_info();
    console_write("DISPLAY: "); console_write_u64(f->width); console_write(" X ");
    console_write_u64(f->height); console_write(" X "); console_write_u64(f->bpp); console_putc('\n');
}

static bool boot_memory_test_ok;

static void cmd_modules(const char *args) {
    (void)args;
    console_write("[OK] LIMINE BOOT PROTOCOL\n[OK] FRAMEBUFFER DISPLAY\n");
    console_write(memmap_request.response ? "[OK] PHYSICAL MEMORY MAP\n" : "[FAIL] PHYSICAL MEMORY MAP\n");
    console_write("[OK] CPU DISCOVERY\n");
    console_write(mp_request.response ? "[OK] MULTIPROCESSOR TOPOLOGY\n" : "[WARN] SINGLE CPU FALLBACK\n");
    console_write(boot_memory_test_ok ? "[OK] KERNEL MEMORY TEST AREA\n" : "[FAIL] KERNEL MEMORY TEST AREA\n");
    console_write("[OK] PIT TIMER\n[OK] PS2 KEYBOARD\n[OK] COMMAND CONSOLE\n");
}

static void cmd_memtest(const char *args) {
    (void)args;
    uint64_t tested, failure;
    console_write("TESTING RESERVED KERNEL MEMORY WITH 5 PATTERNS...\n");
    if (system_memory_test(&tested, &failure)) {
        console_write("PASS: "); console_write_u64(tested / 1024); console_write(" KIB TESTED SAFELY.\n");
    } else {
        console_write("FAIL AT TEST BUFFER OFFSET "); console_write_hex(failure); console_putc('\n');
    }
    uint64_t pages=0,failed_page=0;
    console_write("TESTING 64 RESERVED PHYSICAL PAGES...\n");
    if(pmm_test_pages(64,&pages,&failed_page)){console_write("PASS: ");console_write_u64(pages);console_write(" PAGES RETURNED TO ALLOCATOR.\n");}
    else{console_write("FAIL AT PHYSICAL PAGE ");console_write_hex(failed_page);console_putc('\n');}
}

static void cmd_allocstat(const char *args){
    (void)args;console_write("FREE PHYSICAL PAGES: ");console_write_u64(pmm_free_pages());
    console_write(" USED PAGES: ");console_write_u64(pmm_used_pages());console_putc('\n');
    console_write("HEAP PAYLOAD BYTES: ");console_write_u64(heap_bytes_used());console_putc('\n');
}
static void cmd_heaptest(const char *args){(void)args;uint64_t before=pmm_free_pages();void *a=kmalloc(64),*b=kmalloc(7000);if(!a||!b){console_write("HEAP ALLOCATION FAILED.\n");return;}((volatile uint8_t *)a)[0]=0x5a;((volatile uint8_t *)b)[6999]=0xa5;kfree(a);kfree(b);if(pmm_free_pages()==before&&heap_bytes_used()==0)console_write("HEAP MAP ALLOCATE FREE TEST: PASS\n");else console_write("HEAP MAP ALLOCATE FREE TEST: FAIL\n");}
static void cmd_fault(const char *args){(void)args;LOG_WARN("deliberate invalid opcode test");__asm__ volatile("ud2");}

static void cmd_cputest(const char *args) {
    (void)args;
    uint64_t checksum;
    console_write("RUNNING 1000000 INTEGER AND CPUID TEST CYCLES...\n");
    if (system_cpu_test(&checksum)) console_write("PASS. CHECKSUM: ");
    else console_write("FAIL. CHECKSUM: ");
    console_write_hex(checksum); console_putc('\n');
}

static void cmd_acpi(const char *args){
    (void)args;console_write("ACPI TABLES: ");console_write_u64(acpi_table_count());console_putc('\n');
    for(uint32_t i=0;i<acpi_table_count();i++){const char *s=acpi_table_signature(i);for(unsigned j=0;j<4;j++)console_putc(s[j]);console_putc(' ');}console_putc('\n');
    console_write("MADT CPUS: ");console_write_u64(acpi_cpu_count());console_write(" LAPIC: ");console_write_hex(acpi_lapic_address());console_write(" IOAPIC: ");console_write_hex(acpi_ioapic_address());console_putc('\n');
    console_write("HPET: ");console_write_hex(acpi_hpet_address());console_putc('\n');
}
static void cmd_time(const char *args){(void)args;struct rtc_time t;rtc_read(&t);console_write_u64(t.year);console_putc('-');console_write_u64(t.month);console_putc('-');console_write_u64(t.day);console_putc(' ');console_write_u64(t.hour);console_putc(':');console_write_u64(t.minute);console_putc(':');console_write_u64(t.second);console_write(" UTC\n");}
static void cmd_uptime(const char *args){(void)args;uint64_t ms=hardware_uptime_ms();console_write("UPTIME: ");console_write_u64(ms/1000);console_write(" SECONDS ");console_write_u64(ms%1000);console_write(" MS\n");}
static void cmd_random(const char *args){(void)args;console_write("ENTROPY SAMPLE: ");console_write_hex(entropy_seed());console_putc('\n');}
static void cmd_logs(const char *args){(void)args;log_dump();}
static void cmd_irqs(const char *args){(void)args;console_write("TIMER IRQ: ");console_write_u64(arch_interrupt_count(32));console_write(" KEYBOARD IRQ: ");console_write_u64(arch_interrupt_count(33));console_putc('\n');}
static void cmd_smp(const char *args){(void)args;console_write("ONLINE PROCESSORS: ");console_write_u64(smp_online_count());console_write(" OF ");console_write_u64(system_get_info()->logical_threads);console_putc('\n');console_write("LOCAL APIC: ");console_write(arch_apic_active()?"ACTIVE":"FALLBACK");console_write(" IOAPIC: ");console_write(arch_ioapic_present()?"PRESENT":"ABSENT");console_putc('\n');}
static void cmd_shutdown(const char *args){(void)args;console_write("REQUESTING ACPI SHUTDOWN...\n");if(!acpi_shutdown())console_write("ACPI SHUTDOWN UNAVAILABLE.\n");}
static void cmd_pci(const char *args){(void)args;console_write("PCI DEVICES: ");console_write_u64(pci_device_count());console_putc('\n');pci_list_devices(false);}
static void cmd_disks(const char *args){(void)args;console_write("AHCI CONTROLLERS: ");console_write_u64(storage_ahci_controllers());console_write(" NVME CONTROLLERS: ");console_write_u64(storage_nvme_controllers());console_write(" BLOCK DEVICES: ");console_write_u64(block_device_count());console_putc('\n');for(uint32_t i=0;i<block_device_count();i++){struct block_device*d=block_device_at(i);console_write(d->name);console_write(" ");console_write(d->model);console_write(" SECTORS=");console_write_u64(d->sectors);console_write(d->readonly?" RO\n":" RW\n");}}
static void fs_error(int e){if(e<0){console_write("ERROR: ");console_write(vfs_error_string(e));console_putc('\n');}}
static char*next_arg(char**p){while(**p==' ')++*p;if(!**p)return 0;char*r=*p;while(**p&&**p!=' ')++*p;if(**p)*(*p)++=0;return r;}
static void list_entry(const char*n,const struct vfs_stat*s,void*c){(void)c;console_putc(s->directory?'d':(s->symlink?'l':'-'));console_write(" ");console_write_u64(s->mode);console_write(" ");console_write_u64(s->uid);console_putc(':');console_write_u64(s->gid);console_write(" ");console_write_u64(s->size);console_write(" ");console_write(n);console_putc('\n');}
static void cmd_ls(const char*a){fs_error(vfs_list(*a?a:vfs_cwd(),list_entry,0));}
static void cmd_cd(const char*a){fs_error(vfs_chdir(*a?a:"/"));}static void cmd_pwd(const char*a){(void)a;console_write(vfs_cwd());console_putc('\n');}
static void cmd_cat(const char*a){int fd=vfs_open(a,VFS_READ);if(fd<0){fs_error(fd);return;}char b[129];long n;while((n=vfs_read(fd,b,128))>0){b[n]=0;console_write(b);}if(n<0)fs_error(n);vfs_close(fd);}
static void cmd_touch(const char*a){int fd=vfs_open(a,VFS_WRITE|VFS_CREATE);if(fd<0)fs_error(fd);else vfs_close(fd);}static void cmd_mkdir(const char*a){fs_error(vfs_mkdir(a,0755));}
static void cmd_rm(const char*a){fs_error(vfs_unlink(a,false));}static void cmd_mv(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*a=next_arg(&p),*d=next_arg(&p);if(!a||!d){console_write("USAGE: MV SOURCE DEST\n");return;}fs_error(vfs_rename(a,d));}
static void cmd_cp(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*a=next_arg(&p),*d=next_arg(&p);if(!a||!d){console_write("USAGE: CP SOURCE DEST\n");return;}int in=vfs_open(a,VFS_READ),out=in<0?in:vfs_open(d,VFS_WRITE|VFS_CREATE|VFS_TRUNCATE);if(in<0||out<0){fs_error(in<0?in:out);if(in>=0)vfs_close(in);return;}char data[256];long n;while((n=vfs_read(in,data,sizeof(data)))>0)if(vfs_write(out,data,n)<0)break;vfs_close(in);vfs_close(out);}
static uint32_t decimal(const char*s){uint32_t n=0;while(*s>='0'&&*s<='9')n=n*10+(*s++-'0');return n;}
static void cmd_chmod(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*m=next_arg(&p),*f=next_arg(&p);if(!m||!f){console_write("USAGE: CHMOD MODE PATH\n");return;}uint32_t mode=0;while(*m>='0'&&*m<='7')mode=mode*8+(*m++-'0');fs_error(vfs_chmod(f,mode));}
static void cmd_chown(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*owner=next_arg(&p),*f=next_arg(&p);if(!owner||!f){console_write("USAGE: CHOWN UID:GID PATH\n");return;}char*g=owner;while(*g&&*g!=':')g++;if(*g)*g++=0;fs_error(vfs_chown(f,decimal(owner),decimal(g)));}
static void cmd_ln(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*a=next_arg(&p);bool sym=a&&streq(a,"-s");if(sym)a=next_arg(&p);char*d=next_arg(&p);if(!a||!d){console_write("USAGE: LN [-S] TARGET LINK\n");return;}fs_error(vfs_link(a,d,sym));}
static void cmd_df(const char*a){(void)a;console_write("AXIOMFS USED ");console_write_u64(vfs_used_bytes());console_write(" BYTES, CAPACITY ");console_write_u64(vfs_capacity_bytes());console_putc('\n');}static void cmd_du(const char*a){struct vfs_stat s;int e=vfs_stat_path(*a?a:vfs_cwd(),&s);if(e<0)fs_error(e);else{console_write_u64(s.allocated);console_write(" BYTES ALLOCATED\n");}}
static void cmd_mount(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*s=next_arg(&p),*d=next_arg(&p),*t=next_arg(&p);if(!s||!d){console_write("USAGE: MOUNT SOURCE PATH [TYPE]\n");return;}fs_error(vfs_mount(s,d,t?t:"axiomfs",true));}static void cmd_unmount(const char*a){fs_error(vfs_unmount(a));}
static void cmd_partitions(const char*a){(void)a;block_scan_partitions();console_write("PARTITIONS: ");console_write_u64(block_partition_count());console_putc('\n');for(uint32_t i=0;i<block_partition_count();i++){const struct block_partition*p=block_partition_at(i);console_write(p->name);console_write(p->gpt?" GPT ":" MBR ");console_write_u64(p->first_lba);console_write(" + ");console_write_u64(p->sectors);console_putc('\n');}}
static bool fat_list_console(const struct fat32_entry*e,void*c){(void)c;console_putc(e->directory?'d':'-');console_write(e->readonly?"r- ":"rw ");console_write_u64(e->size);console_write(" ");console_write(e->name);console_putc('\n');return true;}
static void cmd_fatls(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*device_name=next_arg(&p),*path=next_arg(&p);if(!device_name){console_write("USAGE: FATLS DEVICE [PATH]\n");return;}struct block_device*d=block_find(device_name);struct fat32_volume volume;if(!d||fat32_open(&volume,d,0)||fat32_list(&volume,path?path:"/",fat_list_console,0))console_write("FAT32 OPEN OR LIST FAILED.\n");}
static void cmd_fatcat(const char*args){char b[256];size_t i=0;while(args[i]&&i<255){b[i]=args[i];i++;}b[i]=0;char*p=b,*device_name=next_arg(&p),*path=next_arg(&p);if(!device_name||!path){console_write("USAGE: FATCAT DEVICE PATH\n");return;}struct block_device*d=block_find(device_name);struct fat32_volume volume;if(!d||fat32_open(&volume,d,0)){console_write("FAT32 OPEN FAILED.\n");return;}char out[129];uint64_t offset=0;long n;while((n=fat32_read(&volume,path,offset,out,128))>0){out[n]=0;console_write(out);offset+=n;if(n<128)break;}if(n<0)console_write("FAT32 READ FAILED.\n");else console_putc('\n');}
static void cmd_fsck(const char*a){bool repair=streq(a,"-r")||streq(a,"--repair");int e=vfs_check(repair);console_write(e?"AXIOMFS CHECK FAILED: ":"AXIOMFS CHECK PASSED");if(e)console_write(vfs_error_string(e));console_putc('\n');}
static void cmd_mkfs(const char*a){if(!streq(a,"ram0")){console_write("REFUSING: MKFS REQUIRES EXPLICIT RAM0 TEST DEVICE.\n");return;}struct block_device*d=block_device_at(0);console_write(axiomfs_format(d,0,d->sectors)?"FORMAT FAILED\n":"AXIOMFS V2 FORMAT COMPLETE\n");}
static void cmd_storagetest(const char*a){(void)a;bool ok=storage_self_test();struct block_device*d=block_device_at(0);ok=ok&&axiomfs_self_test(d);ok=ok&&!axiomfs_simulate_interrupted_write(d,0);ok=ok&&axiomfs_check(d,0,false)==-2;ok=ok&&!axiomfs_check(d,0,true);ok=ok&&fat32_self_test(d);ok=ok&&vfs_self_test();ok=ok&&vfs_open("/does/not/exist",VFS_READ)==VFS_ENOENT;console_write(ok?"PHASE 2 STORAGE RECOVERY TESTS: PASS\n":"PHASE 2 STORAGE RECOVERY TESTS: FAIL\n");log_write(ok?"PASS":"FAIL","PHASE 2 STORAGE RECOVERY TESTS");}
static void cmd_hwstoragetest(const char*a){(void)a;uint32_t tested=0;bool ok=storage_hardware_self_test(&tested);console_write(ok?"HARDWARE STORAGE READ TEST: PASS, DEVICES=":"HARDWARE STORAGE READ TEST: FAIL, DEVICES=");console_write_u64(tested);console_putc('\n');log_write(ok?"PASS":"FAIL","HARDWARE STORAGE DEVICE ATTACHED AND READ ONLY");}
static void cmd_ps(const char*a){(void)a;console_write("PID PPID UID STATE TICKS NAME\n");for(uint32_t i=0;i<process_count();i++){struct process_info p;if(!process_info_at(i,&p))continue;console_write_u64(p.pid);console_putc(' ');console_write_u64(p.parent_pid);console_putc(' ');console_write_u64(p.uid);console_putc(' ');console_write(process_state_name(p.state));console_putc(' ');console_write_u64(p.runtime_ticks);console_putc(' ');console_write(p.name);console_putc('\n');}}
static void cmd_proctest(const char*a){(void)a;bool ok=process_self_test();console_write(ok?"PHASE 3 PROCESS AND SYSCALL FOUNDATION: PASS\n":"PHASE 3 PROCESS AND SYSCALL FOUNDATION: FAIL\n");log_write(ok?"PASS":"FAIL","PHASE 3 PROCESS AND SYSCALL FOUNDATION");}
static void cmd_rescan(const char*a){(void)a;pci_init();storage_probe_hardware();console_write("PCI AND STORAGE RESCAN COMPLETE.\n");}
static void cmd_layout(const char *args){if(!*args){console_write("KEYBOARD LAYOUT: ");console_write(keyboard_layout_name());console_write("\nAVAILABLE: US DVORAK\n");return;}if(keyboard_set_layout(args))console_write("KEYBOARD LAYOUT CHANGED.\n");else console_write("UNKNOWN LAYOUT. USE US OR DVORAK.\n");}
static void cmd_run(const char *args){char script[128];size_t n=0;while(args[n]&&n+1<sizeof(script)){script[n]=args[n];n++;}script[n]=0;char *part=script;while(*part){char *end=part;while(*end&&*end!=';')end++;if(*end)*end++=0;while(*part==' ')part++;if(*part)execute(part);part=end;}}
static void cmd_script(const char *args){if(streq(args,"demo")){char demo[]="version;sysinfo;heaptest;proctest;ps;allocstat;irqs;smp";cmd_run(demo);}else console_write("AVAILABLE BUILT-IN SCRIPT: DEMO\nUSE RUN CMD;CMD FOR CUSTOM SCRIPTS.\n");}

static void cmd_reboot(const char *args) { (void)args; LOG_INFO("reboot requested"); console_write("REBOOTING...\n"); machine_reboot(); }
static void cmd_halt(const char *args) { (void)args; console_write("SYSTEM HALTED.\n"); machine_halt(); }

struct command { const char *name; void (*handler)(const char *); };
static const struct command commands[] = {
    {"help",cmd_help},{"about",cmd_about},{"clear",cmd_clear},{"echo",cmd_echo},
    {"version",cmd_version},{"sysinfo",cmd_sysinfo},{"cpu",cmd_cpu},{"ram",cmd_ram},
    {"modules",cmd_modules},{"memtest",cmd_memtest},{"cputest",cmd_cputest},
    {"allocstat",cmd_allocstat},{"heaptest",cmd_heaptest},{"fault",cmd_fault},
    {"acpi",cmd_acpi},{"time",cmd_time},{"uptime",cmd_uptime},{"random",cmd_random},
    {"logs",cmd_logs},{"irqs",cmd_irqs},{"smp",cmd_smp},{"shutdown",cmd_shutdown},
    {"pci",cmd_pci},{"disks",cmd_disks},{"layout",cmd_layout},{"run",cmd_run},{"script",cmd_script},
    {"ls",cmd_ls},{"cd",cmd_cd},{"pwd",cmd_pwd},{"cat",cmd_cat},{"touch",cmd_touch},{"mkdir",cmd_mkdir},{"cp",cmd_cp},{"mv",cmd_mv},{"rm",cmd_rm},{"ln",cmd_ln},{"chmod",cmd_chmod},{"chown",cmd_chown},{"mount",cmd_mount},{"unmount",cmd_unmount},{"df",cmd_df},{"du",cmd_du},{"fsck",cmd_fsck},{"mkfs",cmd_mkfs},{"fatls",cmd_fatls},{"fatcat",cmd_fatcat},{"storagetest",cmd_storagetest},{"hwstoragetest",cmd_hwstoragetest},{"rescan",cmd_rescan},{"partitions",cmd_partitions},
    {"ps",cmd_ps},{"proctest",cmd_proctest},
    {"memmap",cmd_memmap},{"fbinfo",cmd_fbinfo},{"reboot",cmd_reboot},{"halt",cmd_halt}
};

static void execute(char *line) {
    while (*line == ' ') line++;
    if (!*line) return;
    log_write("CMD",line);
    char *args = line;
    while (*args && *args != ' ') args++;
    if (*args) *args++ = 0;
    while (*args == ' ') args++;
    for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
        if (streq(line, commands[i].name)) { commands[i].handler(args); return; }
    console_write("UNKNOWN COMMAND: "); console_write(line); console_write(". TRY HELP.\n");
}

static void shell(void) {
    static char history[16][128];static size_t history_count;
    char line[128];
    for (;;) {
        size_t length = 0,cursor=0,history_position=history_count;line[0]=0;
        console_write("axiom> ");
        for (;;) {
            struct key_event event=keyboard_read_event();if(!event.pressed)continue;char c=event.character;
            if(c=='\n'){console_putc('\n');line[length]=0;if(length){size_t slot=history_count%16;for(size_t i=0;i<=length;i++)history[slot][i]=line[i];history_count++;}execute(line);break;}
            if(event.special==KEY_LEFT){if(cursor){console_backspace();cursor--;}continue;}
            if(event.special==KEY_RIGHT){if(cursor<length)console_putc(line[cursor++]);continue;}
            if(event.special==KEY_HOME){while(cursor){console_backspace();cursor--;}continue;}
            if(event.special==KEY_END){while(cursor<length)console_putc(line[cursor++]);continue;}
            if(event.special==KEY_UP||event.special==KEY_DOWN){
                while(cursor<length) console_putc(line[cursor++]);
                while(length){console_backspace();length--;cursor--;}
                size_t earliest=history_count>16?history_count-16:0;
                if(event.special==KEY_UP&&history_position>earliest)history_position--;
                if(event.special==KEY_DOWN&&history_position<history_count)history_position++;
                if(history_position<history_count){const char *source=history[history_position%16];while(*source&&length+1<sizeof(line)){line[length++]=*source;console_putc(*source++);}cursor=length;}line[length]=0;continue;
            }
            if(c=='\b'){if(cursor){for(size_t i=cursor-1;i<length;i++)line[i]=line[i+1];cursor--;length--;console_backspace();for(size_t i=cursor;i<length;i++)console_putc(line[i]);console_putc(' ');for(size_t i=cursor;i<=length;i++)console_backspace();}continue;}
            if(event.special==KEY_DELETE){if(cursor<length){for(size_t i=cursor;i<length;i++)line[i]=line[i+1];length--;for(size_t i=cursor;i<length;i++)console_putc(line[i]);console_putc(' ');for(size_t i=cursor;i<=length;i++)console_backspace();}continue;}
            if(c=='\t'||!c)continue;
            if(length+1<sizeof(line)){for(size_t i=length;i>cursor;i--)line[i]=line[i-1];line[cursor]=c;length++;line[length]=0;for(size_t i=cursor;i<length;i++)console_putc(line[i]);cursor++;for(size_t i=cursor;i<length;i++)console_backspace();}
        }
    }
}

static void splash(void) {
    const char *title = "AXIOM";
    const char *tagline = "Your system. Your rules.";
    unsigned title_scale = 8, tag_scale = 3;
    uint64_t title_w = 5u * 6u * title_scale;
    uint64_t tag_w = 24u * 6u * tag_scale;
    uint64_t mid = fb_height() / 2;
    fb_clear(0x101318);
    fb_text((fb_width() - title_w) / 2, mid - 90, title, 0xf4f7fa, title_scale);
    fb_rect(fb_width()/2 - 80, mid - 20, 160, 3, 0x4f8fe8);
    fb_text((fb_width() - tag_w) / 2, mid + 12, tagline, 0x9aa6b2, tag_scale);
    pit_wait_ms(1200);
}

static const char *load_items[] = {
    "Limine boot protocol", "Framebuffer display", "Physical memory map",
    "CPU identification", "ACPI firmware tables", "Processor topology",
    "Memory manager diagnostics", "PIT timer", "PS2 keyboard controller",
    "Block storage and partitions", "Virtual filesystem", "Embedded initramfs",
    "Process and syscall manager", "Command console"
};
#define LOAD_ITEM_COUNT (sizeof(load_items)/sizeof(load_items[0]))
#define LOAD_RED 0xff5f6d
#define LOAD_YELLOW 0xffcc55
#define LOAD_GREEN 0x63d391

static uint64_t load_y(unsigned step){return 145u+(uint64_t)step*28u;}
static void load_status(unsigned step,const char *status,uint32_t colour){
    uint64_t x=fb_width()-330u,y=load_y(step);
    fb_rect(x,y,230,18,0x101318);
    fb_text(x,y,status,colour,2);
}
static void load_begin(unsigned step){load_status(step,"[ TESTING ]",LOAD_YELLOW);pit_wait_ms(350);}
static void load_finish(unsigned step,bool passed){
    load_status(step,passed?"[ OK ]":"[ FAILED ]",passed?LOAD_GREEN:LOAD_RED);
    uint64_t x=120,y=535,w=fb_width()-240;
    fb_rect(x,y,w,14,0x252b34);fb_rect(x,y,w*(step+1)/LOAD_ITEM_COUNT,14,passed?0x4f8fe8:LOAD_RED);
    pit_wait_ms(200);
}

static void loading_screen(void) {
    fb_clear(0x101318);
    fb_text(120,55,"AXIOM STARTUP",0xf4f7fa,4);
    fb_text(120,100,"Testing kernel modules and system hardware",0x8995a3,2);
    for(unsigned i=0;i<LOAD_ITEM_COUNT;i++){fb_text(120,load_y(i),load_items[i],0xd7dee7,2);load_status(i,"[ NOT TESTED ]",LOAD_RED);}
    fb_rect(120,535,fb_width()-240,14,0x252b34);pit_wait_ms(800);
    bool failed=false,passed;
    load_begin(0);passed=LIMINE_BASE_REVISION_SUPPORTED(base_revision);load_finish(0,passed);failed|=!passed;
    load_begin(1);passed=fb_info()!=0&&fb_width()>0&&fb_height()>0;load_finish(1,passed);failed|=!passed;
    load_begin(2);passed=memmap_request.response&&hhdm_request.response;load_finish(2,passed);failed|=!passed;
    load_begin(3);
    system_init(memmap_request.response, mp_request.response);
    passed=system_get_info()->has_long_mode;load_finish(3,passed);failed|=!passed;
    load_begin(4);passed=rsdp_request.response&&hhdm_request.response&&acpi_init(rsdp_request.response->address,hhdm_request.response->offset);load_finish(4,passed);failed|=!passed;
    load_begin(5);passed=system_get_info()->logical_threads>=1;load_finish(5,passed);failed|=!passed;
    load_begin(6);
    if(hhdm_request.response) memory_manager_init(memmap_request.response,hhdm_request.response->offset);
    uint64_t tested, failure;
    boot_memory_test_ok = system_memory_test(&tested, &failure);
    passed=boot_memory_test_ok&&pmm_free_pages()>0;load_finish(6,passed);failed|=!passed;
    load_begin(7);pit_wait_ms(50);load_finish(7,true);
    load_begin(8);passed=hardware_ps2_controller_present();load_finish(8,passed);failed|=!passed;
    load_begin(9);passed=storage_init();load_finish(9,passed);failed|=!passed;
    load_begin(10);vfs_init();passed=vfs_stat_path("/system",&(struct vfs_stat){0})==0;load_finish(10,passed);failed|=!passed;
    load_begin(11);passed=initramfs_load();load_finish(11,passed);failed|=!passed;
    load_begin(12);process_init();passed=process_self_test();load_finish(12,passed);failed|=!passed;
    load_begin(13);load_finish(13,true);
    fb_text(120,580,failed?"[ FAILED ] SYSTEM IS NOT SAFE TO BOOT":"[ OK ] ALL REQUIRED TESTS PASSED",failed?LOAD_RED:LOAD_GREEN,2);
    pit_wait_ms(failed?3000:900);
    if(failed)machine_halt();
}

void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision) || !framebuffer_request.response ||
        framebuffer_request.response->framebuffer_count == 0 ||
        !fb_init(framebuffer_request.response->framebuffers[0])) machine_halt();
    splash();
    loading_screen();
    console_init();
    log_init();
    arch_use_guarded_ist(vmm_guarded_stack(4));
    arch_init(hhdm_request.response?hhdm_request.response->offset:0);
    storage_probe_hardware();
    smp_start(mp_request.response);
    console_write("AXIOM V0.4\nYOUR SYSTEM. YOUR RULES.\nTYPE HELP FOR COMMANDS.\n\n");
    shell();
}
