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

#ifndef __OGC_LWP_QUEUE_H__
#define __OGC_LWP_QUEUE_H__

#include <gctypes.h>

//#define _LWPQ_DEBUG

#ifdef _LWPQ_DEBUG
extern int printk(const char *fmt,...);
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwpnode {
	struct _lwpnode *next;
	struct _lwpnode *prev;
} lwp_node;

typedef struct _lwpqueue {
	lwp_node *first;
	lwp_node *perm_null;
	lwp_node *last;
} lwp_queue;

void __lwp_queue_initialize(lwp_queue *,void *,u32,u32);
lwp_node* __lwp_queue_get(lwp_queue *);
void __lwp_queue_append(lwp_queue *,lwp_node *);
void __lwp_queue_extract(lwp_node *);
void __lwp_queue_insert(lwp_node *,lwp_node *);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_queue.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
