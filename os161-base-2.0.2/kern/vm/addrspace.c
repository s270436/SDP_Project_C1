/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spinlock.h>
#include <cpu.h>
#include <mips/tlb.h> // here there's the definition of NUM_TLB
#include <uio.h>
#include <vfs.h>
#include <vnode.h>
#include "vm_tlb.h"
#include <vm_stats.h>
#include "pt.h"
#include <coremap.h>
#include "swapfile.h"
#include <current.h> //definition of curproc
#include <cpu.h>

#include <opt-projectc1.h>

#define DUMBVM_STACKPAGES    18		// number of pages that will be assigned to the stack

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct spinlock slock = SPINLOCK_INITIALIZER;

//static struct spinlock freemem_lock = SPINLOCK_INITIALIZER;

static unsigned char *freeRamFrames = NULL;
static unsigned long *allocSize = NULL;
static int nRamFrames = 0;

static int allocTableActive = 0;

void
vm_bootstrap(void){

	int i;
	nRamFrames = ((int)ram_getsize())/PAGE_SIZE;  
	/* alloc freeRamFrame and allocSize */  
	freeRamFrames = kmalloc(sizeof(unsigned char)*nRamFrames);
	if (freeRamFrames==NULL) return;  
	allocSize     = kmalloc(sizeof(unsigned long)*nRamFrames);
	if (allocSize==NULL) {    
		/* reset to disable this vm management */
		freeRamFrames = NULL; return;
	}
	for (i=0; i<nRamFrames; i++) {    
		freeRamFrames[i] = (unsigned char)0;
		allocSize[i]     = 0;  
	}
	spinlock_acquire(&slock);
	allocTableActive = 1;
	spinlock_release(&slock);
	
	swap_bootstrap();
	page_table_init();
	coremap_bootstrap();
	tlb_bootstrap();
	
	init_stats();
}

static void vm_can_sleep(void){
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t alloc_kpages(unsigned npages){ 
	
	paddr_t pa;

	vm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr){
	if (isTableActive()) {
		paddr_t paddr = addr - MIPS_KSEG0;
		freeppages(paddr);
	}
}

struct addrspace *as_create(void){
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize every field of the address space structure
	 */
	as->as_vbase1 = 0;
	as->as_npages1 = 0;	
	as->as_vbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
	#if !OPT_PROJECTC1
        as->as_pbase1 = 0;
        as->as_pbase2 = 0;
    #endif
	as->allocated_pages = 0;

	return as;
}

int as_copy(struct addrspace *old, struct addrspace **ret){
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void as_destroy(struct addrspace *as) {
	/*
	 * Clean up as needed. STEPS:
	  Remove all entries of the current process
	  Close the vnode associated to the addrspace
	  Free the previously allocated addrspace
	*/
	//vm_can_sleep();
	page_table_remove_on_pids(curproc->pid);
	//vfs_close(as->fi.v);

	kfree(as);
}

void as_activate(void) {
	// make curproc's address space the one currently "seen" by the processor.
	// called whenever there is a context switch

	if (proc_getas() == NULL) { //if it is kernel space (kernel is without address space), then skip this part
		return;
	}
	
	reset_tlb_pid_different(curproc->pid);

}

void as_deactivate(void) {
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
					off_t offset, int readable, int writeable, int executable)
{
	/*partially from by dumbvm.c*/

	size_t npages;
	size_t sz2 = sz;

	vm_can_sleep();
	
	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	//(void)as;
	//(void)vaddr;
	//(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		as->fi.code_offset = offset;
		as->fi.code_sz = sz2;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		as->fi.data_offset = offset;
		as->fi.data_sz = sz2;
		return 0;
	}
	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

int as_prepare_load(struct addrspace *as) {
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int as_complete_load(struct addrspace *as) {
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr) {
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

void as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

void
vm_tlbshootdown(const struct tlbshootdown *ts){
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

static int read_elf_page(struct vnode* v_node, paddr_t destPhAdd, size_t len, off_t offset) {

	struct iovec iov;
	struct uio ku;
	int result;

	uio_kinit(&iov, &ku, (void*)PADDR_TO_KVADDR(destPhAdd), len, offset, UIO_READ);
	result = VOP_READ(v_node, &ku);
	if(result){
		return result;
	}
	if(ku.uio_resid != 0){
		return ENOEXEC;
	}
	return result;
}

int vm_fault(int faulttype, vaddr_t faultaddress) { //the goal of this function is to find the related paddr of vaddr and write it into the tlb
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	uint32_t ehi, elo;
	struct addrspace *as;
	
	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
			as_destroy(proc_getas());
			thread_exit();
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
			// Count the fault that has happened
			increment_tlb_faults();
			break;
	    default:
			return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
#if OPT_DUMBVM
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
#endif

#if OPT_PROJECTC1
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
#endif

	// Setting code, data segments and stack
	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	paddr_t paddr_temp;
	pid_t pid = curproc->pid;

	int index_tlb = -1;
	size_t size_to_read;
	entry_t empty_entry;
	int index_page_to_replace;

	uint32_t status = 0;

	int result;

	if(page_table_get_paddr_entry(pid, faultaddress, &paddr_temp, &status) == 1){
		// 1 means found
		paddr = paddr_temp;
		increment_tlb_reloads(); 
	}
	else if(swap_in(as, pid, faultaddress, &paddr_temp) == 1){
		paddr = paddr_temp;
		status = page_table_get_Status_on_Index(paddr>>12);
		increment_page_faults_disk();	// The page is uploaded from disk
	}

// On demand page loading begins here
	else {
		if (faultaddress >= vbase1 && faultaddress < vtop1) {
			// Code segment

			if(as->allocated_pages >= MAX_ALLOCATED_PAGES || (paddr = getppages(1)) == 0) { 

                index_page_to_replace = page_table_replacement(pid, &empty_entry); // find index victim to replace
				index_tlb = page_table_get_Status_on_Index(index_page_to_replace);

                if(index_page_to_replace == -1)
                    return 0;

                swap_out(pid, empty_entry.vaddr, 
				empty_entry.permission_flag, 
				index_page_to_replace * PAGE_SIZE); 

                as->allocated_pages--; 
                paddr = index_page_to_replace*PAGE_SIZE;

            } 
			
            as->allocated_pages++; //increase number of allocated pages for that process
			increment_page_faults_zeroed();
			
            // clean the page just got by allocation (or previously swapped)
            as_zero_region(paddr, 1); 

			if (faultaddress == vbase1){ // Check if I'm at the begin of the first page
				if (as->fi.code_sz<PAGE_SIZE-(as->fi.code_offset&~PAGE_FRAME)) 
				// Check if I need to read < 4 KB
					size_to_read = as->fi.code_sz;
				else
					size_to_read = PAGE_SIZE-(as->fi.code_offset&~PAGE_FRAME);
			}
			else if (faultaddress == vtop1 - PAGE_SIZE){ 
				// Check if I'm at the begin of the LAST page
				size_to_read = as->fi.code_sz - (as->as_npages1-1)*PAGE_SIZE; 
				if (as->fi.code_offset&~PAGE_FRAME) 
					// If there exist, remove a possible offset related to the first page
					size_to_read -= (as->fi.code_offset&~PAGE_FRAME);
			}
			else 
			// Last case: being on an intermediate page of the code segment
				size_to_read = PAGE_SIZE;

			result = read_elf_page(as->fi.v, 
					paddr + (faultaddress==vbase1?as->fi.code_offset&~PAGE_FRAME:0), /* se prima pagina del segmento, scrivo in paddr a partire da offset */
					size_to_read,
					faultaddress==vbase1?as->fi.code_offset:(as->fi.code_offset&PAGE_FRAME)+faultaddress-vbase1);
			
			increment_page_faults_disk();
			increment_page_faults_elf();

			status = 0x01; //READONLY
			if (index_tlb != -1)
				status |= index_tlb<<2;
			page_table_add_entry(faultaddress, paddr, pid, status);

			if (result < 0) {}
				//return -1;
		}
		else if (faultaddress >= vbase2 && faultaddress < vtop2) {
			// Data segment
			if(as->allocated_pages >= MAX_ALLOCATED_PAGES || (paddr = getppages(1)) == 0) { 

                index_page_to_replace = page_table_replacement(pid, &empty_entry); // find index victim to replace
				index_tlb = page_table_get_Status_on_Index(index_page_to_replace);

                if(index_page_to_replace == -1)
                    return 0;

                swap_out(pid, empty_entry.vaddr, 
				empty_entry.permission_flag, 
				index_page_to_replace * PAGE_SIZE); 

                as->allocated_pages--; 
                paddr = index_page_to_replace*PAGE_SIZE;

            } 
			
            as->allocated_pages++; //increase number of allocated pages for that process
			increment_page_faults_zeroed();
			
            // clean the page just got by allocation (or previously swapped)
            as_zero_region(paddr, 1); 

			if (faultaddress == vbase2){ // Check if I'm at the begin of the first page
				if (as->fi.data_sz<PAGE_SIZE-(as->fi.data_offset&~PAGE_FRAME)) 
				// Check if I need to read < 4 KB
					size_to_read = as->fi.data_sz;
				else
					size_to_read = PAGE_SIZE-(as->fi.data_offset&~PAGE_FRAME);
			}
			else if (faultaddress == vtop2 - PAGE_SIZE){ 
				// Check if I'm at the begin of the LAST page
				size_to_read = as->fi.data_sz - (as->as_npages2 - 1)*PAGE_SIZE; 
				if (as->fi.data_offset&~PAGE_FRAME) 
					// If there exist, remove a possible offset related to the first page
					size_to_read -= (as->fi.data_offset&~PAGE_FRAME);
			}
			else 
			// Last case: being on an intermediate page of the code segment
				size_to_read = PAGE_SIZE;

			result = read_elf_page(as->fi.v, 
					paddr + (faultaddress == vbase2?as->fi.data_offset&~PAGE_FRAME:0), /* se prima pagina del segmento, scrivo in paddr a partire da offset */
					size_to_read,
					faultaddress == vbase2?as->fi.data_offset:(as->fi.data_offset&PAGE_FRAME)+faultaddress-vbase2);
			
			increment_page_faults_disk();
			increment_page_faults_elf();

			if (index_tlb != -1)
				status |= index_tlb<<2;
			page_table_add_entry(faultaddress, paddr, pid, status);

			if (result < 0){}
				//return -1;
		}
		else if (faultaddress >= stackbase && faultaddress < stacktop) {
			// Stack 
			if(as->allocated_pages >= MAX_ALLOCATED_PAGES || (paddr = getppages(1)) == 0) { 

                index_page_to_replace = page_table_replacement(pid, &empty_entry); // find index victim to replace
				index_tlb = page_table_get_Status_on_Index(index_page_to_replace);

                if(index_page_to_replace == -1)
                    return 0;

                swap_out(pid, empty_entry.vaddr, 
				empty_entry.permission_flag, 
				index_page_to_replace * PAGE_SIZE); 

                as->allocated_pages--; 
                paddr = index_page_to_replace*PAGE_SIZE;

            } 
			
            as->allocated_pages++; //increase number of allocated pages for that process
			increment_page_faults_zeroed();
			
            // clean the page just got by allocation (or previously swapped)
            as_zero_region(paddr, 1); 

			if (index_tlb != -1)
				status |= index_tlb<<2;
			page_table_add_entry(faultaddress, paddr, pid, status);

			if (result < 0){}
				//return -1;

		}
		else {
			return EFAULT;
		}
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);
	
	ehi = faultaddress | pid << 6;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	
	// Write a new entry inside the TLB
	add_entry(&index_tlb, ehi, elo);
	KASSERT(index_tlb != -1);
	page_table_set_status_at_index(paddr>>12, index_tlb<<2);
	return 0;
}
