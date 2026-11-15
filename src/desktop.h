#ifndef AXIOM_DESKTOP_H
#define AXIOM_DESKTOP_H
#include <stdbool.h>
void desktop_start(void);
void desktop_stop(void);
bool desktop_active(void);
long desktop_poll(void);
#endif
