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

#ifndef __OGC_LWP_HEAP_INL__
#define __OGC_LWP_HEAP_INL__

static __inline__ heap_block* __lwp_heap_head(heap_cntrl *theheap)
{
	return (heap_block*)&theheap->start;
}

static __inline__ heap_block* __lwp_heap_tail(heap_cntrl *heap)
{
	return (heap_block*)&heap->final;
}

static __inline__ heap_block* __lwp_heap_prevblock(heap_block *block)
{
	return (heap_block*)((char*)block - (block->back_flag&~HEAP_BLOCK_USED));
}

static __inline__ heap_block* __lwp_heap_nextblock(heap_block *block)
{
	return (heap_block*)((char*)block + (block->front_flag&~HEAP_BLOCK_USED));
}

static __inline__ heap_block* __lwp_heap_blockat(heap_block *block,u32 offset)
{
	return (heap_block*)((char*)block + offset);
}

static __inline__ heap_block* __lwp_heap_usrblockat(void *ptr)
{
	u32 offset = *(((u32*)ptr)-1);
	return __lwp_heap_blockat(ptr,-offset+-HEAP_BLOCK_USED_OVERHEAD);
}

static __inline__ bool __lwp_heap_prev_blockfree(heap_block *block)
{
	return !(block->back_flag&HEAP_BLOCK_USED);
}

static __inline__ bool __lwp_heap_blockfree(heap_block *block)
{
	return !(block->front_flag&HEAP_BLOCK_USED);
}

static __inline__ bool __lwp_heap_blockused(heap_block *block)
{
	return (block->front_flag&HEAP_BLOCK_USED);
}

static __inline__ u32 __lwp_heap_blocksize(heap_block *block)
{
	return (block->front_flag&~HEAP_BLOCK_USED);
}

static __inline__ void* __lwp_heap_startuser(heap_block *block)
{
	return (void*)&block->next;
}

static __inline__ bool __lwp_heap_blockin(heap_cntrl *heap,heap_block *block)
{
	return ((u32)block>=(u32)heap->start && (u32)block<=(u32)heap->final);
}

static __inline__ bool __lwp_heap_pgsize_valid(u32 pgsize)
{
	return (pgsize!=0 && ((pgsize%PPC_ALIGNMENT)==0));
}

static __inline__ u32 __lwp_heap_buildflag(u32 size,u32 flag)
{
	return (size|flag);
}

#endif
