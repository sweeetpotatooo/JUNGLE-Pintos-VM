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

        if (page->frame != NULL && pml4_is_dirty(pml4, page->va))
        {
                /* Write back modified data to the file. */
                file_write_at(file_page->file, page->frame->kva,
                              file_page->size, file_page->file_ofs);
        }
	
	/* 
	* DEBUG: spt_remove_page를 여기서 호출하면 중복이다. 위의 주석을 참조.
	*/
	// spt_remove_page(spt, page); // spt 제거 -> spt에서 지우면 pml4에서 계속 업데이트가 된다?
	

}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
        dprintfg("[do_mmap] routine start\n");

        struct thread *curr = thread_current();
        struct file *re_file = file_reopen(file);
        if (re_file == NULL)
                return NULL;

        size_t read_bytes = length < file_length(file) ? length : file_length(file);
        size_t zero_bytes = pg_round_up(length) - read_bytes;

        struct mmap_file *mf = malloc(sizeof(struct mmap_file));
        if (mf == NULL)
        {
                file_close(re_file);
                return NULL;
        }

        mf->addr = addr;
        mf->length = pg_round_up(length);
        mf->file = re_file;
        mf->offset = offset;
        list_push_back(&curr->mmap_list, &mf->elem);

        void *upage = addr;
        off_t ofs = offset;

        while (read_bytes > 0 || zero_bytes > 0)
        {
                size_t page_read_bytes = read_bytes > PGSIZE ? PGSIZE : read_bytes;
                size_t page_zero_bytes = PGSIZE - page_read_bytes;

                struct lazy_aux_file_backed *aux = malloc(sizeof(struct lazy_aux_file_backed));
                if (aux == NULL)
                        goto fail;

                aux->file = re_file;
                aux->length = page_read_bytes;
                aux->offset = ofs;
                aux->cnt = 0;

                if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_file_backed, aux))
                {
                        free(aux);
                        goto fail;
                }

                read_bytes -= page_read_bytes;
                zero_bytes -= page_zero_bytes;
                ofs += page_read_bytes;
                upage += PGSIZE;
        }

        return addr;

fail:
        do_munmap(addr);
        return NULL;
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
	file_page->cnt = lazy_aux->cnt;

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
    dprintfg("[do_munmap] routine start. va: %p\n", addr);

    struct thread *curr = thread_current();
    struct supplemental_page_table *spt = &curr->spt;
    struct mmap_file *target = NULL;
    struct list_elem *e;

    for (e = list_begin(&curr->mmap_list); e != list_end(&curr->mmap_list); e = list_next(e))
    {
        struct mmap_file *mf = list_entry(e, struct mmap_file, elem);
        if (mf->addr == addr)
        {
            target = mf;
            break;
        }
    }

    if (target == NULL)
        return;

    void *upage = target->addr;
    size_t remaining = target->length;

    while (remaining > 0)
    {
        struct page *page = spt_find_page(spt, upage);
        if (page != NULL && page_get_type(page) == VM_FILE)
        {
            file_backed_destroy(page);
            spt_remove_page(spt, page);
        }
        upage += PGSIZE;
        remaining -= PGSIZE;
    }

    file_close(target->file);
    list_remove(&target->elem);
    free(target);

    dprintfg("[do_munmap] munmap complete!\n");
}
