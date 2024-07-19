/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef CPU_TLB
/*
 * CPU TLB
 * TLB module cpu/cpu-tlb.c
 */
 
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

int tlb_change_all_page_tables_of(struct pcb_t *proc,  struct memphy_struct * mp)
{
  /* TODO update all page table directory info 
   *      in flush or wipe TLB (if needed)
   */
    tlb_flush_tlb_of(proc, mp);
    return 0;
}

int tlb_flush_tlb_of(struct pcb_t *proc, struct memphy_struct * mp)
{
  // Lặp qua tất cả các trang trong cache TLB
    // for (int i = 0; i < mp->maxsz; i++) {
    //     // Đặt các giá trị của mỗi trang về trạng thái mặc định
    //     mp->tlb[i].pid = -1;
    //     mp->tlb[i].pgnum = -1;
    //     mp->tlb[i].value = -1;
    //     mp->tlb[i].valid = 0;
    // }
    for(int i=0; i<mp->maxsz; i++){
        if(tlb_get_pid(mp, i) == proc->pid){
            tlb_set_value(mp,i,tlb_create_value(0,0,0),-1);
        }
    }
    return 0;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr, val;

  /* By default using vmaid = 0 */
  val = __alloc(proc, 0, reg_index, size, &addr);

  /* TODO update TLB CACHED frame num of the new allocated page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  int n_page = (PAGING_PAGE_ALIGNSZ(proc->mm->symrgtbl[reg_index].rg_end) - PAGING_PAGE_ALIGNSZ(proc->mm->symrgtbl[reg_index].rg_start))/PAGING_PAGESZ;
  printf("SO TRANG DUOC CUNG CAP VA TLB PGN: %d page ",n_page);
  for(int i=0;i<n_page;i++){
      int pgn = PAGING_PGN(proc->mm->symrgtbl[reg_index].rg_start)+i;
      // printf("TLB PGN : %d\n",pgn);
        printf("%d ",pgn);
      tlb_set_value(proc->tlb,
                    tlb_get_addr(proc->tlb,proc->pid,pgn),
                    tlb_create_value(proc->mm->pgd[pgn],pgn,1),
                    proc->pid);
      // printf("DOC RA %08x\n %d",tlb_get_value(proc->tlb,tlb_get_addr(proc->tlb,proc->pid,pgn)),tlb_get_addr(proc->tlb,proc->pid,pgn));
  }
  printf("\n");
  TLBMEMPHY_dump(proc->tlb);
  return val;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  int start_addr = PAGING_PGN(proc->mm->symrgtbl[reg_index].rg_start);
  int end_addr = PAGING_PGN(proc->mm->symrgtbl[reg_index].rg_end);

  __free(proc, 0, reg_index);
  for(int i=start_addr;i<=end_addr;i++){
      // tlb_set_value(proc->tlb,tlb_get_addr(proc->tlb,proc->pid,PAGING_PGN(proc->mm->symrgtbl[reg_index].rg_start)+i),tlb_create_value(reg_index,addr+i,1),proc->pid);

      tlb_set_value(proc->tlb,tlb_get_addr(proc->tlb,proc->pid,PAGING_PGN(proc->mm->symrgtbl[reg_index].rg_start)+i),tlb_create_value(0,0,0),proc->pid);
  }
  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  printf("FREE %d page\n", end_addr-start_addr);
  //TLBMEMPHY_dump(proc->tlb);

  return 0;
}


/*tlbread - CPU TLB-based read a region memory
 *@proc: Process executing the instruction
 *@source: index of source register
 *@offset: source address = [source] + [offset]
 *@destination: destination storage
 */
int tlbread(struct pcb_t * proc, uint32_t source,
            uint32_t offset, 	uint32_t destination) 
{
  BYTE data;
  int frmnum = -1;
	int val =0;
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/
  int page = PAGING_PGN((proc->mm->symrgtbl[source].rg_start + offset));
  int off = PAGING_OFFST((proc->mm->symrgtbl[source].rg_start + offset));
  frmnum = tlb_cache_read(proc->tlb, proc->pid, page, &val);
	if(frmnum<0){
    val = __read(proc, 0, source, offset, &data);
  }else{
    int addr =( frmnum << PAGING_ADDR_FPN_LOBIT )+ off;
    val = MEMPHY_read(proc->mram,addr,&data);
    printf("READ DATA: %d\n",data);
  }
#ifdef IODUMP
  if (frmnum >= 0)
    printf("TLB hit at read region=%d offset=%d\n", 
	         source, offset);
  else 
    printf("TLB miss at read region=%d offset=%d\n", 
	         source, offset);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
  TLBMEMPHY_dump(proc->tlb);
#endif

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  // tlb_cache_write(proc->tlb, proc->pid, source + offset, val);
  return val;
}

/*tlbwrite - CPU TLB-based write a region memory
 *@proc: Process executing the instruction
 *@data: data to be wrttien into memory
 *@destination: index of destination register
 *@offset: destination address = [destination] + [offset]
 */
int tlbwrite(struct pcb_t * proc, int data_,
             uint32_t destination, uint32_t offset)
{
  int val = 0;
  int frmnum = -1;
  BYTE data = data_;
  /* TODO retrieve TLB CACHED frame num of accessing page(s))*/
  /* by using tlb_cache_read()/tlb_cache_write()
  frmnum is return value of tlb_cache_read/write value*/
  int t=0;
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/
  int page = PAGING_PGN((proc->mm->symrgtbl[destination].rg_start + offset));
  int off = PAGING_OFFST((proc->mm->symrgtbl[destination].rg_start + offset));
  printf("PAGE %d\n",page);
  frmnum = tlb_cache_read(proc->tlb, proc->pid, page, &t);
	if(frmnum<0){
    val = __write(proc, 0, destination, offset,data);
  }else{
    int addr = (frmnum << PAGING_ADDR_FPN_LOBIT )+ off;
    val = MEMPHY_write(proc->mram,addr,data);
    printf("WRITE DATA: %d\n",data);
  }
#ifdef IODUMP
  if (frmnum >= 0)
    printf("TLB hit at write region=%d offset=%d value=%d\n",
	          destination, offset, data);
	else
    printf("TLB miss at write region=%d offset=%d value=%d\n",
            destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
  TLBMEMPHY_dump(proc->tlb);
#endif

  return val;
}

//#endif
