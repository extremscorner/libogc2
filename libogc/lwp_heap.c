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

/*-------------------------------------------------------------

Copyright (C) 2004 - 2025
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Extrems' Corner.org

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stdlib.h>
#include <system.h>
#include <processor.h>
#include <sys_state.h>
#include <lwp_config.h>

#include "lwp_heap.h"

u32 __lwp_heap_init(heap_cntrl *theheap,void *start_addr,u32 size,u32 pg_size)
{
	u32 dsize,level;
	heap_block *block;

	if(!__lwp_heap_pgsize_valid(pg_size) || size<HEAP_MIN_SIZE) return 0;

	_CPU_ISR_Disable(level);
	theheap->pg_size = pg_size;
	dsize = (size - HEAP_OVERHEAD);
	
	block = (heap_block*)start_addr;
	block->back_flag = HEAP_DUMMY_FLAG;
	block->front_flag = dsize;
	block->next	= __lwp_heap_tail(theheap);
	block->prev = __lwp_heap_head(theheap);
	
	theheap->start = block;
	theheap->first = block;
	theheap->perm_null = NULL;
	theheap->last = block;
	
	block = __lwp_heap_nextblock(block);
	block->back_flag = dsize;
	block->front_flag = HEAP_DUMMY_FLAG;
	theheap->final = block;
	_CPU_ISR_Restore(level);
	
	return (dsize - HEAP_BLOCK_USED_OVERHEAD);
}

void* __lwp_heap_allocate(heap_cntrl *theheap,u32 size)
{
	u32 excess;
	u32 dsize;
	heap_block *block;
	heap_block *next_block;
	heap_block *tmp_block;
	void *ptr;
	u32 offset,level;


	if(size>=(-1-HEAP_BLOCK_USED_OVERHEAD)) return NULL;

	_CPU_ISR_Disable(level);
	excess = (size % theheap->pg_size);
	dsize = (size + theheap->pg_size + HEAP_BLOCK_USED_OVERHEAD);
	
	if(excess)
		dsize += (theheap->pg_size - excess);

	if(dsize<sizeof(heap_block)) dsize = sizeof(heap_block);
	
	for(block=theheap->first;;block=block->next) {
		if(block==__lwp_heap_tail(theheap)) {
			_CPU_ISR_Restore(level);
			return NULL;
		}
		if(block->front_flag>=dsize) break;
	}
	
	if((block->front_flag-dsize)>(theheap->pg_size+HEAP_BLOCK_USED_OVERHEAD)) {
		block->front_flag -= dsize;
		next_block = __lwp_heap_nextblock(block);
		next_block->back_flag = block->front_flag;
		
		tmp_block = __lwp_heap_blockat(next_block,dsize);
		tmp_block->back_flag = next_block->front_flag = __lwp_heap_buildflag(dsize,HEAP_BLOCK_USED);

		ptr = __lwp_heap_startuser(next_block);
	} else {
		next_block = __lwp_heap_nextblock(block);
		next_block->back_flag = __lwp_heap_buildflag(block->front_flag,HEAP_BLOCK_USED);
		
		block->front_flag = next_block->back_flag;
		block->next->prev = block->prev;
		block->prev->next = block->next;
		
		ptr = __lwp_heap_startuser(block);
	}

	offset = (theheap->pg_size - ((u32)ptr&(theheap->pg_size-1)));
	ptr += offset;
	*(((u32*)ptr)-1) = offset;
	_CPU_ISR_Restore(level);

	return ptr;
}

BOOL __lwp_heap_free(heap_cntrl *theheap,void *ptr)
{
	heap_block *block;
	heap_block *next_block;
	heap_block *new_next;
	heap_block *prev_block;
	heap_block *tmp_block;
	u32 dsize,level;

	_CPU_ISR_Disable(level);

	block = __lwp_heap_usrblockat(ptr);
	if(!__lwp_heap_blockin(theheap,block) || __lwp_heap_blockfree(block)) {
		_CPU_ISR_Restore(level);
		return FALSE;
	}

	dsize = __lwp_heap_blocksize(block);
	next_block = __lwp_heap_blockat(block,dsize);
	
	if(!__lwp_heap_blockin(theheap,next_block) || (block->front_flag!=next_block->back_flag)) {
		_CPU_ISR_Restore(level);
		return FALSE;
	}
	
	if(__lwp_heap_prev_blockfree(block)) {
		prev_block = __lwp_heap_prevblock(block);
		if(!__lwp_heap_blockin(theheap,prev_block)) {
			_CPU_ISR_Restore(level);
			return FALSE;
		}
		
		if(__lwp_heap_blockfree(next_block)) {
			prev_block->front_flag += next_block->front_flag+dsize;
			tmp_block = __lwp_heap_nextblock(prev_block);
			tmp_block->back_flag = prev_block->front_flag;
			next_block->next->prev = next_block->prev;
			next_block->prev->next = next_block->next;
		} else {
			prev_block->front_flag = next_block->back_flag = prev_block->front_flag+dsize;
		}
	} else if(__lwp_heap_blockfree(next_block)) {
		block->front_flag = dsize+next_block->front_flag;
		new_next = __lwp_heap_nextblock(block);
		new_next->back_flag = block->front_flag;
		block->next = next_block->next;
		block->prev = next_block->prev;
		next_block->prev->next = block;
		next_block->next->prev = block;
		
		if(theheap->first==next_block) theheap->first = block;
	} else {
		next_block->back_flag = block->front_flag = dsize;
		block->prev = __lwp_heap_head(theheap);
		block->next = theheap->first;
		theheap->first = block;
		block->next->prev = block;
	}
	_CPU_ISR_Restore(level);

	return TRUE;
}

u32 __lwp_heap_getinfo(heap_cntrl *theheap,heap_iblock *theinfo)
{
	u32 not_done = 1;
	heap_block *theblock = NULL;
	heap_block *nextblock = NULL;
	
	theinfo->free_blocks = 0;
	theinfo->free_size = 0;
	theinfo->used_blocks = 0;
	theinfo->used_size = 0;
	
	if(!__sys_state_up(__sys_state_get())) return 1;

	theblock = theheap->start;
	if(theblock->back_flag!=HEAP_DUMMY_FLAG) return 2;

	while(not_done) {
		if(__lwp_heap_blockfree(theblock)) {
			theinfo->free_blocks++;
			theinfo->free_size += __lwp_heap_blocksize(theblock);
		} else {
			theinfo->used_blocks++;
			theinfo->used_size += __lwp_heap_blocksize(theblock);
		}

		if(theblock->front_flag!=HEAP_DUMMY_FLAG) {
			nextblock = __lwp_heap_nextblock(theblock);
			if(theblock->front_flag!=nextblock->back_flag) return 2;
		}

		if(theblock->front_flag==HEAP_DUMMY_FLAG) 
			not_done = 0;
		else
			theblock = nextblock;
	}
	return 0;
}
