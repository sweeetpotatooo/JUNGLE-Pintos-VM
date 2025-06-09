/* anon.c: 디스크 이미지가 아닌 페이지(즉, 익명 페이지)의 구현 */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#include "devices/disk.h"
#include "threads/mmu.h"

/* 이 아래 줄을 수정하지 마세요 */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

struct bitmap *swap_table;
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
    // TODO: swap 구현 시 아래 내용을 추가해야 함.
    /* swap_disk를 설정하세요. */
    swap_disk = disk_get(1, 1); // NOTE: disk_get 인자값 적절성 검토 완료. 
    size_t swap_size = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE);
    swap_table = bitmap_create(swap_size);
}

// “이 함수는 먼저 page->operations에서 익명 페이지에 대한 핸들러를 설정합니다. 현재 빈 구조체인 anon_page에서
// 일부 정보를 업데이트해야 할 수 있습니다. 이 함수는 익명 페이지(즉, VM_ANON)의 초기화자로 사용됩니다.”

/* 파일 매핑을 초기화합니다 */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
    if (page == NULL){
        return false;
    }
    /* 핸들러 설정 */
    dprintfb("[anon_initializer] routine start page va: %p\n", page->va);
    
    page->operations = &anon_ops;
    
    dprintfb("[anon_initializer] setting anon_ops. %p\n", page->operations);
    
    struct anon_page *anon_page = &page->anon;
    anon_page->swap_index = -1;
    // TODO: anon_page 속성 추가될 경우 여기서 초기화.
    dprintfb("[anon_initializer] done. returning true\n");
    return true; 
}

/* 스왑 디스크에서 내용을 읽어 페이지를 스왑인합니다 */
// disk_read로 데이터 읽은뒤 kva에 데이터 복사
// swap_idx를 -1로 변경, 프레임 테이블에 프레임 삽입
// 프레임과 페이지 매핑
bool
anon_swap_in (struct page *page, void *kva)
{
    struct anon_page *anon_page = &page->anon;
    int swap_idx = anon_page->swap_index;

    /* 이미 메모리에 있는 페이지 */
    if (swap_idx == -1)
        return false;

    /* swap_disk의 ‘swap_idx’ 번째 슬롯 → 8개섹터에 저장 */
    for (int i = 0; i < 8; i++)
    {
        /* 섹터 번호 = 슬롯 시작 섹터 + i, 목적지는 kva + (i × DISK_SECTOR_SIZE) */
        disk_read (swap_disk, swap_idx * 8 + i,(uint8_t *) kva + i * DISK_SECTOR_SIZE);
    }

    /* 스왑 슬롯을 비워 두도록 표시 */
    bitmap_reset (swap_table, swap_idx);

    /* 더 이상 스왑과 연관되지 않았음을 명시 */
    anon_page->swap_index = -1;

    return true;
}

/* anon_swap_out()
 - page가 가리키는 프레임의 4KB 내용을 스왑 디스크에 저장(=swap-out)한다.
 - 저장 위치(스왑 슬롯)는 bitmap(=swap_table)에서 빈 칸을 찾아 할당한다.
 - 완료되면 anon_page->swap_index에 슬롯 번호를 기록하고 프레임은 해제하여 물리 메모리를 회수한다.
 - 남은 스왑 공간이 없으면 PANIC을 일으킨다.
 - 반환: 성공 시 true
 */
static bool
anon_swap_out (struct page *page)
{
    if (page == NULL)
        return false;

    struct anon_page *anon_page = &page->anon;
    struct frame *frame = page->frame;

    /* 이미 swap-out 된 페이지는 다시 내보낼 필요 없음 */
    if (anon_page->swap_index != -1)
        return true;

    /* 스왑 테이블(bitmap)에서 비어 있는 슬롯 검색 */
    size_t slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
    if (slot == BITMAP_ERROR)
        PANIC ("swap space exhausted");

    /* 프레임 데이터를 8 섹터로 나누어 디스크에 기록 */
    for (int i = 0; i < 8; i++)
        disk_write (swap_disk,slot * 8 + i,(uint8_t *) frame->kva + i * DISK_SECTOR_SIZE);

    /* anon_page에 스왑 슬롯 번호 기록 */
    anon_page->swap_index = (int) slot;

    /* 페이지와 프레임 연결 해제 & 페이지 매핑 제거 */
    pml4_clear_page (thread_current ()->pml4, page->va); /* VA→PA 매핑 삭제 */
    frame->page = NULL;                                  /* 역참조 제거     */
    page->frame = NULL;  /* 논리적으로 “메모리에 없음” 표시 */

    return true;
}


/* 익명 페이지를 파괴합니다. PAGE는 호출자가 해제합니다 */
/* anon_destroy()
 익명 페이지가 완전히 사라질 때 호출.
 아직 스왑 슬롯을 점유하고 있다면 bitmap에서 해제
 프레임이 연결돼 있다면 물리 메모리 반납
 page·frame 간 매핑도 끊어 줌
 */
static void
anon_destroy (struct page *page)
{
    if (page == NULL)
        return;

    struct anon_page *anon_page = &page->anon;

    /* 스왑 슬롯 회수 */
    if (anon_page->swap_index != -1)
    {
        bitmap_reset (swap_table, anon_page->swap_index); /* 슬롯을 ‘비어 있음’으로 */
        anon_page->swap_index = -1;
    }

    /* 프레임 반납 */
    if (page->frame != NULL)
    {
        /* 해당 가상 주소의 매핑 제거(PML4) */
        pml4_clear_page (thread_current ()->pml4, page->va);
        /* 역참조 해제 */
        page->frame->page = NULL;
        /* 물리 프레임 반환 */
        free (page->frame);
        page->frame = NULL;
    }
}

