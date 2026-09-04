#ifndef AXIOM_ACCOUNT_H
#define AXIOM_ACCOUNT_H
#include <stdbool.h>
#include <stdint.h>
#define ACCOUNT_MAX 32
enum account_type {ACCOUNT_STANDARD,ACCOUNT_ADMIN,ACCOUNT_SERVICE,ACCOUNT_RECOVERY};
struct account_info {uint32_t uid,gid;enum account_type type;bool enabled,configured;char name[32];};
void account_init(void);
int account_create(const char *name,const char *password,enum account_type type,uint32_t *uid);
int account_authenticate(const char *name,const char *password,uint64_t now_ms);
int account_disable(const char *name,bool disabled);
bool account_begin_session(uint32_t uid);
void account_end_session(void);
const struct account_info *account_current(void);
const struct account_info *account_at(uint32_t index);
uint32_t account_count(void);
bool account_self_test(void);
#endif
