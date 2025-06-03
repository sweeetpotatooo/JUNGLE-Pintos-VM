/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

/* uninit.c: 초기화되지 않은 페이지(uninitialized page)의 구현
 *
 * 모든 페이지는 처음에는 uninit 페이지로 생성됩니다. 첫 페이지 폴트가 발생하면
 * 핸들러 체인은 uninit_initialize(page->operations.swap_in)을 호출합니다.
 * uninit_initialize 함수는 페이지 객체를 초기화하여 anon, file, page_cache와
 * 같은 구체적인 페이지 객체로 변환한 뒤,
 * vm_alloc_page_with_initializer 함수에서 전달된 초기화 콜백을 호출합니다.
 */


#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init, // 얘가 load에서 lazy_load_segment로 넘어옴
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init, // 이건 완전 특수한 목적. lazy_load_segment같은.
			.type = type,
			.aux = aux,
			.page_initializer = initializer, // 페이지 이니셜라이저.
		}
	};
}

/* Initializes the page on the first fault.  첫 폴트 발생 시 페이지를 초기화합니다.
* The template code first fetches vm_initializer and aux 템플릿 코드는 우선 vm_initializer와 aux를 가져옵니다.
* and calls the corresponding page_initializer  그리고 대응하는 페이지 이니셜라이저를 호출합니다.
* through a function pointer.  함수 포인터를 이용해서요.
* You may need to modify the function depending on your design. 당신은 당신 디자인에 맞게 이 함수(함수 포인터가 가리키는 함수?)를 수정하세요
 */

// 최초 페이지 폴트 발생 시 호출
// 내부에서 저장된 초기화 함수(init)와 aux 정보를 사용해 `anon_initializer`, `file_backed_initializer`, `page_cache..` 등 호출
/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) { // 페이지와 커널 가상 주소를 인수로 받음.
	// NOTE: 커널 가상 주소(kva)는 왜 받지? 
	struct uninit_page *uninit = &page->uninit;

	/* 우선 가져오고, page_initialize가 값을 덮어씌운다. */
	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init; // uninit 페이지의 프레임을 초기화하는 함수. (e.g. lazy_load_segment)

	void *aux = uninit->aux; // NOTE: uninit 페이지의 프레임을 초기화하기 위한 보조 데이터. (e.g. lazy_aux)
	
	/* TODO: You may need to fix this function. */

	// NOTE: 아래 page_initializer 자리에 `anon_initializer` 등의 이니셜라이저가 들어간다. 
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true); // claim이 완료된 페이지의 프레임을 초기화하는 함수 `init` 호출.
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */

/* uninit_page 가 보유한 자원을 해제합니다.
 * 대부분의 페이지는 다른 페이지 객체로 변환되지만,
 * 프로세스 종료 시까지 한 번도 참조되지 않은 uninit 페이지가 남아 있을 수 있습니다.
 * PAGE 자체는 호출자가 해제합니다.
 */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit = &page->uninit;
	return;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
