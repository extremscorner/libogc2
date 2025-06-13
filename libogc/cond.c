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

cond.c -- Thread subsystem V

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
#include <errno.h>
#include "asm.h"
#include "mutex.h"
#include "lwp_threadq.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "cond.h"

#define LWP_OBJTYPE_COND				5

#define LWP_CHECK_COND(hndl)		\
{									\
	if(((hndl)==LWP_COND_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_COND))	\
		return NULL;				\
}

typedef struct _cond_st {
	lwp_obj object;
	mutex_t lock;
	lwp_thrqueue wait_queue;
} cond_st;

lwp_objinfo _lwp_cond_objects;

void __lwp_cond_init(void)
{
	__lwp_objmgr_initinfo(&_lwp_cond_objects,LWP_MAX_CONDVARS,sizeof(cond_st));
}

static __inline__ cond_st* __lwp_cond_open(cond_t cond)
{
	LWP_CHECK_COND(cond);
	return (cond_st*)__lwp_objmgr_get(&_lwp_cond_objects,LWP_OBJMASKID(cond));
}

static __inline__ void __lwp_cond_free(cond_st *cond)
{
	__lwp_objmgr_close(&_lwp_cond_objects,&cond->object);
	__lwp_objmgr_free(&_lwp_cond_objects,&cond->object);
}

static cond_st* __lwp_cond_allocate(void)
{
	cond_st *cond;

	__lwp_thread_dispatchdisable();
	cond = (cond_st*)__lwp_objmgr_allocate(&_lwp_cond_objects);
	if(cond) {
		__lwp_objmgr_open(&_lwp_cond_objects,&cond->object);
		return cond;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

static s32 __lwp_cond_waitsupp(cond_t cond,mutex_t mutex,s64 timeout,u8 timedout)
{
	u32 status,mutex_status,level;
	cond_st *thecond;

	thecond = __lwp_cond_open(cond);
	if(!thecond) return EINVAL;

	if(thecond->lock!=LWP_MUTEX_NULL && thecond->lock!=mutex) {
		__lwp_thread_dispatchenable();
		return EINVAL;
	}

	LWP_MutexUnlock(mutex);
	if(!timedout) {
		thecond->lock = mutex;
		_CPU_ISR_Disable(level);
		__lwp_threadqueue_csenter(&thecond->wait_queue);
		_thr_executing->wait.ret_code = 0;
		_thr_executing->wait.queue = &thecond->wait_queue;
		_thr_executing->wait.id = cond;
		_CPU_ISR_Restore(level);
		__lwp_threadqueue_enqueue(&thecond->wait_queue,timeout);
		__lwp_thread_dispatchenable();
		
		status = _thr_executing->wait.ret_code;
		if(status && status!=ETIMEDOUT)
			return status;
	} else {
		__lwp_thread_dispatchenable();
		status = ETIMEDOUT;
	}

	mutex_status = LWP_MutexLock(mutex);
	if(mutex_status)
		return EINVAL;

	return status;
}

static s32 __lwp_cond_signalsupp(cond_t cond,u8 isbroadcast)
{
	lwp_cntrl *thethread;
	cond_st *thecond;
	
	thecond = __lwp_cond_open(cond);
	if(!thecond) return EINVAL;

	do {
		thethread = __lwp_threadqueue_dequeue(&thecond->wait_queue);
		if(!thethread) thecond->lock = LWP_MUTEX_NULL;
	} while(isbroadcast && thethread);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_CondInit(cond_t *cond)
{
	cond_st *ret;
	
	if(!cond) return EINVAL;
	
	ret = __lwp_cond_allocate();
	if(!ret) return EAGAIN;

	ret->lock = LWP_MUTEX_NULL;
	__lwp_threadqueue_init(&ret->wait_queue,LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_CONDVAR,ETIMEDOUT);

	*cond = (cond_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_COND)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchenable();

	return 0;
}

s32 LWP_CondWait(cond_t cond,mutex_t mutex)
{
	return __lwp_cond_waitsupp(cond,mutex,LWP_THREADQ_NOTIMEOUT,FALSE);
}

s32 LWP_CondSignal(cond_t cond)
{
	return __lwp_cond_signalsupp(cond,FALSE);
}

s32 LWP_CondBroadcast(cond_t cond)
{
	return __lwp_cond_signalsupp(cond,TRUE);
}

s32 LWP_CondTimedWait(cond_t cond,mutex_t mutex,const struct timespec *reltime)
{
	s64 timeout = LWP_THREADQ_NOTIMEOUT;
	u8 timedout = FALSE;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return EINVAL;
		timeout = __lwp_wd_calc_ticks(reltime);
		if(timeout<=0) timedout = TRUE;
	}
	return __lwp_cond_waitsupp(cond,mutex,timeout,timedout);
}

s32 LWP_CondDestroy(cond_t cond)
{
	cond_st *ptr;

	ptr = __lwp_cond_open(cond);
	if(!ptr) return EINVAL;

	if(__lwp_threadqueue_first(&ptr->wait_queue)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	__lwp_thread_dispatchenable();

	__lwp_cond_free(ptr);
	return 0;
}
