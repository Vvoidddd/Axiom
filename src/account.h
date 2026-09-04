#ifndef AXIOM_ACCOUNT_H
#define AXIOM_ACCOUNT_H
#include <stdbool.h>
#include <stdint.h>
#define ACCOUNT_MAX 32
#define GROUP_MAX 16
#define ACCOUNT_GROUP_MAX 8
enum account_type {ACCOUNT_STANDARD,ACCOUNT_ADMIN,ACCOUNT_SERVICE,ACCOUNT_RECOVERY};
struct account_info {uint32_t uid,gid,creation_mask;enum account_type type;bool enabled,configured;char name[32],home[64],path[96],locale[16],timezone[32],startup[64];};
struct group_info {uint32_t gid,member_count;char name[32];uint32_t members[ACCOUNT_MAX];};
void account_init(void);
int account_create(const char *name,const char *password,enum account_type type,uint32_t *uid);
int account_create_service(const char *name,uint32_t *uid);
int account_authenticate(const char *name,const char *password,uint64_t now_ms);
int account_disable(const char *name,bool disabled);
int account_remove(const char *name);
int account_set_password(const char *name,const char *password);
int account_set_type(const char *name,enum account_type type);
int account_group_create(const char *name,uint32_t gid);
int account_group_add(const char *group,const char *user);
int account_group_remove(const char *group,const char *user);
const struct group_info *account_group_at(uint32_t index);
uint32_t account_group_count(void);
uint32_t account_groups_for_uid(uint32_t uid,uint32_t *groups,uint32_t capacity);
bool account_is_admin(uint32_t uid);
bool account_verify_current_password(const char *password,uint64_t now_ms);
int account_set_environment(const char *user,const char *key,const char *value);
const char *account_get_environment(const struct account_info *user,const char *key);
void account_audit(const char *event,const char *detail);
long account_read_audit(void *buffer,uint64_t offset,uint32_t size);
bool account_prepare_recovery(void);
bool account_take_recovery_key(char *output,uint32_t capacity);
bool account_rotate_recovery_key(char *output,uint32_t capacity);
bool account_recovery_self_test(void);
bool account_save(void);
bool account_load(void);
bool account_has_admin(void);
bool account_begin_session(uint32_t uid);
void account_end_session(void);
const struct account_info *account_current(void);
const struct account_info *account_at(uint32_t index);
uint32_t account_count(void);
const char *account_type_name(enum account_type type);
bool account_self_test(void);
bool account_database_recovery_self_test(void);
#endif
