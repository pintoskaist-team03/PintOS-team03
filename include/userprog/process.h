#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#define vm

#include "threads/thread.h"
#include "filesys/file.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
struct file *process_get_file (int fd);
int process_add_file (struct file *f);
void process_close_file(int fd);

bool lazy_load_segment (struct page *page, void *aux);

struct lazy_load_info{
	struct file *file;
	uint32_t  page_read_bytes;
	uint32_t  page_zero_bytes;
	off_t ofs;
};
#endif /* userprog/process.h */
