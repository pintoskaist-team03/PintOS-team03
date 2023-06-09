/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"
#include "userprog/process.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"


#define USER_STK_LIMIT (1 << 20)
struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = (struct page *)malloc(sizeof(struct page));

		// 초기화 함수 세팅 - anon, file-backed에 따라 다르게 설정하기
		/* enum vm_type type, void *upage, bool writable,
				vm_initializer *init, void *aux */
		if (VM_TYPE(type) == VM_ANON) {
			uninit_new(new_page, upage, init, type, aux, anon_initializer);
		}
		else if (VM_TYPE(type) == VM_FILE) {
			uninit_new(new_page, upage, init, type, aux, file_backed_initializer);
		}
		else{
			uninit_new(new_page, upage, init, type, aux, NULL);
		}
		// TODO: should modify the field after calling the uninit_new.
		new_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
	}
//	return false;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = NULL;
	page = (struct page *)malloc(sizeof(struct page));
	/* TODO: Fill this function. */
	struct hash_elem *e;
	page->va = pg_round_down(va);
	e = hash_find (&spt->pages, &page->hash_elem);
	free(page); //추가!!!!!!!!!!!!!!!!!!!!!!!!!!!
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *e = hash_insert(&spt->pages, &page->hash_elem);
	if (e == NULL) {
		succ = true;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	hash_delete(&spt->pages, &page->hash_elem);
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	for (struct list_elem *e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		victim = list_entry(e, struct frame, frame_elem);

		if (victim->page == NULL)
			return victim;

		if (!pml4_is_accessed(thread_current()->pml4, victim->page->va))
			return victim;
		
		pml4_set_accessed(thread_current()->pml4, victim->page->va, 0);
	}
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim = vm_get_victim();
	//list_remove(&victim->frame_elem);
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	//유저 메모리 풀에서 페이지를 성공적으로 가져오면, 
	//프레임을 할당하고 프레임 구조체의 멤버들을 초기화한 후 해당 프레임을 반환
	frame = malloc(sizeof(struct frame));
	void *kva = palloc_get_page(PAL_USER);

	if(kva != NULL){
		frame->kva = kva;
	}else{
		free(frame);
		frame=vm_evict_frame(); //쫓아냄
		frame->page = NULL;
		return frame;
	}

	list_push_back(&frame_table,&frame->frame_elem);

	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	/* anonymous 페이지를 할당하여 스택 크기를 늘림 */
	vm_alloc_page_with_initializer (VM_ANON,addr, 1, NULL, NULL);
	vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	uintptr_t rsp;
	/* TODO: Validate the fault */
	/* if문으로 not present인지 확인 -> find page*/
	/* TODO: Your code goes here */
	if(is_kernel_vaddr(addr) || addr==NULL ){
		return false;
	}

	if (not_present) {
		rsp = (user == true)? f->rsp : thread_current()->user_rsp;
		if (USER_STACK - USER_STK_LIMIT <= rsp - 8 && rsp - 8 <= addr && addr <= USER_STACK) {
			vm_stack_growth(pg_round_down(addr));
			return true;
		}
		
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;

		if (write == 1 && page->writable == 0)
			return false;

		return vm_do_claim_page(page);
	}
	/*이 함수에서는 Page Fault가 스택을 증가시켜야하는 경우에 해당하는지 아닌지를 확인해야 합니다.
	스택 증가로 Page Fault 예외를 처리할 수 있는지 확인한 경우, 
	Page Fault가 발생한 주소로 vm_stack_growth를 호출합니다.*/
	/* rsp-8 <= addr <= user_stack이면  */

	return false;
}


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
/* 주어진 va에 페이지를 할당하고, 해당 페이지에 프레임을 할당 */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	/* [수정] spt_find_page로 va에 해당하는 페이지가 있는지 찾음 */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) {
		return false;
	}

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct thread *t = thread_current();
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(t->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

unsigned
supplemental_page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

bool
supplemental_page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init (&spt->pages, supplemental_page_hash, supplemental_page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	/* src의 supplemental page table를 반복하면서
	dst의 supplemental page table의 엔트리의 정확한 복사본을 만드세요 */
	/* 해시 테이블의 요소 하나하나에 대해 action() 을 임의의 순서로 호출합니다.  */
	struct hash_iterator spt_iterator;
	hash_first (&spt_iterator, &src->pages);

	while (hash_next (&spt_iterator)) {
		struct page *src_page = hash_entry (hash_cur (&spt_iterator), struct page, hash_elem);
		if (VM_TYPE(src_page->operations->type) == VM_UNINIT) {
			vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, src_page->uninit.aux);
			continue;
		}
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


void destroy_hash_elem(struct hash_elem *e, void *aux) {
	struct page *p = hash_entry(e, struct page, hash_elem);
    destroy(p);
	free(p);
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->pages, destroy_hash_elem);
}