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
    // HACK: 추가적인 멤버 추가 필요
    
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
