/* anon.c: 디스크 이미지가 아닌 페이지(즉, 익명 페이지)의 구현 */

#include "vm/vm.h"
#include "devices/disk.h"

/* 이 아래 줄을 수정하지 마세요 */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* 이 구조체를 수정하지 마세요 */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* 익명 페이지를 위한 데이터를 초기화합니다 */
void vm_anon_init(void)
{

    // TODO: swap 구현 시 아래 내용을 추가해야 함. 사유는 그때 가서 이해하기.
    /* swap_disk를 설정하세요. */
    // swap_disk = disk_get(1, 1); // NOTE: disk_get 인자값 적절성 검토 완료. 
    // size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
    // swap_table = bitmap_create(swap_size);
}

// “이 함수는 먼저 page->operations에서 익명 페이지에 대한 핸들러를 설정합니다. 현재 빈 구조체인 anon_page에서
// 일부 정보를 업데이트해야 할 수 있습니다. 이 함수는 익명 페이지(즉, VM_ANON)의 초기화자로 사용됩니다.”

/* 파일 매핑을 초기화합니다 */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* 핸들러 설정 */
    dprintfb("[anon_initializer] routine start page va: %p\n", page->va);
    
    page->operations = &anon_ops;
    
    dprintfb("[anon_initializer] setting anon_ops. %p\n", page->operations);
    
    struct anon_page *anon_page = &page->anon;
    // TODO: anon_page 속성 추가될 경우 여기서 초기화.
    dprintfb("[anon_initializer] done. returning true\n");
    return true; 
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑인합니다 */
static bool
anon_swap_in(struct page *page, void *kva)
{
    PANIC("anon swap in"); // 분명히 호출히 호출돼야 하잖아.
    struct anon_page *anon_page = &page->anon;
}

/* 스왑 디스크에 내용을 써서 페이지를 스왑아웃합니다 */
static bool
anon_swap_out(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
}

/* 익명 페이지를 파괴합니다. PAGE는 호출자가 해제합니다 */
static void
anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    return;
    // TODO: 여기도 딱히 몰라.
}
