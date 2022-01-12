#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <spinlock.h>

#include "opt-projectc1.h"

#if OPT_PROJECTC1

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

void coremap_bootstrap(void);
int isTableActive(void);
int freeppages(paddr_t paddr);
paddr_t getppages(unsigned long npages);
void coremap_destroy(void);

/*TLB Structure, top bit of VPN is always zero to indicate User segment*/
//<----------------20------------------->|<----6---->|<----6---->|
//_______________________________________________________________
//|0|     Virtual Page Number            |   ASID    |     0     |  EntryHi
//|______________________________________|___________|___________|
//|       Page Frame Number              |N|D|V|G|       0       |  EntryLo
//|______________________________________|_______|_______________|

/*20 bit Page address*/
//<----------------20------------------->|<---------12---------->|
//_______________________________________________________________
//|           Page Address               |N|D|V|G|0|0|0|0|0|0|0|K|  
//|______________________________________|_______|_______|_______|
/*Macros for managing attibute bits of a page entry*/

#define IS_KERNEL(x) ((x) & 0x00000001)
#define SET_KERNEL(x) ((x) | 0x00000001)

#define IS_VALID(x) ((x) & 0x00000200)
#define SET_VALID(x) ((x) | 0x00000200)

#define IS_DIRTY(x) ((x) & 0x00000400)
#define SET_DIRTY(x) ((x) | 0x00000400)

#define ISSWAPPED(x) ((x) & 0x00000080)
#define SET_SWAPPED(x) ((x) | 0x00000080)

#endif

#endif /* _COREMAP_H_ */
