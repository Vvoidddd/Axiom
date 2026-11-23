/* Exercise the production loader with bounded physical-memory and I/O faults. */
#include "../src/elf.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t file[16384],physical[132][4096];
static bool used[132],opened;
static size_t offset,file_size=sizeof(file);
static int fail_alloc=-1,fail_map=-1;
static uint64_t fail_read=UINT64_MAX;
static unsigned live,mapped;
uint64_t pmm_alloc(void){if(!fail_alloc)return 0;if(fail_alloc>0)fail_alloc--;for(unsigned i=1;i<132;i++)if(!used[i]){used[i]=true;live++;memset(physical[i],0,4096);return i*4096;}return 0;}
void pmm_free(uint64_t p){assert(p&&p/4096<132&&used[p/4096]);used[p/4096]=false;live--;}
void *pmm_direct_map(uint64_t p){assert(used[p/4096]);return physical[p/4096];}
bool vmm_space_create(struct vmm_space*s){s->root_physical=pmm_alloc();return s->root_physical!=0;}
bool vmm_space_destroy(struct vmm_space*s){pmm_free(s->root_physical);s->root_physical=0;return true;}
bool vmm_space_map_user(struct vmm_space*s,uint64_t v,uint64_t p,bool w,bool x){assert(s->root_physical&&used[p/4096]&&!(w&&x));if(!fail_map)return false;if(fail_map>0)fail_map--;if(v==STACK_BASE)assert(w&&!x);mapped++;return true;}
int vfs_stat_path(const char*p,struct vfs_stat*s){(void)p;*s=(struct vfs_stat){.size=file_size};return 0;}
int vfs_open(const char*p,uint32_t f){(void)p;assert(f==VFS_READ&&!opened);opened=true;offset=0;return 0;}
int vfs_close(int fd){assert(!fd&&opened);opened=false;return 0;}
long vfs_seek(int fd,uint64_t at){assert(!fd&&opened);offset=at;return at;}
long vfs_read(int fd,void*out,size_t n){assert(!fd&&opened);if(offset>=fail_read)return VFS_EIO;if(offset>=file_size)return 0;if(n>file_size-offset)n=file_size-offset;memcpy(out,file+offset,n);offset+=n;return n;}
static struct elf_header*h(void){return(void*)file;}
static struct program_header*ph(unsigned i){return(void*)(file+64+i*56);}
static void fixture(void){
    assert(!live&&!opened);memset(file,0,sizeof(file));mapped=0;file_size=sizeof(file);fail_alloc=fail_map=-1;fail_read=UINT64_MAX;
    *h()=(struct elf_header){.ident={0x7f,'E','L','F',2,1,1},.type=2,.machine=0x3e,.version=1,.entry=0x400000,.phoff=64,.ehsize=64,.phentsize=56,.phnum=2};
    *ph(0)=(struct program_header){.type=1,.flags=5,.offset=4096,.vaddr=0x400000,.filesz=16,.memsz=4096,.align=4096};
    *ph(1)=(struct program_header){.type=1,.flags=6,.offset=8192,.vaddr=0x402000,.filesz=8,.memsz=8192,.align=4096};
    memset(file+4096,0x90,16);memset(file+8192,0x5a,8);
}
static void rejected(void){struct user_image image;assert(elf_load_vfs("fixture",&image)<0);assert(!opened&&!live&&!image.page_count&&!image.space.root_physical);}
int main(void){
    fixture();struct user_image image;assert(!elf_load_vfs("fixture",&image));assert(image.page_count==4&&mapped==4);
    assert(((uint8_t*)pmm_direct_map(image.pages[0].physical_address))[0]==0x90);
    assert(((uint8_t*)pmm_direct_map(image.pages[1].physical_address))[8]==0);discard(&image);
    for(int i=0;i<5;i++){fixture();fail_alloc=i;rejected();}
    for(int i=0;i<4;i++){fixture();fail_map=i;rejected();}
    fixture();fail_read=8192;rejected();
    fixture();ph(0)->flags=7;rejected();assert(!mapped);
    fixture();h()->entry=ph(1)->vaddr;rejected();
    fixture();h()->entry=ph(0)->vaddr+16;rejected();
    fixture();ph(1)->vaddr=ph(0)->vaddr;rejected();
    fixture();ph(1)->vaddr=ph(0)->vaddr+32;ph(1)->align=1;rejected();
    fixture();ph(1)->vaddr=GUARD_BASE;rejected();
    fixture();ph(1)->memsz=UINT64_MAX;rejected();
    fixture();ph(1)->memsz=128*4096;rejected();
    fixture();ph(1)->filesz=UINT64_MAX;rejected();
    fixture();ph(1)->offset=UINT64_MAX;rejected();
    fixture();ph(1)->align=3;rejected();
    fixture();ph(1)->offset++;rejected();
    fixture();ph(1)->type=3;rejected();
    fixture();ph(1)->type=0x6474e551;ph(1)->flags=7;rejected();
    fixture();h()->phoff=UINT64_MAX;rejected();
    fixture();h()->phnum=33;rejected();
    fixture();h()->ehsize=0;rejected();
    fixture();h()->ident[6]=0;rejected();
    fixture();ph(1)->memsz=126*4096;assert(!elf_load_vfs("fixture",&image));assert(image.page_count==128);discard(&image);
    // Deterministic malformed-header corpus, including successful mutations.
    uint32_t random=0x12345678;
    for(unsigned i=0;i<1000;i++){fixture();random=random*1664525u+1013904223u;file[random%176]^=(uint8_t)(random>>24);if(!elf_load_vfs("fixture",&image))discard(&image);assert(!live&&!opened);}
    puts("PASS: ELF W^X, executable entry, disjoint segments, stack guard, bounds, failure cleanup and 1000 mutations");
}
