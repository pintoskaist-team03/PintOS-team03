/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"
#include "userprog/process.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. 
 * 이니셜라이저로 보류 중인 페이지 객체를 생성합니다. 페이지를 만들려면 직접 만들지 말고 이 함수 또는 `vm_alloc_page`를 통해 만드세요.
 * */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. 
		 * 페이지를 생성하고, VM 유형에 따라 이니셜을 가져온 다음, uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다. uninit_new를 호출한 후 필드를 수정해야 합니다.
		 * */
		struct page *new_page = (struct page*)malloc(sizeof(struct page));
		if(VM_TYPE(type)==VM_ANON){
			uninit_new(new_page,upage,init,type,aux,anon_initializer);
		}
		else if(VM_TYPE(type)==VM_FILE){
			uninit_new(new_page,upage,init,type,aux,file_backed_initializer);
		}
		new_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt,new_page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. 
보조 페이지 테이블에서로부터 가상 주소(va)와 대응되는 페이지 구조체를 찾아서 반환
: project3-1 구현*/
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	// 인자로 넘겨진 보조 페이지 테이블에서로부터 가상 주소(va)와 대응되는 
	//페이지 구조체를 찾아서 반환합니다. 실패했을 경우 NULL를 반환
	page->va = va;
	struct hash_elem *e = hash_find(&spt->pages, &page->hash_elem);
	return e != NULL ? hash_entry(e,struct page, hash_elem) : NULL;

}

/* Insert PAGE into spt with validation(유효성 검사를 통해 PAGE를 spt에 삽입). :project3-1 구현*/
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *elem = hash_insert(&spt->pages, &page->hash_elem);

	if(elem == NULL){
		succ = true;
		return succ;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * palloc()을 호출하고 프레임을 가져옵니다. 사용 가능한 페이지가 없는 경우 페이지를 퇴거하고 반환합니다.
  이 함수는 항상 유효한 주소를 반환합니다. 
 * 즉, 사용자 풀 메모리가 가득 차면 이 함수는 프레임을 퇴거하여 사용 가능한 메모리 공간을 확보합니다.
 * */
//project3-1 구현
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	//유저 메모리 풀에서 페이지를 성공적으로 가져오면, 
	//프레임을 할당하고 프레임 구조체의 멤버들을 초기화한 후 해당 프레임을 반환
	void *kva = palloc_get_page(PAL_USER);

	if(frame != NULL){
		frame->kva = kva;
		frame->page = NULL;
	}else{
		frame = vm_evict_frame();
		frame->page = NULL;
	}


	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success 
project3-1 구현*/
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. va에 페이지 할당, 해당 페이지에 프레임 할당
project3-1 구현
 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	//va에 페이지 할당하고, return받은 page를 do_claim호출
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt,va);
	if(page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. 주어진 page에 물리 메모리 프레임을 할당
project3-1 구현 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); //frame얻음, 이후 mmu세팅(가상주소와 물리주소를 매핑한 정보를 페이지 테이블에 추가해야함)
	struct thread *curr = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(curr->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table : project3-1 구현 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {

	hash_init(&spt, page_hash, page_less,NULL);

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/*추가 함수*/
/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}
