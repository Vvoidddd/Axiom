#ifndef AXIOM_FORM_H
#define AXIOM_FORM_H
#include "hardware.h"
#include <stddef.h>
struct form_field {char *text;size_t capacity,length,cursor,offset;const char *label;bool secret;};
bool form_edit(struct form_field *field,const struct key_event *event);
void account_form(bool setup,char name[32],char password[73],char confirmation[73]);
void form_wait_continue(void);
#endif
