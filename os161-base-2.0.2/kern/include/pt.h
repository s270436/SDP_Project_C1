#ifndef _PT_H_
#define _PT_H_

#include <types.h>
#include "opt-projectc1.h"
#include <spinlock.h>

typedef enum { READ_ONLY, READ_WRITE } permission_t;

#if OPT_PROJECTC1

typedef struct entry {
	vaddr_t vaddr; //virtual address of the page
	pid_t pid; //process id of the process sharing the page. We don't need to store address space pointer as we can 
    // index into process table using pid and can get the address space by accessing the thread structure.
	uint32_t status; //page status: Kernel, free, dirty, clean, etc.
    permission_t permission_flag; // Page can be READ-ONLY or read-write
    int position_fifo; //used to know when the page was added into the page table
} entry_t;

typedef struct table {
    entry_t * next_entry;
    unsigned int length;
    struct spinlock table_lock;
} table_t;

void lru_update_cnt(void);

void page_table_init(void);

void page_table_add_entry(pid_t pid, vaddr_t vaddr, paddr_t paddr, uint32_t status);

int page_table_get_paddr_entry(pid_t pid, vaddr_t vaddr, paddr_t* paddr, uint32_t* status);

void page_table_reset_entry(int i);

void page_table_destroy(void);

int page_table_replacement(pid_t pid, entry_t *entry);

void page_table_remove_on_pids(pid_t pid);

unsigned char page_table_get_Status_on_Index(int index); 

void page_table_set_status_at_index(int index, unsigned char val);
#endif

#endif