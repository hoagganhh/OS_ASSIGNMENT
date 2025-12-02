/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev_rgit = NULL;

  if (rgit == NULL)
    return -1;

  newrg->rg_start = newrg->rg_end = -1;

  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { 
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start += size;
      }
      else
      { 
        if (prev_rgit == NULL) 
        {
            cur_vma->vm_freerg_list = rgit->rg_next;
        } 
        else 
        {
            prev_rgit->rg_next = rgit->rg_next;
        }
        free(rgit); 
      }
      return 0;
    }
    prev_rgit = rgit;
    rgit = rgit->rg_next;
  }
  return -1;
}

/*__alloc - allocate a region memory */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* Expand the heap (sbrk) */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz = PAGING_PAGE_ALIGNSZ(size); 
  int old_sbrk = cur_vma->sbrk;
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = inc_sz; // Use aligned size
  
  if (syscall(caller->krnl, caller->pid, 17, &regs) == -1) {
      pthread_mutex_unlock(&mmvm_lock);
      return -1;
  }

  /* Successful increase limit */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  *alloc_addr = old_sbrk;

  /* Update sbrk (This should be done by syscall handler, but we update locally for safety) */
  // cur_vma->sbrk += inc_sz; // Uncomment if syscall doesn't update it

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);

  if (rgnode->rg_end == 0) { 
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  /* Reset the symbol table entry */
  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /* Enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1) return -1;
#ifdef IODUMP
  printf("liballoc:%lu\n", (unsigned long)size); 
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}

/*libfree - PAGING-based free a region memory */
int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1) return -1;

#ifdef IODUMP
  printf("libfree:%u\n", reg_index);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif
  return val;
}

/*pg_getpage - get the page in ram */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = pte_get_entry(caller, pgn);

  if (!PAGING_PAGE_PRESENT(pte))
  { 
    addr_t vicpgn, swpfpn, vicfpn;
    addr_t tgtfpn;

    if (find_victim_page(caller->mm, &vicpgn) == -1) return -1;

    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1) return -1;

    uint32_t vict_pte = pte_get_entry(caller, vicpgn);
    vicfpn = PAGING_PTE_FPN(vict_pte);

    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    
    pte_set_swap(caller, vicpgn, 0, swpfpn);

    tgtfpn = vicfpn;
    pte_set_fpn(caller, pgn, tgtfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(pte_get_entry(caller,pgn));
  return 0;
}

/*pg_getval - read value at given offset */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  MEMPHY_read(caller->krnl->mram, phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) return -1;

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  MEMPHY_write(caller->krnl->mram, phyaddr, value);

  return 0;
}

/*__read - read value in region memory */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  if (currg == NULL) return -1;

  return pg_getval(caller->mm, currg->rg_start + offset, data, caller);
}

/*libread - PAGING-based read a region memory */
int libread(struct pcb_t *proc, uint32_t source, addr_t offset, uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);
  *destination = data;

#ifdef IODUMP
  printf("libread:%d\n", data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif
  return val;
}

/*__write - write a region memory */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  if (currg == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1) return -1;

#ifdef IODUMP
  printf("libwrite:%d\n", data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
#endif
  return val;
}

/*free_pcb_memphy - collect all memphy of pcb */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*find_victim_page - find victim page */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  if (!pg) return -1;

  /* Remove the last node (oldest page) */
  if (pg->pg_next == NULL) {
      *retpgn = pg->pgn;
      free(pg);
      mm->fifo_pgn = NULL;
  } else {
      struct pgn_t *prev = NULL;
      while (pg->pg_next) {
        prev = pg;
        pg = pg->pg_next;
      }
      *retpgn = pg->pgn;
      prev->pg_next = NULL;
      free(pg);
  }

  return 0;
}