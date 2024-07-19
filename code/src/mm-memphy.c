
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef MM_PAGING

pthread_mutex_t mp_lock = PTHREAD_MUTEX_INITIALIZER;

int print_rg_memphy(struct pcb_t *caller, struct vm_rg_struct rgnode){
  int pgn_start = PAGING_PGN(rgnode.rg_start);
  int pgn_end = PAGING_PGN(rgnode.rg_end);
  int off_start = PAGING_OFFST(rgnode.rg_start);
  int off_end = PAGING_OFFST(rgnode.rg_end);
  int fpn_start, fpn_end;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  pg_getpage(caller->mm, pgn_start, &fpn_start, caller);
  pg_getpage(caller->mm, pgn_end, &fpn_end, caller);
  
  printf("\tPhysical: Start = Addr 0x%x, End = Addr 0x%x\n", fpn_start + off_start, fpn_end + off_end);
  return 0;
}


/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, int offset) {
  int numstep = 0;
  pthread_mutex_lock(&mp_lock);

  mp->cursor = 0;
  while (numstep < offset && numstep < mp->maxsz) {
    /* Traverse sequentially */
    mp->cursor = (mp->cursor + 1) % mp->maxsz;
    numstep++;
  }

  pthread_mutex_unlock(&mp_lock);

  return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value) {
  pthread_mutex_lock(&mp_lock);
  if (mp == NULL){
    pthread_mutex_unlock(&mp_lock);
    return -1;
  }

  if (!mp->rdmflg){
    pthread_mutex_unlock(&mp_lock);
    return -1; /* Not compatible mode for sequential read */
  }

  MEMPHY_mv_csr(mp, addr);
  *value = (BYTE)mp->storage[addr];

  pthread_mutex_unlock(&mp_lock);

  return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value) {
  if (mp == NULL){
    return -1;
  }

  if (mp->rdmflg)
    *value = mp->storage[addr];
  else{/* Sequential access device */
    return MEMPHY_seq_read(mp, addr, value);
  }

  return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct *mp, int addr, BYTE value) {
  pthread_mutex_lock(&mp_lock);

  if (mp == NULL){
    pthread_mutex_unlock(&mp_lock);
    return -1;
  }

  if (!mp->rdmflg){
    pthread_mutex_unlock(&mp_lock);
    return -1; /* Not compatible mode for sequential read */
  }

  MEMPHY_mv_csr(mp, addr);
  mp->storage[addr] = value;
  pthread_mutex_unlock(&mp_lock);

  return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct *mp, int addr, BYTE data) {
  if (mp == NULL){
    return -1;
  }

  if (mp->rdmflg)
    mp->storage[addr] = data;
  else{ /* Sequential access device */
    return MEMPHY_seq_write(mp, addr, data);
  }

  return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz) {
  /* This setting come with fixed constant PAGESZ */
  int numfp = mp->maxsz / pagesz;
  struct framephy_struct *newfst, *fst;
  int iter = 0;

  if (numfp <= 0){
    return -1;
  }

  /* Init head of free framephy list */
  fst = malloc(sizeof(struct framephy_struct));
  fst->fpn = iter;
  mp->free_fp_list = fst;

  /* We have list with first element, fill in the rest num-1 element member*/
  pthread_mutex_lock(&mp_lock);
  for (iter = 1; iter < numfp; iter++) {
    newfst = malloc(sizeof(struct framephy_struct));
    newfst->fpn = iter;
    newfst->fp_next = NULL;
    fst->fp_next = newfst;
    fst = newfst;
  }
  pthread_mutex_unlock(&mp_lock);

  return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn) {
  pthread_mutex_lock(&mp_lock);
  struct framephy_struct *fpit = mp->free_fp_list;

  if (fpit == NULL){
    pthread_mutex_unlock(&mp_lock);
    return -1;
  }

  *retfpn = fpit->fpn;
  mp->free_fp_list = fpit->fp_next;

  /* MEMPHY is iteratively used up until its exhausted
   * No garbage collector acting then it not been released
   */
  free(fpit);
  pthread_mutex_unlock(&mp_lock);

  return 0;
}

int numFile = 0;

int MEMPHY_dump(struct memphy_struct *mp) {
  /*TODO dump memphy content mp->storage
   *     for tracing the memory content
   */
  for(int i=0;i<mp->maxsz;i++){
    BYTE data;
    
    MEMPHY_read(mp, i, &data);
    //if(data!=0)
    //printf("Addr 0x%x: %d\n", i, (uint32_t)data);
  }
  #ifdef MEM_TO_FILE
  FILE *ptr;
  char file[100] = "\0";
  char *num[10];
  sprintf(num, "%d", numFile);
  strcat(file, "output");
  strcat(file, num);
  strcat(file, ".txt");
  ptr = fopen(file, "w");
  fprintf(ptr, "Memphy content:\n");
  for (int i = 0; i < mp->maxsz; i++) {
    BYTE data;
    MEMPHY_read(mp, i, &data);
    fprintf(ptr, "Addr 0x%x: %d\n", i, (uint32_t)data); // %x is hex
  }
  numFile++;
  fclose(ptr);
  #endif
  return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn) {
  pthread_mutex_lock(&mp_lock);
  struct framephy_struct *fp = mp->free_fp_list;
  struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

  /* Create new node with value fpn */
  newnode->fpn = fpn;
  newnode->fp_next = fp;
  mp->free_fp_list = newnode;
  pthread_mutex_unlock(&mp_lock);

  return 0;
}

/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg) {
  mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
  mp->maxsz = max_size;

  MEMPHY_format(mp, PAGING_PAGESZ);

  mp->rdmflg = (randomflg != 0) ? 1 : 0;

  if (!mp->rdmflg) /* Not Ramdom acess device, then it serial device*/
    mp->cursor = 0;

  return 0;
}

int MEMPHY_put_fp(struct memphy_struct *mp, int fpn)
{
   pthread_mutex_lock(&mp_lock);
   struct framephy_struct *fp = mp->used_fp_list;
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

   /* Create new node with value fpn */
   newnode->fpn = fpn;
   newnode->fp_next = fp;
   mp->used_fp_list = newnode;
   pthread_mutex_unlock(&mp_lock);
   return 0;
}

#endif
