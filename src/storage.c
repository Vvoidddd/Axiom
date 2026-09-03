#include "block.h"
#include "pci.h"
#include "memory.h"
#include "ahci.h"
#include "nvme.h"
#include "log.h"
static uint32_t ahci_count,nvme_count;static uint8_t*ramdisk;
static int ram_read(struct block_device*d,uint64_t l,uint32_t c,void*b){uint8_t*out=b,*in=d->driver;for(uint64_t i=0;i<(uint64_t)c*512;i++)out[i]=in[l*512+i];return 0;}static int ram_write(struct block_device*d,uint64_t l,uint32_t c,void*b){uint8_t*in=b,*out=d->driver;for(uint64_t i=0;i<(uint64_t)c*512;i++)out[l*512+i]=in[i];return 0;}
bool storage_init(void){block_init();pci_init();ahci_count=nvme_count=0;
 ramdisk=kmalloc(2*1024*1024);if(!ramdisk)return false;for(uint64_t i=0;i<2*1024*1024;i++)ramdisk[i]=0;struct block_device r={.name="ram0",.model="Axiom recovery RAM disk",.sectors=4096,.sector_size=512,.present=true,.read=ram_read,.write=ram_write,.driver=ramdisk};if(block_register(&r)<0)return false;
 block_scan_partitions();return true;}
void storage_probe_hardware(void){LOG_INFO("probing PCI storage controllers");for(uint32_t i=0;i<pci_device_count();i++){const struct pci_device*d=pci_device_at(i);if(d->class_code!=1)continue;if(d->subclass==6&&d->prog_if==1){ahci_count++;pci_enable_bus_master(d);ahci_attach(d);}if(d->subclass==8&&d->prog_if==2){nvme_count++;pci_enable_bus_master(d);nvme_attach(d);}}block_scan_partitions();LOG_INFO("PCI storage probe complete");}
uint32_t storage_ahci_controllers(void){return ahci_count;}uint32_t storage_nvme_controllers(void){return nvme_count;}
static void put32(uint8_t*p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}static void put64(uint8_t*p,uint64_t v){put32(p,v);put32(p+4,v>>32);}
bool storage_self_test(void){
 struct block_device*d=block_device_at(0);if(!d)return false;uint8_t a[512],b[512];
 for(unsigned i=0;i<512;i++)a[i]=(uint8_t)(i^0xa5);
 if(block_write(d,8,1,a)||block_flush(d)||block_read(d,8,1,b))return false;
 for(unsigned i=0;i<512;i++)if(a[i]!=b[i])return false;
 for(unsigned i=0;i<512;i++)a[i]=0;
 a[450]=0x83;put32(a+454,64);put32(a+458,128);a[510]=0x55;a[511]=0xaa;if(block_write(d,0,1,a)||block_flush(d))return false;block_scan_partitions();if(block_partition_count()!=1||block_partition_at(0)->gpt||block_partition_at(0)->first_lba!=64)return false;
 for(unsigned i=0;i<512;i++)a[i]=0;
 a[450]=0xee;put32(a+454,1);put32(a+458,4095);a[510]=0x55;a[511]=0xaa;if(block_write(d,0,1,a))return false;
 for(unsigned i=0;i<512;i++)a[i]=0;
 const char sig[]="EFI PART";for(unsigned i=0;i<8;i++)a[i]=sig[i];put64(a+72,2);put32(a+80,1);put32(a+84,128);if(block_write(d,1,1,a))return false;
 for(unsigned i=0;i<512;i++)a[i]=0;
 a[0]=0x28;put64(a+32,128);put64(a+40,255);if(block_write(d,2,1,a)||block_flush(d))return false;block_scan_partitions();if(block_partition_count()!=1||!block_partition_at(0)->gpt||block_partition_at(0)->first_lba!=128)return false;
 for(unsigned i=0;i<512;i++)a[i]=0x5a;
 if(block_write(d,7,1,a))return false;
 d->present=false;if(block_flush(d)==0){d->present=true;return false;}d->present=true;if(block_flush(d))return false;return true;
}
bool storage_hardware_self_test(uint32_t*tested){uint32_t count=0;uint8_t sector[512];for(uint32_t i=0;i<block_device_count();i++){struct block_device*d=block_device_at(i);if(d->driver==ramdisk)continue;if(!d->readonly||block_read(d,0,1,sector))return false;count++;}if(tested)*tested=count;return count>0;}
