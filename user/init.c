#include "libaxiom.h"
int main(void){ax_puts("[user:init] starting Axiom user session\n");if(ax_spawn("/system/bin/shell")<0){ax_puts("[user:init] shell launch failed\n");return 1;}if(ax_spawn("/apps/hello")<0){ax_puts("[user:init] sample app launch failed\n");return 1;}return 0;}
