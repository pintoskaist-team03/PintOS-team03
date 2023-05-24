/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "include/threads/vaddr.h"
#include "lib/kernel/bitmap.h"
#include "include/lib/string.h"
#include "include/threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/*project3 추가*/
struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE/DISK_SECTOR_SIZE; //8


/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
// 스왑 테이블은 각 페이지에 대한 비트를 가지며, 이 비트는 해당 페이지가 스왑인되었는지 여부를 나타냄
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
    swap_disk = NULL;
	swap_disk = disk_get(1,1); //swap_disk를 swap 공간으로 사용하겠다.
	size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE; //스왑공간의 페이지 수 계산
	swap_table = bitmap_create(swap_size); //스왑공간의 각페이지에 대한 상태를 추적
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	struct uninit_page *uninit = &page->uninit;// ANON page를 초기화하기 위해 해당 데이터를 0으로 초기화해줌
	memset(uninit,0,sizeof(struct uninit_page));
	
	/* Set up the handler */
	page->operations = &anon_ops;

	//해당 페이지는 물리 메모리 위에 있으므로 swap_index의 값을 -1로 설정
	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1; //해당 페이지가 스왑공간에는 저장되어 있지 않음

	return true;
}

/*승훈 코드*/
/* Swap in the page by read contents from the swap disk. */
// static bool
// anon_swap_in (struct page *page, void *kva) {
// 	struct anon_page *anon_page = &page->anon;

// 	/* swap out된 페이지가 디스크 스왑 영역 어디에 저장되었는지는
// 	   anon_page 구조체 안에 저장되어 있다. */

// 	int page_no = anon_page->swap_index;
// 	bitmap_scan_and_flip(swap_table, anon_page->swap_index, 8, true);
// 	/*스왑 테이블에서 해당 스왑 슬롯이 진짜 사용중인지 체크*/
// 	// if(bitmap_test(swap_table,page_no)==false){
// 	// 	return false;
// 	// }

// 	void *dst = kva;
// 	/* 해당 스왑 영역의 데이터를 가상 주소 공간 kva에 써 준다. */
// 	for(int i = 0; i<SECTORS_PER_PAGE; i++){
// 		disk_read(swap_disk, page_no+i, dst);
// 		dst += DISK_SECTOR_SIZE;
// 	}
// 	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);
// 	anon_page->swap_index = -1;

// 	return true;

// 	/*해당 swap slot false로 만들어줌(다음번에 쓸 수 있게)*/
// }

// /* Swap out the page by writing contents to the swap disk. */
// static bool
// anon_swap_out (struct page *page) {
// 	struct anon_page *anon_page = &page->anon;

// 	/*비트맵을 처음부터 순회해 false값을 가진 비트를 하나 찾음.
// 	->페이지를 할당 받을 수 있는 스왑슬롯을 찾는다*/
// 	int page_no = bitmap_scan_and_flip(swap_table,0,8,false); //swap_table에서 처음부터 순회하며 값이 false인 비트(할당되지 않은 스왑 슬롯)를 찾음
// 	//해당 비트의 인덱스 위치를 반환

// 	if(page_no == BITMAP_ERROR){
// 		return false;
// 	}

// 	void *src = page->frame->kva;
// 	//한 page를 disk에 쓰기 위해 SECTORS_PER_PAGE(8)개의 섹터의 저장
// 	//이때 disk의 각 섹터의 크기(512)만큼 써준다
// 	for(int i = 0; i<SECTORS_PER_PAGE;i++){
// 		disk_write(swap_disk,page_no+i, src);
// 		src += DISK_SECTOR_SIZE;
// 		//disk_write() 함수를 사용하여 페이지의 각 섹터를 스왑 슬롯에 씀
// 		 //스왑 테이블에서 해당 페이지에 대한 스왑 슬롯의 비트를 true로 설정하여 할당된 상태로 표시

// 	}
	
// 	// swap table의 해당 page에 대한 swap slot의 bit를 ture로 바꿔준다.
//     // 해당 page의 pte에서 present bit을 0으로 바꿔준다.
//     // 이제 프로세스가 이 page에 접근하면 page fault가 뜬다.

// 	pml4_clear_page(thread_current()->pml4, page->va);
// 	palloc_free_page(page->frame->kva);
// 	free(page->frame);
// 	page->frame = NULL;
// 	//현재 스레드의 PML4를 사용하여 페이지의 가상 주소에 해당하는 PTE(Paging Table Entry)에서 Present 비트를 0으로 설정합니다. 
// 	//이렇게 함으로써 프로세스가 해당 페이지에 접근하면 페이지 폴트가 발생

// 	// page의 swap_index 값을 이 page가 저장된 swap slot의 번호로 써준다.
// 	anon_page->swap_index = page_no;
// 	return true;
// }

/*내꺼*/
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	int page_no = anon_page->swap_index;

    if(bitmap_test(swap_table, page_no) == false){
        return false;
    }
    // 해당 swap 영역의 data를 가상 주소공간 kva에 써준다.
    for(int i=0; i< SECTORS_PER_PAGE; ++i){
        disk_read(swap_disk, (page_no * SECTORS_PER_PAGE) + i, kva + (DISK_SECTOR_SIZE * i));
    }
    // 해당 swap slot false로 만들어줌(다음번에 쓸 수 있게)
    bitmap_set(swap_table, page_no, false);
    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	int page_no = bitmap_scan_and_flip(swap_table, 0, 1, false);
    if(page_no == BITMAP_ERROR){
        return false;
    }
    // 한 page를 disk에 쓰기 위해 SECTORS_PER_PAGE개의 섹터에 저장한다.
    // 이 때 disk의 각 섹터의 크기(DISK_SECTOR_SIZE)만큼 써 준다.
    for(int i=0; i<SECTORS_PER_PAGE; ++i){
        disk_write(swap_disk, (page_no * SECTORS_PER_PAGE) + i, page->va + (DISK_SECTOR_SIZE * i));
    }
    // swap table의 해당 page에 대한 swap slot의 bit를 ture로 바꿔준다.
    // 해당 page의 pte에서 present bit을 0으로 바꿔준다.
    // 이제 프로세스가 이 page에 접근하면 page fault가 뜬다.
    
    pml4_clear_page(thread_current()->pml4, page->va);
    // page의 swap_index 값을 이 page가 저장된 swap slot의 번호로 써준다.
    anon_page->swap_index = page_no;
    
    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
    struct anon_page *anon_page = &page->anon;
}