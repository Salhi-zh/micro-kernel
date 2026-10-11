#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stdbool.h>

/* Page sizes and masks */
#define PAGE_SIZE 4096
#define PAGE_MASK 0xFFFFF000

/* Page Entry Flags */
#define PAGE_PRESENT  0x01   // Page is present in RAM
#define PAGE_RW       0x02   // Read/Write permissions
#define PAGE_USER     0x04   // Ring 3 (User) can access this page

/* Initialize the Physical and Virtual Memory Managers */
void paging_init(uint32_t total_memory_kb);

/* Map a virtual address to a physical address with specific flags */
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

/* Allocate a physical 4KB frame (PMM) */
uint32_t pmm_alloc_page(void);

#endif
