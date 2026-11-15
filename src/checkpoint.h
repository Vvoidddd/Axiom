#ifndef AXIOM_CHECKPOINT_H
#define AXIOM_CHECKPOINT_H
#include <stdbool.h>
/* attach: -1 unsafe/corrupt, 0 live session, 1 blank prepared disk, 2 restored. */
int checkpoint_attach(void);
bool checkpoint_save(void);
bool checkpoint_active(void);
const char *checkpoint_status(void);
#endif
