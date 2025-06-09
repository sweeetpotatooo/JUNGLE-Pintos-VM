/* anon.c: 디스크 이미지가 아닌 페이지(즉, 익명 페이지)의 구현 */

#include "vm/vm.h"
#include "devices/disk.h"
#include <bitmap.h>
#include "threads/vaddr.h"

/* 이 아래 줄을 수정하지 마세요 */
static struct disk *swap_disk;
static struct bitmap *swap_table;
#define SECTORS_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)
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
    /* 스왑 디스크와 스왑 테이블을 초기화합니다. */
    swap_disk = disk_get(1, 1);
    if (swap_disk != NULL) {
        size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
        swap_table = bitmap_create(swap_size);
    }
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
    anon_page->swap_slot = BITMAP_ERROR;
    dprintfb("[anon_initializer] done. returning true\n");
    return true; 
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑인합니다 */
static bool
anon_swap_in(struct page *page, void *kva)
{
    struct anon_page *anon_page = &page->anon;
    if (anon_page->swap_slot == BITMAP_ERROR)
        return false;

    disk_sector_t sector = anon_page->swap_slot * SECTORS_PER_PAGE;
    for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
        disk_read(swap_disk, sector + i, (uint8_t *)kva + DISK_SECTOR_SIZE * i);

    bitmap_reset(swap_table, anon_page->swap_slot);
    anon_page->swap_slot = BITMAP_ERROR;
    return true;
}

/* 스왑 디스크에 내용을 써서 페이지를 스왑아웃합니다 */
static bool
anon_swap_out(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    size_t slot = bitmap_scan_and_flip(swap_table, 0, 1, false);
    if (slot == BITMAP_ERROR)
        return false;

    disk_sector_t sector = slot * SECTORS_PER_PAGE;
    for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
        disk_write(swap_disk, sector + i,
                   (uint8_t *)page->frame->kva + DISK_SECTOR_SIZE * i);

    anon_page->swap_slot = slot;
    page->frame = NULL;
    return true;
}

/* 익명 페이지를 파괴합니다. PAGE는 호출자가 해제합니다 */
static void
anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    if (anon_page->swap_slot != BITMAP_ERROR)
        bitmap_reset(swap_table, anon_page->swap_slot);
}
