/*
 * Copyright (C) 1989-1999 On-Line Applications Research Corporation (OAR).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __OGC_LWP_HEAP_H__
#define __OGC_LWP_HEAP_H__

#include <gctypes.h>
#include "machine/asm.h"

#define HEAP_BLOCK_USED					1
#define HEAP_BLOCK_FREE					0

#define HEAP_DUMMY_FLAG					(0+HEAP_BLOCK_USED)

#define HEAP_OVERHEAD					(sizeof(u32)*2)
#define HEAP_BLOCK_USED_OVERHEAD		(sizeof(void*)*2)
#define HEAP_MIN_SIZE					(HEAP_OVERHEAD+sizeof(heap_block))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _heap_block_st heap_block;
struct _heap_block_st {
	u32 back_flag;
	u32 front_flag;
	heap_block *next;
	heap_block *prev;
};

typedef struct _heap_iblock_st {
	u32 free_blocks;
	u32 free_size;
	u32 used_blocks;
	u32 used_size;
} heap_iblock;

typedef struct _heap_cntrl_st {
	heap_block *start;
	heap_block *final;

	heap_block *first;
	heap_block *perm_null;
	heap_block *last;
	u32 pg_size;
	u32 reserved;
} heap_cntrl;

u32 __lwp_heap_init(heap_cntrl *theheap,void *start_addr,u32 size,u32 pg_size);
void* __lwp_heap_allocate(heap_cntrl *theheap,u32 size);
BOOL __lwp_heap_free(heap_cntrl *theheap,void *ptr);
u32 __lwp_heap_getinfo(heap_cntrl *theheap,heap_iblock *theinfo);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_heap.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
