#include "smp.h"
#include "log.h"
#include "memory.h"
static volatile uint64_t online=1;
static volatile uint64_t probe_completed,probe_failed;
static smp_startup_probe_fn startup_probe;
extern void ap_entry_trampoline(struct limine_mp_info *cpu);
void ap_idle_c(struct limine_mp_info *cpu){
    __atomic_add_fetch(&online,1,__ATOMIC_SEQ_CST);
    if(startup_probe){if(!startup_probe(cpu->lapic_id))__atomic_add_fetch(&probe_failed,1,__ATOMIC_SEQ_CST);__atomic_add_fetch(&probe_completed,1,__ATOMIC_SEQ_CST);}
    for(;;)__asm__ volatile("cli; hlt");
}
void smp_set_startup_probe(smp_startup_probe_fn probe){startup_probe=probe;probe_completed=probe_failed=0;}
bool smp_start(struct limine_mp_response *response){
    uint64_t expected=response&&response->cpu_count?response->cpu_count:1;
    if(startup_probe){if(!startup_probe(response?response->bsp_lapic_id:0))probe_failed++;probe_completed++;}
    if(!response){LOG_INFO("single processor startup probe complete");return probe_failed==0;}
    for(uint64_t i=0;i<response->cpu_count;i++)if(response->cpus[i]->lapic_id!=response->bsp_lapic_id){void *stack=vmm_guarded_stack(4);if(!stack)continue;response->cpus[i]->extra_argument=(uint64_t)(uintptr_t)stack;__atomic_store_n(&response->cpus[i]->goto_address,ap_entry_trampoline,__ATOMIC_RELEASE);}
    for(volatile uint64_t spin=0;spin<100000000&&online<response->cpu_count;spin++)__asm__ volatile("pause");
    for(volatile uint64_t spin=0;spin<100000000&&probe_completed<expected;spin++)__asm__ volatile("pause");
    LOG_INFO("application processors entered idle scheduler");
    return online==response->cpu_count&&probe_completed==expected&&probe_failed==0;
}
uint64_t smp_online_count(void){return __atomic_load_n(&online,__ATOMIC_ACQUIRE);}
bool smp_startup_probe_passed(void){return probe_completed==online&&probe_failed==0;}
