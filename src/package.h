#ifndef AXIOM_PACKAGE_H
#define AXIOM_PACKAGE_H
#include <stdbool.h>
#include <stdint.h>
struct package_info{char name[32],version[16],executable[96];uint32_t abi;bool system_package;};
void package_init(void);
uint32_t package_count(void);
const struct package_info *package_at(uint32_t index);
bool package_self_test(void);
#endif
