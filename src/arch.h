#ifndef AXIOM_ARCH_H
#define AXIOM_ARCH_H
#include <stdint.h>
#include <stdbool.h>
void arch_init(uint64_t hhdm_offset);
void arch_interrupt_dispatch(uint64_t *frame);
uint64_t arch_interrupt_count(uint8_t vector);
bool arch_apic_active(void);
bool arch_ioapic_present(void);
void arch_use_guarded_ist(void *stack_top);
#endif
