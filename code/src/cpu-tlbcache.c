/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef MM_TLB
/*
 * Memory physical based TLB Cache
 * TLB cache module tlb/tlbcache.c
 *
 * TLB cache is physically memory phy
 * supports random access 
 * and runs at high speed
 */


#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

#define init_tlbcache(mp,sz,...) init_memphy(mp, sz, (1, ##__VA_ARGS__))

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
uint32_t tlb_create_value(uint32_t pte,uint32_t pgn,int _valid){
    uint32_t fpn = pte & 0x00001FFF;
    uint32_t pgnum = pgn; pgnum=pgnum<<13;
    uint32_t valid = _valid; valid=valid<<31;
    uint32_t swap = pte & 0x40000000; swap=swap<<30;
    uint32_t val = valid | swap | pgnum | fpn;
    // printf("PTE: %08x\n",pgn);
    // printf("FPN %08x, PGN %08x, \n",fpn,pgnum);
    return val;
} // Create a TLB value
int tlb_get_pid(struct memphy_struct* mp,int addr){
    if(!mp) return -1;
    if(tlb_empty(mp,addr)) return -1;
    return mp->storage[addr*5+4];
} // Get PID from TLB value
int tlb_empty(struct memphy_struct* mp, int addr){
    if(!mp) return -1;
    if(addr<0 || addr>=mp->maxsz) return -1;
    uint32_t val = tlb_get_value(mp,addr);
    val = val & 0x80000000;
    // if(mp->storage[addr*5]==0 && mp->storage[addr*5+1]==0 && mp->storage[addr*5+2]==0 && mp->storage[addr*5+3]==0 && mp->storage[addr*5+4]==0) return 1;
    return !val;

} // Empty TLB value
uint32_t tlb_get_pgn(struct memphy_struct* mp, int addr){
    if(!mp) return -1;
    if(tlb_empty(mp,addr)) return -1;
    uint32_t pgn = tlb_get_value(mp,addr);
    pgn = pgn & 0x07FFE000;
    pgn = pgn >> 13;
    return pgn;
} // Get PGN from TLB value
int tlb_set_value(struct memphy_struct* mp, int addr, uint32_t value,int pid){
    if(!mp) return -1;
    if(addr<0 || addr>=mp->maxsz) return -1;
    // printf("GHI VAO TLB: %08x\n",value);
    mp->storage[addr*5] = (value & 0xFF000000 )>> 24;
    mp->storage[addr*5+1] = (value &0x00FF0000 ) >> 16;
    mp->storage[addr*5+2] = (value & 0x0000FF00) >> 8;
    mp->storage[addr*5+3] = (value & 0x000000FF) ;
    mp->storage[addr*5+4] = pid;
    return 0;

} // Set TLB value
int tlb_get_addr(struct memphy_struct* mp, int pid, int pgn){
    uint32_t pid_ = pid;
    pid_ = pid_ * 9173;
    pid_ = pid_ + pgn+971;
    pid_ = pid_ % mp->maxsz; 
    return pid_;
} // Get TLB address
int tlb_get_fpn(struct memphy_struct* mp, int addr){
    if(!mp) return -1;
    if(tlb_empty(mp,addr)) return -1;
    uint32_t fpn = tlb_get_value(mp,addr);
    fpn = fpn & 0x00001FFF;
    return fpn;
} // Get FPN from TLB value
uint32_t tlb_get_value(struct memphy_struct* mp, int addr){
    uint32_t val = 0;
    uint32_t t1=( mp->storage[addr*5]  & 0x000000FF ) << 24;
    uint32_t t2=( mp->storage[addr*5+1] & 0x000000FF ) <<16;
    uint32_t t3=( mp->storage[addr*5+2] & 0x000000FF) <<8;
    uint32_t t4=( mp->storage[addr*5+3] & 0x000000FF) ;
    val =t1|t2|t3|t4;
    return val;
} // Get TLB value

int tlb_cache_read(struct memphy_struct * mp, int pid, int pgnum, int* value)
{
   /* TODO: the identify info is mapped to 
    *      cache line by employing:
    *      direct mapped, associated mapping etc.
    */
    if (mp == NULL || pgnum < 0 || pgnum >= mp->maxsz)
        return -1; // Tham số không hợp lệ hoặc mp không tồn tại
    int addr = tlb_get_addr(mp, pid, pgnum); // Lấy địa chỉ TLB từ PID và PGN
    int pid_tlb = tlb_get_pid(mp, addr); // Lấy PID từ TLB
    int pgnum_tlb = tlb_get_pgn(mp, addr); // Lấy PGN từ TLB
    if(pid!=pid_tlb) return -1;
    if(pgnum!=pgnum_tlb) return -1;
    *value = tlb_get_fpn(mp, addr); // Lấy FPN từ TLB
    return *value;
    return 0; // Đọc thành công từ bộ nhớ vật lý
}

/*
 *  tlb_cache_write write TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_write(struct memphy_struct *mp, int pid, int pgnum, int value)
{
   /* TODO: the identify info is mapped to 
    *      cache line by employing:
    *      direct mapped, associated mapping etc.
    */
     if (mp == NULL || pgnum < 0 || pgnum >= mp->maxsz)
        return -1; // Tham số không hợp lệ hoặc mp không tồn tại

    int addr = pgnum * PAGE_SIZE; // Tính địa chỉ bắt đầu của trang

    // Ghi giá trị vào bộ nhớ vật lý
    TLBMEMPHY_write(mp, addr, value);

    // Cập nhật TLB cache
    for (int i = 0; i < mp->maxsz; i++) {
        if (!mp->tlb[i].valid) {
            mp->tlb[i].pid = pid;
            mp->tlb[i].pgnum = pgnum;
            mp->tlb[i].value = value;
            mp->tlb[i].valid = 1;
            break;
        }
    }
    return 0; // Ghi thành công vào bộ nhớ vật lý
}

/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int TLBMEMPHY_read(struct memphy_struct * mp, int addr, int *value)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   *value = mp->storage[addr];

   return 0;
}


/*
 *  TLBMEMPHY_write natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int TLBMEMPHY_write(struct memphy_struct * mp, int addr, int data)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   mp->storage[addr] = data;

   return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */


int TLBMEMPHY_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
    *     for tracing the memory content
    */
    if (mp == NULL) {
        printf("Error: Invalid memory physical structure\n");
        return -1;
    }
    for(int i = 0; i < mp->maxsz/5; i++){
        if(!tlb_empty(mp,i))
        printf("Memory physical content %d: %08x %02x\n", i, tlb_get_value(mp,i),tlb_get_pid(mp,i));
        //printf("So Trang va PGN: %d %d\n",tlb_get_fpn(mp,i),tlb_get_pgn(mp,i));
    }
    
    return 0;
}


/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct memphy_struct *mp, int max_size)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;
   mp->tlb = (struct tlb_entry_t *)malloc(max_size * sizeof(struct tlb_entry_t));
   for (int i = 0; i < max_size; i++) {
       mp->tlb[i].valid = 0;
   }
   mp->rdmflg = 1;

   return 0;
}

//#endif
