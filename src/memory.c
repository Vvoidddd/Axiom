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
static void *physical_pointer(uint64_t physical);
static uint64_t *leaf_entry(uint64_t virtual_address);
static uint64_t *next_table(uint64_t *table,unsigned index,bool create);
static uint64_t *leaf_entry_root(uint64_t root,uint64_t virtual_address,bool create,uint64_t flags){
    uint64_t*pml4=physical_pointer(root&~0xfffull);unsigned a=(virtual_address>>39)&511,b=(virtual_address>>30)&511,c=(virtual_address>>21)&511;
    uint64_t*pdpt=next_table(pml4,a,create);if(!pdpt)return 0;if(flags&PTE_USER)pml4[a]|=PTE_USER;
    uint64_t*pd=next_table(pdpt,b,create);if(!pd)return 0;if(flags&PTE_USER)pdpt[b]|=PTE_USER;
    uint64_t*pt=next_table(pd,c,create);if(!pt)return 0;if(flags&PTE_USER)pd[c]|=PTE_USER;
    return &pt[(virtual_address>>12)&511];
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
    if(!free_head) return 0;
    struct free_page *node=free_head;free_head=node->next;free_count--;used_count++;
    uint64_t physical=(uint64_t)(uintptr_t)node-direct_offset; memset(node,0,PAGE_SIZE); return physical;
}
void pmm_free(uint64_t physical){if(!physical)return;struct free_page *node=physical_pointer(physical);node->next=free_head;free_head=node;free_count++;if(used_count)used_count--;}
uint64_t pmm_free_pages(void){return free_count;}
uint64_t pmm_used_pages(void){return used_count;}

static uint64_t *next_table(uint64_t *table,unsigned index,bool create){
    if(!(table[index]&PTE_PRESENT)){if(!create)return 0;uint64_t page=pmm_alloc();if(!page)return 0;table[index]=page|PTE_PRESENT|PTE_WRITE;}
    return physical_pointer(table[index]&0x000ffffffffff000ull);
}
bool vmm_map(uint64_t virtual_address,uint64_t physical_address,uint64_t flags){
    uint64_t cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));uint64_t *pml4=physical_pointer(cr3&~0xfffull);
    unsigned pml4i=(virtual_address>>39)&511,pdpti=(virtual_address>>30)&511,pdi=(virtual_address>>21)&511;
    uint64_t *pdpt=next_table(pml4,pml4i,true);if(!pdpt)return false;if(flags&PTE_USER)pml4[pml4i]|=PTE_USER;
    uint64_t *pd=next_table(pdpt,pdpti,true);if(!pd)return false;if(flags&PTE_USER)pdpt[pdpti]|=PTE_USER;
    uint64_t *pt=next_table(pd,pdi,true);if(!pt)return false;if(flags&PTE_USER)pd[pdi]|=PTE_USER;
    pt[(virtual_address>>12)&511]=(physical_address&~0xfffull)|PTE_PRESENT|flags;
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
void *vmm_map_mmio(uint64_t physical_address,size_t bytes){
    if(!bytes)return 0;
    uint64_t offset=physical_address&(PAGE_SIZE-1),physical=physical_address&~(PAGE_SIZE-1);
    uint64_t pages=(offset+bytes+PAGE_SIZE-1)/PAGE_SIZE,base=next_virtual;next_virtual+=pages*PAGE_SIZE;
    for(uint64_t i=0;i<pages;i++)if(!vmm_map(base+i*PAGE_SIZE,physical+i*PAGE_SIZE,PTE_WRITE|PTE_NX))return 0;
    return(void *)(uintptr_t)(base+offset);
}
void *vmm_guarded_stack(size_t usable_pages){
    uint64_t base=next_virtual;next_virtual+=(usable_pages+2)*PAGE_SIZE;
    for(size_t i=0;i<usable_pages;i++){uint64_t p=pmm_alloc();if(!p||!vmm_map(base+(i+1)*PAGE_SIZE,p,PTE_WRITE|PTE_NX))return 0;}
    return (void *)(uintptr_t)(base+(usable_pages+1)*PAGE_SIZE);
}
struct allocation {uint64_t magic,pages;size_t size;};
void *kmalloc(size_t size){
    if(!size) return 0;
    uint64_t pages=(sizeof(struct allocation)+size+PAGE_SIZE-1)/PAGE_SIZE;
    uint64_t base=next_virtual;next_virtual+=pages*PAGE_SIZE;
    for(uint64_t i=0;i<pages;i++){uint64_t p=pmm_alloc();if(!p||!vmm_map(base+i*PAGE_SIZE,p,PTE_WRITE|PTE_NX))return 0;}
    struct allocation *a=(void *)(uintptr_t)base;a->magic=0xa710c0de5a11c0deull;a->pages=pages;a->size=size;heap_used+=size;
    return a+1;
}
static uint64_t *leaf_entry(uint64_t virtual_address){uint64_t cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));uint64_t *pml4=physical_pointer(cr3&~0xfffull);uint64_t *pdpt=next_table(pml4,(virtual_address>>39)&511,false);if(!pdpt)return 0;uint64_t *pd=next_table(pdpt,(virtual_address>>30)&511,false);if(!pd)return 0;uint64_t *pt=next_table(pd,(virtual_address>>21)&511,false);if(!pt)return 0;return &pt[(virtual_address>>12)&511];}
void kfree(void *pointer){if(!pointer)return;struct allocation *a=(struct allocation *)pointer-1;if(a->magic!=0xa710c0de5a11c0deull)return;uint64_t base=(uint64_t)(uintptr_t)a,pages=a->pages,size=a->size;a->magic=0;for(uint64_t i=0;i<pages;i++){uint64_t virtual_address=base+i*PAGE_SIZE;uint64_t *entry=leaf_entry(virtual_address);if(entry&&(*entry&PTE_PRESENT)){uint64_t physical=*entry&0x000ffffffffff000ull;*entry=0;__asm__ volatile("invlpg (%0)"::"r"(virtual_address):"memory");pmm_free(physical);}}if(heap_used>=size)heap_used-=size;}
uint64_t heap_bytes_used(void){return heap_used;}
bool pmm_test_pages(uint64_t pages,uint64_t *tested,uint64_t *failed_page){
    if(pages>256) pages=256;
    uint64_t held[256],count=0;
    for(;count<pages;count++){held[count]=pmm_alloc();if(!held[count])break;volatile uint64_t *p=physical_pointer(held[count]);for(unsigned i=0;i<PAGE_SIZE/8;i++)p[i]=0xaa55aa55aa55aa55ull^(held[count]+i);}
    bool ok=true;for(uint64_t n=0;n<count;n++){volatile uint64_t *p=physical_pointer(held[n]);for(unsigned i=0;i<PAGE_SIZE/8;i++)if(p[i]!=(0xaa55aa55aa55aa55ull^(held[n]+i))){ok=false;if(failed_page)*failed_page=held[n];break;}if(!ok)break;}
    for(uint64_t n=0;n<count;n++) pmm_free(held[n]);
    if(tested) *tested=count;
    return ok;
}
