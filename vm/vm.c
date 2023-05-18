/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"

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
	/* TODO: Your code goes dhere. */
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
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		
		bool (*initializer)(struct page *, enum vm_type, void *);
		
		switch(type){
			case VM_ANON: case VM_ANON|VM_MARKER_0: // 왜 두번째 케이스에서 저렇게 이중(?)으로 체크하지?
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}
		
		struct page *new_page = malloc(sizeof(struct page));
		uninit_new (new_page, upage, init, type, aux, initializer);

		new_page->writable = writable;
		
		/* TODO: Insert the page into the spt. */
		
		spt_insert_page(spt, new_page);
		
		// printf("upage = %x, init result = %x \n", pg_round_down(upage), spt_find_page(spt, upage));
		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	struct page p;
	struct hash_elem *e;

	// p.va = va;
	p.va = pg_round_down(va); // dummy for hashing

	e = hash_find (&spt->spt_hash, &p.spt_elem);
	return e != NULL ? hash_entry (e, struct page, spt_elem) : NULL;

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_find(&spt->spt_hash, &page->spt_elem);
	if(e != NULL) // page already in SPT
		return succ; // false, fail

	// page not in SPT
	hash_insert (&spt->spt_hash, &page->spt_elem);	
	return succ = true;
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
	// victim = list_entry(list_pop_front(&frame_table), struct frame, elem); // FIFO algorithm
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
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	// 유저 풀에서 할당해주기
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);

	if (kva == NULL){ // NULL이면(사용 가능한 페이지가 없으면) 
		// 처리
	}
	else{ // 사용 가능한 페이지가 있으면
		frame = calloc(1, sizeof(struct frame)); // 페이지 사이즈만큼 메모리 할당
		frame->kva = kva;
	}
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	
	vm_alloc_page(VM_ANON | VM_MARKER_0, pg_round_down(addr), true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	struct thread *curr = thread_current();
	// printf(">> rsp = %x, addr = %x\n", curr->rsp, addr);
	
	void *rsp = user ? f->rsp : curr ->rsp; // a page fault occurs in the kernel
	const int GROWTH_LIMIT = 32; // heuristic
	const int STACK_LIMIT = USER_STACK - (1<<20); // 1MB size limit on stack
	
	// 여기서 스택 그로운지 어떻게 알지??
	if((uint64_t)addr > STACK_LIMIT && USER_STACK > (uint64_t)addr && (uint64_t)addr > (uint64_t)rsp - GROWTH_LIMIT){
		vm_stack_growth (addr);  
	}
	if (spt_find_page(spt, addr) == NULL){
		return false;
	}
		
	bool result = vm_claim_page (addr);
	
	return result;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	// struct page *page = NULL;
	/* TODO: Fill this function */
	struct supplemental_page_table *spt = &thread_current()->spt; 
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* P3 추가 */

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *cur = thread_current();
	// bool writable = page->writable; // [vm.h] struct page에 bool writable; 추가
	pml4_set_page(cur->pml4, page->va, frame->kva, true);

	// add the mapping from the virtual address to the physical address in the page table.
	return swap_in (page, frame->kva);
}

unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED) {
    const struct page *p = hash_entry(p_, struct page, spt_elem);
    return hash_bytes(&p->va, sizeof p->va);
}

// 해시 테이블 초기화할 때 해시 요소들 비교하는 함수의 포인터
// a가 b보다 작으면 true, 반대면 false
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
    const struct page *a = hash_entry(a_, struct page, spt_elem);
    const struct page *b = hash_entry(b_, struct page, spt_elem);

    return a->va < b->va;
}
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}
void hash_action_copy (struct hash_elem *e, void *hash_aux);
/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// 포크를 위해 페이지 테이블을 복제해야 한다. 
	// src을 dst로 복사하자. 
	// 복사하는 함수가 있나?
	src->spt_hash.aux = dst; // pass 'dst' as aux to 'hash_apply'
	hash_apply(&src->spt_hash, hash_action_copy);
	return true;
}
struct lazy_load_info {
	struct file *file;
	size_t page_read_bytes;
	size_t page_zero_bytes;
	off_t offsetof;
};
void hash_action_copy (struct hash_elem *e, void *hash_aux) {
	struct thread *t = thread_current();
	ASSERT(&t->spt == (struct supplemental_page_table *)hash_aux); 

	struct page *page = hash_entry(e, struct page, spt_elem);

	enum vm_type type = page->operations->type;
	if(type == VM_UNINIT) {
		struct uninit_page *uninit = &page->uninit;
		vm_initializer *init = uninit->init;
		void *aux = uninit->aux;

		// copy aux (struct lazy_load_info *)
		struct lazy_load_info *lazy_load_info = malloc(sizeof(struct lazy_load_info));
		if(lazy_load_info == NULL) {
			// #ifdef DBG
			// malloc fail - kernel pool all used
		}
		memcpy(lazy_load_info, (struct lazy_load_info *)aux, sizeof(struct lazy_load_info));

		lazy_load_info->file = file_reopen(((struct lazy_load_info *)aux)->file); // get new struct file (calloc)
		vm_alloc_page_with_initializer(uninit->type, page->va, page->writable, init, lazy_load_info);

	}
	if(type & VM_ANON == VM_ANON) { // include stack pages
		// when __do_fork is called, thread_current is the child thread so we can just use vm_alloc_page
		vm_alloc_page(type, page->va, page->writable);

		struct page *newpage = spt_find_page(&t->spt, page->va); // copied page
		vm_do_claim_page(newpage);

		ASSERT(page->frame != NULL);
		memcpy(newpage->frame->kva, page->frame->kva, PGSIZE);
	}
	
}

void hash_action_destroy (struct hash_elem *e, void *aux);
/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// struct thread *t = thread_current();
	
	hash_destroy(spt, hash_action_destroy);
}
void remove_page(struct page *page){
	struct thread *t = thread_current();

	pml4_clear_page(t->pml4, page->va);

	if (page->frame != NULL){
		page->frame->page = NULL;
	}
}

void hash_action_destroy (struct hash_elem *e, void *aux){
	struct thread *t = thread_current();
	struct page *page = hash_entry(e, struct page, spt_elem);
	
	if (page->frame != NULL){ // 동적 할당을 free해주기 전에 포인터를 NULL로 바꿔준다. 
		page->frame->page = NULL;		
	}
	
	remove_page(page);	
}