/*
 * @Author: Yinwhe
 * @Date: 2021-07-10 20:06:58
 * @LastEditors: Yinwhe
 * @LastEditTime: 2021-07-14 22:07:22
 * @Description: file information
 * @Copyright: Copyright (c) 2021
 */
#pragma once
// #define DEBUG
#include "slub.h"
#include "buddy.h"
#include "types.h"

struct task_struct;
struct mm_struct;
struct file;

/* Page Relevant */
#define PAGE_SHIFT 12
#define PPN_SHIFT 10
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~((1UL << PAGE_SHIFT) - 1))
#define MEM_SHIFT (24 - PAGE_SHIFT)

/* align to PAGESIZE */
#define PAGE_ROUNDUP(a)  (((a)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PAGE_ROUNDDOWN(a) (((a)) & ~(PAGE_SIZE-1))

/* permission bit */
#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)

/* permission flags */
#define MAP_FILE        0x00
#define MAP_TYPE	    0x0f		/* Mask for type of mapping */
#define MAP_FIXED	    0x10		/* Interpret addr exactly */
#define MAP_ANONYMOUS	0x20		/* don't use a file */

#define PROT_NONE	    0x0	        //页内容不可被访问
#define PROT_READ	    0x1	        //页内容可以被读取
#define PROT_WRITE	    0x2	        //页可以被写入内容
#define PROT_EXEC	    0x4	        //页内容可以被执行
#define PROT_SEM        0x8 
#define PROT_GROWSDOWN  0x01000000
#define PROT_GROWSUP    0x02000000

/* shift a physical address to the right place for a PTE. */
#define PA2PTE(pa)      ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte)     (((pte) >> 10) << 12)
#define PTE_FLAGS(pte)  ((pte) & 0x3FF)
#define VA_OFFSET(va)   ((va)&(0xfff))

/* extract the three 9-bit page table indices from a virtual address. */
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PAGE_SHIFT+(9*(level)))
#define PX(level, va)   ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

#define SV39 (8L<<60)


extern pagetable_t kernel_pagetable;

extern char text_start[];
extern char text_end[];
extern char rodata_start[];
extern char rodata_end[];
extern char rodata_start[];
extern char rodata_end[];
extern char data_start[];
extern char data_end[];
extern char bss_start[];
extern char bss_end[];
extern char _end[];

void paging_init();
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
void delete_mapping(uint64 *pgtbl, uint64 va, uint64 sz, uint8 free_frame);
pagetable_t walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 kwalkaddr(pagetable_t kpt, uint64 va);;

int uvmap(struct task_struct *utask, void* src, uint size, uint8 aligned);
void *do_mmap(struct file *file, struct mm_struct *mm, void *start, 
    size_t len, unsigned long prot, unsigned long flags, off_t off);
int munmap(void *start, size_t len);
int do_munmap(struct mm_struct *mm, unsigned long start, size_t len);
void *mmap(void *start, size_t len, unsigned long prot, 
                unsigned long flags, int fd, off_t off);