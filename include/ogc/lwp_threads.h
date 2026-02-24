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

Copyright (C) 2004 - 2026
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

#ifndef __OGC_LWP_THREADS_H__
#define __OGC_LWP_THREADS_H__

#include <gctypes.h>
#include <stdlib.h>
#include "lwp_states.h"
#include "lwp_tqdata.h"
#include "lwp_watchdog.h"
#include "lwp_objmgr.h"
#include "context.h"

//#define _LWPTHREADS_DEBUG
#define LWP_TIMESLICE_TIMER_ID			0x00070040

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	LWP_CPU_BUDGET_ALGO_NONE = 0,
	LWP_CPU_BUDGET_ALGO_TIMESLICE
} lwp_cpu_budget_algorithms;

typedef enum
{
	LWP_DETACH_STATE_DETACHED = 0,
	LWP_DETACH_STATE_JOINABLE
} lwp_detach_state;

typedef struct _lwpwaitinfo {
	u32 id;
	u32 cnt;
	void *ret_arg;
	void *ret_arg_1;
	u32 option;
	u32 ret_code;
	lwp_queue block2n;
	lwp_thrqueue *queue;
} lwp_waitinfo;

typedef struct _lwpcntrl {
	lwp_obj object;
	u8  cur_prio,real_prio;
	u32 suspendcnt,res_cnt;
	u32 isr_level;
	u32 cur_state;
	u32 cpu_time_budget;
	lwp_cpu_budget_algorithms budget_algo;
	bool is_preemptible;
	lwp_waitinfo wait;
	prio_cntrl priomap;
	wd_cntrl timer;

	void* (*entry)(void *);
	void *arg;
	void *stack;
	u32 stack_size;
	u8 stack_allocated;
	lwp_queue *ready;
	lwp_detach_state detach_state;
	lwp_thrqueue join_list;
	frame_context context;		//16
	struct _reent libc_reent;
} lwp_cntrl, *lwp_cntrl_t;

extern lwp_cntrl *_thr_main;
extern lwp_cntrl *_thr_idle;
extern lwp_cntrl *_thr_executing;
extern lwp_cntrl *_thr_heir;
extern lwp_cntrl *_thr_allocated_fp;
extern vu32 _context_switch_want;
extern vu32 _thread_dispatch_disable_level;

extern wd_cntrl _lwp_wd_timeslice;
extern u32 _lwp_ticks_per_timeslice;
extern lwp_queue _lwp_thr_ready[];

void __thread_dispatch();
void __lwp_thread_yield();
void __lwp_thread_closeall();
void __lwp_thread_setstate(lwp_cntrl *,u32);
void __lwp_thread_clearstate(lwp_cntrl *,u32);
void __lwp_thread_changepriority(lwp_cntrl *,u32,u32);
void __lwp_thread_setpriority(lwp_cntrl *,u32);
void __lwp_thread_settransient(lwp_cntrl *);
void __lwp_thread_suspend(lwp_cntrl *);
void __lwp_thread_resume(lwp_cntrl *,u32);
void __lwp_thread_loadenv(lwp_cntrl *);
void __lwp_thread_ready(lwp_cntrl *);
u32 __lwp_thread_init(lwp_cntrl *,void *,u32,u32,bool,lwp_cpu_budget_algorithms,u32);
u32 __lwp_thread_start(lwp_cntrl *,void* (*)(void*),void *);
void __lwp_thread_exit(lwp_cntrl *,void *);
void __lwp_thread_close(lwp_cntrl *);
void __lwp_thread_startmultitasking();
void __lwp_thread_stopmultitasking(void (*exitfunc)());
lwp_obj* __lwp_thread_getobject(lwp_cntrl *);
u32 __lwp_evaluatemode();

u32 __lwp_isr_in_progress();
void __lwp_thread_resettimeslice();
void __lwp_rotate_readyqueue(u32);
void __lwp_thread_delayended(void *);
void __lwp_thread_tickle_timeslice(void *);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_threads.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
