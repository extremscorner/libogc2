#include <stdlib.h>
#include <system.h>
#include <string.h>
#include <asm.h>
#include <processor.h>
#include "system.h"
#include "lwp_wkspace.h"

heap_cntrl __wkspace_heap;
static heap_iblock __wkspace_iblock;
static u32 __wkspace_heap_size = 0;

u32 __lwp_wkspace_heapsize(void)
{
	return __wkspace_heap_size;
}

u32 __lwp_wkspace_heapfree(void)
{
	__lwp_heap_getinfo(&__wkspace_heap,&__wkspace_iblock);
	return __wkspace_iblock.free_size;
}

u32 __lwp_wkspace_heapused(void)
{
	__lwp_heap_getinfo(&__wkspace_heap,&__wkspace_iblock);
	return __wkspace_iblock.used_size;
}

void __lwp_wkspace_init(u32 size)
{
	void *heap_addr = NULL;

	heap_addr = SYS_AllocArena1MemLo(size,PPC_CACHE_ALIGNMENT);
#if defined(HW_RVL)
	if(!heap_addr)
		heap_addr = SYS_AllocArena2MemLo(size,PPC_CACHE_ALIGNMENT);
#endif
	memset(heap_addr,0,size);
	__wkspace_heap_size = __lwp_heap_init(&__wkspace_heap,heap_addr,size,PPC_ALIGNMENT);
}
