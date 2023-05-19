/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"
#include "userprog/process.h"
#include "include/threads/vaddr.h"


/*추가*/
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

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
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,vm_initializer *init, void *aux) {

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
	struct hash_elem *e;
	/* TODO: Fill this function. */
	// 인자로 넘겨진 보조 페이지 테이블에서로부터 가상 주소(va)와 대응되는 
	//페이지 구조체를 찾아서 반환합니다. 실패했을 경우 NULL를 반환
	page = (struct page*)malloc(sizeof(struct page));
	page->va = pg_round_down(va); //내부에서 va가 가리키는 가상 페이지의 시작 (페이지 오프셋이 0으로 설정된 va)을 반환
	e = hash_find(&spt->pages, &page->hash_elem);
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
	frame = malloc(sizeof(struct frame));
	void *kva = palloc_get_page(PAL_USER);

	if(kva != NULL){
		frame->kva = kva;
		frame->page = NULL;
	}else{
		PANIC("TODO");
		frame->page = NULL;
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	void *growth_stk = pg_round_down(addr);
	vm_alloc_page_with_initializer(VM_ANON,growth_stk,1,NULL,NULL);
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
	uintptr_t usr_rsp;

	if(user){ 
		usr_rsp = f->rsp;
	}else{
		usr_rsp = thread_current()->user_rsp;
	}

	if(not_present){ //True: not-present page:  페이지가 물리메모리에 존재하지 않는 경우

		if(USER_STACK - (1<<20) <= addr && (USER_STACK>=addr) && (usr_rsp-8 <= addr)){
			// printf("fault_addr: %x\n",addr);
			// printf("USER_STACK: %x\n",USER_STACK);
			// printf("USER_STACK - 0x100000: %x\n",USER_STACK - 0x100000);
			vm_stack_growth(addr);
		}

		page = spt_find_page(spt,addr);
		if(page == NULL){
			return false;
		}
		return vm_do_claim_page (page);
	}
	return false;
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
	page = spt_find_page(spt,va);
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

	hash_init(&spt->pages, page_hash, page_less,NULL);

}

/* Copy supplemental page table from src to dst */
// bool
// supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
// 		struct supplemental_page_table *src UNUSED) {
// /* src의 supplemental page table를 반복하면서
// 	dst의 supplemental page table의 엔트리의 정확한 복사본을 만드세요 */
// 	/*src의 각각 페이지를 반복하면서 dst의 엔트리에 정확히 복사*/

// 	struct hash_iterator i;

// 	hash_first(&i, &src->pages);
// 	while (hash_next (&i)) {
// 		struct page *src_page = hash_entry (hash_cur (&i), struct page, hash_elem);
// 		if (VM_TYPE(src_page->operations->type) == VM_UNINIT) {
// 			vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux);
// 			continue;
// 		}
// 		vm_alloc_page(src_page->operations->type, src_page->va, src_page->writable);
// 		struct page *dst_page = spt_find_page(dst, src_page->va);
// 		vm_claim_page(src_page->va);

// 		memcpy (dst_page->frame->kva, src_page->frame->kva, (size_t)PGSIZE);
// 	}
// 	return true;
// }

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
    // 해시테이블을 순회하기 위해 필요한 구조체
    struct hash_iterator i;
    /* 1. SRC의 해시 테이블의 각 bucket 내 elem들을 모두 복사한다. */
    hash_first(&i, &src->pages);
    while (hash_next(&i)){ // src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);     // 부모 페이지의 type
        void *upage = parent_page->va;                          // 부모 페이지의 가상 주소
        bool writable = parent_page->writable;              // 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;    // 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        // 부모 타입이 uninit인 경우
        if(parent_page->operations->type == VM_UNINIT) { 
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
                // 자식 프로세스의 유저 메모리에 UNINIT 페이지를 하나 만들고 SPT 삽입.
                return false;
        }
        else {  // 즉, else문의 코드는 실제 부모의 물리메모리에 매핑되어있던 데이터는 없는상태이다 그래서 아래에서 memcpy로 부모의 데이터 또한 복사해 넣어준다.
            if(!vm_alloc_page(type, upage, writable)) // type에 맞는 페이지 만들고 SPT 삽입.
                return false;
            if(!vm_claim_page(upage))  // 바로 물리 메모리와 매핑하고 Initialize한다.
                return false;
        }

        // UNIT이 아닌 모든 페이지에 대응하는 물리 메모리 데이터를 부모로부터 memcpy
        if (parent_page->operations->type != VM_UNINIT) { 
            struct page* child_page = spt_find_page(dst, upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
  return true;  
}

void destroy_hash_elem(struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e,struct page, hash_elem);
	destroy(page);
	free(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	스레드에 의해 보조 페이지 테이블이 소유된 경우 해당 테이블을 파괴해야 합니다.
	 * TODO: writeback all the modified contents to the storage. 
	 변경된 내용을 저장소에 기록*/


	 /*1. 이를 위해 보조 페이지 테이블에 저장된 페이지 엔트리를 반복하여 각 페이지에 대해 파괴 작업을 수행해야 합니다. 
	 예를 들어, hash_destroy() 함수를 사용하여 해시 테이블을 파괴할 수 있습니다.
	 2.
	 */
	/*페이지 엔트리를 반복하면서 테이블의 페이지에 destroy(page)를 호출해야함 */
	// struct hash_iterator i;
	// hash_first(&i,&spt->pages);
	// while(hash_next(&i)){
	// 	struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);

	// 	if(page->operations->type == VM_FILE){
			//do_mumap(page->va); 구현 필요
	// 	}
	// }

	hash_clear(&spt->pages,&destroy_hash_elem);
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
