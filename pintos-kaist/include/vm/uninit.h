#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
struct uninit_page {
	// 페이지 내용을 초기화한다.
	/* Initiate the contets of the page */
	vm_initializer *init; // 이거는 uninit page 고유의 속성.
	enum vm_type type;
	void *aux;
	// 페이지 구조체를 초기화하고 가상 주소와 물리 주소를 매핑한다. -> uninit page의 부모 구조체 page를 초기화한다?
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
