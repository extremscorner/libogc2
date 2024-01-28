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

#ifndef __OGC_LWP_MUTEX_INL__
#define __OGC_LWP_MUTEX_INL__

static __inline__ u32 __lwp_mutex_locked(lwp_mutex *mutex)
{
	return (mutex->lock==LWP_MUTEX_LOCKED);
}

static __inline__ u32 __lwp_mutex_ispriority(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_PRIORITY);
}

static __inline__ u32 __lwp_mutex_isfifo(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_FIFO);
}

static __inline__ u32 __lwp_mutex_isinheritprio(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_INHERITPRIO);
}

static __inline__ u32 __lwp_mutex_isprioceiling(lwp_mutex_attr *attrs)
{
	return (attrs->mode==LWP_MUTEX_PRIORITYCEIL);
}

static __inline__ u32 __lwp_mutex_seize_irq_trylock(lwp_mutex *mutex,u32 *isr_level)
{
	lwp_cntrl *exec;
	u32 level = *isr_level;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_MUTEX_SUCCESSFUL;
	if(!__lwp_mutex_locked(mutex)) {
		mutex->lock = LWP_MUTEX_LOCKED;
		mutex->holder = exec;
		mutex->nest_cnt = 1;
		if(__lwp_mutex_isinheritprio(&mutex->atrrs) || __lwp_mutex_isprioceiling(&mutex->atrrs))
			exec->res_cnt++;
		if(!__lwp_mutex_isprioceiling(&mutex->atrrs)) {
			_CPU_ISR_Restore(level);
			return 0;
		}
		{
			u32 prioceiling,priocurr;
			
			prioceiling = mutex->atrrs.prioceil;
			priocurr = exec->cur_prio;
			if(priocurr==prioceiling) {
				_CPU_ISR_Restore(level);
				return 0;
			}
			if(priocurr>prioceiling) {
				__lwp_thread_dispatchdisable();
				_CPU_ISR_Restore(level);
				__lwp_thread_changepriority(mutex->holder,mutex->atrrs.prioceil,FALSE);
				__lwp_thread_dispatchenable();
				return 0;
			}
			exec->wait.ret_code = LWP_MUTEX_CEILINGVIOL;
			mutex->nest_cnt = 0;
			exec->res_cnt--;
			_CPU_ISR_Restore(level);
			return 0;
		}
		return 0;
	}

	if(__lwp_thread_isexec(mutex->holder)) {
		switch(mutex->atrrs.nest_behavior) {
			case LWP_MUTEX_NEST_ACQUIRE:
				mutex->nest_cnt++;
				_CPU_ISR_Restore(level);
				return 0;
			case LWP_MUTEX_NEST_ERROR:
				exec->wait.ret_code = LWP_MUTEX_NEST_NOTALLOWED;
				_CPU_ISR_Restore(level);
				return 0;
			case LWP_MUTEX_NEST_BLOCK:
				break;
		}
	}
	return 1;
}

#define __lwp_mutex_seize(_mutex_t,_id,_wait,_timeout,_level) \
	do { \
		if(__lwp_mutex_seize_irq_trylock(_mutex_t,&_level)) { \
			if(!_wait) { \
				_CPU_ISR_Restore(_level); \
				_thr_executing->wait.ret_code = LWP_MUTEX_UNSATISFIED_NOWAIT; \
			} else { \
				__lwp_threadqueue_csenter(&(_mutex_t)->wait_queue); \
				_thr_executing->wait.queue = &(_mutex_t)->wait_queue; \
				_thr_executing->wait.id = _id; \
				__lwp_thread_dispatchdisable(); \
				_CPU_ISR_Restore(_level); \
				__lwp_mutex_seize_irq_blocking(_mutex_t,_timeout); \
			} \
		} \
	} while(0)

#endif
