/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);
boollazy_load_file_backed(struct page *page, void *aux);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};



/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	PANIC("filebacked swap in"); // 분명히 호출히 호출돼야 하잖아.
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	// bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux)
	struct lazy_aux_file_backed *aux;
	
	while(length > 0)
	{
		aux = malloc(sizeof(struct lazy_aux_file_backed));
		aux->file = file_reopen(file);
		aux->writable = writable;
		
		if(length > PGSIZE) aux->length = PGSIZE;
		else aux->length = length; 

		aux->offset = offset;
		
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file_backed, aux)){
			free(aux);
			return NULL;
		}
		// 쓴 만큼 offset, length 업데이트.
		offset += aux->length;
		length -= aux->length;
		//addr = (void *)((char *) addr +  PGSIZE);
	}
	
	// 1. addr로부터 페이지 생성
	// 1-1. lazy_load, aux 초기화해서 넘겨주기.
	// 1-2. 복사(length, offset, 등등) 이거 바로 해줘요? 그럼 또 lazy 아니잖아. -> 이 내용이 lazy_load에서 타입 체크후에 복사 바로 하면 되지 않겠나.
	// 1-3. 나머자 내용은 0으로 채워야 함.

	return addr;
}

bool
lazy_load_file_backed(struct page *page, void *aux)
{
	/* 파일에서 페이지 컨텐츠를 읽어옵니다. */
	/* 이 함수는 주소 VA에서 첫 페이지 폴트가 발생했을 때 호출됩니다. */
	/* 이 함수를 호출할 때 VA를 사용할 수 있습니다. */
	dprintfd("[lazy_load_file_backed] routine start. page: %p, page->va: %p\n", page, page->va);
	void *va = page->va; 
	memset(page->frame->kva, 0, PGSIZE); // zero bytes 복사.
	
	/* Load this page. */
	struct lazy_aux_file_backed *lazy_aux = (struct lazy_aux_file_backed *)aux; 
	// aux 멤버 정의 필요.
	
	dprintfd("[lazy_load_file_backed] reading file\n"); 
	if (file_read_at(lazy_aux->file, page->frame->kva, lazy_aux->length, lazy_aux->offset) != (int)lazy_aux->length)
	{
		free(lazy_aux); 
		return false;
	}
	return true;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	
}
