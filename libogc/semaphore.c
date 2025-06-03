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

semaphore.c -- Thread subsystem IV

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
#include <asm.h>
#include "lwp_sema.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "semaphore.h"

#define LWP_OBJTYPE_SEM				4

#define LWP_CHECK_SEM(hndl)		\
{									\
	if(((hndl)==LWP_SEM_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_SEM))	\
		return NULL;				\
}

typedef struct _sema_st
{
	lwp_obj object;
	lwp_sema sema;
} sema_st;

lwp_objinfo _lwp_sema_objects;

void __lwp_sema_init(void)
{
	__lwp_objmgr_initinfo(&_lwp_sema_objects,LWP_MAX_SEMAS,sizeof(sema_st));
}

static __inline__ sema_st* __lwp_sema_open(sem_t sem)
{
	LWP_CHECK_SEM(sem);
	return (sema_st*)__lwp_objmgr_get(&_lwp_sema_objects,LWP_OBJMASKID(sem));
}

static __inline__ void __lwp_sema_free(sema_st *sema)
{
	__lwp_objmgr_close(&_lwp_sema_objects,&sema->object);
	__lwp_objmgr_free(&_lwp_sema_objects,&sema->object);
}

static sema_st* __lwp_sema_allocate(void)
{
	sema_st *sema;

	__lwp_thread_dispatchdisable();
	sema = (sema_st*)__lwp_objmgr_allocate(&_lwp_sema_objects);
	if(sema) {
		__lwp_objmgr_open(&_lwp_sema_objects,&sema->object);
		return sema;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

static s32 __lwp_sema_waitsupp(sem_t sem,u8 block,u64 timeout)
{
	sema_st *lwp_sem;

	lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_seize(&lwp_sem->sema,lwp_sem->object.id,block,timeout);
	__lwp_thread_dispatchenable();

	switch(_thr_executing->wait.ret_code) {
		case LWP_SEMA_SUCCESSFUL:
			break;
		case LWP_SEMA_UNSATISFIED_NOWAIT:
			return EAGAIN;
		case LWP_SEMA_DELETED:
			return EAGAIN;
		case LWP_SEMA_TIMEOUT:
			return ETIMEDOUT;
		case LWP_SEMA_MAXCNT_EXCEEDED:
			break;
	}
	return 0;
}

s32 LWP_SemInit(sem_t *sem,u32 start,u32 max)
{
	lwp_semattr attr;
	sema_st *ret;

	if(!sem) return -1;
	
	ret = __lwp_sema_allocate();
	if(!ret) return -1;

	attr.max_cnt = max;
	attr.mode = LWP_SEMA_MODEFIFO;
	__lwp_sema_initialize(&ret->sema,&attr,start);

	*sem = (sem_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_SEM)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchenable();
	return 0;
}

s32 LWP_SemWait(sem_t sem)
{
	return __lwp_sema_waitsupp(sem,TRUE,LWP_THREADQ_NOTIMEOUT);
}

s32 LWP_SemTimedWait(sem_t sem,const struct timespec *reltime)
{
	u64 timeout = LWP_THREADQ_NOTIMEOUT;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return EINVAL;
		timeout = __lwp_wd_calc_ticks(reltime);
	}
	return __lwp_sema_waitsupp(sem,TRUE,timeout);
}

s32 LWP_SemTryWait(sem_t sem)
{
	return __lwp_sema_waitsupp(sem,FALSE,LWP_THREADQ_NOTIMEOUT);
}

s32 LWP_SemPost(sem_t sem)
{
	sema_st *lwp_sem;

	lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_surrender(&lwp_sem->sema,lwp_sem->object.id);
	__lwp_thread_dispatchenable();

	return 0;
}

s32 LWP_SemGetValue(sem_t sem,u32 *value)
{
	sema_st *lwp_sem;

	if(!value) return -1;

	lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	*value = __lwp_sema_getcount(&lwp_sem->sema);
	__lwp_thread_dispatchenable();

	return 0;
}

s32 LWP_SemDestroy(sem_t sem)
{
	sema_st *lwp_sem;

	lwp_sem = __lwp_sema_open(sem);
	if(!lwp_sem) return -1;

	__lwp_sema_flush(&lwp_sem->sema,-1);
	__lwp_thread_dispatchenable();

	__lwp_sema_free(lwp_sem);
	return 0;
}
