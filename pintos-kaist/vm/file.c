/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <string.h>

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);
bool lazy_load_file_backed(struct page *page, void *aux);
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
	// - 파일 기반 페이지 서브시스템 초기화
	// - 필요한 자료구조 초기화 등을 여기에 구현
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	// - 파일 기반 페이지 초기화 함수
	// - `page->operations`에 destroy, swap_in 등의 함수 포인터를 설정
	// - 해당 페이지가 참조할 파일 등의 정보도 설정 필요

	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	// file_page 멤버 초기화.
	file_page->file = NULL;
	file_page->file_ofs  = 0;
        file_page->size = 0;
        return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
        struct file_page *file_page = &page->file;
        if (file_read_at(file_page->file, kva, file_page->size, file_page->file_ofs) != (int)file_page->size)
                return false;
        return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
        struct file_page *file_page UNUSED = &page->file;
        return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	// 	- 파일 기반 페이지를 제거하는 함수
	// - 페이지가 **dirty 상태**면, 변경 사항을 파일에 기록(write-back)해야 함
	// - 여기서 `page` 구조체 자체를 `free`할 필요는 없음 → 호출자가 해제함
	//    - 호출자 = spt_remove_page → vm_dealloc_page
	//   	- 여기서 destroy 호출 후 구조체 free까지 해줌
	// - destroy에서 구현할 로직?
	//    - 매핑된 프레임 해제?
	//    - spt_remove_page에서 구현하는것이 좋을듯하다
	//   - write-back 구현
	struct file_page *file_page = &page->file; 
	struct pml4 *pml4 = thread_current()->pml4;
	struct supplemental_page_table *spt = &thread_current()->spt;
	if (pml4_is_dirty(pml4, page->va))
	{
		// write back
		// file_write_at (struct file *file, const void *buffer, off_t size, off_t file_ofs) 
		file_write_at(file_page->file, page->va, file_page->size, file_page->file_ofs); // Writes SIZE bytes만큼 쓴다.
	}
	file_close(file_page->file);
	spt_remove_page(spt, page); // spt 제거 -> spt에서 지우면 pml4에서 계속 업데이트가 된다?
	

}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{

	dprintff("[do_mmap] routine start\n");
	struct lazy_aux_file_backed *aux; // 자세한 설명은 구조체 참조
	
	while (length > 0) // length 값이 남아 있는 동안
	{
		dprintff("[do_mmap] loop\n");
		aux = malloc(sizeof(struct lazy_aux_file_backed)); // aux 값을 저장할 버퍼를 커널 메모리 풀에 할당
		aux->file = file_reopen(file); // 각 페이지가 하나의 파일을 참조해야 하므로 참조 카운트를 유지하기 위해 file_reopen 사용
		aux->writable = writable; 
		
		if (length > PGSIZE) // 만약 파일로부터 복사해야 하는 바이트 수가 PGSIZE 이상이라면 
		aux->length = PGSIZE; // PGSIZE와 동일한 값으로 설정.
		else // 그 이하라면
		aux->length = length; // 주어진 length만큼 복사.
		
		aux->offset = offset;
		
		dprintff("[do_mmap] allocating page with initializer\n");
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file_backed, aux)) // 페이지 할당.
		{
			free(aux);
			return NULL;
		}
		// 쓴 만큼 offset, length 업데이트.
		offset += aux->length;
		length -= aux->length;
		addr -= PGSIZE;
	}

	// 이대로 리턴하면 가장 아래의 addr 값을 리턴하지 않나? 여러 페이지가 할당됐을 텐데.
	return addr;
}

bool lazy_load_file_backed(struct page *page, void *aux)
{
	/* 파일에서 페이지 컨텐츠를 읽어옵니다. */
	/* 이 함수는 주소 VA에서 첫 페이지 폴트가 발생했을 때 호출됩니다. */
	/* 이 함수를 호출할 때 VA를 사용할 수 있습니다. */
	dprintff("[lazy_load_file_backed] routine start. page: %p, page->va: %p\n", page, page->va);
	void *va = page->va;
	memset(page->frame->kva, 0, PGSIZE); // zero bytes 복사.

	/* Load this page. */
	struct lazy_aux_file_backed *lazy_aux = (struct lazy_aux_file_backed *)aux;
	// aux 멤버 정의 필요.
	// file page 업데이트
	struct file_page *file_page = &page->file; //file_backed에 page 정보를 저장한다
	file_page->file = lazy_aux->file; // file 정보 저장
	file_page->file_ofs = lazy_aux->offset; // 
	file_page->size = lazy_aux->length;
	
	dprintff("[lazy_load_file_backed] reading file\n");
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
	// 프로세스가 종료되면 매핑 자동해제. munmap할 필요는 없음.
	// 매핑 해제 시 수정된 페이지는 파일에 반영
	// 수정되지 않은 페이지는 반영할 필요 없음
	// munmap 하고 spt제거?
	// 파일 close, remove는 매핑에 반영되지 않음( 프레임은 가마니)
	// 한 파일을 여러번 mmap하는 경우에는 file_reopen을 통해 독립된 참조. -> 하나의 file이 여러번 mmap 되어 있는 걸 어떻게 알지?

	struct supplemental_page_table *spt = &thread_current()-> spt; // 현재 스레드의 spt 정보 참조
	struct page *page = spt_find_page(spt, addr); // spt정보를 가져온다.
	file_backed_destroy(page);
}
