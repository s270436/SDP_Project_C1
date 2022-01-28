#include "swapfile.h"
#include <types.h>
#include <vm.h>
#include <lib.h>
#include <vfs.h>
#include <spinlock.h>
#include <vnode.h>
#include <stat.h>
#include <kern/fcntl.h>
#include <kern/iovec.h>
#include <uio.h>
#include <addrspace.h>
#include <pt.h>
#include <types.h>
#include <coremap.h>
#include "pt.h"
#include <vm_stats.h>

#define FILESIZE 9437184 // 9 * 1024 * 1024 (9 MB)
#define NUMBERENTRIES FILESIZE/PAGE_SIZE // (9 * 1024 * 1024) / PAGE_SIZE = 2304
#define FILENAME "emu0:SWAPFILE" //emu0 is the default secondary memory

typedef struct swap_track {
    permission_t permission_flag;
    pid_t pid;
    vaddr_t vaddr;
    unsigned char valid; //0 invalid, 1 valid
} swap_track;

swap_track track[NUMBERENTRIES]; //track as static array since we already know the size of swapfile and page size. No need to allocate it as dynamic

struct vnode *swap_vnode;
static struct spinlock slock = SPINLOCK_INITIALIZER; //Init spinlock like this in every other file

void swap_bootstrap(void) {
    int i;
    int err;
    struct stat info_file; //used to get information in the file (size, ...)
    char path[sizeof(FILENAME)];

    //parameters to create swapfile if it does not exist
    struct iovec iov;
    struct uio myuio;
    int kb = 1024;
    int kb_in_FILESIZE = FILESIZE/kb;
    char buf[kb_in_FILESIZE];

	strcpy(path, FILENAME);

    if((err = vfs_open(path, O_CREAT | O_RDWR, 0, &swap_vnode))) { //open file 
        panic("[ERR] swapfile.c: error %d opening swapfile %s\n", err, FILENAME);
    }

    VOP_STAT(swap_vnode, &info_file); //get information of the file and store it into the stat variable
    if(info_file.st_size < FILESIZE) {

        for(i=0; i<kb_in_FILESIZE + 1; i++) {
            uio_kinit(&iov, &myuio, (void *)buf, 1, kb*i, UIO_WRITE);
            if ((err = VOP_WRITE(swap_vnode, &myuio))) 
                panic("[ERR] swapfile.c: write error %d\n",err);
        }

        VOP_STAT(swap_vnode, &info_file); //get information of the file and store it into the stat variable
        if(info_file.st_size < FILESIZE) {
            panic("[ERR] swapfile.c: swapfile size %lu, it must be at least %lu. Increase size!\n", (unsigned long) info_file.st_size, (unsigned long) FILESIZE);
        }
    }

    for(i=0; i<NUMBERENTRIES; i++) {
        track[i].permission_flag = 0;
        track[i].pid = -1;
        track[i].vaddr = 0;
        track[i].valid = 0;
    }
}

void swap_out(pid_t pid, vaddr_t vaddr, permission_t permission_flag, paddr_t paddr) { //load frame from ram into swapfile
    int i;
    int err;
    struct iovec iov;
    struct uio myuio;

    if (vaddr>=MIPS_KSEG0) // check I am in MIPS_KUSEG area
        panic("[ERR] swapfile.c: vaddr cannot be greater than MIPS_KSEG0\n");

    spinlock_acquire(&slock);
    for(i=0; i<NUMBERENTRIES; i++) {
        if(track[i].valid == 0)
            break;
    }

    if(i == NUMBERENTRIES)
        panic("[ERR] swapfile.c: out of swap space\n");

    uio_kinit(&iov, &myuio, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, i*PAGE_SIZE, UIO_WRITE);
    if ((err = VOP_WRITE(swap_vnode, &myuio))) 
        panic("[ERR] swapfile.c: write error %d\n",err);

    //Set the entries
    track[i].valid = 1;
    track[i].pid = pid;
    track[i].permission_flag = permission_flag;
    track[i].vaddr = vaddr;
    
    spinlock_release(&slock);

    page_table_reset_entry(paddr/PAGE_SIZE); //invalid pagetable entry

    increment_page_faults_swapout();
}

int swap_in(struct addrspace *as, pid_t pid, vaddr_t vaddr, paddr_t *paddr) { //load from swapfile to ram
    int i;
    int err;
    int index_page_to_replace;
    struct iovec iov;
    struct uio myuio;
    entry_t empty_entry;

    spinlock_acquire(&slock);
    for(i=0; i<NUMBERENTRIES; i++) {
        if(track[i].pid == pid && track[i].vaddr == vaddr && track[i].valid == 1) {

            if(as->allocated_pages == MAX_ALLOCATED_PAGES || (*paddr = getppages(1)) == 0) { //if I reached the max number of allocated pages or there is no free frame available, then I need to swap out a page before swap in.

                index_page_to_replace = page_table_replacement(pid, &empty_entry); //find index victim to replace

                if(index_page_to_replace == -1)
                    return 0;

                swap_out(pid, empty_entry.vaddr, empty_entry.permission_flag, index_page_to_replace * PAGE_SIZE);
                as->allocated_pages--; 
                *paddr = index_page_to_replace*PAGE_SIZE;

            }

            as->allocated_pages++; //increase number of allocated pages for that process

            // clean the page just got by allocation (or previously swapped)
            as_zero_region(*paddr, 1);
            increment_page_faults_zeroed();

            // perform the I/O
            uio_kinit(&iov, &myuio, (void *)PADDR_TO_KVADDR(*paddr), PAGE_SIZE, i*PAGE_SIZE, UIO_READ);
            if ((err = VOP_READ(swap_vnode, &myuio))) 
                panic("[ERR] swapfile.c: read error %d\n",err);

            if (myuio.uio_resid!=0) // uio_resid is the amount of data left to transfer. If there is more, then error
		        panic("[ERR] swapfile.c: uio_resid != 0\n");

            track[i].pid = -1;
            track[i].valid = 0;
            
            // add the recently swapped-in page in the IPT
            page_table_add_entry(vaddr, *paddr, pid, track[*paddr/PAGE_SIZE].permission_flag);
            increment_page_faults_swapin();

            spinlock_release(&slock);

            return 1;
        }
    }
    spinlock_release(&slock);

    if(i == NUMBERENTRIES) // I did't find the page to swap in into the ram
        return 0;

    return 1;
}

void swap_destroy(void)
{
	spinlock_cleanup(&slock);
	vfs_close(swap_vnode);
}
