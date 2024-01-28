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

#ifndef __OGC_LWP_THREADQ_H__
#define __OGC_LWP_THREADQ_H__

#include <gctypes.h>
#include <lwp_tqdata.h>
#include <lwp_threads.h>
#include <lwp_watchdog.h>

#define LWP_THREADQ_NOTIMEOUT		LWP_WD_NOTIMEOUT

#ifdef __cplusplus
extern "C" {
#endif

lwp_cntrl* __lwp_threadqueue_firstfifo(lwp_thrqueue *queue);
lwp_cntrl* __lwp_threadqueue_firstpriority(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueuefifo(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeuefifo(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueuepriority(lwp_thrqueue *queue,lwp_cntrl *thethread,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeuepriority(lwp_thrqueue *queue);
void __lwp_threadqueue_init(lwp_thrqueue *queue,u32 mode,u32 state,u32 timeout_state);
lwp_cntrl* __lwp_threadqueue_first(lwp_thrqueue *queue);
void __lwp_threadqueue_enqueue(lwp_thrqueue *queue,u64 timeout);
lwp_cntrl* __lwp_threadqueue_dequeue(lwp_thrqueue *queue);
void __lwp_threadqueue_flush(lwp_thrqueue *queue,u32 status);
void __lwp_threadqueue_extract(lwp_thrqueue *queue,lwp_cntrl *thethread);
void __lwp_threadqueue_extractfifo(lwp_thrqueue *queue,lwp_cntrl *thethread);
void __lwp_threadqueue_extractpriority(lwp_thrqueue *queue,lwp_cntrl *thethread);
u32 __lwp_threadqueue_extractproxy(lwp_cntrl *thethread);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_threadq.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
