#include "filesystems.h"
#include <stdint.h>
#define AXIOMFS_MAGIC 0x5346584d4f495841ull
enum filesystem_kind filesystem_probe(struct block_device*d,uint64_t lba){uint8_t b[512];if(block_read(d,lba,1,b))return FS_UNKNOWN;uint64_t magic=0;for(unsigned i=0;i<8;i++)magic|=(uint64_t)b[i]<<(i*8);if(magic==AXIOMFS_MAGIC)return FS_AXIOMFS;if(!fat32_validate(d,lba))return FS_FAT32;return FS_UNKNOWN;}
