#include "acpi.h"
#include "io.h"
#include <stddef.h>
#include <stdint.h>

struct rsdp {char sig[8];uint8_t checksum;char oem[6];uint8_t revision;uint32_t rsdt;uint32_t length;uint64_t xsdt;uint8_t ext_checksum,reserved[3];} __attribute__((packed));
struct sdt {char sig[4];uint32_t length;uint8_t revision,checksum;char oemid[6],oemtable[8];uint32_t oemrev,creator,creatorrev;} __attribute__((packed));
static uint64_t hhdm,lapic_address,ioapic_address,hpet_address;
static uint32_t cpu_count,table_count;
static uint32_t irq_gsi[16];
static uint16_t irq_flags[16];
static struct sdt *tables[32],*fadt;
static uint16_t pm1a_control,s5_type;
static bool valid(const void *data,size_t length){const uint8_t *p=data;uint8_t sum=0;while(length--)sum+=*p++;return sum==0;}
static bool sig(const char *a,const char *b){return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2]&&a[3]==b[3];}
static void parse_madt(struct sdt *header){
    uint8_t *p=(uint8_t *)header+sizeof(*header);lapic_address=*(uint32_t *)p;p+=8;
    uint8_t *end=(uint8_t *)header+header->length;
    while(p+2<=end&&p[1]>=2&&p+p[1]<=end){
        if(p[0]==0&&p[1]>=8&&(*(uint32_t *)(p+4)&1))cpu_count++;
        else if(p[0]==1&&p[1]>=12&&!ioapic_address)ioapic_address=*(uint32_t *)(p+4);
        else if(p[0]==2&&p[1]>=10&&p[3]<16){irq_gsi[p[3]]=*(uint32_t *)(p+4);irq_flags[p[3]]=*(uint16_t *)(p+8);}
        else if(p[0]==5&&p[1]>=12)lapic_address=*(uint64_t *)(p+4);
        p+=p[1];
    }
}
static uint32_t aml_integer(uint8_t **cursor){uint8_t *p=*cursor;uint32_t v;if(*p==0){v=0;p++;}else if(*p==1){v=1;p++;}else if(*p==0x0a){v=p[1];p+=2;}else if(*p==0x0b){v=*(uint16_t *)(p+1);p+=3;}else{v=*p++;}*cursor=p;return v;}
static void parse_s5(struct sdt *dsdt){
    uint8_t *p=(uint8_t *)dsdt+sizeof(*dsdt),*end=(uint8_t *)dsdt+dsdt->length;
    for(;p+8<end;p++)if(p[0]=='_'&&p[1]=='S'&&p[2]=='5'&&p[3]=='_'){
        p+=4;if(*p==0x12){p++;uint8_t package=*p++;if(package&0xc0)p+=(package>>6);p++;s5_type=(uint16_t)(aml_integer(&p)<<10);return;}
    }
}
bool acpi_init(void *address,uint64_t hhdm_offset){
    hhdm=hhdm_offset;for(unsigned i=0;i<16;i++)irq_gsi[i]=i;if(!address)return false;struct rsdp *r=address;
    if(!valid(r,r->revision>=2?r->length:20))return false;
    struct sdt *root=(void *)(uintptr_t)((r->revision>=2&&r->xsdt?r->xsdt:r->rsdt)+hhdm);
    if(!valid(root,root->length)) return false;
    bool is_xsdt=sig(root->sig,"XSDT");
    size_t width=is_xsdt?8:4,count=(root->length-sizeof(*root))/width;uint8_t *entries=(uint8_t *)root+sizeof(*root);
    for(size_t i=0;i<count;i++){uint64_t physical=is_xsdt?((uint64_t *)entries)[i]:((uint32_t *)entries)[i];struct sdt *t=(void *)(uintptr_t)(physical+hhdm);if(!valid(t,t->length))continue;if(table_count<32)tables[table_count++]=t;if(sig(t->sig,"APIC"))parse_madt(t);if(sig(t->sig,"FACP"))fadt=t;if(sig(t->sig,"HPET")&&t->length>=56)hpet_address=*(uint64_t *)((uint8_t *)t+44);}
    if(fadt){uint8_t *f=(uint8_t *)fadt;uint32_t dsdt=*(uint32_t *)(f+40);pm1a_control=(uint16_t)*(uint32_t *)(f+64);if(dsdt)parse_s5((void *)(uintptr_t)(dsdt+hhdm));}
    return true;
}
uint64_t acpi_lapic_address(void){return lapic_address;}
uint64_t acpi_ioapic_address(void){return ioapic_address;}
uint32_t acpi_cpu_count(void){return cpu_count;}
uint32_t acpi_table_count(void){return table_count;}
uint32_t acpi_irq_gsi(uint8_t irq){return irq<16?irq_gsi[irq]:irq;}
uint16_t acpi_irq_flags(uint8_t irq){return irq<16?irq_flags[irq]:0;}
uint64_t acpi_hpet_address(void){return hpet_address;}
const char *acpi_table_signature(uint32_t index){return index<table_count?tables[index]->sig:0;}
bool acpi_shutdown(void){if(!pm1a_control||!s5_type)return false;outw(pm1a_control,(uint16_t)(s5_type|(1u<<13)));return true;}
static uint8_t rtc_register(uint8_t reg){outb(0x70,reg);return inb(0x71);}
static uint8_t bcd(uint8_t x){return (uint8_t)((x&15)+10*(x>>4));}
void rtc_read(struct rtc_time *t){while(rtc_register(0x0a)&0x80){}uint8_t status=rtc_register(0x0b);t->second=rtc_register(0);t->minute=rtc_register(2);t->hour=rtc_register(4);t->day=rtc_register(7);t->month=rtc_register(8);uint8_t year=rtc_register(9);if(!(status&4)){t->second=bcd(t->second);t->minute=bcd(t->minute);t->hour=bcd(t->hour&0x7f);t->day=bcd(t->day);t->month=bcd(t->month);year=bcd(year);}t->year=2000+year;}
uint64_t entropy_seed(void){uint64_t value;unsigned char ok;__asm__ volatile("rdrand %0; setc %1":"=r"(value),"=qm"(ok));if(ok)return value;uint32_t low,high;__asm__ volatile("rdtsc":"=a"(low),"=d"(high));return (((uint64_t)high<<32)|low)^lapic_address;}
