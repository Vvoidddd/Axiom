#ifndef AXIOM_SMP_H
#define AXIOM_SMP_H
#include <stdint.h>
#include <stdbool.h>
#include <limine.h>
typedef bool (*smp_startup_probe_fn)(uint64_t processor_id);
void smp_set_startup_probe(smp_startup_probe_fn probe);
bool smp_start(struct limine_mp_response *response);
uint64_t smp_online_count(void);
bool smp_startup_probe_passed(void);
#endif
