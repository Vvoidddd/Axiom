#ifndef AXIOM_HARDWARE_H
#define AXIOM_HARDWARE_H

#include <stdint.h>
#include <stdbool.h>
enum key_special {KEY_NONE=0,KEY_LEFT=1,KEY_RIGHT=2,KEY_UP=3,KEY_DOWN=4,KEY_DELETE=5,KEY_HOME=6,KEY_END=7};
struct key_event {uint8_t scancode;char character;uint8_t special;bool pressed,shift,caps;};

void pit_wait_ms(uint32_t milliseconds);
char keyboard_read_char(void);
struct key_event keyboard_read_event(void);
bool keyboard_poll_event(struct key_event *event);
bool keyboard_set_layout(const char *name);
const char *keyboard_layout_name(void);
void hardware_irq_init(void);
void hardware_timer_irq(void);
void hardware_keyboard_irq(void);
uint64_t hardware_uptime_ms(void);
bool hardware_ps2_controller_present(void);
void machine_reboot(void);
__attribute__((noreturn)) void machine_halt(void);

#endif
