#include <types.h>
#include <lib.h>
#include <vm_stats.h>

static int tlb_faults = 0;
static int tlb_faults_free = 0;
static int tlb_faults_replace = 0;
static int tlb_invalidations = 0;
static int tlb_reloads = 0;
static int page_faults_zeroed = 0;
static int page_faults_disk = 0;
static int page_faults_elf = 0;
static int page_faults_swapin = 0;
static int page_faults_swapout = 0;
static int swapfile_writes = 0;

extern void init_stats(void) {
    tlb_faults = 0;
    tlb_faults_free = 0;
    tlb_faults_replace = 0;
    tlb_invalidations = 0;
    tlb_reloads = 0;
    page_faults_zeroed = 0;
    page_faults_disk = 0;
    page_faults_elf = 0;
    page_faults_swapin = 0;
    page_faults_swapout = 0;
    swapfile_writes = 0;
}

extern void increment_tlb_faults(void) {   //number of TLB misses occurred (not including faults that cause a program to crash). tlb_faults = tlb_faults_free + tlb_faults_replace = tlb_reloads + page_faults_disk + page_faults_zeroed;
    tlb_faults++;
    // kprintf("tlb_faults=%d\n", tlb_faults);
}

extern void increment_tlb_faults_free(void) {  //number of TLB misses for which there was free space in the TLB to add the new TLB entry (so no replacement)
    tlb_faults_free++;
    // kprintf("tlb_faults_free=%d\n", tlb_faults_free);
}

extern void increment_tlb_faults_replace(void) {   //number of TLB misses for which there was no free space for the new TLB entry (so replacement)
    tlb_faults_replace++;
    // kprintf("tlb_faults_replace=%d\n", tlb_faults_replace);
}

extern void increment_tlb_invalidations(void) {   //number of times the entire TLB was invalidated 
    tlb_invalidations++;
    // kprintf("tlb_invalidations=%d\n", tlb_invalidations);
}

extern void increment_tlb_reloads(void) {  //number of TLB misses for pages taht were already in memory
    tlb_reloads++;
    // kprintf("tlb_reloads=%d\n", tlb_reloads);
}

extern void increment_page_faults_zeroed(void) {
    page_faults_zeroed++;
    // kprintf("page_faults_zeroed=%d\n", page_faults_zeroed);
}

extern void increment_page_faults_disk(void) {
    page_faults_disk++;
    // kprintf("page_faults_disk=%d\n", page_faults_disk);
}

extern void increment_page_faults_elf(void) {
    page_faults_elf++;
    // kprintf("page_faults_elf=%d\n", page_faults_elf);
}

extern void increment_page_faults_swapin(void) {
    page_faults_swapin++;
    // kprintf("page_faults_swapin=%d\n", page_faults_swapin);
}

extern void increment_page_faults_swapout(void) {
    page_faults_swapout++;
    // kprintf("page_faults_swapout=%d\n", page_faults_swapout);
}

extern void increment_swapfile_writes(void) {
    swapfile_writes++;
    // kprintf("swapfile_writes=%d\n", swapfile_writes);
}

extern void print_all_statistics(void) {
    kprintf("STATISTICS:\ntlb_faults=%d, tlb_faults_free=%d, tlb_faults_replace=%d, tlb_invalidations=%d, tlb_reloads=%d, page_faults_zeroed=%d, page_faults_disk=%d, page_faults_elf=%d, page_faults_swapin=%d, page_faults_swapout=%d, swapfile_writes=%d\n", tlb_faults, tlb_faults_free, tlb_faults_replace, tlb_invalidations, tlb_reloads, page_faults_zeroed, page_faults_disk, page_faults_elf, page_faults_swapin, page_faults_swapout, swapfile_writes);
    if( (tlb_faults_free + tlb_faults_replace) != tlb_faults)
        kprintf("Warning: TLB FAULTS with Free + TLB Faults with Replace is NOT equal to TLB Faults\n");
    
    if( (tlb_reloads + page_faults_disk + page_faults_zeroed) != tlb_faults)
        kprintf("Warning: TLB reloads + PF Disk + PF Zeroed is NOT equal to TLB Faults\n");

    if ((page_faults_elf + page_faults_swapin + page_faults_swapout) != page_faults_disk)
        kprintf("Warning: PF from ELF + PF from Swapfile is NOT equal to PF Disk\n");
}