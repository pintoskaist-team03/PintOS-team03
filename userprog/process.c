#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
void argument_stack(char **argv, int argc, void **rsp);
struct thread *get_child_process(int pid);

/*------project2 추가함수--------*/
struct thread *get_child_process(int pid){
	/* 자식 리스트에 접근하여 프로세스 디스크립터 검색 */
/* 해당 pid가 존재하면 프로세스 디스크립터 반환 */
/* 리스트에 존재하지 않으면 NULL 리턴 */
	struct thread *curr = thread_current();
	struct list *childList = &curr->child_list;

	for(struct list_elem *e = list_begin(childList); e!= list_end(childList); e=list_next(e)){
		struct thread *tmp_t = list_entry(e,struct thread, child_elem);
		if(tmp_t->tid == pid){
			return tmp_t;
		}
	}
	return NULL;
}
struct file *process_get_file (int fd){
	struct thread *curr = thread_current();
	/* 파일 디스크립터에 해당하는 파일 객체를 리턴 */
	if(curr->fdt[fd] != NULL){
		return curr->fdt[fd];
	}
	return NULL;	/* 없을 시 NULL 리턴 */
}
int process_add_file (struct file *f){
/* 파일 객체를 파일 디스크립터 테이블에 추가*/
	// struct thread *curr = thread_current();
	// int fd = curr->next_fd;
	
	// if(fd >64){ //크기 지정 어캐하징.
	// 	return -1;
	// }
	// curr->fdt[fd] = f;
	// curr->next_fd +=1;
	// return curr->next_fd; /* 파일 디스크립터 리턴 */
	//파일 디스크립터가 닫힌 후에도 재사용이 가능하고, 파일 디스크립터의 순서를 유지해야 
	//하는 경우에는 비어있는 자리를 찾는 과정이 필요
	struct thread *curr = thread_current();
  //파일 디스크립터 테이블에서 비어있는 자리를 찾습니다.
	while (curr->next_fd < FDCOUNT_LIMIT  && curr->fdt[curr->next_fd] != NULL) {
		curr->next_fd++;
	}

	// 파일 디스크립터 테이블이 꽉 찬 경우 에러를 반환
	if (curr->next_fd >= FDCOUNT_LIMIT ) {
		return -1;
	}

	curr->fdt[curr->next_fd] = f;
	return curr->next_fd;
}

void process_close_file(int fd){
	struct thread *curr = thread_current();
/* 파일 디스크립터에 해당하는 파일을 닫음 */
	file_close(curr->fdt[fd]);
/* 파일 디스크립터 테이블 해당 엔트리 초기화 */
	curr->fdt[fd] =0;
}


/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0); //'0'페이지 유형 지정 X , 커널 가상 주소 반환
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE); //(1 << PGBITS) PGBITS =12
	//file_name을 fn_copy로 복사

	/*----------추가 코드------------------------*/
	char *save_ptr;
	file_name =strtok_r(file_name," ",&save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	//해당 file_name으로 thread create
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
/*인자로 들어온 if_: 부모의 최신 if임*/

	struct thread *parent = thread_current();
	memcpy(&parent->parent_if,if_,sizeof(struct intr_frame)); //부모 프로세스 메모리를 복사
	tid_t pid = thread_create (name,PRI_DEFAULT, __do_fork, parent); //부모 스레드의 자식리스트로push back
			// 자식프로세스는 생성만 되고 ready큐에 대기중

	if(pid == TID_ERROR){
		return TID_ERROR;
	}
	struct thread *child = get_child_process(pid); 
	sema_down(&child->fork_sema); //부모스레드가 child->fork_semak 다운, 부모스레드는 멈춤->자식스레드 do_fork 실행
	if(child->exit_status == -1){
		return TID_ERROR;
	}
	return pid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. 
 이 함수를 pml4_for_each에 전달하여 부모의 주소 공간을 복제합니다. 이것은 프로젝트 2에만 해당됩니다.
 */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	/*pte(pagetable entry) 복제, 해당 entry 가리키는 va의 페이지를 부모스레드의
	페이지 테이블에서 가져와 자식 스레드의 페이지 테이블에 새로운 페이지로 추가*/
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if(is_kernel_vaddr(va)){ //부모가 kernel page이면 반환
		return true;
	}

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);
	//부모의 pml4에서 'va'에 해당하는 페이지 가져옴
	if(parent_page == NULL){
		return false;
	}

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page (PAL_USER); //새로운 user영역 페이지 할당받음
	if(newpage == NULL){
		return false;
	}

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result).
	 * 부모 페이지를 복사해 3에서 새로 할당받은 페이지에 넣어준다. 
	 * 이때 부모 페이지가 writable인지 아닌지 확인하기 위해 is_writable() 함수를 이용한다. */
	memcpy(newpage,parent_page,PGSIZE); //PGSIZE 1<<12
	writable = is_writable(pte); //주어진 페이지 테이블 엔트리(pte)가 쓰기 가능한지를 확인하는 매크로

	/* 5. Add new page to  child's page table at address VA with WRITABLE
	 *    permission. */
	// 자식 스레드의 페이지 테이블에 새로운 페이지를 추가
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. 
 * 부모 스레드의 복사복 만듦
 * */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current (); //자식 프로세스임
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if;
	parent_if=&parent->parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	//부모 스레드의 인터럽트 프레임을 자식 스레드로 가져오는 역할, 메모리 영역의 데이터를 복사
	memcpy (&if_, parent_if, sizeof (struct intr_frame));//현재 실행 중인 프로세스의 CPU 컨텍스트 정보를 복제하는 과정
	if_.R.rax = 0; // ?????????????

	/* 2. Duplicate PT : page table 복제 */
	current->pml4 = pml4_create(); // 자식 스레드는 독립적인 가상 주소 공간을 가지게 됨
	if (current->pml4 == NULL)
		goto error;

	process_activate (current); //자식 스레드 활성화
	/*???다시 찾기???? 자식 스레드의 페이지 테이블을 적용하고, 
	CR3 레지스터를 업데이트하여 가상 주소 변환에 사용되는 페이지 테이블을 설정하는 작업을 수행*/
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
	//부모 스레드의 페이지 테이블을 순회하고, 각 페이지 테이블 엔트리에 대해 duplicate_pte 함수를 적용하는 것을 의미
	//duplicate_pte()는 각 pte를 복제해 자식 프로세스의 페이지테이블의 새로운 엔트리로 추가
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.
	 * 힌트) 파일 객체를 복제하려면 include/filesys/file.h에서 `file_duplicate`를 사용합니다. 
	 * 함수가 부모의 리소스를 성공적으로 복제할 때까지 부모는 fork()에서 반환하지 않아야 합니다.
	 * */
	if(parent->next_fd == FDCOUNT_LIMIT){
		goto error;
	}
/*부모 프로세스의 파일 디스크립터 테이블(parent->fdt)을 순회하면서 
각 파일 객체를 복제하여 자식 프로세스의 파일 디스크립터 테이블(current->fdt)에 복사하는 역할*/
	for(int i = 0; i<FDCOUNT_LIMIT; i++){
		struct file *file = parent->fdt[i];
		if(file == NULL)
			continue;
		if(true){
			struct file *newfile;
			if(file>2) 
				newfile = file_duplicate(file); //file_duplicate 함수를 사용하여 부모 스레드의 파일 객체를 복제하여 자식 스레드에게 전달
			else
				newfile = file; //0, 1, 2는 표준 입력, 표준 출력, 표준 오류로 사용되는 파일 디스크립터이므로, 이들은 복제하지 않고 그대로 사용
			current->fdt[i] = newfile;
		}
	}
	current->next_fd = parent->next_fd;
	// child loaded successfully, wake up parent in process_fork
	sema_up(&current->fork_sema); //복제가 완료되면 sema_up으로 부모프로세스 깨움, 부모프로세스 ready큐 대기중
	// process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	current->exit_status = TID_ERROR;
	sema_up(&current->fork_sema);
	exit(-1);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */

int
process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;
	char *token, *save_ptr;
	char *argv[128];
	int argc;
	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	argc = 0;
	for(token = strtok_r(file_name," ", &save_ptr); token != NULL;){
		argv[argc]= token;
		token = strtok_r (NULL, " ", &save_ptr);
		argc = argc+1;
	}

	file_name = argv[0];
	/* We first kill the current context */
	process_cleanup ();

	/* And then load the binary */
	success = load (file_name, &_if);

	void **rspp = &_if.rsp;
	argument_stack(argv, argc, &_if.rsp);
	_if.R.rdi = argc;
	_if.R.rsi = *rspp + sizeof(void *);

	//hex_dump(_if.rsp, _if.rsp, USER_STACK - (uint64_t)_if.rsp, true);
	/* If load failed, quit. */
	if (!success){
		palloc_free_page (file_name);
		return -1;
	}

	palloc_free_page (file_name);

	/* Start switched process. */
	do_iret (&_if);
	NOT_REACHED ();
}

void argument_stack(char **argv, int argc, void **rsp){ 
//함수 호출 규약에 따라 유저 스택에 프로그램 이름과 인자들을 저장

	for(int i = argc-1; i >=0; i--){
		*rsp = *rsp - (strlen(argv[i])+1);//'\0'포함, rsp(스택포인터이동)
		memcpy(*rsp,argv[i], strlen(argv[i])+1);// arg 스택에 복사
		argv[i] = (char *)*rsp;
	}

	if((uintptr_t)*rsp % 8 != 0){
		uintptr_t padding = (uintptr_t)*rsp % 8; //uintptr_t padding = 8-(uintptr_t)*rsp % 8;
		*rsp = *rsp-padding;
		memset(*rsp,0,padding);
	}

	// null 넣기
	*rsp = (void*)((uintptr_t)*rsp - 8);
	*(char **)*rsp = 0;

	for(int i = argc-1; i>=0; i--){
		*rsp = (void*)((uintptr_t)*rsp - 8);
		*(char **)*rsp = argv[i];
	}

	//return address
	*rsp = (void*)((uintptr_t)*rsp - 8);
	**(char **)rsp = 0; //**(char **)rsp = 0;
} 

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	struct thread *child = get_child_process(child_tid);
	if(child == NULL){
		return -1;
	}

	sema_down(&child->wait_sema); // 부모 프로세스가 자식 프로세스의 실행을 기다리도록 하는 동기화 작업.부모 프로세스의 실행을 일시 정지
	list_remove(&child->child_elem);
	/*주어진 원소(child_elem)를 연결 리스트에서 제거하는 작업을 수행
	 부모 프로세스는 자식 프로세스의 목록을 유지하며, 자식 프로세스가 종료되었을 때 이 목록에서 해당 자식 프로세스를 제거해야함
	=> 부모 프로세스는 정확하고 유효한 자식 목록을 유지할 수 있음
	*/
	sema_up(&child->free_sema);
	//자식 프로세스가 종료되었음을 부모 프로세스에게 알리고, 자원의 안전한 해제를 보장
	//list remove한 뒤에 sema_up으로 자식 프로세스 종료

	return child->exit_status; 
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */ 
	//palloc_free_page(curr->fdt); // 메모리 누수
	palloc_free_multiple(curr->fdt,FDT_PAGES);

	file_close(curr->running);

	sema_up(&curr->wait_sema);
	sema_down(&curr->free_sema); //현재 자식 스레드가 free_sema의 waiter에 대기

	process_cleanup ();
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. 
next? 스레드에서 사용자 코드를 실행하기 위한 CPU를 설정합니다.
 이 함수는 컨텍스트가 전환될 때마다 호출됩니다.
 */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. 
 *  주어진 실행 파일을 메모리에 로드하고, 프로세스의 페이지 디렉터리를 활성화하며, 
 * 스택을 설정하고, 실행 파일의 시작 주소를 설정하는 역할 */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}
	file_deny_write(file);
	t->running = file;
	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry; //ELF 헤더의 시작주소

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	//file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
