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
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <stdio.h>
#include "threads/palloc.h"
#include "lib/kernel/stdio.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "vm/vm.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/fat.h"



void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void check_address(void *addr);
void halt(void);
void exit(int status);
tid_t fork (const char *thread_name,struct intr_frame *f);
int exec (const char *file);
int wait (tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size) ;
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/*project3 추가*/
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);

/*project4 추가*/
bool chdir(const char *dir);
bool mkdir (const char *dir);
bool readdir(int fd, char *name);
bool isdir(int fd);
struct cluster_t *inumber(int fd);
int symlink(const char* target, const char* linkpath);

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
	/*p3 - Growth stack*/
	thread_current()->user_rsp = f->rsp;

    switch (f->R.rax) // rax는 system call number이다.
    {
	case SYS_HALT:
        halt();
        break;
    case SYS_EXIT:
        exit(f->R.rdi); //실행할 때 첫번째 인자가 R.rdi에 저장됨
        break;
    case SYS_FORK:
        f->R.rax = fork(f->R.rdi, f);
        break;
    case SYS_EXEC:
        f->R.rax = exec(f->R.rdi);
        break;
    case SYS_WAIT:
        f->R.rax = process_wait(f->R.rdi);
        break;		
    case SYS_CREATE:
        f->R.rax = create(f->R.rdi, f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = remove(f->R.rdi);
        break;		
    case SYS_OPEN:
        f->R.rax = open(f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK:
        seek(f->R.rdi, f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell(f->R.rdi);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap (f->R.rdi);
		break;
	case SYS_CHDIR:
		f->R.rax = chdir(f->R.rdi);
		break;
	case SYS_MKDIR:
		f->R.rax = mkdir(f->R.rdi);
		break;
	case SYS_READDIR:
		f->R.rax = readdir(f->R.rdi, f->R.rsi);
		break;
	case SYS_ISDIR:
		f->R.rax = isdir(f->R.rax);
		break;
	case SYS_INUMBER:
		f->R.rax = inumber(f->R.rax);
		break;
	case SYS_SYMLINK:
		f->R.rax = symlink(f->R.rdi, f->R.rsi);
		break;
    // default:
    //     exit(-1);
    //     break;
    }
}
void halt(void)
{
    power_off();
}
// userprog/syscall.c
void exit(int status)
{
    struct thread *cur = thread_current();
    cur->exit_status = status;                         // 종료시 상태를 확인, 정상종료면 state = 0
    printf("%s: exit(%d)\n", thread_name(), status); // 종료 메시지 출력
    thread_exit();                                     // thread 종료
}
tid_t fork (const char *thread_name,struct intr_frame *f){
	return process_fork(thread_name,f);
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
	// 문제점) SYS_EXEC - process_exec의 process_cleanup 때문에 f->R.rdi 날아감.
	// 여기서 file_name 동적할당해서 복사한 뒤, 그걸 넘겨주기
	int siz = strlen(file)+1;
	char *file_copy = palloc_get_page(PAL_ZERO); //근데 이해 안감,,
	strlcpy(file_copy,file,siz);

	if(process_exec(file_copy) == -1)
		return -1;
	
	NOT_REACHED();
}
int wait (tid_t pid){
	return process_wait(pid);
}
bool create(const char *file, unsigned initial_size){
	check_address(file);
	lock_acquire(&filesys_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return success;
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
	lock_acquire(&filesys_lock);
	struct file *fileobj = filesys_open(file);
	if(fileobj == NULL){
		lock_release(&filesys_lock);
		return -1;
	}
	int fd = process_add_file(fileobj);
	if(fd == -1) //fd table 꽉참
		file_close(fileobj);
	lock_release(&filesys_lock);
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
	check_address(buffer);

	lock_acquire(&filesys_lock);
	if(fd == 1){
		lock_release(&filesys_lock);
		return -1;
	}

	if(fd == 0){
		input_getc();
		lock_release(&filesys_lock);
		return size;
	}
  	struct file *fileobj= process_get_file(fd);
	if(fileobj){
		struct page *page = spt_find_page(&thread_current()->spt,buffer);
		if(page != NULL && page->writable == 0){
			lock_release(&filesys_lock);
			exit(-1);
		}
	}
		
	size = file_read(fileobj,buffer,size);
	lock_release(&filesys_lock);	
	return size;
}

int write (int fd, const void *buffer, unsigned size) {

	check_address(buffer);

	lock_acquire(&filesys_lock);
	if(fd == 1){
		 putbuf(buffer, size);  //문자열을 화면에 출력해주는 함수
		//putbuf(): 버퍼 안에 들어있는 값 중 사이즈 N만큼을 console로 출력
		lock_release(&filesys_lock);
		return size;
	}
	struct file *fileobj= process_get_file(fd);
	if(fileobj == NULL){
		lock_release(&filesys_lock);
		return -1;
	}
	
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
	struct file *fileobj= process_get_file(fd);
	if(fileobj == NULL)
		return;
	//file_close(fd);
	process_close_file(fd);/* 파일 디스크립터 엔트리 초기화 */
}


void *mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	// struct file *file = process_get_file(fd);
	// if (file == NULL)
	// 	return NULL;
	// if (file_length(file) == 0)
	// 	return NULL;
	// if (!length || (int)length < 0)
	// 	return NULL;
	// if (addr == 0)
	// 	return NULL;
	// if (fd < 2)
	// 	return NULL;
	// if (offset % PGSIZE)
	// 	return NULL;
	// if (is_kernel_vaddr(addr))
	// 	return NULL;
	// if (addr != pg_round_down(addr))
	// 	return NULL;
	// if (spt_find_page(&thread_current()->spt, addr))
	// 	return NULL;
	if(offset % PGSIZE != 0){
		return NULL;
	}
	if(fd == 0 || fd == 1){
		exit(-1);
	}
	struct file *fileobj = process_get_file(fd);

	if(fileobj == NULL){
		return NULL;
	}
	if(length == 0 || KERN_BASE <= length || addr == NULL || is_kernel_vaddr(addr) || pg_round_down(addr) != addr){
		return NULL;
	}
	if (spt_find_page(&thread_current()->spt, addr)) {
        return NULL;
	}
	return do_mmap(addr, length, writable, fileobj, offset);
}
void munmap (void *addr){
	do_munmap(addr);
}

bool chdir(const char *dir){
	if(dir == NULL)
		return false;

	//dir의 파일경로를 cp_dir에 복사
	char *cp_dir = (char*)malloc(strlen(dir)+1);
	strlcpy(cp_dir,dir,strlen(dir)+1);

	struct dir *chdir = NULL;

	if(cp_dir[0] == '/'){ //절대 경로로 디렉토리 되어 있다면
		chdir = dir_open_root();
	}
	else{ //상대 경로로 디렉토리 되어 있다면
		chdir = dir_reopen(thread_current()->cur_dir);
	}

	//chdir경로를 분석하여 디렉토리를 반환
	char *token, *savePtr;
	token = strtok_r(cp_dir,"/",&savePtr);

	struct inode *inode = NULL;
	while (token != NULL)
	{
		//chdir에서 token이름의 파일을 검색해 inode의 정보 저장
		if(!dir_lookup(chdir,token,&inode)){
			dir_close(chdir);
			return false;
		}

		//inode가 파일일 경우 null 반환
		if(!inode_is_dir(inode)){
			dir_close(chdir);
			return false;
		}

		//chdir의 디렉토리 정보를 메모리에서 해지
		dir_close(chdir);

		//inode의 디렉토리 정보를 dir에 저장
		chdir = dir_open(inode);

		//token에 검색할 경로 이름 저장
		token = strtok_r(NULL,"/",&savePtr);
	}
	//스레드의 현재 작업 디레톡리를 변경
	dir_close(thread_current()->cur_dir);
	thread_current()->cur_dir = chdir;
	free(cp_dir);
	return true;
}
//디렉토리 생성, 기존에 존재하는 이름은 생성X
bool mkdir (const char *dir){
	lock_acquire(&filesys_lock); // 이거 맞는지....?
	bool new_dir = filesys_create_dir(dir); // filesys_create_dir() 함수 없는뎅
	lock_release(&filesys_lock);
	return new_dir;
}

//디렉토리 내 파일 존재 여부 확인
bool readdir(int fd, char *name){	
	if(name == NULL)
		return false;
	
	struct file *target = process_get_file(fd);
	if(target == NULL)
		return false;
	
	//fd의 file->inode가 디렉토리인지 검사
	if(!inode_is_dir(file_get_inode(target)))
		return false;
	
	//p_file을 dir 자료구조로 포인팅
    struct dir *p_file = target;
    if (p_file->pos == 0) {
        dir_seek(p_file, 2 * sizeof(struct dir_entry));		// ".", ".." 제외
	}

	// 디렉터리의 엔트리에서 ".", ".." 이름을 제외한 파일이름을 name에 저장
	bool result = dir_readdir(p_file,name);

	return result;
}

// file의 directory 여부 판단
bool isdir(int fd){
	struct file *target = process_get_file(fd);
	if(target == NULL)
		return false;
	return inode_is_dir(file_get_inode(target));
}

//file의 inode가 기록된 sector 찾기
struct cluster_t *inumber(int fd){
	struct file *target = process_get_file(fd);

	if(target == NULL)
		return false;
	return inode_get_inumber(file_get_inode(target));
}

//바로가기 file생성
int symlink(const char* target, const char* linkpath){
	//SOFT LINK
	bool success = false;
	char *cp_link = (char *)malloc(strlen(linkpath) +1);
	strlcpy(cp_link, linkpath, strlen(linkpath)+1);

	//cp_name의 경로분석
	char* file_link = (char * )malloc(strlen(cp_link) +1);
	struct dir * dir = parse_path(cp_link, file_link);

	cluster_t inode_cluster = fat_create_chain(0);

	//link file 전용 inode 생성 및 directory 에 추가
	success = (dir != NULL
			&& link_inode_create(inode_cluster, target)
			&& dir_add(dir, file_link, inode_cluster)
		);
	if(!success && inode_cluster != 0){
		fat_remove_chain(inode_cluster,0);
	}
	dir_close(dir);
	free(cp_link);
	free(file_link);

	return success -1;
}


void check_address(void *addr){
	struct thread *curr = thread_current();
	if(addr== NULL || !is_user_vaddr(addr)){
		exit(-1);
	} 
}