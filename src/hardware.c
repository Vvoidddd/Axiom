#include "hardware.h"
#include "io.h"
#include <stdbool.h>

static volatile uint64_t ticks;
static volatile struct key_event key_queue[128];
static volatile unsigned key_read, key_write;
static bool shift_down, caps_lock;
static bool extended;
static unsigned layout;

static uint64_t interrupt_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r" (flags) :: "memory");
    return flags;
}

static void interrupt_restore(uint64_t flags) {
    __asm__ volatile ("pushq %0; popfq" :: "r" (flags) : "memory", "cc");
}

static const char keymap[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [12]='-',[13]='=',[14]='\b',[15]='\t',[16]='q',[17]='w',[18]='e',[19]='r',[20]='t',
    [21]='y',[22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\n',[30]='a',
    [31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',[39]=';',
    [40]='\'', [41]='`',[43]='\\',[44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',
    [50]='m',[51]=',',[52]='.',[53]='/',[57]=' '
};
static const char dvorak[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [12]='[',[13]=']',[14]='\b',[15]='\t',[16]='\'',[17]=',',[18]='.',[19]='p',[20]='y',
    [21]='f',[22]='g',[23]='c',[24]='r',[25]='l',[26]='/',[27]='=',[28]='\n',[30]='a',
    [31]='o',[32]='e',[33]='u',[34]='i',[35]='d',[36]='h',[37]='t',[38]='n',[39]='s',
    [40]='-',[43]='\\',[44]=';',[45]='q',[46]='j',[47]='k',[48]='x',[49]='b',[50]='m',
    [51]='w',[52]='v',[53]='z',[57]=' '
};

void pit_wait_ms(uint32_t milliseconds) {
    /* Channel 2 mode 0 gives a firmware-independent polling delay. */
    while (milliseconds) {
        uint32_t slice = milliseconds > 50 ? 50 : milliseconds;
        uint16_t count = (uint16_t)(1193u * slice);
        uint8_t gate = inb(0x61);
        outb(0x61, (uint8_t)((gate & ~2u) | 1u));
        outb(0x43, 0xb0);
        outb(0x42, (uint8_t)count);
        outb(0x42, (uint8_t)(count >> 8));
        while ((inb(0x61) & 0x20u) == 0) __asm__ volatile ("pause");
        outb(0x61, gate);
        milliseconds -= slice;
    }
}

char keyboard_read_char(void) {
    for(;;){struct key_event event=keyboard_read_event();if(event.pressed&&event.character)return event.character;}
}
struct key_event keyboard_read_event(void){
    for(;;){
        uint64_t flags=interrupt_save();
        if(key_read!=key_write){struct key_event e=key_queue[key_read++&127];interrupt_restore(flags);return e;}
        if(flags&(1ull<<9))__asm__ volatile("sti; hlt" ::: "memory");
        else __asm__ volatile("pause");
    }
}
bool keyboard_poll_event(struct key_event*event){
    if(!event)return false;
    bool found=false;
    uint64_t flags=interrupt_save();
    if(key_read!=key_write){*event=key_queue[key_read++&127];found=true;}
    interrupt_restore(flags);
    return found;
}
static bool same(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
bool keyboard_set_layout(const char *name){if(same(name,"us")){layout=0;return true;}if(same(name,"dvorak")){layout=1;return true;}return false;}
const char *keyboard_layout_name(void){return layout?"dvorak":"us";}

void hardware_irq_init(void){ uint16_t divisor=1193182u/1000u; outb(0x43,0x36);outb(0x40,(uint8_t)divisor);outb(0x40,(uint8_t)(divisor>>8)); }
void hardware_timer_irq(void){ ticks++; }
void hardware_keyboard_irq(void){
    uint8_t raw=inb(0x60);if(raw==0xe0){extended=true;return;}bool released=(raw&0x80)!=0;uint8_t code=raw&0x7f;
    if(code==42||code==54){shift_down=!released;return;}if(code==58&&!released){caps_lock=!caps_lock;return;}
    const char *map=layout?dvorak:keymap;char c=extended?0:map[code];uint8_t special=KEY_NONE;
    if(extended){if(code==75)special=KEY_LEFT;else if(code==77)special=KEY_RIGHT;else if(code==72)special=KEY_UP;else if(code==80)special=KEY_DOWN;else if(code==83)special=KEY_DELETE;else if(code==71)special=KEY_HOME;else if(code==79)special=KEY_END;extended=false;}
    if(c>='a'&&c<='z'&&(shift_down!=caps_lock))c=(char)(c-'a'+'A');
    struct key_event event={code,c,special,!released,shift_down,caps_lock};unsigned next=key_write+1;if(next-key_read<=128)key_queue[key_write++&127]=event;
}
uint64_t hardware_uptime_ms(void){return ticks;}
bool hardware_ps2_controller_present(void){return inb(0x64)!=0xff;}

void machine_reboot(void) {
    for (unsigned i = 0; i < 100000 && (inb(0x64) & 2u); i++) io_wait();
    outb(0x64, 0xfe);
    machine_halt();
}

__attribute__((noreturn)) void machine_halt(void) {
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
