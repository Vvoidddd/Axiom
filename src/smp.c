#include "smp.h"
#include "log.h"
#include "memory.h"
static volatile uint64_t online=1;
extern void ap_entry_trampoline(struct limine_mp_info *cpu);
void ap_idle_c(struct limine_mp_info *cpu){
    (void)cpu;__atomic_add_fetch(&online,1,__ATOMIC_SEQ_CST);
    for(;;)__asm__ volatile("cli; hlt");
}
void smp_start(struct limine_mp_response *response){
    if(!response)return;
    for(uint64_t i=0;i<response->cpu_count;i++)if(response->cpus[i]->lapic_id!=response->bsp_lapic_id){void *stack=vmm_guarded_stack(4);if(!stack)continue;response->cpus[i]->extra_argument=(uint64_t)(uintptr_t)stack;__atomic_store_n(&response->cpus[i]->goto_address,ap_entry_trampoline,__ATOMIC_RELEASE);}
    for(volatile uint64_t spin=0;spin<100000000&&online<response->cpu_count;spin++)__asm__ volatile("pause");
    LOG_INFO("application processors entered idle scheduler");
}
uint64_t smp_online_count(void){return __atomic_load_n(&online,__ATOMIC_ACQUIRE);}
