/* vm.c: 가상 메모리 객체에 대한 공통 인터페이스 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h>
#include "threads/vaddr.h"
// project 3
#include "threads/mmu.h"
#include "vm/uninit.h"
#include "lib/kernel/hash.h"
#include "userprog/process.h"
#include <string.h>
#include "filesys/file.h"

struct lazy_load_args
{
	struct file *file;
	off_t ofs;
	size_t page_read_bytes;
	size_t zero_bytes;
};

/* 가상 메모리 서브시스템을 초기화합니다.
 * 각 서브시스템의 초기화 코드를 호출합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* Project 4용 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* 이 위의 코드는 수정하지 마세요. */
	/* TODO: 여기에 여러분의 코드를 작성하세요. */
}

/* 페이지의 타입을 가져옵니다.
 * 이 함수는 페이지가 초기화된 후 어떤 타입인지 확인할 때 유용합니다.
 * 이 함수는 이미 완전히 구현되어 있습니다. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* 헬퍼 함수들 */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

// 이 함수는 어디서 뭘 하는 함수인가요? 가상공간 어딘가에 페이지를 만들어 내는 함수.
// 주어진 타입으로 uninit page를 생성합니다.
// 초기화되지 않은 페이지의 swap_in 핸들러는 페이지를 자동으로 주어진 타입에 맞게 초기화하고, 주어진 AUX와 함께 INIT을 호출합니다.
// 페이지 구조체를 얻은 후, 해당 페이지를 프로세스의 supplemental page table에 삽입합니다.
// vm.h에서 정의된 VM_TYPE 매크로를 사용하는 것이 유용할 수 있습니다.
/* Implement vm_alloc_page_with_initializer().
 * You should fetch an appropriate initializer according
 * to the passed vm_type and call uninit_new with it. */
/* 초기화 함수와 함께 대기 중인 페이지 객체를 생성합니다.
 * 페이지를 생성하고 싶다면 직접 만들지 말고 반드시 `vm_alloc_page_with_initializer`나 `vm_alloc_page`를 사용해야 합니다. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT) // DEBUG: 여기서 걸림. 주어진 타입으로 uninit_page를 바꿔줘야 되는 함수. uninit으로 uninit을 만들라는 이상한 명령인지 검사.
	// removed debug;

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* 해당 upage가 이미 존재하는지 확인합니다. */
	upage = pg_round_down(upage);

	if (spt_find_page(spt, upage) == NULL) // 페이지가 존재하지 않는다면
	{
		// debug removed;
		/*
		 * NOTE:  페이지 메타데이터는 프로세스 생명주기와 독립적으로 관리되어야 함. 따라서 함수의 생명 주기에 종속된 스택 변수 대신
		 * 별도의 메모리 공간에 메타 데이터를 저장해야 함. 이렇게 활용하기 위해 메모리에 커널 풀 영역이 있음.
		 */
		struct page *page = malloc(sizeof(struct page)); // page 메타 데이터 저장을 위한 메모리 할당.
		if (page == NULL)
		{
			return false;
		}

		// debug removed;

		/* 페이지를 생성. uninit_new에 page 주소와 va(upage) 값을 넘겨주면 알아서 초기화해준다.
		 * VM 타입에 맞는 초기화 함수를 가져와서,
		 * uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
		 * TODO: 생성 이후 필요한 필드를 수정하세요.
		 */

		bool (*initializer)(struct page *, enum vm_type, void *);

		switch (VM_TYPE(type))
		{
		case (VM_ANON):
			// debug removed;
			initializer = anon_initializer;
			break;
		case (VM_FILE):
			initializer = file_backed_initializer;
			// debug removed;
			break;
		};

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		// debug removed;

		/* 생성한 페이지를 spt에 삽입하세요. */
		return spt_insert_page(spt, page);
	}

err:
	return false;
}

/* spt에서 VA를 찾아서 페이지를 반환합니다. 실패 시 NULL을 반환합니다. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
	// debug removed;
	struct page *page = NULL;

	/*
	 * DEBUG: palloc으로 페이지를 따로 할당하고 free하는 식으로 검색용 페이지를 썼으나 탐색 실패. 왜 실패했는지는 모르겠음.
	 * 스택 변수로 바꾼 후 탐색 성공.
	 */

	struct page find_page;
	memset(&find_page, 0, sizeof(find_page)); // 전체 초기화

	struct hash_elem *e;

	find_page.va = pg_round_down(va);

	e = hash_find(&spt->hash, &find_page.hash_elem); // hash_find는 hash 구조체와 hash_elem을 인수로 받음.

	if (e != NULL)
	{
		// debug removed;
		return hash_entry(e, struct page, hash_elem);
	}
	else
	{
		// debug removed;
		return NULL;
	}
}

/* 유효성 검사를 통해 PAGE를 spt에 삽입합니다. */
bool spt_insert_page(struct supplemental_page_table *spt,
					 struct page *page)
{
	int succ = false;
	// debug removed;
	if (spt == NULL || page == NULL)
	{
		// debug removed;
		return false;
	}

	if (hash_insert(&spt->hash, &page->hash_elem) == NULL) // null이면 삽입 성공
	{
		// debug removed;
		succ = true;
	}
	else
	{
		// debug removed;
		succ = false;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	hash_delete(&spt, &page->hash_elem);
	vm_dealloc_page(page);

	return true;
}

/* 교체될 프레임(struct frame)을 선택하여 가져옵니다. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: 페이지 교체 정책은 여러분이 결정할 수 있습니다. */

	return victim;
}

/* 하나의 페이지를 교체하고 해당 프레임을 반환합니다.
 * 실패 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: victim을 스왑 아웃하고 교체된 프레임을 반환하세요. */

	return NULL;
}

/* Gets a new physical page from the user pool by calling palloc_get_page.
 * When successfully got a page from the user pool, also allocates a frame, initialize its members, and returns it.
 * After you implement vm_get_frame, you have to allocate all user space pages (PALLOC_USER) through this function.
 * You don't need to handle swap out for now in case of page allocation failure.
 * Just mark those case with PANIC ("todo") for now. */

/* palloc()으로 프레임을 할당받습니다.
 * 사용 가능한 페이지가 없다면 페이지를 교체하여 빈 공간을 만듭니다.
 * 항상 유효한 주소를 반환해야 합니다. 즉, 유저 풀 메모리가 가득 차더라도
 * 이 함수는 페이지를 교체해서라도 공간을 확보해야 합니다. */
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = malloc(sizeof(struct frame));

	if (frame == NULL)
	{
		PANIC("TODO");
	}

	// NOTE: PAL_USER인 이유는 주석에 user space pages를 본 함수로 할당받아야 한다고 명시되어 있어서 이렇게 함. 악성 프로그램이 고의로 커널풀 메모리 고갈시키는 거 막기 위한 분리.
	frame->page = NULL;
	ASSERT(frame->page == NULL);

	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL)
	{
		free(frame);			  // frame 메타 데이터 자료구조 해제
		frame = vm_evict_frame(); // TODO: evict frame 함수가 아직 구현되지 않음.
	}
	ASSERT(frame->kva != NULL);
	return frame;
}

/* 스택 확장 */
// - 주어진 주소 `addr`까지 스택 크기를 확장합니다.
// - 하나 이상의 **익명 페이지(anonymous page)**를 할당해 폴트가 더 이상 발생하지 않도록 합니다. // HACK: "하나 이상?" -> 단일 page 확장
// - 이때 **`addr`은 반드시 페이지 단위(PGSIZE)로 내림(round down)** 처리 후 할당해야 합니다.
// - 대부분의 운영체제는 **스택의 최대 크기를 제한**합니다.
// - 예: 유닉스 계열의 `ulimit` 명령으로 조정 가능
// - GNU/Linux 시스템의 기본값: 약 **8MB**
// - 본 프로젝트에서는 스택 최대 크기를 **1MB**로 제한해야 합니다. // HACK: 제한 안 걸어 놨음.
static void
vm_stack_growth(void *addr)
{
        /* Allocate a new anonymous stack page at the given address. */
        void *addr_aligned = pg_round_down(addr);
        /* Create an uninitialised page marked as stack. */
        if (vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_STACK,
                                           addr_aligned, true, NULL, NULL))
        {
                /* Immediately claim the page so that it becomes usable. */
                vm_claim_page(addr_aligned);
        }
}

/* 쓰기 보호된 페이지에 대한 예외를 처리합니다. */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}
/*
Handling page fault
The most important user of the supplemental page table is
the page fault handler. In project 2, a page fault always indicated a bug in the kernel
or a user program. In project 3, this is no longer true.
Now, a page fault might only indicate that the page must be brought in
from a file or swap slot. You will have to implement a more sophisticated page fault handler
 to handle these cases. The page fault handler, which is page_fault() in userprog/exception.c,
 calls your page fault handler, vm_try_handle_fault() in vm/vm.c.
 Your page fault handler needs to do roughly the following:

1. Locate the page that faulted in the supplemental page table.
If the memory reference is valid, use the supplemental page table entry to locate the data that goes in the page,
which might be in the file system, or in a swap slot, or it might simply be an all-zero page.
If you implement sharing (i.e., Copy-on-Write),
the page's data might even already be in a page frame, but not in the page table.
If the supplemental page table indicates that the user process should not expect any data at the address it was trying to access,
or if the page lies within kernel virtual memory, or if the access is an attempt to write to a read-only page, then the access is invalid.
Any invalid access terminates the process and thereby frees all of its resources.

2. Obtain a frame to store the page. If you implement sharing, the data you need may already be in a frame,
in which case you must be able to locate that frame.

3. Fetch the data into the frame, by reading it from the file system or swap, zeroing it, etc.
 If you implement sharing, the page you need may already be in a frame,
in which case no action is necessary in this step.

4. Point the page table entry for the faulting virtual address to the physical page.
You can use the functions in threads/mmu.c.

 */

// 페이지 폴트 처리

// 보충 페이지 테이블의 가장 중요한 사용자는 페이지 폴트 핸들러입니다. 프로젝트 2에서는 페이지 폴트가 항상 커널 또는 사용자 프로그램에서 버그를 의미했습니다.
// 그러나 프로젝트 3에서는 이제 그렇지 않습니다. 이제 페이지 폴트는 단순히 페이지를 파일이나 스왑 슬롯에서 가져와야 함을 나타냅니다. 이 경우를 처리하기 위해 더 정교한 페이지 폴트 핸들러를 구현해야 합니다.
// 페이지 폴트 핸들러인 page_fault() (userprog/exception.c)는 vm_try_handle_fault()라는 함수(이 함수는 vm/vm.c에 있음)를 호출하여 이를 처리합니다. 이 페이지 폴트 핸들러는 대체로 다음과 같이 해야 합니다:

// 	1.	보충 페이지 테이블에서 폴트가 발생한 페이지 찾기
// 메모리 참조가 유효한 경우, 보충 페이지 테이블 항목을 사용하여 페이지에 들어갈 데이터를 찾습니다. 데이터는 파일 시스템, 스왑 슬롯에 있을 수 있으며, 아예 0으로 채워진 페이지일 수도 있습니다.
// 공유(예: Copy-on-Write)를 구현하면 페이지의 데이터가 이미 페이지 프레임에 있을 수도 있지만, 페이지 테이블에는 없을 수 있습니다. 보충 페이지 테이블이 사용자 프로세스가 해당 주소에서 데이터를 기대해서는 안 된다고 나타내거나,
// 페이지가 커널 가상 메모리 범위 내에 있거나, 읽기 전용 페이지에 쓰기를 시도하는 경우, 이 접근은 유효하지 않습니다. 모든 유효하지 않은 접근은 프로세스를 종료시키고 그 자원을 모두 해제합니다.

// 	2.	페이지를 저장할 프레임 확보하기
// 공유를 구현한 경우, 필요한 데이터가 이미 프레임에 있을 수 있으므로 그 프레임을 찾을 수 있어야 합니다.

// 	3.	프레임에 데이터를 가져오기
// 데이터를 파일 시스템 또는 스왑에서 읽어오거나, 데이터를 0으로 채우는 등의 방법으로 프레임에 데이터를 가져옵니다. 공유를 구현한 경우,
// 필요한 페이지가 이미 프레임에 있을 수도 있으므로 이 단계에서는 아무 작업도 필요하지 않을 수 있습니다.

// 	4.	폴팅된 가상 주소에 대해 페이지 테이블 항목을 물리적 페이지로 설정하기
// threads/mmu.c에 있는 함수들을 사용하여 이를 설정합니다.

/* 성공 시 true를 반환합니다. */

// - 페이지 폴트가 발생하면 `userprog/exception.c`의 `page_fault()`에서 `vm_try_handle_fault` 함수를 호출합니다.
// - 이 함수에서 **해당 페이지 폴트가 스택 확장으로 처리 가능한지**를 판단해야 합니다.
// - 판단 기준을 만족하면 `vm_stack_growth()`를 호출해 해당 주소까지 스택을 확장합니다.
bool vm_try_handle_fault(struct intr_frame *f, void *addr,
                                                 bool user, bool write, bool not_present)
{
        struct supplemental_page_table *spt = &thread_current()->spt;
        void *rsp = is_kernel_vaddr(f->rsp) ? thread_current()->rsp : f->rsp;

        if (not_present)
        {
                if (addr >= rsp - STACK_HEURISTIC && addr >= STACK_MAX && addr < USER_STACK)
                        vm_stack_growth(addr);
        }

        struct page *page = spt_find_page(spt, addr);

        if (page == NULL)
                return false;

        if (write && !page->writable)
                return false;

        return vm_do_claim_page(page);
}



/* 페이지를 해제합니다.
 * 이 함수는 수정하지 마세요. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* VA에 할당된 페이지를 할당(claim)합니다. */
// - 주소 `va`에 대해 `struct page`를 찾아오고,
// - `vm_do_claim_page()`를 호출합니다.
// claim이란?
// 1. va가 주어짐(va는 결국 페이지 주소에 딱 맞지 않게 주어질 수도 있는 것임)
// 2. va에 해당하는 페이지(가상 연속 공간)를 찾아냄
// 3. 그 페이지에 프레임(물리 연속 공간)을 바로 연결지어줌.
bool vm_claim_page(void *va)
{
	ASSERT(va != NULL);
	// debug removed;

	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va); // va를 가지고 현재 쓰레드의 spt 에서 페이지를 찾아냄.
	if (page == NULL)
	{
		// spt에 없으면 false 리턴.
		// debug removed;
		return false;
	}
// debug removed;
	return vm_do_claim_page(page); // 있으면 바로 프레임 할당해서 돌려줌.
}

/* 주어진 PAGE를 할당하고 MMU 설정을 합니다. */
// - 주어진 페이지를 물리 프레임에 연결합니다.
// - 내부적으로 `vm_get_frame()`을 호출해 프레임을 할당한 후,
// 페이지 테이블에 가상 주소 → 물리 주소 매핑을 추가해야 합니다.
// - 성공 여부를 반환해야 합니다.
static bool
vm_do_claim_page(struct page *page)
{
	if (page == NULL)
	{
		return false;
	}
	// debug removed;

	struct frame *frame = vm_get_frame(); // 메모리 공간에서 프레임 하나 확보
	ASSERT(frame != NULL);

	/* 링크 설정 */
	frame->page = page; // 각각을 의미하는 구조체를 서로 링크시켜줌.
	page->frame = frame;

	/* 페이지의 VA와 프레임의 PA를 매핑하기 위해 페이지 테이블 엔트리를 삽입하세요. */
	if (pml4_get_page(thread_current()->pml4, page->va) == NULL) // 기존에 매핑된 페이지에 새로운 물리 프레임의 유출 방지
	{
		if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
		{
			// debug removed;
			return false;
		}
	}

	// debug removed;
	return swap_in(page, frame->kva);
}

uint64_t page_hash(const struct hash_elem *e, void *aux)
{
	struct page *p = hash_entry(e, struct page, hash_elem); // hash_elem으로부터 페이지 구조체를 얻습니다.
	return hash_bytes(&p->va, sizeof(p->va));				// p->va로 해쉬 값을 생성한다.
}

/* 두 page의 va를 기준으로 정렬을 비교한다. */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	/* 이 함수는 해시 테이블 충돌 시 내부 정렬에 사용된다고 한다. */
	struct page *page_a = hash_entry(a, struct page, hash_elem);
	struct page *page_b = hash_entry(b, struct page, hash_elem);
	return page_a->va < page_b->va;
}

/* 새로운 supplemental page table을 초기화합니다. */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->hash, page_hash, page_less, NULL);
}

// Helper function to destroy a page during cleanup
static void
spt_destroy_page_in_copy_failure(struct hash_elem *e, void *aux UNUSED)
{
	struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}

/* src에서 dst로 supplemental page table을 복사합니다. */
// - `src`의 supplemental page table을 `dst`에 복사
// - fork 시 부모의 실행 컨텍스트를 자식에게 복사할 때 사용
// - 각 페이지를 순회하며 `uninit_page`로 생성하고 **즉시 claim 처리**해야 함
// TODO: 구현 하다 말았음.
// bool supplemental_page_table_copy(struct supplemental_page_table *dst,
// 								  struct supplemental_page_table *src)
// {
// 	ASSERT(src != NULL); // 포인터 유효성 검사
// 	ASSERT(dst != NULL);
// debug removed;
// 	struct hash_iterator hi;	 // 이터레이터 선언
// 	hash_first(&hi, &src->hash); // 이터레이터 초기화

// 	while (hash_next(&hi)) // 각 페이지를 순회하는 루프.
// 	{
// 		struct page *src_page = hash_entry(hash_cur(&hi), struct page, hash_elem);
// 		void *new_aux_ptr = NULL;

// 		enum vm_type type_for_uninit_new = page_get_type(src_page); // 타입 획득.
// 		vm_initializer *init_func = src_page->uninit.init;			// uninit.init 함수(e.g. lazy_load)

// 		if (src_page->uninit.aux != NULL) // init 함수를 위한 aux(매개변수 등)가 존재할 경우 그걸 카피 해줘야 해요.
// 		{
// 			if (VM_TYPE(type_for_uninit_new) == VM_FILE ||						// 타입이 file backed page 이거나 anonymous일 경우 초기화 함수가 존재한다면
// 				(VM_TYPE(type_for_uninit_new) == VM_ANON && init_func != NULL)) // lazy load 같은 함수가 있을 경우. -> 왜 하는지 모르겠음.
// 			{

// 				struct lazy_load_args *src_load_args = (struct lazy_load_args *)src_page->uninit.aux; // src 페이지가 가지고 있는 lazy_laod aux 포인터를 복사.
// 				new_aux_ptr = malloc(sizeof(struct lazy_load_args));								  // 실제 aux 데이터를 저장할 커널 공간 할당.
// 				if (new_aux_ptr == NULL)															  // 할당 실패 예외처리.
// 				{
// 					goto error_cleanup;
// 				}
// 				memcpy(new_aux_ptr, src_load_args, sizeof(struct lazy_load_args)); // 기존 aux에서 새 aux로 커널 메모리를 복사.

// 				if (src_load_args->file != NULL) // 만약 파일이 존재할 경우 파일 복사를 해야함. (파일 포인터 자체는 위에서 복사가 돼 있음. memcpy할 때.)
// 				{
// 					((struct lazy_load_args *)new_aux_ptr)->file = file_duplicate(src_load_args->file); // file_duplicate를 따로 써야 함.

// 					if (((struct lazy_load_args *)new_aux_ptr)->file == NULL) // 파일 복사 실패 예외 처리
// 					{
// 						free(new_aux_ptr);
// 						new_aux_ptr = NULL;
// 						goto error_cleanup;
// 					}
// 				}
// 			}
// 		}
// 		// with_initializer로 바꾸기
// 		vm_alloc_page_with_initializer(type_for_uninit_new, src_page->va, src_page->writable, init_func, new_aux_ptr);
// 		struct page *child_page = spt_find_page(&thread_current()->spt, src_page->va);

// 		// 부모 페이지의 writable 복사
// 		child_page->writable = src_page->writable; // ?

// 		// 목적지 보조 페이지 테이블에 자식 페이지 삽입
// 		spt_insert_page(dst, child_page);
// 		vm_do_claim_page(child_page);
// 		memcpy(child_page->frame->kva, src_page->frame->kva, PGSIZE);
// 	}
// 	return true;

// error_cleanup:
// 	hash_destroy(&dst->hash, spt_destroy_page_in_copy_failure);
// 	hash_init(&dst->hash, page_hash, page_less, NULL);
// 	return false;
// }
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src)
{
	struct hash_iterator i;
	hash_first(&i, &src->hash);

	while (hash_next(&i))
	{
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = page_get_type(src_page);
		void *upage = src_page->va;

		// 1. 새로운 페이지를 dst SPT에 할당
		if (!vm_alloc_page_with_initializer(type, upage, src_page->writable,
											src_page->uninit.init, src_page->uninit.aux))
		{
			return false;
		}

		// 2. 새로 할당된 페이지를 찾고 claim
		struct page *dst_page = spt_find_page(dst, upage);
		if (!vm_claim_page(upage))
		{
			return false;
		}

		// 3. 부모의 프레임이 존재하면, 자식의 프레임으로 데이터 복사
		if (src_page->frame != NULL)
		{
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* supplemental page table이 가지고 있는 리소스를 해제합니다. */
void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
	/* TODO: 해당 스레드가 가지고 있는 supplemental page table의 모든 항목을 제거하고,
	 * TODO: 수정된 내용을 저장소에 기록하세요(write-back). */
	hash_clear(&spt->hash, spt_destructor); // HACK: destructor 뭘로 줘야 하는지 모르겠음
}

void spt_destructor(struct hash_elem *he)
{
	struct page *page = hash_entry(he, struct page, hash_elem);
	vm_dealloc_page(page);
}