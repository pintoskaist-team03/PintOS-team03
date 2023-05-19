/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "userprog/process.h"
#include "include/threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
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
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	size_t read_bytes = length < file_length(file) ? length:file_length;
	size_t zero_bytes = PGSIZE-(read_bytes%PGSIZE);

	thread_current()->page_cnt = zero_bytes==0?(read_bytes/PGSIZE):(read_bytes/PGSIZE+1);

	void *start_addr = addr;

	struct file *reopen_file = file_reopen(file); //mmap하는 동안 외부에서 해당 파일을 close()할 경우 예외처리

	while (read_bytes>0 || zero_bytes > 0)
	{
		size_t tmp_read_bytes = read_bytes <PGSIZE ? read_bytes:PGSIZE;

		struct lazy_load_info *aux = NULL;
		aux = (struct lazy_load_info*)malloc(sizeof(struct lazy_load_info));
		aux->file = reopen_file;
		aux->ofs = offset;
		aux->page_read_bytes = read_bytes;
		if(read_bytes - tmp_read_bytes < 0){
			aux->page_zero_bytes = zero_bytes;
		}

		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,lazy_load_segment,aux)){
			return NULL;
		}

		read_bytes -= tmp_read_bytes;
		addr += PGSIZE;
		offset += tmp_read_bytes;
		if(read_bytes - tmp_read_bytes < 0){
			zero_bytes -= zero_bytes;
		}

	}
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *curr = thread_current();
	struct page *page = spt_find_page(&curr->spt,addr);

	int num_page =  curr->page_cnt;

	while(num_page == 0){
		
		struct lazy_load_info *tmp_aux = (struct lazy_load_info*)page->uninit.aux;
		
		if(pml4_is_dirty(&curr->pml4,addr)){
			file_write_at(tmp_aux->file,addr,tmp_aux->page_read_bytes, tmp_aux->ofs);
			pml4_set_dirty(&curr->pml4,addr,0);
		}
		pml4_clear_page(&curr->pml4,addr);
		addr -= PGSIZE;
		page = spt_find_page(&curr->spt,addr);
		num_page -= 1;
	}
}
