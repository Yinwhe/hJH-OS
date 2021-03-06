#include "riscv.h"
#include "types.h"
#include "vm.h"
#include "sysctl.h"
#include "sched.h"
#include "memlayout.h"
#include "put.h"
#include "string.h"

/* This is the first kernel pagetable */
pagetable_t kernel_pagetable=NULL;

/** 
 * @brief walk through the table and add entryitem if needed (when alloc = 1)
 * @param pagetable:pagetable needed to be walked
 * @param va: virtual address
 * @param alloc: whether to alloc memory for pagetables if necessary
 */
pagetable_t walk(pagetable_t pagetable, uint64 va, int alloc){
    for(int level = 2; level > 0; level--){
        pte_t* pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V)
            pagetable = (pte_t*)PTE2PA(*pte);
        else{//or we need to add this pagetable
            if(!alloc || ((pagetable = (pde_t *)alloc_pages(1)) == 0)) 
                return NULL;//error, either because alloc=0 or kframe_alloc fails.
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0,va)];
}

/**
 * @brief 模拟MMU走pagetable
 * 
 * @param kpt pagetable
 * @param va 虚拟地址
 * @return 解析出来的虚拟地址
 */
uint64 kwalkaddr(pagetable_t kpt, uint64 va)
{
    uint64 off = va % PAGE_SIZE;
    pte_t *pte;
    uint64 pa;

    pte = walk(kpt, va, 0);
    if(pte == 0)
        panic("kvmpa");
    if((*pte & PTE_V) == 0)
        panic("kvmpa");

    pa = PTE2PA(*pte);
    return pa+off;
}

/**
 * @brief Map physical address to virtual address
 * @param pgtbl: pagetable
 * @param va: virtual address
 * @param pa: physical address
 * @param perm: permission bits
 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm){
    if(sz==0) return;

    uint64 pgStart = PAGE_ROUNDDOWN(va);
    uint64 pgEnd = PAGE_ROUNDDOWN(va+sz-1);
    while(pgStart<=pgEnd){
        pte_t* pte = walk(pgtbl, pgStart, 1);
        if(pte==0)
            panic("walk error, overflow!");
        else if(*pte & PTE_V)
            panic("walk error, remap!");
        else *pte = PA2PTE(pa) | PTE_V | perm;

        pgStart += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
}

/**
 * @brief 释放页表映射，也可以释放掉相应的页
 * 
 * @param pgtbl 要进行操作的页表
 * @param va 虚拟地址
 * @param sz 要释放的内存大小
 * @param free_frame 如果为1，则同时释放页
 */
void delete_mapping(uint64 *pgtbl, uint64 va, uint64 sz, uint8 free_frame){
    if(sz==0) return;

    uint64 pgStart = PAGE_ROUNDDOWN(va);
    uint64 pgEnd = PAGE_ROUNDDOWN(va+sz-1);
    while(pgStart<=pgEnd){
        pte_t* pte = walk(pgtbl, pgStart, 0);
        uint64 pa = PTE2PA(*pte);
        if(pte==0)
            panic("delete_mapping error, pte already 0!");
        else *pte = 0;

        if(free_frame)
            kfree((void*)pa);
        pgStart+=PAGE_SIZE;
    }
}

/**
 * @brief Initialize pages for later uses
 */
void paging_init(){
    #ifndef QEMU
    sysctl_pll_enable(SYSCTL_PLL1);
    sysctl_clock_enable(SYSCTL_CLOCK_PLL1);
    #endif
    /* Initialize memory space */
    memset((void*)_end, 0, MEM_END-(uint64)_end);
    init_buddy_system();

    kernel_pagetable = (pagetable_t)alloc_pages(1);
    
    memset(kernel_pagetable, 0, PAGE_SIZE);

    // UART
    create_mapping(kernel_pagetable, UARTHS_BASE_ADDR, UARTHS_P, PAGE_SIZE, PTE_R | PTE_W);

    #ifdef QEMU
    create_mapping(kernel_pagetable, VIRTIO0, VIRTIO0_P, PAGE_SIZE, PTE_R|PTE_W);
    #endif

    // CLINT
    create_mapping(kernel_pagetable, CLINT_BASE_ADDR, CLINT_P, 0x10000, PTE_R | PTE_W);
    // PLIC
    create_mapping(kernel_pagetable, PLIC_BASE_ADDR, PLIC_P, 0x4000, PTE_R | PTE_W);
    create_mapping(kernel_pagetable, PLIC_BASE_ADDR + 0x200000, PLIC_P + 0x200000, 0x4000, PTE_R | PTE_W);

    #ifndef QEMU
    // GPIOHS
    create_mapping(kernel_pagetable, GPIOHS_BASE_ADDR, GPIOHS_P, 0x1000, PTE_R | PTE_W);

    // DMAC
    create_mapping(kernel_pagetable, DMAC_BASE_ADDR, DMAC_P, 0x1000, PTE_R | PTE_W);

    // SPI_SLAVE
    create_mapping(kernel_pagetable, SPI_SLAVE_BASE_ADDR, SPI_P, 0x1000, PTE_R | PTE_W);

    // FPIOA
    create_mapping(kernel_pagetable, FPIOA_BASE_ADDR, FPIOA_P, 0x1000, PTE_R | PTE_W);

    // SPI0
    create_mapping(kernel_pagetable, SPI0_BASE_ADDR, SPI0_P, 0x1000, PTE_R | PTE_W);

    // SPI1
    create_mapping(kernel_pagetable, SPI1_BASE_ADDR, SPI1_P, 0x1000, PTE_R | PTE_W);

    // SPI2
    create_mapping(kernel_pagetable, SPI2_BASE_ADDR, SPI2_P, 0x1000, PTE_R | PTE_W);

    // SYSCTL
    create_mapping(kernel_pagetable, SYSCTL_BASE_ADDR, SYSCTL_P, 0x1000, PTE_R | PTE_W);
    #endif

    #ifdef DEBUG
    printf("text_start:%p, text_end:%p\n", text_start, text_end);
    printf("rodata_start:%p, rodata_end:%p\n", rodata_start, rodata_end);
    printf("data_start:%p, data_end:%p\n", data_start, data_end);
    printf("bss_start:%p, bss_end:%p\n", bss_start, bss_end);
    #endif

    create_mapping(kernel_pagetable,SBI_BASE, SBI_BASE,SBI_SIZE,PTE_R|PTE_X);
    create_mapping(kernel_pagetable,KERNEL_BASE,KERNEL_BASE,(uint64)text_end-(uint64)text_start,PTE_R|PTE_X);
    create_mapping(kernel_pagetable,(uint64)rodata_start,(uint64)rodata_start,(uint64)rodata_end-(uint64)rodata_start,PTE_R);
    create_mapping(kernel_pagetable,(uint64)data_start,(uint64)data_start,(uint64)data_end-(uint64)data_start,PTE_R|PTE_W);
    create_mapping(kernel_pagetable,(uint64)bss_start,(uint64)bss_start, MEM_END-(uint64)bss_start,PTE_R|PTE_W);

    asm volatile("csrw satp, %0"::"r"(SV39|((uint64)kernel_pagetable>>12)));
    asm volatile("sfence.vma");

    slub_init();
    printf("[vm]paging init donw!\n");
    return;
}

/**
 * @brief 获取一段没有被占用的虚拟内存
 * 
 * @param length 需要的内存大小
 * @return 虚拟内存的起始地址 
 */
uint64 get_unmapped_area(size_t length){
    void *start = 0;
    int i, n = (length>>PAGE_SHIFT);
    while(1){
        for(i=0;i<n;i++){
            uint64 pte = (uint64)walk(current->mm->pagetable, (uint64)start+PAGE_SIZE*i, 0);
            if((pte&PTE_V)!=0)
                break;
        }
        if(i==n) break;

        start=start+PAGE_SIZE*(i+1);
    } 
    return (uint64)start;
}



void *do_mmap(struct file *file, struct mm_struct *mm, void *start, 
    size_t len, unsigned long prot, unsigned long flags, off_t off)
{
    /* Code to fetch an non-mapped address */
    uint64 astart= (uint64)start;
    size_t alength = PAGE_ROUNDUP(len);
    int i, pgnum = alength>>PAGE_SHIFT;
    for(i=0; i<pgnum; i++){
        uint64 pte = (uint64)walk(current->mm->pagetable, (uint64)start+PAGE_SIZE*i, 0);
        if((pte&PTE_V)!=0)
            break;
    }
    if(i!=pgnum)
        astart=get_unmapped_area(alength);

    struct vm_area_struct* newVMA= (struct vm_area_struct*)kmalloc(sizeof(struct vm_area_struct));
    newVMA->vm_start=astart;
    newVMA->vm_end=astart+alength;
    newVMA->vm_page_prot=prot;
    newVMA->vm_flags = flags;
    newVMA->vm_mm=mm;
    newVMA->file=file;
    //insert
    if(mm->vma){
        newVMA->vm_prev=mm->vma;
        newVMA->vm_next=mm->vma->vm_next;
        mm->vma->vm_next->vm_prev=newVMA;
        mm->vma->vm_next=newVMA;
    }
    else{
        mm->vma=newVMA;
        newVMA->vm_prev=newVMA;
        newVMA->vm_next=newVMA;
    }

    if(file){
        file->f_pos = off;
        newVMA->file_map_pos = off;
        newVMA->file_map_len = len;
    }

    #ifdef DEBUG
    printf("[S] New vm_area_struct: start %p, end %p, prot [r:%d,w:%d,x:%d]\n",newVMA->vm_start, newVMA->vm_end, (prot&PROT_READ)!=0, (prot&PROT_WRITE)!=0, (prot&PROT_EXEC)!=0);
    #endif
    return (void*)astart;
}

/**
 * @brief 分配内存，或将文件或设备映射到内存中
 * 
 * @param start 如果不为NULL，内核会在此地址创建映射；
 *              否则，内核会选择一个合适的虚拟地址。
 * @param len   表示映射到进程地址空间的大小。
 * @param prot  内存区域的读/写/执行属性。
 * @param flags 内存映射的属性，共享、私有、匿名、文件等。
 * @param fd    表示这是一个文件映射，fd是打开文件的句柄。如果是文件映射，需要指定fd；匿名映射就指定一个特殊的-1。
 * @param off   在文件映射时，表示相对文件头的偏移量；返回的地址是偏移量对应的虚拟地址。
 */
void *mmap(void *start, size_t len, unsigned long prot, 
                unsigned long flags, int fd, off_t off)
{
    struct file * file = NULL;
    if(!(flags&MAP_ANONYMOUS) && fd>=2){ // File map
        file = current->FTable[fd];
        if(!file)
            panic("Error during mmap file, file not exist!");
    }

    return do_mmap(file, current->mm, start, len, prot, flags, off);
}

int do_munmap(struct mm_struct *mm, unsigned long start, size_t len){
    size_t  alength = PAGE_ROUNDUP(len);
    void    *end = start+alength;
    struct vm_area_struct *vma=mm->vma;
    uint64 va = 0;

    if(!vma) return -1;
    do {
        if (vma->vm_start==start&&vma->vm_end==end)
        {
            va=(uint64)start;
            /* Sync */
            if(vma->file){
                vma->file->f_pos = vma->file_map_pos;
                fwrite(vma->file, va, alength);
            }
            /* Remove from list */
            if(vma->vm_next==vma){
                current->mm->vma=0;
            }
            else {
                if (mm->vma==vma)
                    mm->vma = vma->vm_next;
                vma->vm_next->vm_prev = vma->vm_prev;
                vma->vm_prev->vm_next = vma->vm_next;
            }
            kfree(vma);
            break;
        }
        vma=vma->vm_next;
    }while (vma!=current->mm->vma);

    if (!va) return -1;
    return 0;
}

int munmap(void *start, size_t len)
{
    return do_munmap(current->mm, start, len);
}

/**
 * @brief 将给定src的用户进程映射到0处
 * 
 * @param utask 用户进程pcb
 * @param src 指定的用户code
 * @param size code的大小
 * @param aligned 为了映射到地址0处，该code必须按页对其，如果不是aligned的，将重新分配内存并将user code复制过去后再映射；否则直接映射
 * @return 成功则返回0，否则返回-1
 */
int uvmap(struct task_struct *utask, void* src, uint size, uint8 aligned){
    if(!utask||!size) return -1;

    void* mem = 0;
    if(!aligned){
        mem = kmalloc(PAGE_ROUNDUP(size));
        memcpy(mem, src, size);
    }else{
        mem = (void*)kwalkaddr(utask->mm->pagetable, (uint64)src);
    }

	do_mmap(NULL, utask->mm, 0, size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS, 0);
    create_mapping(utask->mm->pagetable, 0, (uint64)mem, size, PTE_R|PTE_W|PTE_X|PTE_U);
    utask->size = size;
    
    return 0;
}