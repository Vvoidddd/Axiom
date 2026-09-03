#ifndef AXIOM_ELF_H
#define AXIOM_ELF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "memory.h"
#define USER_IMAGE_MAX_PAGES 128u
struct user_page{uint64_t virtual_address,physical_address;};
struct user_image{struct vmm_space space;uint64_t entry,stack_top;struct user_page pages[USER_IMAGE_MAX_PAGES];uint32_t page_count;};
int elf_load_vfs(const char *path,struct user_image *image);
#endif
