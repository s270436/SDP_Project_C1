#ifndef _VM_TLB_H_
#define _VM_TLB_H_

#include "opt-projectc1.h"

#if OPT_PROJECTC1

void tlb_bootstrap(void);
void write_entry(int index, uint32_t vaddr, uint32_t paddr);
void add_entry(int *index_tlb, uint32_t vaddr, uint32_t paddr);
int read_entry(uint32_t vaddr, uint32_t *paddr);
void reset_one_entry_by_index(int index);
void reset_tlb(void);
void reset_tlb_pid_different(pid_t current_pid);

#endif

#endif