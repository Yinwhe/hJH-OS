/*
 * @Author: Yinwhe
 * @Date: 2021-07-10 20:06:58
 * @LastEditors: Yinwhe
 * @LastEditTime: 2021-07-12 13:28:27
 * @Description: file information
 * @Copyright: Copyright (c) 2021
 */
#ifndef _SLUB_H
#define _SLUB_H

#include "list.h"
#include "vm.h"
#include "memlayout.h"

#define NR_PARTIAL 9
#define STRUCTURE_SIZE 16UL
#define VM_START KERNEL_VM_BASE

extern void *page_base;
struct page {
	unsigned long flags;		
	int count;		
	struct page *header;
	struct page *next;
	struct list_head slub_list;
	struct kmem_cache *slub;	/* Pointer to slab */
	void *freelist;
};

struct cache_area{
	void *base;
	size_t size;
	void *freelist;
};

struct kmem_cache {
	/* kmem_cache_cpu */
	void **freelist;	/* Pointer to next available object */
	unsigned long tid;	/* Globally unique transaction id */
	struct page *page;	/* The slab from which we are allocating */

	/* Used for retrieving partial slabs, etc. */
	int refcount;
	unsigned long min_partial;
	size_t size;	/* The size of an object including metadata */
	size_t object_size;/* The size of an object without metadata */
	unsigned int offset;	/* Free pointer offset */
	unsigned long nr_page_per_slub;

	void (*init_func)(void *);
	unsigned int inuse;		/* Offset to metadata */
	unsigned int align;		/* Alignment */
	unsigned int red_left_pad;	/* Left redzone padding size */
	const char *name;	/* Name (only for display!) */
	struct list_head list;	/* List of slab caches */

	/* kmem_cache_node */
	unsigned long nr_partial;
	struct list_head partial;
#ifdef CONFIG_SLUB_DEBUG
	unsigned long nr_slabs;
	unsigned long total_objects;
	struct list_head full;
#endif
};


void slub_init();
struct kmem_cache *kmem_cache_create(const char *, size_t, unsigned int, int, void*(void *));

void *kmem_cache_alloc(struct kmem_cache*);
void kmem_cache_free(void *);


void *kmalloc(size_t);
void kfree(void *);

#endif
