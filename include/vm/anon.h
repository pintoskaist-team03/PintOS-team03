#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    int swap_index;//swap된 데이터들이 저장된 섹터 구역
    //익명 페이지가 스왑 영역에 저장된 위치를 식별하는 데 사용
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
