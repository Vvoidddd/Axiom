#include "elf.h"
#include "vfs.h"

#define PT_LOAD 1u
#define PF_X 1u
#define PF_W 2u
#define STACK_BASE 0x7fffffffe000ull
#define GUARD_BASE (STACK_BASE-4096)
struct elf_header{uint8_t ident[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;}__attribute__((packed));
struct program_header{uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;}__attribute__((packed));
static int read_exact(int fd,uint64_t offset,void*buffer,size_t size){if(vfs_seek(fd,offset)<0)return-1;uint8_t*out=buffer;size_t done=0;while(done<size){long n=vfs_read(fd,out+done,size-done);if(n<=0)return-1;done+=(size_t)n;}return 0;}
static uint64_t page_for(struct user_image*image,uint64_t address){uint64_t page=address&~0xfffull;for(uint32_t i=0;i<image->page_count;i++)if(image->pages[i].virtual_address==page)return image->pages[i].physical_address;return 0;}
static int map_segment_page(struct user_image*image,uint64_t address,bool writable,bool executable){
    if(image->page_count>=USER_IMAGE_MAX_PAGES)return-1;
    uint64_t physical=pmm_alloc();if(!physical)return-1;
    if(!vmm_space_map_user(&image->space,address&~0xfffull,physical,writable,executable)){pmm_free(physical);return-1;}
    image->pages[image->page_count++]=(struct user_page){address&~0xfffull,physical};return 0;
}
/* Only unpublished, exclusively owned images may be discarded here. */
static void discard(struct user_image*image){
    if(image->space.root_physical&&!vmm_space_destroy(&image->space))return;
    for(uint32_t i=0;i<image->page_count;i++)pmm_free(image->pages[i].physical_address);
    *image=(struct user_image){0};
}
int elf_load_vfs(const char*path,struct user_image*image){
    if(!path||!image)return-1;
    *image=(struct user_image){0};
    struct vfs_stat stat;if(vfs_stat_path(path,&stat)||stat.directory||stat.size<sizeof(struct elf_header))return-1;
    int fd=vfs_open(path,VFS_READ);if(fd<0)return fd;
    struct elf_header header;struct program_header segments[32];unsigned count=0,pages=1;bool entry_ok=false;
    if(read_exact(fd,0,&header,sizeof(header)))goto fail;
    if(header.ident[0]!=0x7f||header.ident[1]!='E'||header.ident[2]!='L'||header.ident[3]!='F'||header.ident[4]!=2||header.ident[5]!=1||header.ident[6]!=1||header.type!=2||header.machine!=0x3e||header.version!=1||header.ehsize!=sizeof(header)||header.phentsize!=sizeof(struct program_header)||!header.phnum||header.phnum>32||header.phoff>stat.size||(uint64_t)header.phnum*header.phentsize>stat.size-header.phoff)goto fail;
    /* Validate all segments before allocating. Disjoint PT_LOAD pages prevent
     * segment order from silently choosing conflicting permissions. */
    for(uint16_t i=0;i<header.phnum;i++){
        struct program_header ph;if(read_exact(fd,header.phoff+(uint64_t)i*sizeof(ph),&ph,sizeof(ph)))goto fail;
        if(ph.type==2||ph.type==3||(ph.type==0x6474e551u&&(ph.flags&PF_X)))goto fail;
        if(ph.type!=PT_LOAD)continue;
        if((ph.flags&~7u)||((ph.flags&(PF_W|PF_X))==(PF_W|PF_X))||ph.filesz>ph.memsz||ph.offset>stat.size||ph.filesz>stat.size-ph.offset||ph.vaddr<0x1000||ph.vaddr>=GUARD_BASE||ph.memsz>GUARD_BASE-ph.vaddr)goto fail;
        if(ph.align>1&&((ph.align&(ph.align-1))||((ph.vaddr-ph.offset)&(ph.align-1))))goto fail;
        if(!ph.memsz)continue;
        uint64_t first=ph.vaddr&~0xfffull,last=(ph.vaddr+ph.memsz+0xfffull)&~0xfffull;
        uint64_t needed=(last-first)/4096;if(needed>USER_IMAGE_MAX_PAGES-pages)goto fail;
        for(unsigned j=0;j<count;j++){
            uint64_t other_first=segments[j].vaddr&~0xfffull,other_last=(segments[j].vaddr+segments[j].memsz+0xfffull)&~0xfffull;
            if(first<other_last&&other_first<last)goto fail;
        }
        if((ph.flags&PF_X)&&header.entry>=ph.vaddr&&header.entry-ph.vaddr<ph.filesz)entry_ok=true;
        pages+=(unsigned)needed;segments[count++]=ph;
    }
    if(!entry_ok||!vmm_space_create(&image->space))goto fail;
    image->entry=header.entry;
    for(unsigned i=0;i<count;i++){
        const struct program_header*ph=&segments[i];uint64_t first=ph->vaddr&~0xfffull,last=(ph->vaddr+ph->memsz+0xfffull)&~0xfffull;
        for(uint64_t page=first;page<last;page+=4096)if(map_segment_page(image,page,(ph->flags&PF_W)!=0,(ph->flags&PF_X)!=0))goto fail;
        uint64_t copied=0;
        while(copied<ph->filesz){
            uint64_t address=ph->vaddr+copied,physical=page_for(image,address);size_t offset=address&0xfff,amount=4096-offset;
            if(amount>ph->filesz-copied)amount=(size_t)(ph->filesz-copied);
            if(!physical||read_exact(fd,ph->offset+copied,(uint8_t*)pmm_direct_map(physical)+offset,amount))goto fail;
            copied+=amount;
        }
    }
    if(map_segment_page(image,STACK_BASE,true,false))goto fail;
    image->stack_top=STACK_BASE+4096;vfs_close(fd);return 0;
fail:
    vfs_close(fd);discard(image);return-1;
}
