// include/memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include <multiboot.h>   // Now uses full multiboot info

void physmem_init(multiboot_info_t* mbi);
void* phys_alloc_page(void);
void phys_free_page(void* page);

void paging_init(void);
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

#endif
