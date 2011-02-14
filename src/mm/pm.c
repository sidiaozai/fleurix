#include <param.h>
#include <x86.h>
#include <proto.h>
#include <proc.h>
#include <unistd.h>
//
#include <page.h>
#include <vm.h>
//
#include <buf.h>
#include <conf.h>
#include <hd.h>
//
#include <super.h>
#include <inode.h>
#include <file.h>


/*
 * the map for page frames. Each physical page is associated with one reference
 * count, and it's free on 0. Only 0 can be allocated via pgalloc().  
 * Reference count is increased on a fork.
 *
 * */
struct page coremap[NPAGE];
struct page pgfreelist;

/* returns the page struct via a physical page number. */
struct page* pgfind(uint pn){
    int nr;
    if (nr<0 || nr>=NPAGE ) {
        panic("bad page number.");
    }
    return &coremap[nr];
}

/* 
 * allocate an free physical page. (always success)
 * get the head of the freelist, remove it and increase the refcount.
 * */ 
struct page* pgalloc(){
    struct page *pp;

    if (pgfreelist.pg_next == NULL) {
        panic("no free page.\n");
        return NULL;
    }
    
    cli();
    pp = pgfreelist.pg_next;
    pgfreelist.pg_next = pp->pg_next;
    pp->pg_count = 1;
    sti();
    return pp;
}

/*
 * free a physical page. decrease the reference count and put it back
 * to the freelist. 
 */
int pgfree(struct page *pp){
    if (pp->pg_count==0) {
        panic("freeing a free page.");
    }

    cli();
    pp->pg_count--;
    if (pp->pg_count==0) {
        pp->pg_next = pgfreelist.pg_next;
        pgfreelist.pg_next = pp;
    }
    sti();
    return pp->pg_num;
}

/*
 * map a linear address to physical address, and flush the TLB.
 * */
int pgattach(struct pde *pgd, uint vaddr, struct page *pp, uint flag){
    struct pte *pte;

    pte = find_pte(vaddr, 1);
    pte->pt_off = pp->pg_num;
    pte->pt_flag = flag;
    lpgd(pgd);
    return 0;
}

/* initialize pages' free list. */
int pm_init(){
    struct page *pp, *ph;
    uint i, pn;

    // mark the reserved pages
    // 640kb ~ 1mb is system reserved, BIOS and blah
    // 1mb ~ __kend__ is kernel reserved.
    for (pn=0; pn<3; pn++) {
        coremap[pn].pg_num = pn;
        coremap[pn].pg_flag = PG_RSVD;
        coremap[pn].pg_count = 100;
    }
    for (pn=640*1024/PAGE; pn<PPN(&__kend__); pn++){
        coremap[pn].pg_num = pn;
        coremap[pn].pg_flag = PG_RSVD;
        coremap[pn].pg_count = 100;
    }
    // link all the free pages into freelist.
    ph = &pgfreelist;
    for (pn=0; pn<NPAGE; pn++) {
        pp = &coremap[pn];
        if ((pp->pg_flag & PG_RSVD) || (pp->pg_count > 0))  {
            continue;
        }
        pp->pg_num = pn;
        pp->pg_flag = 0;
        pp->pg_count = 0;
        //
        pp->pg_next = NULL;
        ph->pg_next = pp;
        ph = pp;
    }
}

/* Map the top 4mb virtual memory as physical memory. Initiliaze
 * the page-level allocator, set the fault handler and misc.
 * */
void mm_init(){
    int pn;

    // map the entire physical memory into the kernel's address space.
    for (pn=0; pn<PMEM/(PAGE*1024); pn++) {
        pgd0[pn].pd_off = pn << 10;
        pgd0[pn].pd_flag = PTE_PS | PTE_P | PTE_W; // note: set it 4mb via a PTE_S
    }
    //
    for (pn=PMEM/(PAGE*1024); pn<1024; pn++) {
        pgd0[pn].pd_flag &= ~PTE_P;
    }
    // init physical page allocator
    pm_init();
    // set fault handler
    set_hwint(0x0E, do_pgfault);
    // load page directory and enable the MMU.
    lpgd((uint)pgd0);
    mmu_enable();
}
