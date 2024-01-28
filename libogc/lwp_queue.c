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

#include <stdlib.h>
#include "asm.h"
#include "processor.h"
#include "lwp_queue.h"

void __lwp_queue_initialize(lwp_queue *queue,void *start_addr,u32 num_nodes,u32 node_size)
{
	u32 count;
	lwp_node *curr;
	lwp_node *next;

#ifdef _LWPQ_DEBUG
	printf("__lwp_queue_initialize(%p,%p,%d,%d)\n",queue,start_addr,num_nodes,node_size);
#endif
	count = num_nodes;
	curr = __lwp_queue_head(queue);
	queue->perm_null = NULL;
	next = (lwp_node*)start_addr;
	
	while(count--) {
		curr->next = next;
		next->prev = curr;
		curr = next;
		next = (lwp_node*)(((void*)next)+node_size);
	}
	curr->next = __lwp_queue_tail(queue);
	queue->last = curr;
}

lwp_node* __lwp_queue_get(lwp_queue *queue)
{
	u32 level;
	lwp_node *ret = NULL;
	
	_CPU_ISR_Disable(level);
	if(!__lwp_queue_isempty(queue)) {
		ret	 = __lwp_queue_firstnodeI(queue);
	}
	_CPU_ISR_Restore(level);
	return ret;
}

void __lwp_queue_append(lwp_queue *queue,lwp_node *node)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(queue,node);
	_CPU_ISR_Restore(level);
}

void __lwp_queue_extract(lwp_node *node)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	__lwp_queue_extractI(node);
	_CPU_ISR_Restore(level);
}

void __lwp_queue_insert(lwp_node *after,lwp_node *node)
{
	u32 level;
	
	_CPU_ISR_Disable(level);
	__lwp_queue_insertI(after,node);
	_CPU_ISR_Restore(level);
}
