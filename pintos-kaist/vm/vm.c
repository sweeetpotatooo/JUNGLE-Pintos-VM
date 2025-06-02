/* vm.c: 가상 메모리 객체에 대한 공통 인터페이스 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h>
#include "threads/vaddr.h"
// project 3
#include "threads/mmu.h"
#include "vm/uninit.h"

/* 가상 메모리 서브시스템을 초기화합니다.
 * 각 서브시스템의 초기화 코드를 호출합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_fhle_init();
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

// 주어진 타입으로 초기화되지 않은 페이지를 생성합니다.
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

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* 해당 upage가 이미 존재하는지 확인합니다. */
	if (spt_find_page(spt, upage) == NULL) // 페이지가 존재하지 않는다면
	{
		// HACK: 플래그는? 말록이야? 아니야? 몰라?
		struct page *page = palloc_get_page(PAL_USER); // 물리가 바로 나오는 거잖아. 그럼 lazy loading이 아니잖아.
		/* TODO: 페이지를 생성하고(이러고 나면 page->va 참조 가능), -> va가 여전히 알쏭달쏭.
		 * VM 타입에 맞는 초기화 함수를 가져와서, -> 이건 처리된 거 같음.
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요. -> 이건?
		 * TODO: 생성 이후 필요한 필드를 수정하세요. -> 뭐가 필요한데요??
		 */
		page->va = upage;
		page->writable = writable;
		switch (VM_TYPE(type))
		{
		case (VM_ANON):
			// HACK: va가 문제. 어떤 값을 va로 넘겨줘야 할지 불분명. VM_uninit에 대해 처리?.
			uninit_new(page, page->va, init, type, aux, anon_initializer);
			break;
		case (VM_FILE):
			uninit_new(page, page->va, init, type, aux, file_backed_initializer);
			break;
		};

		/* 생성한 페이지를 spt에 삽입하세요. */
		return spt_insert_page(&thread_current()->spt, page); // install page 역할을 해주는 거 아닌가?
	}

err:
	return false;
}

/* spt에서 VA를 찾아서 페이지를 반환합니다. 실패 시 NULL을 반환합니다. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = NULL;
	// - 가상 주소 `va`에 해당하는 페이지를 supplemental page table에서 찾습니다.
	// - 찾지 못하면 `NULL`을 반환합니다.
	struct page page;
	struct hash_elem *e;

	page->va = va;
	e = hash_find(&spt->hash, &page->hash_elem); // hash_find는 hash 구조체와 hash_elem을 인수로 받음.

	if (e != NULL)
	{
		return hash_entry(e, struct page, hash_elem);
	}
	else
	{
		return NULL;
	}
}

/* 유효성 검사를 통해 PAGE를 spt에 삽입합니다. */
bool spt_insert_page(struct supplemental_page_table *spt,
					 struct page *page)
{
	int succ = false;

	if (spt == NULL || page == NULL) // 좋았어.
	{
		return false;
	}

	if (hash_insert(&spt->hash, &page->hash_elem) == NULL) // null이면 삽입 성공
	{
		succ = true;
	}
	else // 실패
	{
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

	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	if (frame->kva == NULL)
	{
		free(frame);
		frame = vm_evict_frame(); // HACK: evict frame 함수가 아직 구현되지 않음.
	}
	ASSERT(frame->kva != NULL);
	return frame;
}

/* 스택 확장 */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* 쓰기 보호된 페이지에 대한 예외를 처리합니다. */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* 성공 시 true를 반환합니다. */
bool vm_try_handle_fault(struct intr_frame *f, void *addr,
						 bool user, bool write, bool not_present)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: 예외에 대한 유효성 검사 수행 */
	/* TODO: 여기에 코드를 작성하세요. */

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
bool vm_claim_page(void *va)
{
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va); // va를 가지고 현재 쓰레드의 spt 에서 페이지를 찾아냄.
	if (page == NULL)								  // spt에 없으면 false 리턴.
		return false;
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
	struct frame *frame = vm_get_frame();

	/* 링크 설정 */
	frame->page = page;
	page->frame = frame;

	/* 페이지의 VA와 프레임의 PA를 매핑하기 위해 페이지 테이블 엔트리를 삽입하세요. */
	if (pml4_get_page(thread_current()->pml4, page->va) == NULL) // HACK: 확신 없음.
	{
		if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) // HACK: writable 이렇게 추가하는 거 맞나요?
		{
			return false;
		}
	}
	return swap_in(page, frame->kva);
}

uint64_t page_hash(const struct hash_elem *e, void *aux)
{
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p, sizeof(p->va));
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

/* src에서 dst로 supplemental page table을 복사합니다. */
// - `src`의 supplemental page table을 `dst`에 복사
// - fork 시 부모의 실행 컨텍스트를 자식에게 복사할 때 사용
// - 각 페이지를 순회하며 `uninit_page`로 생성하고 **즉시 claim 처리**해야 함
// TODO: 구현 하다 말았음.
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
								  struct supplemental_page_table *src)
{
	ASSERT(src != NULL);
	ASSERT(dst != NULL);

	struct hash_iterator hi;
	struct hash *src_hash = &src->hash;

	// hash iterator initialized
	hash_first(&hi, src_hash);
	for (; hi.elem != NULL; hash_next(&hi))
	{
		struct page *src_page = hash_entry(hi.elem, struct page, hash_elem);
		struct page *dest_page;
		vm_alloc_page(page_get_type(src_page), src_page->va, src_page->writable); // 결국 uninit_page를 만들긴 함.
		vm_claim_page(src_page->va);											  // src_page의 가상 주소를 가지고 현재 쓰레드의 spt에서 페이지를 찾아서 프레임을 할당해준다. spt 복사하는데 이게 왜 있어야 하는 거?
		spt_insert_page(src, src_page);
	}
}

/* supplemental page table이 가지고 있는 리소스를 해제합니다. */
void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
	/* TODO: 해당 스레드가 가지고 있는 supplemental page table의 모든 항목을 제거하고,
	 * TODO: 수정된 내용을 저장소에 기록하세요(write-back). */
	// NOTE: 민혁이가 clear 쓰라고 함
	hash_clear(&spt->hash, spt_destructor); // HACK: destructor 뭘로 줘야 하는지 모르겠음

}

void spt_destructor(struct hash_elem *he)
{
	struct page *page = hash_entry(he, struct page, hash_elem);
	vm_dealloc_page(page);
}