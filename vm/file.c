/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "userprog/process.h"
#include "include/threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

struct lock file_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
	lock_init(&file_lock);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct lazy_load_info * arg = (struct lazy_load_info *)page->uninit.aux;
	struct file_page *file_page = &page->file;

	file_page->file = arg->file;
	file_page->file_ofs = arg->ofs;
	file_page->read_bytes = arg->page_read_bytes;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct supplemental_page_table *spt = &thread_current()->spt;

	if(pml4_is_dirty(thread_current()->pml4,page->va)){
		file_write_at(file_page->file,page->va,file_page->read_bytes, file_page->file_ofs);
		pml4_set_dirty(thread_current()->pml4,page->va,0);
	}
	pml4_clear_page(thread_current()->pml4,page->va);

}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	size_t read_bytes = length < file_length(file) ? length:file_length(file);
	size_t zero_bytes = read_bytes%PGSIZE ==0 ? 0 : PGSIZE-(read_bytes%PGSIZE);
	void * start_addr = addr;
	
	thread_current()->page_cnt = zero_bytes==0?(read_bytes/PGSIZE):(read_bytes/PGSIZE)+1;
	//lock_acquire(&file_lock);
	struct file *reopen_file = file_reopen(file); //mmap하는 동안 외부에서 해당 파일을 close()할 경우 예외처리
	//lock_release(&file_lock);

	while (read_bytes>0 || zero_bytes > 0)
	{
		size_t tmp_read_bytes = read_bytes <PGSIZE ? read_bytes:PGSIZE;
		size_t tmp_zero_bytes = PGSIZE - tmp_read_bytes;

		struct lazy_load_info *aux = NULL;
		aux = (struct lazy_load_info*)malloc(sizeof(struct lazy_load_info));
		aux->file = reopen_file;
		aux->ofs = offset;
		aux->page_read_bytes = tmp_read_bytes;
		aux->page_zero_bytes = tmp_zero_bytes;

		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,lazy_load_segment,aux)){
			return NULL;
		}

		read_bytes -= tmp_read_bytes;
		zero_bytes -= tmp_zero_bytes;
		addr += PGSIZE;
		offset += tmp_read_bytes;
	}
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *curr = thread_current();
	struct page *page = spt_find_page(&curr->spt,addr);
	int num_page =  curr->page_cnt;
	while(num_page != 0){
		struct lazy_load_info *tmp_aux = (struct lazy_load_info*)page->uninit.aux;
		
		//spt_remove_page(&curr->spt,page); 

		if(pml4_is_dirty(curr->pml4,addr)){
			file_write_at(tmp_aux->file,addr,tmp_aux->page_read_bytes, tmp_aux->ofs);
			pml4_set_dirty(curr->pml4,addr,0);
		}
		pml4_clear_page(curr->pml4,addr);

		addr += PGSIZE;
		page = spt_find_page(&curr->spt,addr);
		num_page -= 1;
	}
}
