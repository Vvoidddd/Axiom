#include "pci.h"
#include "io.h"
#include "console.h"
#include "hardware.h"
static struct pci_device devices[PCI_MAX_DEVICES];static uint32_t count;
static uint32_t raw_read(uint8_t b,uint8_t s,uint8_t f,uint8_t o){outl(0xcf8,0x80000000u|((uint32_t)b<<16)|((uint32_t)s<<11)|((uint32_t)f<<8)|(o&0xfc));return inl(0xcfc);}
static void raw_write(uint8_t b,uint8_t s,uint8_t f,uint8_t o,uint32_t v){outl(0xcf8,0x80000000u|((uint32_t)b<<16)|((uint32_t)s<<11)|((uint32_t)f<<8)|(o&0xfc));outl(0xcfc,v);}
void pci_init(void){count=0;for(unsigned b=0;b<256&&count<PCI_MAX_DEVICES;b++)for(unsigned s=0;s<32&&count<PCI_MAX_DEVICES;s++){if((raw_read(b,s,0,0)&0xffff)==0xffff)continue;unsigned fs=((raw_read(b,s,0,0x0c)>>16)&0x80)?8:1;for(unsigned f=0;f<fs&&count<PCI_MAX_DEVICES;f++){uint32_t id=raw_read(b,s,f,0);if((id&0xffff)==0xffff)continue;struct pci_device*d=&devices[count++];uint32_t t=raw_read(b,s,f,8);d->bus=b;d->slot=s;d->function=f;d->vendor_id=id;d->device_id=id>>16;d->revision=t;d->prog_if=t>>8;d->subclass=t>>16;d->class_code=t>>24;d->irq=raw_read(b,s,f,0x3c)&0xff;for(unsigned n=0;n<6;n++)d->bar[n]=raw_read(b,s,f,0x10+n*4);}}}
uint32_t pci_device_count(void){if(!count)pci_init();return count;}
uint32_t pci_storage_count(void){if(!count)pci_init();uint32_t n=0;for(uint32_t i=0;i<count;i++)if(devices[i].class_code==1)n++;return n;}
const struct pci_device*pci_device_at(uint32_t i){if(!count)pci_init();return i<count?&devices[i]:0;}
uint32_t pci_config_read32(const struct pci_device*d,uint8_t o){return raw_read(d->bus,d->slot,d->function,o);}
void pci_config_write32(const struct pci_device*d,uint8_t o,uint32_t v){raw_write(d->bus,d->slot,d->function,o,v);}
uint64_t pci_bar_address(const struct pci_device*d,unsigned n){if(n>=6)return 0;uint32_t lo=d->bar[n];if(lo&1)return lo&~3u;uint64_t a=lo&~15u;if(((lo>>1)&3)==2&&n<5)a|=(uint64_t)d->bar[n+1]<<32;return a;}
void pci_enable_bus_master(const struct pci_device*d){pci_config_write32(d,4,pci_config_read32(d,4)|6);}
static const char*class_name(uint8_t c){switch(c){case 1:return"STORAGE";case 2:return"NETWORK";case 3:return"DISPLAY";case 4:return"MULTIMEDIA";case 6:return"BRIDGE";case 12:return"SERIAL BUS";default:return"DEVICE";}}
void pci_list_devices(bool storage_only){if(!count)pci_init();unsigned lines=0;for(uint32_t i=0;i<count;i++){const struct pci_device*d=&devices[i];if(storage_only&&d->class_code!=1)continue;console_write("BDF ");console_write_u64(d->bus);console_putc(':');console_write_u64(d->slot);console_putc('.');console_write_u64(d->function);console_write(" ID ");console_write_hex((uint32_t)d->device_id<<16|d->vendor_id);console_putc(' ');console_write(class_name(d->class_code));console_putc('\n');if(++lines%30==0){console_write("-- MORE -- PRESS A KEY");keyboard_read_char();console_putc('\n');}}if(!lines)console_write("NO MATCHING PCI DEVICES.\n");}
