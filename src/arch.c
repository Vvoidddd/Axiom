#include "arch.h"
#include "hardware.h"
#include "io.h"
#include "log.h"
#include "acpi.h"
#include "memory.h"
#include "process.h"
#include <stddef.h>

struct idt_entry { uint16_t low,selector; uint8_t ist,flags; uint16_t mid; uint32_t high,zero; } __attribute__((packed));
struct table_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
struct tss64 { uint32_t r0; uint64_t rsp[3]; uint64_t r1; uint64_t ist[7]; uint64_t r2; uint16_t r3,iomap; } __attribute__((packed));
static struct idt_entry idt[256];
static uint64_t gdt[7];
static struct tss64 tss;
static uint8_t ist_stack[16384] __attribute__((aligned(16)));
static uint8_t ring0_stack[16384] __attribute__((aligned(16)));
static uint64_t counts[256];
extern void *isr_stub_table[];
extern void isr_255(void);
extern void isr_128(void);
extern void arch_return_from_user(uint64_t result) __attribute__((noreturn));
static volatile uint32_t *lapic;
static volatile uint32_t *ioapic;
static bool lapic_enabled;
static void ioapic_write(uint8_t reg,uint32_t value){ioapic[0]=reg;ioapic[4]=value;}

static void idt_set(unsigned v, void *handler, uint8_t ist) {
    uint64_t a=(uint64_t)(uintptr_t)handler;
    idt[v]=(struct idt_entry){(uint16_t)a,0x08,ist,0x8e,(uint16_t)(a>>16),(uint32_t)(a>>32),0};
}
static void gdt_init(void) {
    gdt[0]=0; gdt[1]=0x00af9a000000ffffull; gdt[2]=0x00af92000000ffffull;
    if(!tss.ist[0]) tss.ist[0]=(uint64_t)(uintptr_t)(ist_stack+sizeof(ist_stack));
    tss.rsp[0]=(uint64_t)(uintptr_t)(ring0_stack+sizeof(ring0_stack));
    tss.iomap=sizeof(tss);
    uint64_t base=(uint64_t)(uintptr_t)&tss, limit=sizeof(tss)-1;
    gdt[3]=(limit&0xffff)|((base&0xffffff)<<16)|(0x89ull<<40)|(((limit>>16)&15)<<48)|(((base>>24)&255)<<56);
    gdt[4]=base>>32;gdt[5]=0x00aff2000000ffffull;gdt[6]=0x00affa000000ffffull;
    struct table_ptr p={sizeof(gdt)-1,(uint64_t)(uintptr_t)gdt};
    __asm__ volatile("lgdt %0\n pushq $0x08\n leaq 1f(%%rip),%%rax\n pushq %%rax\n lretq\n1:\n mov $0x10,%%ax\n mov %%ax,%%ds\n mov %%ax,%%es\n mov %%ax,%%ss\n mov $0x18,%%ax\n ltr %%ax"::"m"(p):"rax","memory");
}
static void pic_init(void) {
    outb(0x20,0x11); io_wait(); outb(0xa0,0x11); io_wait();
    outb(0x21,0x20); io_wait(); outb(0xa1,0x28); io_wait();
    outb(0x21,4); io_wait(); outb(0xa1,2); io_wait();
    outb(0x21,1); io_wait(); outb(0xa1,1); io_wait();
    outb(0x21,0xfc); outb(0xa1,0xff);
}
static void apic_init(uint64_t hhdm){
    (void)hhdm;
    uint64_t lapic_phys=acpi_lapic_address(),ioapic_phys=acpi_ioapic_address();
    if(lapic_phys&&vmm_map(0xffffd00000000000ull,lapic_phys,2|8|16)){uint32_t low,high;__asm__ volatile("rdmsr":"=a"(low),"=d"(high):"c"(0x1b));low|=0x800;__asm__ volatile("wrmsr"::"a"(low),"d"(high),"c"(0x1b));lapic=(void *)(uintptr_t)0xffffd00000000000ull;lapic[0xf0/4]=0x100|255;lapic_enabled=true;}
    if(ioapic_phys&&lapic_enabled&&vmm_map(0xffffd00000001000ull,ioapic_phys,2|8|16)){
        ioapic=(void *)(uintptr_t)0xffffd00000001000ull;ioapic[0]=1;uint32_t version=ioapic[4];unsigned maximum=(version>>16)&255;
        for(unsigned i=0;i<=maximum;i++){ioapic_write(0x10+2*i,1u<<16);ioapic_write(0x11+2*i,0);}
        uint32_t destination=(lapic[0x20/4]>>24)<<24;
        for(unsigned irq=0;irq<2;irq++){uint32_t gsi=acpi_irq_gsi(irq);if(gsi>maximum)continue;uint16_t flags=acpi_irq_flags(irq);uint32_t low=32+irq;if((flags&3)==3)low|=1u<<13;if((flags&12)==12)low|=1u<<15;ioapic_write(0x10+2*gsi,low);ioapic_write(0x11+2*gsi,destination);}
        outb(0x21,0xff);outb(0xa1,0xff);
    }
}
void arch_init(uint64_t hhdm) {
    __asm__ volatile("cli");KASSERT(tss.ist[0]!=0);gdt_init();
    for(unsigned i=0;i<34;i++) idt_set(i,isr_stub_table[i],i<32?1:0);
    idt_set(255,isr_255,0);
    idt_set(128,isr_128,0);idt[128].flags=0xee;
    struct table_ptr p={sizeof(idt)-1,(uint64_t)(uintptr_t)idt}; __asm__ volatile("lidt %0"::"m"(p));
    pic_init(); apic_init(hhdm); hardware_irq_init(); LOG_INFO("GDT TSS IDT PIC APIC and IRQ devices online");
    __asm__ volatile("sti");
}
void arch_interrupt_dispatch(uint64_t *f) {
    uint64_t vector=f[15], error=f[16], rip=f[17]; counts[vector&255]++;
    if(vector==32){ hardware_timer_irq();if(lapic_enabled)lapic[0xb0/4]=0;else outb(0x20,0x20);process_timer_interrupt(f,hardware_uptime_ms());return; }
    if(vector==33){ hardware_keyboard_irq();if(lapic_enabled)lapic[0xb0/4]=0;else outb(0x20,0x20);return; }
    if(vector==128){if(f[14]==0xa710)arch_return_from_user(f[9]);if(process_syscall_interrupt(f))return;f[14]=(uint64_t)process_syscall(f[14],f[9],f[8],f[11],f[5]);return;}
    if(vector==255) return;
    (void)error;(void)rip;panic_frame("CPU exception",f);
}
uint64_t arch_interrupt_count(uint8_t vector){ return counts[vector]; }
bool arch_apic_active(void){return lapic_enabled;}
bool arch_ioapic_present(void){return ioapic!=0;}
void arch_use_guarded_ist(void *stack_top){if(stack_top)tss.ist[0]=(uint64_t)(uintptr_t)stack_top;}
