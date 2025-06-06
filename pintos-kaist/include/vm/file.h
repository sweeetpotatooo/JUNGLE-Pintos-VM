#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

/* page의 자식 중 하나. e.g. anon_page */
struct file_page {
	struct file *file;
	off_t size;
	off_t file_ofs;
	size_t cnt;
};

/* file_backed_page의 vm_initializer 함수의 인자값. file_backed page의 첫 번째 페이지 폴트 발생 시 
* file의 offset으로부터 length 길이의 바이트만큼 페이지에 복사. */
struct lazy_aux_file_backed { 
	size_t length; 
	int writable;
	struct file *file;
	off_t offset;
	size_t cnt;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
bool lazy_load_file_backed(struct page *page, void *aux);

void do_munmap (void *va);
#endif
