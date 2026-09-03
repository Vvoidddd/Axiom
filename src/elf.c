#include "elf.h"
#include "vfs.h"

#define PT_LOAD 1u
#define PF_X 1u
#define PF_W 2u

struct elf_header{uint8_t ident[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;}__attribute__((packed));
struct program_header{uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;}__attribute__((packed));

static int read_exact(int fd,uint64_t offset,void*buffer,size_t size){if(vfs_seek(fd,offset)<0)return-1;uint8_t*out=buffer;size_t done=0;while(done<size){long n=vfs_read(fd,out+done,size-done);if(n<=0)return-1;done+=(size_t)n;}return 0;}
static uint64_t page_for(struct user_image*image,uint64_t address){uint64_t page=address&~0xfffull;for(uint32_t i=0;i<image->page_count;i++)if(image->pages[i].virtual_address==page)return image->pages[i].physical_address;return 0;}
static int map_segment_page(struct user_image*image,uint64_t address,bool writable,bool executable){if(page_for(image,address))return 0;if(image->page_count>=USER_IMAGE_MAX_PAGES)return-1;uint64_t physical=pmm_alloc();if(!physical||!vmm_space_map_user(&image->space,address&~0xfffull,physical,writable,executable))return-1;image->pages[image->page_count++]=(struct user_page){address&~0xfffull,physical};return 0;}

int elf_load_vfs(const char*path,struct user_image*image){
    if(!path||!image)return-1;
    struct vfs_stat stat;if(vfs_stat_path(path,&stat)||stat.directory||stat.size<sizeof(struct elf_header))return-1;
    int fd=vfs_open(path,VFS_READ);if(fd<0)return fd;
    struct elf_header header;if(read_exact(fd,0,&header,sizeof(header))){vfs_close(fd);return-1;}
    if(header.ident[0]!=0x7f||header.ident[1]!='E'||header.ident[2]!='L'||header.ident[3]!='F'||header.ident[4]!=2||header.ident[5]!=1||header.type!=2||header.machine!=0x3e||header.version!=1||header.phentsize!=sizeof(struct program_header)||!header.phnum||header.phnum>32||header.phoff>stat.size||(uint64_t)header.phnum*header.phentsize>stat.size-header.phoff){vfs_close(fd);return-1;}
    *image=(struct user_image){0};if(!vmm_space_create(&image->space)){vfs_close(fd);return-1;}image->entry=header.entry;
    for(uint16_t n=0;n<header.phnum;n++){struct program_header ph;if(read_exact(fd,header.phoff+(uint64_t)n*sizeof(ph),&ph,sizeof(ph))){vfs_close(fd);return-1;}if(ph.type!=PT_LOAD)continue;if(ph.filesz>ph.memsz||ph.offset>stat.size||ph.filesz>stat.size-ph.offset||ph.vaddr<0x1000||ph.vaddr>=0x0000800000000000ull||ph.memsz>0x0000800000000000ull-ph.vaddr){vfs_close(fd);return-1;}uint64_t first=ph.vaddr&~0xfffull,last=(ph.vaddr+ph.memsz+0xfffull)&~0xfffull;for(uint64_t page=first;page<last;page+=4096)if(map_segment_page(image,page,(ph.flags&PF_W)!=0,(ph.flags&PF_X)!=0)){vfs_close(fd);return-1;}uint64_t copied=0;while(copied<ph.filesz){uint64_t address=ph.vaddr+copied,physical=page_for(image,address);size_t offset=address&0xfff,amount=4096-offset;if(amount>ph.filesz-copied)amount=(size_t)(ph.filesz-copied);if(!physical||read_exact(fd,ph.offset+copied,(uint8_t*)pmm_direct_map(physical)+offset,amount)){vfs_close(fd);return-1;}copied+=amount;}}
    vfs_close(fd);uint64_t stack=pmm_alloc();if(!stack||!vmm_space_map_user(&image->space,0x7fffffffe000ull,stack,true,false)||!vmm_space_user_range_valid(&image->space,image->entry,1,false))return-1;image->pages[image->page_count++]=(struct user_page){0x7fffffffe000ull,stack};image->stack_top=0x7ffffffff000ull;return 0;
}
