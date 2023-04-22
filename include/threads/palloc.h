#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stdint.h>
#include <stddef.h>

/* How to allocate pages. */
enum palloc_flags {
	PAL_ASSERT = 001,           /* PAL_ASSERT: 할당 실패 시 panic을 발생시 Panic on failure. */
	PAL_ZERO = 002,             /* Zero page contents.할당된 페이지의 내용을 모두 0으로 초기화 */
	PAL_USER = 004              /* User page.유저 영역에서 페이지를 할당 */
};

/* Maximum number of pages to put in user pool. */
extern size_t user_page_limit;

uint64_t palloc_init (void);
void *palloc_get_page (enum palloc_flags);
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);

#endif /* threads/palloc.h */
