#ifndef AXIOM_ACPI_H
#define AXIOM_ACPI_H
#include <stdbool.h>
#include <stdint.h>
struct rtc_time {uint16_t year;uint8_t month,day,hour,minute,second;};
bool acpi_init(void *rsdp,uint64_t hhdm_offset);
uint64_t acpi_lapic_address(void);
uint64_t acpi_ioapic_address(void);
uint32_t acpi_cpu_count(void);
uint32_t acpi_table_count(void);
uint32_t acpi_irq_gsi(uint8_t irq);
uint16_t acpi_irq_flags(uint8_t irq);
uint64_t acpi_hpet_address(void);
const char *acpi_table_signature(uint32_t index);
bool acpi_shutdown(void);
void rtc_read(struct rtc_time *time);
uint64_t entropy_seed(void);
#endif
