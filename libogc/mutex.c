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

mutex.c -- Thread subsystem III

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
#include "lwp_mutex.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "mutex.h"

#define LWP_OBJTYPE_MUTEX			3

#define LWP_CHECK_MUTEX(hndl)		\
{									\
	if(((hndl)==LWP_MUTEX_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_MUTEX))	\
		return NULL;				\
}

typedef struct _mutex_st
{
	lwp_obj object;
	lwp_mutex mutex;
} mutex_st;

lwp_objinfo _lwp_mutex_objects;

static s32 __lwp_mutex_locksupp(mutex_t lock,u32 wait_status,s64 timeout)
{
	u32 level;
	mutex_st *p;

	if(lock==LWP_MUTEX_NULL || LWP_OBJTYPE(lock)!=LWP_OBJTYPE_MUTEX) return EINVAL;
	
	p = (mutex_st*)__lwp_objmgr_getisrdisable(&_lwp_mutex_objects,LWP_OBJMASKID(lock),&level);
	if(!p) return EINVAL;

	__lwp_mutex_seize(&p->mutex,p->object.id,wait_status,timeout,level);

	switch(_thr_executing->wait.ret_code) {
		case LWP_MUTEX_SUCCESSFUL:
			break;
		case LWP_MUTEX_UNSATISFIED_NOWAIT:
			return EBUSY;
		case LWP_MUTEX_NEST_NOTALLOWED:
			return EDEADLK;
		case LWP_MUTEX_NOTOWNER:
			return EPERM;
		case LWP_MUTEX_DELETED:
			return EINVAL;
		case LWP_MUTEX_TIMEOUT:
			return ETIMEDOUT;
		case LWP_MUTEX_CEILINGVIOL:
			return EINVAL;
	}
	return 0;
}

void __lwp_mutex_init(void)
{
	__lwp_objmgr_initinfo(&_lwp_mutex_objects,LWP_MAX_MUTEXES,sizeof(mutex_st));
}

static __inline__ mutex_st* __lwp_mutex_open(mutex_t lock)
{
	LWP_CHECK_MUTEX(lock);
	return (mutex_st*)__lwp_objmgr_get(&_lwp_mutex_objects,LWP_OBJMASKID(lock));
}

static __inline__ void __lwp_mutex_free(mutex_st *lock)
{
	__lwp_objmgr_close(&_lwp_mutex_objects,&lock->object);
	__lwp_objmgr_free(&_lwp_mutex_objects,&lock->object);
}

static mutex_st* __lwp_mutex_allocate(void)
{
	mutex_st *lock;

	__lwp_thread_dispatchdisable();
	lock = (mutex_st*)__lwp_objmgr_allocate(&_lwp_mutex_objects);
	if(lock) {
		__lwp_objmgr_open(&_lwp_mutex_objects,&lock->object);
		return lock;
	}
	__lwp_thread_dispatchunnest();
	return NULL;
}

s32 LWP_MutexInit(mutex_t *mutex,bool use_recursive)
{
	lwp_mutex_attr attr;
	mutex_st *ret;
	
	if(!mutex) return EINVAL;

	ret = __lwp_mutex_allocate();
	if(!ret) return EAGAIN;

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = (use_recursive)?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = LWP_PRIO_MIN + 1;
	__lwp_mutex_initialize(&ret->mutex,&attr,LWP_MUTEX_UNLOCKED);

	*mutex = (mutex_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_MUTEX)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchunnest();
	return 0;
}

s32 LWP_MutexDestroy(mutex_t mutex)
{
	mutex_st *p;

	p = __lwp_mutex_open(mutex);
	if(!p) return EINVAL;

	if(__lwp_mutex_locked(&p->mutex)) {
		__lwp_thread_dispatchenable();
		return EBUSY;
	}
	__lwp_mutex_flush(&p->mutex,LWP_MUTEX_DELETED);
	__lwp_thread_dispatchenable();

	__lwp_mutex_free(p);
	return 0;
}

s32 LWP_MutexLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_MUTEX_SUCCESSFUL,LWP_THREADQ_NOTIMEOUT);
}

s32 LWP_MutexTimedLock(mutex_t mutex,const struct timespec *reltime)
{
	u32 wait_status = LWP_MUTEX_SUCCESSFUL;
	s64 timeout = LWP_THREADQ_NOTIMEOUT;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return EINVAL;
		timeout = __lwp_wd_calc_ticks(reltime);
		if(timeout<=0) wait_status = LWP_MUTEX_TIMEOUT;
	}
	return __lwp_mutex_locksupp(mutex,wait_status,timeout);
}

s32 LWP_MutexTryLock(mutex_t mutex)
{
	return __lwp_mutex_locksupp(mutex,LWP_MUTEX_UNSATISFIED_NOWAIT,LWP_THREADQ_NOTIMEOUT);
}

s32 LWP_MutexUnlock(mutex_t mutex)
{
	u32 status;
	mutex_st *lock;

	lock = __lwp_mutex_open(mutex);
	if(!lock) return EINVAL;

	status = __lwp_mutex_surrender(&lock->mutex);
	__lwp_thread_dispatchenable();

	switch(status) {
		case LWP_MUTEX_SUCCESSFUL:
			break;
		case LWP_MUTEX_UNSATISFIED_NOWAIT:
			return EBUSY;
		case LWP_MUTEX_NEST_NOTALLOWED:
			return EDEADLK;
		case LWP_MUTEX_NOTOWNER:
			return EPERM;
		case LWP_MUTEX_DELETED:
			return EINVAL;
		case LWP_MUTEX_TIMEOUT:
			return ETIMEDOUT;
		case LWP_MUTEX_CEILINGVIOL:
			return EINVAL;
	}
	return 0;
}
