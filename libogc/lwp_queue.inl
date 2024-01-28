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

#ifndef __OGC_LWP_QUEUE_INL__
#define __OGC_LWP_QUEUE_INL__

static __inline__ lwp_node* __lwp_queue_head(lwp_queue *queue)
{
	return (lwp_node*)queue;
}

static __inline__ lwp_node* __lwp_queue_tail(lwp_queue *queue)
{
	return (lwp_node*)&queue->perm_null;
}

static __inline__ u32 __lwp_queue_istail(lwp_queue *queue,lwp_node *node)
{
	return (node==__lwp_queue_tail(queue));
}

static __inline__ u32 __lwp_queue_ishead(lwp_queue *queue,lwp_node *node)
{
	return (node==__lwp_queue_head(queue));
}

static __inline__ lwp_node* __lwp_queue_firstnodeI(lwp_queue *queue)
{
	lwp_node *ret;
	lwp_node *new_first;
#ifdef _LWPQ_DEBUG
	printk("__lwp_queue_firstnodeI(%p)\n",queue);
#endif

	ret = queue->first;
	new_first = ret->next;
	queue->first = new_first;
	new_first->prev = __lwp_queue_head(queue);
	return ret;
}

static __inline__ void __lwp_queue_init_empty(lwp_queue *queue)
{
	queue->first = __lwp_queue_tail(queue);
	queue->perm_null = NULL;
	queue->last = __lwp_queue_head(queue);
}

static __inline__ u32 __lwp_queue_isempty(lwp_queue *queue)
{
	return (queue->first==__lwp_queue_tail(queue));
}

static __inline__ u32 __lwp_queue_onenode(lwp_queue *queue)
{
	return (queue->first==queue->last);
}

static __inline__ void __lwp_queue_appendI(lwp_queue *queue,lwp_node *node)
{
	lwp_node *old;
#ifdef _LWPQ_DEBUG
	printk("__lwp_queue_appendI(%p,%p)\n",queue,node);
#endif
	node->next = __lwp_queue_tail(queue);
	old = queue->last;
	queue->last = node;
	old->next = node;
	node->prev = old;
}

static __inline__ void __lwp_queue_extractI(lwp_node *node)
{
#ifdef _LWPQ_DEBUG
	printk("__lwp_queue_extractI(%p)\n",node);
#endif
	lwp_node *prev,*next;
	next = node->next;
	prev = node->prev;
	next->prev = prev;
	prev->next = next;
}

static __inline__ void __lwp_queue_insertI(lwp_node *after,lwp_node *node)
{
	lwp_node *before;
	
#ifdef _LWPQ_DEBUG
	printk("__lwp_queue_insertI(%p,%p)\n",after,node);
#endif
	node->prev = after;
	before = after->next;
	after->next = node;
	node->next = before;
	before->prev = node;
}

static __inline__ void __lwp_queue_prepend(lwp_queue *queue,lwp_node *node)
{
	__lwp_queue_insert(__lwp_queue_head(queue),node);
}

static __inline__ void __lwp_queue_prependI(lwp_queue *queue,lwp_node *node)
{
	__lwp_queue_insertI(__lwp_queue_head(queue),node);
}

static __inline__ lwp_node* __lwp_queue_getI(lwp_queue *queue)
{
	if(!__lwp_queue_isempty(queue))
		return __lwp_queue_firstnodeI(queue);
	else
		return NULL;
}

#endif
