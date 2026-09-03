#include "nvme.h"
#include "block.h"
#include "memory.h"
#include <stdbool.h>
#include <stdint.h>

#define NVME_QUEUE_DEPTH 16u
struct nvme_command { uint8_t opcode,flags; uint16_t id; uint32_t nsid,reserved0,reserved1; uint64_t metadata,prp1,prp2; uint32_t cdw10,cdw11,cdw12,cdw13,cdw14,cdw15; };
struct nvme_completion { uint32_t result,reserved; uint16_t head,qid,id,status; };
struct nvme_queue { struct nvme_command *submission; struct nvme_completion *completion; uint64_t submission_physical,completion_physical; uint16_t tail,head; uint8_t phase,qid; };
struct nvme_disk { volatile uint8_t *registers; uint32_t doorbell_stride,namespace_id; struct nvme_queue admin,io; uint8_t *bounce; uint64_t bounce_physical; };
static struct nvme_disk disks[8];static uint32_t disk_count;
static uint32_t read32(volatile uint8_t *base,uint32_t offset){return *(volatile uint32_t *)(base+offset);}static void write32(volatile uint8_t *base,uint32_t offset,uint32_t value){*(volatile uint32_t *)(base+offset)=value;}static uint64_t read64(volatile uint8_t *base,uint32_t offset){return *(volatile uint64_t *)(base+offset);}static void write64(volatile uint8_t *base,uint32_t offset,uint64_t value){*(volatile uint64_t *)(base+offset)=value;}
static void zero(void *memory,uint32_t bytes){uint8_t*p=memory;while(bytes--)*p++=0;}
static bool wait_ready(struct nvme_disk*d,bool ready){for(uint32_t i=0;i<50000000;i++){uint32_t status=read32(d->registers,0x1c);if(!!(status&1)==ready)return !(status&2);__asm__ volatile("pause");}return false;}
static void ring(struct nvme_disk*d,uint16_t qid,bool completion,uint16_t value){uint32_t index=(uint32_t)qid*2+(completion?1:0);write32(d->registers,0x1000+index*d->doorbell_stride,value);}
static int submit(struct nvme_disk*d,struct nvme_queue*q,const struct nvme_command*command){uint16_t id=q->tail;struct nvme_command c=*command;c.id=id;q->submission[q->tail]=c;q->tail=(q->tail+1)%NVME_QUEUE_DEPTH;ring(d,q->qid,false,q->tail);for(uint32_t spin=0;spin<50000000;spin++){struct nvme_completion *done=&q->completion[q->head];if((done->status&1)==q->phase){uint16_t status=done->status>>1;q->head=(q->head+1)%NVME_QUEUE_DEPTH;if(!q->head)q->phase^=1;ring(d,q->qid,true,q->head);return status?-(int)status:0;}__asm__ volatile("pause");}return -1;}
static bool allocate_queue(struct nvme_queue*q,uint8_t id){q->submission_physical=pmm_alloc();q->completion_physical=pmm_alloc();if(!q->submission_physical||!q->completion_physical)return false;q->submission=pmm_direct_map(q->submission_physical);q->completion=pmm_direct_map(q->completion_physical);zero(q->submission,4096);zero(q->completion,4096);q->tail=q->head=0;q->phase=1;q->qid=id;return true;}
static int identify(struct nvme_disk*d,uint32_t namespace_id,uint32_t cns){zero(d->bounce,4096);struct nvme_command command={.opcode=6,.nsid=namespace_id,.prp1=d->bounce_physical,.cdw10=cns};return submit(d,&d->admin,&command);}
static int read_blocks(struct block_device*block,uint64_t lba,uint32_t count,void*buffer){struct nvme_disk*d=block->driver;uint8_t*out=buffer;while(count){uint32_t amount=count>8?8:count;struct nvme_command command={.opcode=2,.nsid=d->namespace_id,.prp1=d->bounce_physical,.cdw10=lba,.cdw11=lba>>32,.cdw12=amount-1};if(submit(d,&d->io,&command))return -1;for(uint32_t i=0;i<amount*512;i++)out[i]=d->bounce[i];out+=amount*512;lba+=amount;count-=amount;}return 0;}
uint32_t nvme_attach(const struct pci_device*device){
    if(disk_count>=8)return 0;
    uint64_t physical=pci_bar_address(device,0);if(!physical)return 0;volatile uint8_t*registers=vmm_map_mmio(physical,0x2000);if(!registers)return 0;struct nvme_disk*d=&disks[disk_count];d->registers=registers;uint64_t cap=read64(registers,0);if((cap&0xffff)+1<NVME_QUEUE_DEPTH)return 0;d->doorbell_stride=4u<<(uint32_t)((cap>>32)&15);
    uint32_t cc=read32(registers,0x14);if(cc&1){write32(registers,0x14,cc&~1u);if(!wait_ready(d,false))return 0;}if(!allocate_queue(&d->admin,0))return 0;write32(registers,0x24,(NVME_QUEUE_DEPTH-1)|((NVME_QUEUE_DEPTH-1)<<16));write64(registers,0x28,d->admin.submission_physical);write64(registers,0x30,d->admin.completion_physical);write32(registers,0x14,(6u<<16)|(4u<<20)|1u);if(!wait_ready(d,true))return 0;
    d->bounce_physical=pmm_alloc();if(!d->bounce_physical)return 0;d->bounce=pmm_direct_map(d->bounce_physical);if(identify(d,0,1))return 0;char model[40];for(unsigned i=0;i<40;i++)model[i]=(char)d->bounce[24+i];if(!allocate_queue(&d->io,1))return 0;struct nvme_command create_cq={.opcode=5,.prp1=d->io.completion_physical,.cdw10=1u|((NVME_QUEUE_DEPTH-1)<<16),.cdw11=1u};if(submit(d,&d->admin,&create_cq))return 0;struct nvme_command create_sq={.opcode=1,.prp1=d->io.submission_physical,.cdw10=1u|((NVME_QUEUE_DEPTH-1)<<16),.cdw11=(1u<<16)|1u};if(submit(d,&d->admin,&create_sq))return 0;
    d->namespace_id=1;if(identify(d,1,0))return 0;uint64_t sectors=*(uint64_t*)d->bounce;uint8_t format=d->bounce[26]&15,shift=d->bounce[128+format*4+2];if(!sectors||shift!=9)return 0;struct block_device block={.sectors=sectors,.sector_size=512,.readonly=true,.present=true,.read=read_blocks,.driver=d};block.name[0]='n';block.name[1]='v';block.name[2]='0'+disk_count;block.name[3]=0;unsigned end=40;while(end&&model[end-1]==' ')end--;for(unsigned i=0;i<end;i++)block.model[i]=model[i];block.model[end]=0;if(block_register(&block)<0)return 0;disk_count++;return 1;
}
