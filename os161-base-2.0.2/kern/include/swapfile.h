#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include "opt-projectc1.h"
#include <pt.h>
#include <types.h>
#include <addrspace.h>

#if OPT_PROJECTC1

void swap_bootstrap(void);

void swap_out(pid_t pid, vaddr_t vaddr, permission_t permission_flag, paddr_t paddr);

int swap_in(struct addrspace *as, pid_t pid, vaddr_t vaddr, paddr_t *paddr);

void swap_destroy(void);

#endif

#endif