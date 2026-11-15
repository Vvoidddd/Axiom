#include "checkpoint.h"
#include "snapshot.h"
#include "block.h"
#include "ahci.h"
#include "memory.h"
#include "vfs.h"
#include "log.h"

#define DATA_SECTORS 131072u
#define SLOT0 2048u
#define SLOT1 34816u
#define DATA_MAGIC 0x3130415441445841ull /* AXDATA01 */
#define SLOT_MAGIC 0x31544f4c53565841ull /* AXVSLOT1 */
struct disk_label {uint64_t magic;uint32_t version,sectors,slot0,slot1,checksum;uint8_t reserved[484];};
struct disk_slot {uint64_t magic,generation;uint32_t version,bytes,data_crc,header_crc;uint8_t reserved[480];};
_Static_assert(sizeof(struct disk_label)==512,"data label");
_Static_assert(sizeof(struct disk_slot)==512,"slot header");
static struct block_device*disk;
static uint64_t generation;
static unsigned current_slot;
static bool failed;
static uint32_t crc(const void*buffer,size_t size){const uint8_t*p=buffer;uint32_t c=~0u;while(size--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^(0xedb88320u&-(c&1u));}return ~c;}
static bool zero_sector(const void*p){const uint8_t*b=p;for(unsigned i=0;i<512;i++)if(b[i])return false;return true;}
static uint64_t slot_lba(unsigned i){return i?SLOT1:SLOT0;}
/* Direct driver I/O avoids stale block-cache entries for alternating generations. */
static int read_raw(uint64_t lba,uint32_t count,void*p){return !disk||!disk->present||lba>disk->sectors||count>disk->sectors-lba?-1:disk->read(disk,lba,count,p);}
static int write_raw(uint64_t lba,uint32_t count,void*p){return !disk||!disk->present||disk->readonly||!disk->write||lba>disk->sectors||count>disk->sectors-lba?-1:disk->write(disk,lba,count,p);}
bool checkpoint_active(void){return disk!=0&&!failed;}
const char*checkpoint_status(void){return failed?"Checkpoint failed; previous disk generation retained":disk?"Data disk enabled; use SAVE before power-off":"Live session: no prepared data disk";}
static bool valid_header(const struct disk_slot*s){return s->magic==SLOT_MAGIC&&s->version==1&&s->generation&&s->generation!=UINT64_MAX&&s->bytes>=144&&s->bytes<=SNAPSHOT_MAX&&s->header_crc==crc(s,28);}
int checkpoint_attach(void){
    disk=0;generation=0;current_slot=1;failed=false;
    struct disk_label label;
    for(unsigned i=0;i<block_device_count();i++){
        struct block_device*d=block_device_at(i);
        if(d->sector_size!=512||d->sectors!=DATA_SECTORS||d->read(d,0,1,&label)||label.magic!=DATA_MAGIC)continue;
        if(label.version!=1||label.sectors!=DATA_SECTORS||label.slot0!=SLOT0||label.slot1!=SLOT1||label.checksum!=crc(&label,24)||disk){failed=true;return -1;}
        disk=d;
    }
    if(!disk)return 0;
    if(!ahci_enable_checkpoint_writes(disk)){failed=true;return -1;}
    struct disk_slot slots[2];if(read_raw(SLOT0,1,&slots[0])||read_raw(SLOT1,1,&slots[1])){failed=true;return -1;}
    if(zero_sector(&slots[0])&&zero_sector(&slots[1])){log_write("CHECKPOINT","prepared data disk ready");return 1;}
    unsigned order=valid_header(&slots[1])&&(!valid_header(&slots[0])||slots[1].generation>slots[0].generation)?1:0;
    void*buffer=kmalloc(SNAPSHOT_MAX);if(!buffer){failed=true;return -1;}
    for(unsigned pass=0;pass<2;pass++){
        unsigned i=order^pass;const struct disk_slot*s=&slots[i];
        if(!valid_header(s)||read_raw(slot_lba(i)+1,(s->bytes+511)/512,buffer)||crc(buffer,s->bytes)!=s->data_crc)continue;
        if(vfs_snapshot_import(buffer,s->bytes))continue;
        generation=s->generation;current_slot=i;kfree(buffer);
        log_write("CHECKPOINT",pass?"recovered previous generation":"restored saved session");return 2;
    }
    kfree(buffer);failed=true;return -1;
}
bool checkpoint_save(void){
    if(!disk)return !failed;
    if(failed||generation==UINT64_MAX-1)return false;
    void*buffer=kmalloc(SNAPSHOT_MAX);if(!buffer)return false;
    struct vfs_security_context saved;vfs_capture_security_context(&saved);vfs_set_credentials(0,0);
    long bytes=vfs_snapshot_export(buffer,SNAPSHOT_MAX);vfs_restore_security_context(&saved);
    if(bytes<0){kfree(buffer);log_write("CHECKPOINT","snapshot rejected; no disk data changed");return false;}
    uint8_t*b=buffer;unsigned sectors=((unsigned)bytes+511)/512;
    for(unsigned i=(unsigned)bytes;i<sectors*512;i++)b[i]=0;
    struct disk_slot s={.magic=SLOT_MAGIC,.generation=generation+1,.version=1,.bytes=(uint32_t)bytes,.data_crc=crc(buffer,(size_t)bytes)};
    s.header_crc=crc(&s,28);unsigned next=current_slot^1;uint64_t lba=slot_lba(next);
    // Invalidate only the inactive generation, flush payload, commit header last.
    uint8_t invalid[512];for(unsigned i=0;i<512;i++)invalid[i]=0xa5;
    bool ok=!write_raw(lba,1,invalid)&&!disk->sync(disk)&&!write_raw(lba+1,sectors,buffer)&&!disk->sync(disk)&&!write_raw(lba,1,&s)&&!disk->sync(disk);
    kfree(buffer);
    if(ok){generation++;current_slot=next;log_write("CHECKPOINT","saved session committed");}
    else {failed=true;log_write("CHECKPOINT","disk write failed; previous generation retained");}
    return ok;
}
