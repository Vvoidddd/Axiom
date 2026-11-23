#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limine.h>
#include "memory.h"

void *memcpy(void *restrict dst, const void *restrict src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    unsigned char *d = dst;
    while (n--) *d++ = (unsigned char)value;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    while (n--) if (*x != *y) return *x - *y; else { x++; y++; }
    return 0;
}

#define PAGE_SIZE 4096ull
#define PTE_PRESENT 1ull
#define PTE_WRITE 2ull
#define PTE_USER 4ull
#define PTE_NX (1ull<<63)
struct free_page { struct free_page *next; };
static struct free_page *free_head;
static uint64_t direct_offset, free_count, used_count;
static uint64_t next_virtual=0xffffc00000000000ull, heap_used;
static int allocation_fail_after=-1; /* Only set inside the interrupt-masked self-test. */
static void *physical_pointer(uint64_t physical);
static uint64_t *leaf_entry(uint64_t virtual_address);
static uint64_t *leaf_entry_root(uint64_t root,uint64_t virtual_address,bool create,uint64_t flags){
    uint64_t*table=physical_pointer(root&~0xfffull),*entries[3]={0},old[3]={0};
    unsigned shifts[]={39,30,21},depth=0;
    for(;depth<3;depth++){
        entries[depth]=&table[(virtual_address>>shifts[depth])&511];old[depth]=*entries[depth];
        if(old[depth]&(1ull<<7))break; /* Never interpret a huge page as a table. */
        if(!(old[depth]&PTE_PRESENT)){
            if(!create)break;
            uint64_t page=pmm_alloc();if(!page)break;
            *entries[depth]=page|PTE_PRESENT|PTE_WRITE;
        }
        table=physical_pointer(*entries[depth]&0x000ffffffffff000ull);
    }
    if(depth<3){
        while(depth){depth--;if(!(old[depth]&PTE_PRESENT)){pmm_free(*entries[depth]&0x000ffffffffff000ull);*entries[depth]=old[depth];}}
        return 0;
    }
    if(flags&PTE_USER)for(unsigned i=0;i<3;i++)*entries[i]|=PTE_USER;
    return &table[(virtual_address>>12)&511];
}

static void *physical_pointer(uint64_t physical){ return (void *)(uintptr_t)(physical+direct_offset); }
void *pmm_direct_map(uint64_t physical){return physical_pointer(physical);}
uint64_t pmm_physical_address(const void *virtual_address){
    uint64_t address=(uint64_t)(uintptr_t)virtual_address;
    if(address>=direct_offset)return address-direct_offset;
    uint64_t *entry=leaf_entry(address);
    return entry&&(*entry&PTE_PRESENT)?(*entry&0x000ffffffffff000ull)|(address&0xfffull):0;
}
void memory_manager_init(struct limine_memmap_response *map, uint64_t hhdm_offset){
    direct_offset=hhdm_offset; free_head=0; free_count=used_count=0;
    if(!map)return;
    for(uint64_t e=0;e<map->entry_count;e++) if(map->entries[e]->type==LIMINE_MEMMAP_USABLE){
        uint64_t begin=(map->entries[e]->base+PAGE_SIZE-1)&~(PAGE_SIZE-1);
        uint64_t end=(map->entries[e]->base+map->entries[e]->length)&~(PAGE_SIZE-1);
        for(uint64_t p=begin;p<end;p+=PAGE_SIZE){struct free_page *node=physical_pointer(p);node->next=free_head;free_head=node;free_count++;}
    }
}
uint64_t pmm_alloc(void){
    if(allocation_fail_after==0)return 0;
    if(allocation_fail_after>0)allocation_fail_after--;
    if(!free_head) return 0;
    struct free_page *node=free_head;free_head=node->next;free_count--;used_count++;
    uint64_t physical=(uint64_t)(uintptr_t)node-direct_offset; memset(node,0,PAGE_SIZE); return physical;
}
void pmm_free(uint64_t physical){if(!physical)return;struct free_page *node=physical_pointer(physical);node->next=free_head;free_head=node;free_count++;if(used_count)used_count--;}
uint64_t pmm_free_pages(void){return free_count;}
uint64_t pmm_used_pages(void){return used_count;}

bool vmm_map(uint64_t virtual_address,uint64_t physical_address,uint64_t flags){
    if((flags&PTE_USER)&&(flags&PTE_WRITE)&&!(flags&PTE_NX))return false;
    uint64_t cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    uint64_t*entry=leaf_entry_root(cr3,virtual_address,true,flags);if(!entry)return false;
    *entry=(physical_address&~0xfffull)|PTE_PRESENT|flags;
    __asm__ volatile("invlpg (%0)"::"r"(virtual_address):"memory");return true;
}
bool vmm_map_user(uint64_t virtual_address,uint64_t physical_address,bool writable,bool executable){
    if(virtual_address>=0x0000800000000000ull||(virtual_address&0xfff)||(physical_address&0xfff))return false;
    return vmm_map(virtual_address,physical_address,PTE_USER|(writable?PTE_WRITE:0)|(executable?0:PTE_NX));
}
bool vmm_user_range_valid(uint64_t address,size_t bytes,bool writable){
    if(!bytes)return true;
    if(address>=0x0000800000000000ull||bytes>0x0000800000000000ull-address)return false;
    uint64_t first=address&~0xfffull,last=(address+bytes-1)&~0xfffull;
    for(uint64_t page=first;;page+=PAGE_SIZE){uint64_t*entry=leaf_entry(page);if(!entry||!(*entry&PTE_PRESENT)||!(*entry&PTE_USER)||(writable&&!(*entry&PTE_WRITE)))return false;if(page==last)break;}
    return true;
}
bool vmm_space_create(struct vmm_space*space){
    if(!space)return false;
    uint64_t root=pmm_alloc();if(!root)return false;uint64_t current;__asm__ volatile("mov %%cr3,%0":"=r"(current));
    uint64_t*destination=physical_pointer(root),*source=physical_pointer(current&~0xfffull);for(unsigned i=256;i<512;i++)destination[i]=source[i];
    space->root_physical=root;return true;
}
bool vmm_space_map_user(struct vmm_space*space,uint64_t virtual_address,uint64_t physical_address,bool writable,bool executable){
    if(writable&&executable)return false;
    if(!space||!space->root_physical||virtual_address>=0x0000800000000000ull||(virtual_address&0xfff)||(physical_address&0xfff))return false;
    uint64_t flags=PTE_USER|(writable?PTE_WRITE:0)|(executable?0:PTE_NX),*entry=leaf_entry_root(space->root_physical,virtual_address,true,flags);if(!entry)return false;
    *entry=(physical_address&~0xfffull)|PTE_PRESENT|flags;return true;
}
bool vmm_space_user_range_valid(const struct vmm_space*space,uint64_t address,size_t bytes,bool writable){
    if(!space||!space->root_physical)return false;
    if(!bytes)return true;
    if(address>=0x0000800000000000ull||bytes>0x0000800000000000ull-address)return false;
    uint64_t first=address&~0xfffull,last=(address+bytes-1)&~0xfffull;for(uint64_t page=first;;page+=PAGE_SIZE){uint64_t*entry=leaf_entry_root(space->root_physical,page,false,0);if(!entry||!(*entry&PTE_PRESENT)||!(*entry&PTE_USER)||(writable&&!(*entry&PTE_WRITE)))return false;if(page==last)break;}return true;
}
static void destroy_user_tables(uint64_t physical,unsigned level){
    uint64_t*table=physical_pointer(physical);
    if(level>1)for(unsigned i=0;i<512;i++)if((table[i]&PTE_PRESENT)&&!(table[i]&(1ull<<7)))destroy_user_tables(table[i]&0x000ffffffffff000ull,level-1);
    pmm_free(physical);
}
bool vmm_space_destroy(struct vmm_space*space){
    if(!space||!space->root_physical)return false;
    uint64_t current;__asm__ volatile("mov %%cr3,%0":"=r"(current));
    if((current&~0xfffull)==space->root_physical)return false;
    uint64_t*root=physical_pointer(space->root_physical);
    for(unsigned i=0;i<256;i++)if(root[i]&PTE_PRESENT)destroy_user_tables(root[i]&0x000ffffffffff000ull,3);
    pmm_free(space->root_physical);space->root_physical=0;return true;
}
static uint64_t *active_root(void){uint64_t root;__asm__ volatile("mov %%cr3,%0":"=r"(root));return physical_pointer(root&~0xfffull);}
static bool table_empty(const uint64_t*table){for(unsigned i=0;i<512;i++)if(table[i]&PTE_PRESENT)return false;return true;}
/* Only prune levels below the PDPT: user CR3s share kernel PDPT pointers.
 * Freeing an existing kernel PDPT would leave those roots dangling. */
static void unmap_range(uint64_t base,uint64_t pages,bool release){
    uint64_t*root=active_root();
    for(uint64_t i=0;i<pages;i++){
        uint64_t address=base+i*PAGE_SIZE,*a=&root[(address>>39)&511];if(!(*a&PTE_PRESENT))continue;
        uint64_t*pdpt=physical_pointer(*a&0x000ffffffffff000ull),*b=&pdpt[(address>>30)&511];if(!(*b&PTE_PRESENT)||(*b&(1ull<<7)))continue;
        uint64_t*pd=physical_pointer(*b&0x000ffffffffff000ull),*c=&pd[(address>>21)&511];if(!(*c&PTE_PRESENT)||(*c&(1ull<<7)))continue;
        uint64_t*pt=physical_pointer(*c&0x000ffffffffff000ull),*entry=&pt[(address>>12)&511];
        if(*entry&PTE_PRESENT){uint64_t physical=*entry&0x000ffffffffff000ull;*entry=0;__asm__ volatile("invlpg (%0)"::"r"(address):"memory");if(release)pmm_free(physical);}
        if(table_empty(pt)){pmm_free(*c&0x000ffffffffff000ull);*c=0;}
        if(table_empty(pd)){pmm_free(*b&0x000ffffffffff000ull);*b=0;}
    }
}
/* ponytail: successful allocations use a monotonic 512 GiB virtual arena;
 * add virtual-range reuse for long-lived allocation churn. Failures consume none. */
static bool arena_fits(uint64_t pages){return pages&&pages<=(0xffffc08000000000ull-next_virtual)/PAGE_SIZE;}
static void rollback_range(uint64_t base,uint64_t pages,bool release,bool had_root){
    unmap_range(base,pages,release);
    uint64_t*entry=&active_root()[(base>>39)&511];
    if(!had_root&&(*entry&PTE_PRESENT)&&table_empty(physical_pointer(*entry&0x000ffffffffff000ull))){pmm_free(*entry&0x000ffffffffff000ull);*entry=0;}
}
static void *allocate_range(uint64_t pages,bool guarded){
    if(pages>UINT64_MAX-2||!arena_fits(pages+(guarded?2:0)))return 0;
    uint64_t base=next_virtual,first=base+(guarded?PAGE_SIZE:0);
    bool had_root=(active_root()[(base>>39)&511]&PTE_PRESENT)!=0;
    for(uint64_t i=0;i<pages;i++){
        uint64_t p=pmm_alloc();
        if(!p||!vmm_map(first+i*PAGE_SIZE,p,PTE_WRITE|PTE_NX)){
            if(p)pmm_free(p);
            rollback_range(first,i,true,had_root);return 0;
        }
    }
    next_virtual+=(pages+(guarded?2:0))*PAGE_SIZE;
    return(void *)(uintptr_t)first;
}
void *vmm_map_mmio(uint64_t physical_address,size_t bytes){
    if(!bytes||bytes>UINT64_MAX-physical_address)return 0;
    uint64_t offset=physical_address&(PAGE_SIZE-1),physical=physical_address&~(PAGE_SIZE-1);
    if(bytes>UINT64_MAX-offset-(PAGE_SIZE-1))return 0;
    uint64_t pages=(offset+bytes+PAGE_SIZE-1)/PAGE_SIZE,base=next_virtual;if(!arena_fits(pages))return 0;
    bool had_root=(active_root()[(base>>39)&511]&PTE_PRESENT)!=0;
    for(uint64_t i=0;i<pages;i++)if(!vmm_map(base+i*PAGE_SIZE,physical+i*PAGE_SIZE,PTE_WRITE|PTE_NX)){rollback_range(base,i,false,had_root);return 0;}
    next_virtual+=pages*PAGE_SIZE;return(void *)(uintptr_t)(base+offset);
}
void *vmm_guarded_stack(size_t usable_pages){
    if(!usable_pages)return 0;
    void*first=allocate_range(usable_pages,true);
    return first?(uint8_t*)first+usable_pages*PAGE_SIZE:0;
}
struct allocation {uint64_t magic,pages;size_t size;};
void *kmalloc(size_t size){
    if(!size||size>SIZE_MAX-sizeof(struct allocation)-(PAGE_SIZE-1))return 0;
    uint64_t pages=(sizeof(struct allocation)+size+PAGE_SIZE-1)/PAGE_SIZE;
    void*memory=allocate_range(pages,false);if(!memory)return 0;
    uint64_t base=(uint64_t)(uintptr_t)memory;
    struct allocation *a=(void *)(uintptr_t)base;a->magic=0xa710c0de5a11c0deull;a->pages=pages;a->size=size;heap_used+=size;
    return a+1;
}
static uint64_t *leaf_entry(uint64_t virtual_address){uint64_t cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));return leaf_entry_root(cr3,virtual_address,false,0);}
void kfree(void *pointer){if(!pointer)return;struct allocation *a=(struct allocation *)pointer-1;if(a->magic!=0xa710c0de5a11c0deull)return;uint64_t base=(uint64_t)(uintptr_t)a,pages=a->pages,size=a->size;a->magic=0;unmap_range(base,pages,true);if(heap_used>=size)heap_used-=size;}
uint64_t heap_bytes_used(void){return heap_used;}
bool memory_allocation_self_test(void){
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
    uint64_t saved_virtual=next_virtual,bytes_before=heap_used,pages_before=free_count;
    uint64_t scratch=(next_virtual+PAGE_SIZE+0x3fffffffull)&~0x3fffffffull;
    bool ok=scratch<0xffffc07f00000000ull;
    if(ok)for(unsigned kind=0;kind<3;kind++)for(int fail=0;fail<=6;fail++){
        next_virtual=kind==2?scratch-PAGE_SIZE:scratch;
        uint64_t base=next_virtual,before=free_count;allocation_fail_after=fail;
        void*p=kind==0?kmalloc(9000):kind==1?vmm_guarded_stack(3):vmm_map_mmio(0xfee00000,8192);
        allocation_fail_after=-1;
        if((fail==0&&p)||(fail==6&&!p))ok=false;
        if(!p){if(next_virtual!=base)ok=false;}
        else if(kind==0){((volatile uint8_t*)p)[0]=0x5a;((volatile uint8_t*)p)[8999]=0xa5;kfree(p);}
        else if(kind==1){
            uint64_t*low=leaf_entry(base),*high=leaf_entry((uint64_t)(uintptr_t)p);
            if((low&&(*low&PTE_PRESENT))||(high&&(*high&PTE_PRESENT)))ok=false;
            ((volatile uint8_t*)p)[-1]=0xa5;unmap_range(base+PAGE_SIZE,3,true);
        }else unmap_range(base,2,false);
        if(free_count!=before||heap_used!=bytes_before)ok=false;
        for(unsigned i=0;i<5;i++){uint64_t*entry=leaf_entry(base+i*PAGE_SIZE);if(entry&&(*entry&PTE_PRESENT))ok=false;}
    }
    next_virtual=saved_virtual;
    /* A wholly missing page-table chain must roll back all three levels. */
    const uint64_t address=0xffffb00000000000ull;uint64_t*root_entry=&active_root()[(address>>39)&511];
    if(*root_entry&PTE_PRESENT)ok=false;
    else for(int fail=0;fail<=3;fail++){
        uint64_t before=free_count;allocation_fail_after=fail;
        bool mapped=vmm_map(address,0xfee00000,PTE_WRITE|PTE_NX);allocation_fail_after=-1;
        if(mapped!=(fail==3))ok=false;
        if(mapped)rollback_range(address,1,false,false);
        if((*root_entry&PTE_PRESENT)||free_count!=before)ok=false;
    }
    if(vmm_map(address,0xfee00000,PTE_USER|PTE_WRITE)||vmm_map_user(0x400000,0xfee00000,true,true))ok=false;
    for(int fail=0;fail<=3;fail++){
        struct vmm_space space={0};uint64_t before=free_count;
        if(!vmm_space_create(&space)){ok=false;break;}
        if(vmm_space_map_user(&space,0x400000,0xfee00000,true,true))ok=false;
        allocation_fail_after=fail;
        bool mapped=vmm_space_map_user(&space,0x400000,0xfee00000,false,true);allocation_fail_after=-1;
        if(mapped!=(fail==3))ok=false;
        if(!vmm_space_destroy(&space)||space.root_physical||free_count!=before)ok=false;
    }
    if(kmalloc(SIZE_MAX)||vmm_guarded_stack(SIZE_MAX)||vmm_map_mmio(UINT64_MAX,2))ok=false;
    if(next_virtual!=saved_virtual||heap_used!=bytes_before||free_count!=pages_before)ok=false;
    __asm__ volatile("pushq %0; popfq"::"r"(flags):"memory");return ok;
}
bool pmm_test_pages(uint64_t pages,uint64_t *tested,uint64_t *failed_page){
    if(pages>256) pages=256;
    uint64_t held[256],count=0;
    for(;count<pages;count++){held[count]=pmm_alloc();if(!held[count])break;volatile uint64_t *p=physical_pointer(held[count]);for(unsigned i=0;i<PAGE_SIZE/8;i++)p[i]=0xaa55aa55aa55aa55ull^(held[count]+i);}
    bool ok=true;for(uint64_t n=0;n<count;n++){volatile uint64_t *p=physical_pointer(held[n]);for(unsigned i=0;i<PAGE_SIZE/8;i++)if(p[i]!=(0xaa55aa55aa55aa55ull^(held[n]+i))){ok=false;if(failed_page)*failed_page=held[n];break;}if(!ok)break;}
    for(uint64_t n=0;n<count;n++) pmm_free(held[n]);
    if(tested) *tested=count;
    return ok;
}
