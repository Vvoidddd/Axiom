#ifndef AXIOM_CONFIG_H
#define AXIOM_CONFIG_H
#include <stdbool.h>
#include <stdint.h>

#define CONFIG_ENTRY_MAX 32
struct config_entry { char key[40], value[128]; };
struct config_document {
    uint32_t schema, count;
    struct config_entry entries[CONFIG_ENTRY_MAX];
};

void config_clear(struct config_document *document, uint32_t schema);
const char *config_get(const struct config_document *document, const char *key);
bool config_set(struct config_document *document, const char *key, const char *value);
bool config_load(const char *path, uint32_t expected_schema,
                 const struct config_document *defaults,
                 struct config_document *document);
bool config_save_atomic(const char *path, const struct config_document *document);
bool config_system_init(void);
bool config_system_apply(void);
const struct config_document *config_system(void);
bool config_system_set(const char *key, const char *value);
bool config_user_init(const char *name, uint32_t uid, uint32_t gid);
void config_user_end(void);
const struct config_document *config_user(void);
bool config_user_set(const char *key, const char *value);
bool config_self_test(void);
#endif
