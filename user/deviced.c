#include "libaxiom.h"
int main(void){for(;;){if(ax_service_heartbeat()<0)return 1;ax_sleep(250);}return 0;}
