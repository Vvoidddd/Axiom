#ifndef AXIOM_SMP_H
#define AXIOM_SMP_H
#include <stdint.h>
#include <limine.h>
void smp_start(struct limine_mp_response *response);
uint64_t smp_online_count(void);
#endif
