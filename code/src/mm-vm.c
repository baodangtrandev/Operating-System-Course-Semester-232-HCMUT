/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "mm.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef MM_PAGING

pthread_mutex_t vm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt) {
  pthread_mutex_lock(&vm_lock);
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt.rg_start >= rg_elmt.rg_end){
    pthread_mutex_unlock(&vm_lock);
    return -1;
  }

  struct vm_rg_struct *new_rg = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
  new_rg->rg_start = rg_elmt.rg_start;
  new_rg->rg_end = rg_elmt.rg_end;
  new_rg->rg_next = NULL;

  if (rg_node != NULL)
    new_rg->rg_next = rg_node;

  /* Enlist the new region */
  
  mm->mmap->vm_freerg_list = new_rg;
  pthread_mutex_unlock(&vm_lock);
  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid) {
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = 0;

  while (vmait < vmaid) {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid) {
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr) {
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  // Check if there is free room in list vm_freerg_list of vma
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    struct framephy_struct *frame = NULL;
    int n_page = (PAGING_PAGE_ALIGNSZ(rgnode.rg_end) - PAGING_PAGE_ALIGNSZ( rgnode.rg_start))/PAGING_PAGESZ;
    if(alloc_pages_range(caller,n_page, &frame) < 0){
      return -1;
    }
    vmap_page_range(caller,caller->mm->symrgtbl[rgid].rg_start, n_page,frame,& caller->mm->symrgtbl[rgid]); 

    *alloc_addr = rgnode.rg_start;
      #ifdef EX
  printf("\n\tRun ALLOC %d: Process %2d\n", size,  caller->pid);
  printf("\tRange ID: %d, Start: %lu, End: %lu\n", rgid, rgnode.rg_start, rgnode.rg_end);
    print_rg_memphy(caller, rgnode);
  printf("\tUsed Region List: \n");
  for(int rgit = 0 ; rgit < PAGING_MAX_SYMTBL_SZ; rgit++){
      if(caller->mm->symrgtbl[rgit].rg_start == 0 && caller->mm->symrgtbl[rgit].rg_end == 0){
        continue;
      }
      printf("\trg[%ld->%ld]\n", caller->mm->symrgtbl[rgit].rg_start, caller->mm->symrgtbl[rgit].rg_end);
    }
  printf("\tFree Region List:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  #endif
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /*Attempt to increate limit to get space */
  if( caller->mm->mmap->sbrk + size < caller->mm->mmap->vm_end){
    caller->mm->symrgtbl[rgid].rg_start = caller->mm->mmap->sbrk;
    caller->mm->symrgtbl[rgid].rg_end = caller->mm->mmap->sbrk + size;
    caller->mm->mmap->sbrk += size;
    *alloc_addr = caller->mm->symrgtbl[rgid].rg_start;
      #ifdef EX
  printf("\n\tRun ALLOC %d: Process %2d\n", size,  caller->pid);
  printf("\tRange ID: %d, Start: %lu, End: %lu\n", rgid, caller->mm->symrgtbl[rgid].rg_start, caller->mm->symrgtbl[rgid].rg_end);
    print_rg_memphy(caller, caller->mm->symrgtbl[rgid]);
  printf("\tUsed Region List: \n");
  for(int rgit = 0 ; rgit < PAGING_MAX_SYMTBL_SZ; rgit++){
      if(caller->mm->symrgtbl[rgit].rg_start == 0 && caller->mm->symrgtbl[rgit].rg_end == 0){
        continue;
      }
      printf("\trg[%ld->%ld]\n", caller->mm->symrgtbl[rgit].rg_start, caller->mm->symrgtbl[rgit].rg_end);
    }
  printf("\tFree Region List:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  #endif
    return 0;
  }

  
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  // int inc_limit_ret
  int old_sbrk;

  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  if (inc_vma_limit(caller, vmaid, inc_sz) < 0) {
    printf("Failed to increase virtual memory area limit!\n");
    return -1;
  }

  /*Successful increase limit */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  cur_vma->sbrk += inc_sz;
    #ifdef EX
  printf("\n\tRun ALLOC %d: Process %2d\n", size,  caller->pid);
  printf("\tRange ID: %d, Start: %lu, End: %lu\n", rgid, caller->mm->symrgtbl[rgid].rg_start, caller->mm->symrgtbl[rgid].rg_end);
  print_rg_memphy(caller, caller->mm->symrgtbl[rgid]);
  printf("\tUsed Region List: \n");
  for(int rgit = 0 ; rgit < PAGING_MAX_SYMTBL_SZ; rgit++){
      if(caller->mm->symrgtbl[rgit].rg_start == 0 && caller->mm->symrgtbl[rgit].rg_end == 0){
        continue;
      }
      printf("\trg[%ld->%ld]\n", caller->mm->symrgtbl[rgit].rg_start, caller->mm->symrgtbl[rgit].rg_end);
    }
  printf("\tFree Region List:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  #endif
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid) {
  struct vm_rg_struct rgnode;

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;
  int i;
  int page_start = PAGING_PGN(caller->mm->symrgtbl[rgid].rg_start);
  int page_end = PAGING_PGN(caller->mm->symrgtbl[rgid].rg_end);
  for ( i = page_start; i <= page_end; i++)
  {
    /* code */
    caller->mm->pgd[i] = 0;
  }
  
  /* TODO: Manage the collect freed region to freerg_list */
  rgnode.rg_start = caller->mm->symrgtbl[rgid].rg_start;
  rgnode.rg_end = caller->mm->symrgtbl[rgid].rg_end;

  caller->mm->symrgtbl[rgid].rg_start = 0;
  caller->mm->symrgtbl[rgid].rg_end = 0;
  
  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, rgnode);
  
  #ifdef EX
  printf("\n\tRun FREE: Process %2d\n", caller->pid);
  printf("\tRange ID: %d, Start: %lu, End: %lu\n", rgid, rgnode.rg_start, rgnode.rg_end);
  print_rg_memphy(caller, rgnode);
  printf("\tUsed Region List: \n");
  for(int rgit = 0 ; rgit < PAGING_MAX_SYMTBL_SZ; rgit++){
      if(caller->mm->symrgtbl[rgit].rg_start == 0 && caller->mm->symrgtbl[rgit].rg_end == 0){
        continue;
      }
      printf("\trg[%ld->%ld]\n", caller->mm->symrgtbl[rgit].rg_start, caller->mm->symrgtbl[rgit].rg_end);
    }
  printf("\tFree Region List:\n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  #endif
  
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index) {
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index) {
  return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte)) { /* Page is not online, make it actively living */
    int vicpgn, swpfpn;
    int vicfpn;
    uint32_t vicpte;

    int tgtfpn = PAGING_SWP(pte); // the target frame storing our variable

    /* TODO: Play with your paging theory here */
    /* Find victim page */
    find_victim_page(caller->mm, &vicpgn);

    vicpte = mm->pgd[vicpgn];

    vicfpn = PAGING_FPN(vicpte);

    /* Get free frame in MEMSWP */
    MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

    /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
    /* Copy victim frame to swap */
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    /* Copy target frame from swap to mem */
    __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

    /* Update page table */
    pte_set_swap(&vicpte, 0, swpfpn);

    /* Update its online status of the target page */
    // pte_set_fpn() & mm->pgd[pgn];
    pte_set_fpn(&pte, vicfpn);

    /* Update fifo_pgn of process */
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }
  // printf("PTE %08x\n",pte);
  *fpn = (pte)&0xFFF;

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data,
              struct pcb_t *caller) {
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */
  // printf("RUN %d %d %d\n",fpn,pgn,off);
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram, phyaddr, data);

  #ifdef EX
  printf("\tPosition: Addr 0x%x\n", phyaddr);
  #endif
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value,
              struct pcb_t *caller) {
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  printf("RUN %d\n",phyaddr);
  MEMPHY_write(caller->mram, phyaddr, value);
  #ifdef EX
  printf("\tPosition: Addr 0x%x\n", phyaddr);
  #endif
  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data) {
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  #ifdef EX
  printf("\n\tRun READ: Process %2d\n", caller->pid);
  #endif
  // MEMPHY_dump(caller->mram);
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);
  #ifdef EX
  printf("\tUsed frame list: \n");
  print_list_fp(caller->mram->used_fp_list);
  #endif
  // MEMPHY_dump(caller->mram);
  return 0;
}

/*pgwrite - PAGING-based read a region memory */
int pgread(struct pcb_t *proc, // Process executing the instruction
           uint32_t source,    // Index of source register
           uint32_t offset,    // Source address = [source] + [offset]
           uint32_t destination) {
  BYTE data;
  MEMPHY_dump(proc->mram);
  if(proc->mm->symrgtbl[source].rg_start <= 0 && proc->mm->symrgtbl[source].rg_end <= 0){
    printf("Read from invalid memory region\n");
      proc->pc=proc->code->size;

    return -1;
  }
  int val = __read(proc, 0, source, offset, &data);

  // proc->regs[destination] = (uint32_t)data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  printf("Value: %d\n", data);
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value) {
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  #ifdef EX
  printf("\n\tRun WRITE: Process %2d\n", caller->pid);
  #endif
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  
  #ifdef EX
  printf("\tUsed frame list: \n");
  print_list_fp(caller->mram->used_fp_list);
  #endif
  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(struct pcb_t *proc,   // Process executing the instruction
            BYTE data,            // Data to be wrttien into memory
            uint32_t destination, // Index of destination register
            uint32_t offset) {
    if(proc->mm->symrgtbl[destination].rg_start <= 0 && proc->mm->symrgtbl[destination].rg_end <= 0){
      printf("Write to invalid memory region\n");
      proc->pc=proc->code->size;
      return -1;  
    }
    int t = __write(proc, 0, destination, offset, data);
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return t;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller) {
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++) {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte)) {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid,
                                             int size, int alignedsz) {
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart,
                             int vmaend) {
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  /* TODO validate the planned memory area is not overlapped */
  struct vm_area_struct *vma = caller->mm->mmap;
  while (vma != NULL) {
    if (vma != cur_vma) {
      if ((vmaend > vma->vm_start && vmaend <= vma->vm_end) ||
          (vmastart <= vma->vm_start && vmaend >= vma->vm_end)) {
        /* Overlap detected */
        return -1;
      }
    }
    vma = vma->vm_next;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz) {
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area =
      get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  // Save the vm_end of the current vma
  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1; /*Overlap and failed allocation */

  /* The obtained vm area (only)
   * now will be alloc real ram region */
  if(cur_vma->vm_start == cur_vma->vm_end)
  {
    cur_vma->vm_freerg_list = NULL;
  }
  
  cur_vma->vm_end += inc_amt;
  if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage,
                 newrg) < 0)
    return -1; /* Map the memory to MEMRAM */

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn) {
  struct pgn_t *pgit = mm->fifo_pgn;
  struct pgn_t *pre = pgit;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (pgit == NULL){
    *retpgn = 0;
    return -1;
  }

  // Get the end of the page list
  while (pgit->pg_next != NULL) {
    pgit = pgit->pg_next;
  }

  while (pre->pg_next != pgit) {
    pre = pre->pg_next;
  }
  *retpgn = pgit->pgn;
  pre->pg_next = NULL;

  /* Put the victim page to the head of the page list */
  // enlist_pgn_node(&mm->fifo_pgn, pgit->pgn);

  free(pgit);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg) {
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  // printf("st: %lu, en: %lu\n", caller->mm->mmap->vm_freerg_list->rg_start, caller->mm->mmap->vm_freerg_list->rg_start);
  // printf("Vmaid = %d, size = %d", vmaid, size);

  // printf("Start: %lu, End: %lu\n", rgit->rg_start, rgit->rg_end);
  
  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL) {
    if (rgit->rg_start + size <= rgit->rg_end) { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end) {
        rgit->rg_start = rgit->rg_start + size;
      } else { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL) {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        } else {                         /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    } else {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

#endif
