#ifndef THREADS_VADDR_H
#define THREADS_VADDR_H

#include <debug.h>
#include <stdint.h>
#include <stdbool.h>

#include "threads/loader.h"

/* Functions and macros for working with virtual addresses.
 *
 * See pte.h for functions and macros specifically for x86
 * hardware page tables. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). 페이지 오프셋” 부분의 비트들에 1, 나머지는 0으로 설정되어 있는 비트마스킹 매크로 */

/* Offset within a page. */
#define pg_ofs(va) ((uint64_t) (va) & PGMASK) //가상 주소 va 의 페이지 오프셋을 뽑아내 반환

#define pg_no(va) ((uint64_t) (va) >> PGBITS) //가상 주소 va 의 페이지 번호를 뽑아내 반환

/* Round up to nearest page boundary. */
#define pg_round_up(va) ((void *) (((uint64_t) (va) + PGSIZE - 1) & ~PGMASK)) //가장 근접한 페이지 경계 값으로 올림된 va를 반환

/* Round down to nearest page boundary. */
#define pg_round_down(va) (void *) ((uint64_t) (va) & ~PGMASK) //내부에서 va가 가리키는 가상 페이지의 시작 (페이지 오프셋이 0으로 설정된 va)을 반환

/* Kernel virtual address start */
#define KERN_BASE LOADER_KERN_BASE //유저 가상 메모리와 커널 가상 메모리의 경계

/* User stack start */
#define USER_STACK 0x47480000

/* Returns true if VADDR is a user virtual address. */
#define is_user_vaddr(vaddr) (!is_kernel_vaddr((vaddr)))

/* Returns true if VADDR is a kernel virtual address. */
#define is_kernel_vaddr(vaddr) ((uint64_t)(vaddr) >= KERN_BASE)

// FIXME: add checking
/* Returns kernel virtual address at which physical address PADDR
 *  is mapped. 
 0 ~ (물리 주소의 크기) 내에 존재하는 물리 주소(pa)와 대응되는 커널 가상 주소를 반환*/
#define ptov(paddr) ((void *) (((uint64_t) paddr) + KERN_BASE))

/* Returns physical address at which kernel virtual address VADDR
 * is mapped.
 커널 가상 주소(va)와 대응되는 물리 주소를 반환 */
#define vtop(vaddr) \
({ \
	ASSERT(is_kernel_vaddr(vaddr)); \
	((uint64_t) (vaddr) - (uint64_t) KERN_BASE);\
})

#endif /* threads/vaddr.h */
