#ifndef AXIOM_FORM_H
#define AXIOM_FORM_H
#include "hardware.h"
#include <stddef.h>
struct form_field {char *text;size_t capacity,length,cursor,offset;const char *label;bool secret;};
bool form_edit(struct form_field *field,const struct key_event *event);
enum form_error {FORM_VALID,FORM_NAME,FORM_PASSWORD,FORM_CONFIRM};
enum form_error form_validate_setup(const char *name,const char *password,const char *confirmation);
void form_message(const char *message);
void account_form(bool setup,char name[32],char password[73],char confirmation[73]);
void form_wait_continue(void);
#endif
