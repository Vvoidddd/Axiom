#include "ahci.h"
#include "block.h"
#include "memory.h"
#include <stdbool.h>
#include <stdint.h>

#define SATA_SIG_ATA 0x00000101u
#define CMD_ST 1u
#define CMD_FRE (1u << 4)
#define CMD_FR (1u << 14)
#define CMD_CR (1u << 15)
#define ATA_IDENTIFY 0xecu
#define ATA_READ_DMA_EXT 0x25u

struct hba_port { volatile uint32_t clb,clbu,fb,fbu,is,ie,cmd,r0,tfd,sig,ssts,sctl,serr,sact,ci,sntf,fbs,r1[11],vendor[4]; };
struct hba_memory { volatile uint32_t cap,ghc,is,pi,vs,ccc_ctl,ccc_pts,em_loc,em_ctl,cap2,bohc; uint8_t reserved[116],vendor[96]; struct hba_port port[32]; };
struct command_header { uint16_t flags,prdt_length; volatile uint32_t transferred; uint32_t table_low,table_high,reserved[4]; };
struct prdt_entry { uint32_t base_low,base_high,reserved,byte_count; };
struct command_table { uint8_t fis[64],atapi[16],reserved[48]; struct prdt_entry prdt[1]; };
struct register_fis { uint8_t type,flags,command,feature0,lba0,lba1,lba2,device,lba3,lba4,lba5,feature1; uint16_t count; uint8_t icc,control,reserved[4]; };
struct ahci_disk { struct hba_port *port; struct command_header *headers; struct command_table *table; uint8_t *bounce; uint64_t bounce_physical; };
static struct ahci_disk disks[8];
static uint32_t disk_count;

static bool wait_clear(volatile uint32_t *reg,uint32_t mask){for(uint32_t i=0;i<10000000;i++){if(!(*reg&mask))return true;__asm__ volatile("pause");}return false;}
static bool stop(struct hba_port *p){p->cmd&=~CMD_ST;if(!wait_clear(&p->cmd,CMD_CR))return false;p->cmd&=~CMD_FRE;return wait_clear(&p->cmd,CMD_FR);}
static void start(struct hba_port *p){p->cmd|=CMD_FRE;p->cmd|=CMD_ST;}
static void zero(void *memory,uint32_t bytes){uint8_t *p=memory;while(bytes--)*p++=0;}
static int issue(struct ahci_disk *disk,uint8_t command,uint64_t lba,uint16_t sectors){
    struct hba_port *p=disk->port;if(!wait_clear(&p->tfd,0x88))return -1;p->is=0xffffffffu;
    struct command_header *h=&disk->headers[0];h->flags=5;h->prdt_length=1;h->transferred=0;zero(disk->table,4096);
    struct register_fis *f=(void *)disk->table->fis;f->type=0x27;f->flags=0x80;f->command=command;f->device=1u<<6;
    f->lba0=lba;f->lba1=lba>>8;f->lba2=lba>>16;f->lba3=lba>>24;f->lba4=lba>>32;f->lba5=lba>>40;f->count=sectors;
    disk->table->prdt[0].base_low=disk->bounce_physical;disk->table->prdt[0].base_high=disk->bounce_physical>>32;disk->table->prdt[0].byte_count=(uint32_t)sectors*512-1;
    p->ci|=1;for(uint32_t i=0;i<30000000;i++){if(!(p->ci&1))return (p->is&(1u<<30))?-1:0;if(p->is&(1u<<30))return -1;__asm__ volatile("pause");}return -1;
}
static int read_blocks(struct block_device *block,uint64_t lba,uint32_t count,void *buffer){struct ahci_disk *disk=block->driver;uint8_t *out=buffer;while(count){uint16_t amount=count>8?8:(uint16_t)count;if(issue(disk,ATA_READ_DMA_EXT,lba,amount))return -1;for(uint32_t i=0;i<(uint32_t)amount*512;i++)out[i]=disk->bounce[i];out+=(uint32_t)amount*512;lba+=amount;count-=amount;}return 0;}
static uint64_t identify_size(const uint16_t *id){uint64_t n=(uint64_t)id[100]|(uint64_t)id[101]<<16|(uint64_t)id[102]<<32|(uint64_t)id[103]<<48;if(!n)n=(uint64_t)id[60]|(uint64_t)id[61]<<16;return n;}
static bool attach_port(struct hba_port *port){
    if((port->ssts&15)!=3||((port->ssts>>8)&15)!=1||port->sig!=SATA_SIG_ATA||disk_count>=8)return false;
    uint64_t command_list=pmm_alloc(),fis=pmm_alloc(),table=pmm_alloc(),bounce=pmm_alloc();if(!command_list||!fis||!table||!bounce)return false;
    if(!stop(port))return false;
    struct ahci_disk *disk=&disks[disk_count];zero(pmm_direct_map(command_list),4096);zero(pmm_direct_map(fis),4096);zero(pmm_direct_map(table),4096);
    port->clb=command_list;port->clbu=command_list>>32;port->fb=fis;port->fbu=fis>>32;disk->port=port;disk->headers=pmm_direct_map(command_list);disk->table=pmm_direct_map(table);disk->bounce=pmm_direct_map(bounce);disk->bounce_physical=bounce;
    disk->headers[0].table_low=table;disk->headers[0].table_high=table>>32;port->serr=0xffffffffu;port->is=0xffffffffu;start(port);if(issue(disk,ATA_IDENTIFY,0,1))return false;
    const uint16_t *id=(const uint16_t *)disk->bounce;uint64_t sectors=identify_size(id);if(!sectors)return false;struct block_device block={.sectors=sectors,.sector_size=512,.readonly=true,.present=true,.read=read_blocks,.driver=disk};block.name[0]='s';block.name[1]='d';block.name[2]=(char)('0'+disk_count);block.name[3]=0;
    unsigned at=0;for(unsigned i=27;i<47&&at<39;i++){block.model[at++]=id[i]>>8;block.model[at++]=id[i];}while(at&&block.model[at-1]==' ')at--;block.model[at]=0;if(block_register(&block)<0)return false;disk_count++;return true;
}
uint32_t ahci_attach(const struct pci_device *device){uint64_t base=pci_bar_address(device,5);if(!base)return 0;struct hba_memory *hba=vmm_map_mmio(base,4096);if(!hba)return 0;hba->ghc|=1u<<31;uint32_t before=disk_count,ports=hba->pi;for(unsigned i=0;i<32;i++)if(ports&(1u<<i))attach_port(&hba->port[i]);return disk_count-before;}
