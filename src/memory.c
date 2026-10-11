
// src/memory.c – improved physical and virtual memory manager
#include <memory.h>
#include <serial.h>
#include <string.h>

/* Physical Memory Manager (bitmap for first 128MB) */
#define PAGE_SIZE 4096
#define PAGE_COUNT (128 * 1024 * 1024 / PAGE_SIZE)  /* 32768 pages */

static uint8_t bitmap[PAGE_COUNT / 8];
static uint32_t last_alloc = 0;
static uint32_t total_pages = 0;      /* actual number of usable pages */

static inline void bitmap_set(uint32_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}
static inline void bitmap_clear(uint32_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}
static inline bool bitmap_test(uint32_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

/* Initialize physical memory using Multiboot memory map */
void physmem_init(multiboot_info_t* mbi)
{
    /* First, mark all pages as used */
    memset(bitmap, 0xFF, sizeof(bitmap));
    total_pages = 0;

    /* If memory map is available, use it */
    if (mbi->flags & (1 << 6)) {   /* bit 6 = memory map present */
        multiboot_mmap_entry_t* mmap = (multiboot_mmap_entry_t*)mbi->mmap_addr;
        while ((uint32_t)mmap < mbi->mmap_addr + mbi->mmap_length) {
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint64_t start = mmap->addr;
                uint64_t end = mmap->addr + mmap->len;
                uint32_t first_page = (start + PAGE_SIZE - 1) / PAGE_SIZE;
                uint32_t last_page = end / PAGE_SIZE;
                if (last_page > PAGE_COUNT) last_page = PAGE_COUNT;
                for (uint32_t i = first_page; i < last_page; i++) {
                    bitmap_clear(i);
                }
                if (last_page > total_pages) total_pages = last_page;
            }
            /* Advance to next entry */
            mmap = (multiboot_mmap_entry_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    } else {
        /* Fallback: use mem_upper (less accurate) */
        uint32_t mem_upper_kb = mbi->mem_upper;   /* memory above 1MB, in KB */
        total_pages = mem_upper_kb * 1024 / PAGE_SIZE;
        if (total_pages > PAGE_COUNT) total_pages = PAGE_COUNT;
        /* Free all pages above 4MB (index 1024) */
        for (uint32_t i = 1024; i < total_pages; i++) {
            bitmap_clear(i);
        }
    }
}

void* phys_alloc_page(void)
{
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t idx = (last_alloc + i) % total_pages;
        if (!bitmap_test(idx)) {
            bitmap_set(idx);
            last_alloc = (idx + 1) % total_pages;
            return (void*)(idx * PAGE_SIZE);
        }
    }
    PANIC("Out of physical memory");
    return NULL;
}

void phys_free_page(void* page)
{
    uint32_t addr = (uint32_t)page;
    if (addr % PAGE_SIZE != 0) PANIC("Unaligned page free");
    uint32_t idx = addr / PAGE_SIZE;
    if (idx < total_pages && bitmap_test(idx)) {
        bitmap_clear(idx);
    }
}

/* Paging – dynamic page table allocation */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

void paging_init(void)
{
    /* Identity map first 4MB using a newly allocated page table */
    uint32_t* first_table = (uint32_t*)phys_alloc_page();
    for (int i = 0; i < 1024; i++) {
        first_table[i] = (i * PAGE_SIZE) | 0x3;   /* present, r/w (no user) */
    }
    page_directory[0] = (uint32_t)first_table | 0x3; /* present, r/w */

    /* Clear remaining directory entries */
    for (int i = 1; i < 1024; i++) {
        page_directory[i] = 0;   /* not present */
    }

    /* Load page directory and enable paging */
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(page_directory) : "eax"
    );
}

/* Map a virtual page to a physical page with given flags */
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    uint32_t* pt;
    if (!(page_directory[pd_idx] & 0x1)) {   /* not present */
        /* Allocate a new page table */
        pt = (uint32_t*)phys_alloc_page();
        memset(pt, 0, PAGE_SIZE);
        page_directory[pd_idx] = (uint32_t)pt | 0x3;   /* present, r/w (inherit flags?) */
        /* In practice, you may want to store the flags in the directory entry as well.
           For simplicity, we keep the directory entry present and r/w. */
    } else {
        pt = (uint32_t*)(page_directory[pd_idx] & 0xFFFFF000);
    }

    pt[pt_idx] = (phys & 0xFFFFF000) | flags | 0x1;   /* add present bit */
    /* Invalidate TLB entry for this page */
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}
