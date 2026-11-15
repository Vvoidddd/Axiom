#ifndef AXIOM_MOUSE_H
#define AXIOM_MOUSE_H
#include <stdbool.h>
#include <stdint.h>
struct mouse_event {int dx,dy;uint8_t buttons;};
struct mouse_decoder {uint8_t packet[3],count;};
bool mouse_decode(struct mouse_decoder *decoder,uint8_t byte,struct mouse_event *event);
bool mouse_init(void);
bool mouse_available(void);
void mouse_receive(uint8_t byte);
bool mouse_poll_event(struct mouse_event *event);
#endif
