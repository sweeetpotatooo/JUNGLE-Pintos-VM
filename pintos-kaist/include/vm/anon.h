#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
// struct page;
// enum vm_type;

/* There is a struct to describe an anonymous page - 
* anon_page in include/vm/anon.h. 
* It is currently empty, but you may add members 
* to store necessary information or state of an anonymous page as you implement.  */
struct anon_page {
    struct page *page; 
    /* 스왑 디스크에서의 슬롯 위치. */
    size_t swap_slot;
    
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
