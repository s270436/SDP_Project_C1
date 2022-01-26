#include <types.h>
#include <lib.h>
#include <vm_tlb.h>
#include <vm_stats.h>
#include <mips/tlb.h> // here is the definition of NUM_TLB
#include <spinlock.h>
#include <vm.h> //here is the definition of PAGE_FRAME to remove the offset from the address

unsigned char bitmap[NUM_TLB];

static struct spinlock slock = SPINLOCK_INITIALIZER;

void tlb_bootstrap(void) {
    int index;
    for(index=0;index<NUM_TLB;index++) {
        bitmap[index] = 0;
    }
}

static int tlb_get_rr_victim(void) { //select victim inside TLB when there is no space
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}

void write_entry(int index, uint32_t vaddr_no_offset_with_pid, uint32_t paddr_with_flags) {
    bitmap[index] = 1;
    tlb_write(vaddr_no_offset_with_pid, paddr_with_flags, index); //set to 0 the last 8 bits
}

// This function just add a new entry in tlb table if there's an empty slot, otherwise replace any entry without 
// taking into account the index
void add_entry(int *index_tlb, uint32_t vaddr_no_offset_with_pid, uint32_t paddr_with_flags) {
    int index;
    
    spinlock_acquire(&slock);
    if(*index_tlb == -1) {
        for(index=0; index<NUM_TLB; index++) {
            if(bitmap[index] == 0) {
                increment_tlb_faults_free();
                break;
            }
        }
        if(index == NUM_TLB) {
            increment_tlb_faults_replace();
            index = tlb_get_rr_victim();
        }
    } else
         increment_tlb_faults_free();   
    
    write_entry(index, vaddr_no_offset_with_pid, paddr_with_flags);
    increment_tlb_faults();

    *index_tlb = index;

    spinlock_release(&slock);
}

int read_entry(uint32_t vaddr, uint32_t *paddr) {

    spinlock_acquire(&slock);

    uint32_t vaddr_no_offset = vaddr & PAGE_FRAME;
    uint32_t offset = vaddr & ~PAGE_FRAME;
    uint32_t paddr_tmp;

    int index = tlb_probe(vaddr_no_offset, *paddr);     // tlb_probe looks for a match with vaddr and returns the index
                                                        // paddr must be set but not used by tlb_probe
    if(index < 0)
        return -1;
    
    tlb_read(NULL, &paddr_tmp, index);
    *paddr = paddr_tmp | offset;

    spinlock_release(&slock);
    return 0;
}

void reset_one_entry_by_index(int index) {
    spinlock_acquire(&slock);

    bitmap[index]=0;
    tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(),index);

    spinlock_release(&slock);
}

void reset_tlb(void) {
    int index;

    spinlock_acquire(&slock);

    for(index=0;index<NUM_TLB;index++) {
        bitmap[index]=0;
        tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(),index);
    }

    increment_tlb_invalidations();

    spinlock_release(&slock);
}

void reset_tlb_pid_different(pid_t current_pid) {
    uint32_t vaddr, paddr;
	bool every_entry_has_pid_different = true;
	pid_t tmp_pid_tlb;
    int index;

	spinlock_acquire(&slock);
    

	for (index=0; index<NUM_TLB; index++) {
		tlb_read(&vaddr, &paddr, index);
		tmp_pid_tlb = (vaddr & TLBHI_PID) >> 6;
		if (tmp_pid_tlb == current_pid)
            every_entry_has_pid_different = false;
		else
			reset_one_entry_by_index(index);
	}
	if (every_entry_has_pid_different)
		increment_tlb_invalidations();

	spinlock_release(&slock);
}