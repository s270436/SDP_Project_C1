#include <types.h>
#include <spinlock.h>
#include <lib.h>
#include <pt.h>
#include <vm.h>
#include <coremap.h>

static table_t * page_table;

void lru_update_cnt(void){
    unsigned int i;

    if (page_table!=NULL){
        spinlock_acquire(&page_table->table_lock);
        for (i=0; i<page_table->length; i++){
            if (page_table->next_entry[i].pid!=-1)
                page_table->next_entry[i].position_fifo+=1;
        }
        spinlock_release(&page_table->table_lock);
    }
}

void page_table_init(void) {

    unsigned int length = ram_getsize()/PAGE_SIZE;
    unsigned int i = 0;

    page_table = kmalloc(sizeof(table_t));
    if(page_table == NULL) 
        panic("[ERR] pt.c: error to allocate page table\n");

    page_table->next_entry = kmalloc(length * sizeof(entry_t));
    if(page_table->next_entry == NULL)
        panic("[ERR] pt.c: error to allocate next entry in page table\n");

    page_table->length = (unsigned int)length;
    
    spinlock_init(&page_table->table_lock);

    spinlock_acquire(&page_table->table_lock);
    for(i=0; i<length; i++){
        page_table->next_entry[i].pid = -1;
        page_table->next_entry[i].vaddr = 0;
        page_table->next_entry[i].status = 0;
        page_table->next_entry[i].position_fifo = 0;
    }
    spinlock_release(&page_table->table_lock);
}

void page_table_add_entry(pid_t pid, vaddr_t vaddr, paddr_t paddr, uint32_t status) { 
    
    unsigned int i;
    int last_position_fifo = -1;
    unsigned int frame_index = (int) paddr >> 12;
    
    KASSERT(frame_index < page_table->length);
    
    spinlock_acquire(&page_table->table_lock);
    for(i=0; i<page_table->length; i++){
        if(pid == page_table->next_entry[i].pid && page_table->next_entry[i].position_fifo > last_position_fifo)
            last_position_fifo = page_table->next_entry[i].position_fifo;
    }
    page_table->next_entry[frame_index].pid = pid;
    page_table->next_entry[frame_index].vaddr = vaddr;
    page_table->next_entry[frame_index].status = status;
    page_table->next_entry[frame_index].position_fifo = last_position_fifo + 1;
    spinlock_release(&page_table->table_lock);
}

int page_table_get_paddr_entry(pid_t pid, vaddr_t vaddr, paddr_t* paddr, uint32_t* status) { 
    unsigned int i = 0, last = page_table->length;
    int result;
    entry_t e;

    spinlock_acquire(&page_table->table_lock);

    for(i=0; i<page_table->length; i++)
    {
        e = page_table->next_entry[i];
        if(e.pid == pid && e.vaddr == vaddr)
        {
            last = i;
        }
    }
    spinlock_release(&page_table->table_lock);

    if(last == page_table->length) {
        result = 0;
    }
    else {
        *paddr = last * PAGE_SIZE;
        *status = page_table->next_entry[last].status;
        result = 1;
    }
    return result;
}

int page_table_replacement(pid_t pid, entry_t *entry){ 
    //local page table replacement. I choose the oldest page for a process with pid = pid
    unsigned int i;
    int index_replacement = -1;

    for(i=0; i<page_table->length; i++) {
        if(page_table->next_entry[i].position_fifo == 0 && page_table->next_entry[i].pid == pid) {
            entry->pid = page_table->next_entry[i].pid;
            entry->permission_flag = page_table->next_entry[i].permission_flag;
            entry->vaddr = page_table->next_entry[i].vaddr;
            entry->status = page_table->next_entry[i].status;
            entry->position_fifo = page_table->next_entry[i].position_fifo;
            index_replacement = i;
            break;
        }
    }

    return index_replacement;   // if -1 is returned, then no index has been found
}

void page_table_reset_entry(int index) {
    unsigned int i;
    pid_t pid = page_table->next_entry[index].pid;
    int position_fifo = page_table->next_entry[index].position_fifo;

    spinlock_acquire(&page_table->table_lock);
    page_table->next_entry[index].pid = -1;
    page_table->next_entry[index].vaddr = 0;
    page_table->next_entry[index].status = 0;
    page_table->next_entry[index].position_fifo = 0;

    for(i=0; i<page_table->length; i++){
        if(pid == page_table->next_entry[i].pid && page_table->next_entry[i].position_fifo > position_fifo)
            page_table->next_entry[i].position_fifo--;
    }
    spinlock_release(&page_table->table_lock);
}

void page_table_remove_on_pids(pid_t pid){
    unsigned int i;

    if (pid < 0){
        panic("error on pid: it is invalid\n");
    }

    // do it in mutual exclusion
    spinlock_acquire(&page_table->table_lock);
    for(i = 0; i < page_table->length; i++){
        if(page_table->next_entry[i].pid == pid){
            page_table_reset_entry(i);
            freeppages(i * PAGE_SIZE);
        }
    }
    spinlock_release(&page_table->table_lock);
}

void page_table_destroy(void) {
    unsigned int i = 0;
    unsigned int len = page_table->length;
    spinlock_acquire(&page_table->table_lock);    
    for(i=0; i < len; i++) {
        kfree((void*)&page_table->next_entry[i]);
    }
    spinlock_release(&page_table->table_lock);
    kfree(page_table);
}

unsigned char page_table_get_Status_on_Index(int index){
    return page_table->next_entry[index].status;
}

void page_table_set_status_at_index(int index, unsigned char val){
    page_table->next_entry[index].status |= val;
}