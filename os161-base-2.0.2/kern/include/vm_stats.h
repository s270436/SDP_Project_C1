#ifndef _VM_STATS_H_
#define _VM_STATS_H_

#include "opt-projectc1.h"

#if OPT_PROJECTC1

extern void init_stats(void);
extern void increment_tlb_faults(void);
extern void increment_tlb_faults_free(void);
extern void increment_tlb_faults_replace(void);
extern void increment_tlb_invalidations(void);
extern void increment_tlb_reloads(void);
extern void increment_page_faults_zeroed(void);
extern void increment_page_faults_disk(void);
extern void increment_page_faults_elf(void);
extern void increment_page_faults_swapin(void);
extern void increment_page_faults_swapout(void);
extern void increment_swapfile_writes(void);
extern void print_all_statistics(void);

#endif

#endif