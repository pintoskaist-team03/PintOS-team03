#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "lib/user/syscall.h"
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <stdio.h>
#include "threads/palloc.h"
#include "lib/kernel/stdio.h"
#include "threads/synch.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);
/*-----------------------------추가함수-----------------------*/
void check_address(void *addr);
void half(void);
void exit(int status);
pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size) ;
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

struct lock filesys_lock;

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uintptr_t *user_stack_pointer = f->rsp;
	check_address(user_stack_pointer); //주소 유효성검사

	uint64_t system_call_number = f->R.rax;
	uint64_t ARG0 = f->R.rdi; //argc
	uint64_t ARG1 = f->R.rsi; //argv[0]
	uint64_t ARG2 = f->R.rdx;
	uint64_t ARG3 = f->R.r10;
	uint64_t ARG4 = f->R.r8;
	uint64_t ARG5 = f->R.r9;

	switch (system_call_number)
	{
	case SYS_HALT:
		halt(); //구현o
		break;
	case SYS_EXIT:
		exit(ARG0); //구현o
		break;
	case SYS_FORK:
		f->R.rax = fork(ARG0);
		break;
	case SYS_EXEC:
		exec(ARG0);
		break;
	case SYS_WAIT:
		wait(ARG0);
		break;						
	case SYS_CREATE: //구현o
		f->R.rax =create(ARG0,ARG1);
		break;		
	case SYS_REMOVE: //구현o
		f->R.rax =remove(ARG0);
		break;	
	case SYS_OPEN:
		f->R.rax = open(ARG0);
		break;	
	case SYS_FILESIZE:
		f->R.rax = filesize(ARG0);
		break;	
	case SYS_READ:
		f->R.rax = read(ARG0,ARG1,ARG2);
		break;	
	case SYS_WRITE:
		f->R.rax = write(ARG0,ARG1,ARG2);
		break;	
	case SYS_SEEK:
		seek(ARG0,ARG1);
		break;		
	case SYS_TELL:
		tell(ARG0);
		break;	
	case SYS_CLOSE:
		close(ARG0);
		break;		
	default:
		exit(-1);
		break;
}
}

//------------------------------------------------
void halt(void){
	power_off();
}

void exit(int status){
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n",curr->name,status); //status: 프로그램이 정상적으로 종료됐는지 확인(정상적 종료 시 '0')
	thread_exit();
}
pid_t fork (const char *thread_name){
	// struct thread *curr = thread_current();
	// return process_fork(thread_name,curr->tf);
}
int exec (const char *file){
/*
현재의 프로세스가 cmd_line에서 이름이 주어지는 실행가능한 프로세스로 변경됩니다. 
이때 주어진 인자들을 전달합니다. 성공적으로 진행된다면 어떤 것도 반환하지 않습니다. 
만약 프로그램이 이 프로세스를 로드하지 못하거나 다른 이유로 돌리지 못하게 되면 
exit state -1을 반환하며 프로세스가 종료됩니다. 
1. filename이 프로세스의 유저영역 메모리에 있는지 확인
2. filename을 저장해줄 페이지 할당받고, 해당 페이지에 filename 넣어줌
3. process_exec()를 실행 해, 현재 실행중인 프로세스를 filename으로 context switching하는 작업을 진행
*/
	check_address(file);
	struct thread *curr = thread_current();

	// 문제점) SYS_EXEC - process_exec의 process_cleanup 때문에 f->R.rdi 날아감.
	// 여기서 file_name 동적할당해서 복사한 뒤, 그걸 넘겨주기
	int siz = strlen(file)+1;
	char *file_copy = palloc_get_page(PAL_ZERO); //근데 이해 안감,,
	strlcpy(file_copy,file,siz);

	if(process_exec(file_copy) == -1)
		return -1;
	
	NOT_REACHED();
	return 0;
}

int wait (pid_t pid){
	return process_wait(pid);
}
bool create(const char *file, unsigned initial_size){
	check_address(file);
	return filesys_create(file,initial_size);
}


bool remove(const char *file){
	check_address(file);
	return filesys_remove(file);
	
}
int open (const char *file){
/* 파일을 open */
/* 해당 파일 객체에 파일 디스크립터 부여 */
/* 파일 디스크립터 리턴 */
/* 해당 파일이 존재하지 않으면 -1 리턴 */
	check_address(file);
	struct file *fileobj = filesys_open(file);

	if(fileobj == NULL)
		return -1;
	int fd = process_add_file(fileobj);

	if(fd == -1) //fd table 꽉참
		file_close(fileobj);
	return fd;
}

int filesize (int fd){
/* 파일 디스크립터를 이용하여 파일 객체 검색 */
  struct file *fileobj= process_get_file(fd);

  if(fileobj == NULL){ /* 해당 파일이 존재하지 않으면 -1 리턴 */
	return -1;
  }
/* 해당 파일의 길이를 리턴 */
	return file_length(fileobj);
}
int read (int fd, void *buffer, unsigned size) {
	/* 파일에 동시 접근이 일어날 수 있으므로 Lock 사용 */
/* 파일 디스크립터를 이용하여 파일 객체 검색 */
/* 파일 디스크립터가 0일 경우 키보드에 입력을 버퍼에 저장 후
버퍼의 저장한 크기를 리턴 (input_getc() 이용) */
/* 파일 디스크립터가 0이 아닐 경우 파일의 데이터를 크기만큼 저
장 후 읽은 바이트 수를 리턴 */
	lock_acquire(&filesys_lock);
	if(fd == 0){
		input_getc();
		lock_release(&filesys_lock);
		return size;
	}
  	struct file *fileobj= process_get_file(fd);
	size = file_read(fileobj,buffer,size);
	lock_release(&filesys_lock);	
	return size;
}

int write (int fd, const void *buffer, unsigned size) {
	/* 파일에 동시 접근이 일어날 수 있으므로 Lock 사용 */
/* 파일 디스크립터를 이용하여 파일 객체 검색 */
/* 파일 디스크립터가 1일 경우 버퍼에 저장된 값을 화면에 출력
후 버퍼의 크기 리턴 (putbuf() 이용) */
/* 파일 디스크립터가 1이 아닐 경우 버퍼에 저장된 데이터를 크기
만큼 파일에 기록후 기록한 바이트 수를 리턴 */
	lock_acquire(&filesys_lock);
	if(fd == 1){
		 putbuf(buffer, size);  //문자열을 화면에 출력해주는 함수
		//putbuf(): 버퍼 안에 들어있는 값 중 사이즈 N만큼을 console로 출력
		lock_release(&filesys_lock);
		return size;
	}
  	struct file *fileobj= process_get_file(fd);
	size = file_write(fileobj,buffer,size);
	lock_release(&filesys_lock);
	return size;
}

void seek (int fd, unsigned position) {
	/* 파일 디스크립터를 이용하여 파일 객체 검색 */
	struct file *fileobj = process_get_file(fd);
	file_seek(fileobj, position);
/* 해당 열린 파일의 위치(offset)를 position만큼 이동 */
}

unsigned tell (int fd) {
	/* 파일 디스크립터를 이용하여 파일 객체 검색 */
	struct file *fileobj = process_get_file(fd);
	file_tell(fileobj);
/* 해당 열린 파일의 위치를 반환 */
}

void close (int fd) {
	/* 해당 파일 디스크립터에 해당하는 파일을 닫음 */
	struct thread *curr = thread_current();
	curr->fdt[fd] = 0; /* 파일 디스크립터 엔트리 초기화 */
}

//-----------------------------------------
void check_address(void *addr){
	struct thread *curr = thread_current();
	if(pml4_get_page(curr->pml4, addr) == NULL || !is_user_vaddr(addr) || addr== NULL){
		//유효하지 않으면 exit(-1) //프로세스 종료
		//포인터가 가리키는 주소가 유저 영역(0x8048000~0xc0000000)의 주소인지 확인
		exit(-1);
	} 
}