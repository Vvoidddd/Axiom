#include "mouse.h"
#include "io.h"

static struct mouse_decoder decoder;
static volatile struct mouse_event queue[128];
static volatile unsigned read_index,write_index;
static bool available;

bool mouse_decode(struct mouse_decoder*d,uint8_t byte,struct mouse_event*event){
    if(!d||!event)return false;
    if(!d->count&&!(byte&8))return false;
    d->packet[d->count++]=byte;
    if(d->count<3)return false;
    d->count=0;
    uint8_t flags=d->packet[0];
    if(flags&0xc0)return false;
    event->dx=(int)d->packet[1]-((flags&0x10)?256:0);
    event->dy=-((int)d->packet[2]-((flags&0x20)?256:0));
    event->buttons=flags&7;
    return true;
}
void mouse_receive(uint8_t byte){
    struct mouse_event event;
    if(available&&mouse_decode(&decoder,byte,&event)&&write_index-read_index<128)
        queue[write_index++&127]=event;
}
bool mouse_poll_event(struct mouse_event*event){
    if(!event)return false;
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
    bool found=read_index!=write_index;
    if(found)*event=queue[read_index++&127];
    __asm__ volatile("pushq %0; popfq"::"r"(flags):"memory","cc");
    return found;
}
static bool ready(void){for(unsigned i=0;i<100000;i++){if(!(inb(0x64)&2))return true;io_wait();}return false;}
static bool command(uint8_t value){if(!ready())return false;outb(0x64,value);return true;}
static bool data(uint8_t value){if(!ready())return false;outb(0x60,value);return true;}
static bool response(uint8_t*value,bool auxiliary){
    for(unsigned i=0;i<100000;i++){
        uint8_t status=inb(0x64);
        if(status&1){uint8_t byte=inb(0x60);if(((status&0x20)!=0)==auxiliary){*value=byte;return true;}}
        io_wait();
    }
    return false;
}
static bool mouse_command(uint8_t value){
    for(unsigned tries=0;tries<3;tries++){
        uint8_t ack;
        if(!command(0xd4)||!data(value)||!response(&ack,true))return false;
        if(ack==0xfa)return true;
        if(ack!=0xfe)return false;
    }
    return false;
}
bool mouse_init(void){
    /* Called during arch_init with interrupts off, before interactive input. */
    available=false;decoder=(struct mouse_decoder){0};read_index=write_index=0;
    uint8_t config=0;bool configured=false;
    if(!command(0xad)||!command(0xa7))goto done;
    for(unsigned i=0;i<32&&(inb(0x64)&1);i++)(void)inb(0x60);
    if(!command(0x20)||!response(&config,false))goto done;
    configured=true;
    if(!command(0xa8)||!mouse_command(0xf6)||!mouse_command(0xf4))goto done;
    available=true;
done:
    if(configured){
        /* Keep keyboard translation intact; enable IRQ12 only for a responding mouse. */
        uint8_t next=(config|1u)&~0x10u;
        if(available)next=(next|2u)&~0x20u;else next=(next&~2u)|0x20u;
        if(!command(0x60)||!data(next))available=false;
    }
    command(0xae);
    return available;
}
bool mouse_available(void){return available;}
