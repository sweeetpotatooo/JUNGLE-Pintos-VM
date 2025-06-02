/* anon.c: 디스크 이미지가 아닌 페이지(즉, 익명 페이지)의 구현 */

#include "vm/vm.h"
#include "devices/disk.h"

/* 이 아래 줄을 수정하지 마세요 */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* 이 구조체를 수정하지 마세요 */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* 익명 페이지를 위한 데이터를 초기화합니다 */
void
vm_anon_init (void) {
    /* TODO: swap_disk를 설정하세요. */
    swap_disk = NULL;
	swap_disk = disk_get(1, 1); // HACK: disk_get 인자값을 잘 모르겠음.
}

/* 파일 매핑을 초기화합니다 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
    /* 핸들러 설정 */
    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;
	anon_page->page = page;
	// HACK: type을 어디의 멤버로 설정할지 모르겠음. kva도 마찬가지.
	
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑인합니다 */
static bool
anon_swap_in (struct page *page, void *kva) {
    struct anon_page *anon_page = &page->anon;
}

/* 스왑 디스크에 내용을 써서 페이지를 스왑아웃합니다 */
static bool
anon_swap_out (struct page *page) {
    struct anon_page *anon_page = &page->anon;
}

/* 익명 페이지를 파괴합니다. PAGE는 호출자가 해제합니다 */
static void
anon_destroy (struct page *page) {
    struct anon_page *anon_page = &page->anon;
}
