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
		if (write == 1 && page->writable == 0) //추가
			return false;
			
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
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
    // 해시테이블을 순회하기 위해 필요한 구조체
    struct hash_iterator i;
    /* 1. SRC의 해시 테이블의 각 bucket 내 elem들을 모두 복사한다. */
    hash_first(&i, &src->pages);
	while (hash_next (&i)) {
		struct page *src_page = hash_entry (hash_cur (&i), struct page, hash_elem);
		if (VM_TYPE(src_page->operations->type) == VM_UNINIT) {
			vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux);
			continue;
		}
		
		// vm_claim_page(src_page->va);
		if (src_page->operations->type == VM_ANON)
		{
			vm_alloc_page(src_page->operations->type, src_page->va, src_page->writable);
			struct page *dst_page = spt_find_page(dst, src_page->va);
			vm_claim_page(src_page->va);
			memcpy (dst_page->frame->kva, src_page->frame->kva, (size_t)PGSIZE);
		}
		else if (src_page->operations->type == VM_FILE)
		{
			struct lazy_load_info *aux = (struct lazy_load_info*)malloc(sizeof(struct lazy_load_info));
			/* src initializer가 호출될 때 file_page 구조체 내에 저장해 둔 file/ofs/read_bytes를 꺼낸다. */
			/* 같은 파일이 아닌 복제한 파일을 넣어 준다. 자식이 파일을 쓰고 닫아 버리면 접근할 수 없기 때문(?) */
			aux->file = file_duplicate(src_page->file.file);
			aux->ofs = src_page->file.file_ofs;
			aux->page_read_bytes = src_page->file.read_bytes;

			vm_alloc_page_with_initializer(src_page->operations->type, src_page->va, src_page->writable, NULL, aux);
			struct page *dst_page = spt_find_page(dst, src_page->va);

			/* dst_page의 경우 프레임 할당받지 않기 때문에, initializer가 호출되지 않는다. 따라서 직접 initializer를 호출 */
			file_backed_initializer(dst_page, VM_FILE, NULL);
			pml4_set_page(thread_current()->pml4, dst_page->va, src_page->frame->kva, src_page->writable);
			dst_page->frame = src_page->frame;
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
