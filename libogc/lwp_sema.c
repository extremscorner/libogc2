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

#include "asm.h"
#include "lwp_sema.h"

void __lwp_sema_initialize(lwp_sema *sema,lwp_semattr *attrs,u32 init_count)
{
	sema->attrs = *attrs;
	sema->count = init_count;

	__lwp_threadqueue_init(&sema->wait_queue,__lwp_sema_ispriority(attrs)?LWP_THREADQ_MODEPRIORITY:LWP_THREADQ_MODEFIFO,LWP_STATES_WAITING_FOR_SEMAPHORE,LWP_SEMA_TIMEOUT);
}

u32 __lwp_sema_surrender(lwp_sema *sema,u32 id)
{
	u32 level,ret;
	lwp_cntrl *thethread;
	
	ret = LWP_SEMA_SUCCESSFUL;
	if((thethread=__lwp_threadqueue_dequeue(&sema->wait_queue))) return ret;
	else {
		_CPU_ISR_Disable(level);
		if(sema->count<=sema->attrs.max_cnt)
			++sema->count;
		else
			ret = LWP_SEMA_MAXCNT_EXCEEDED;
		_CPU_ISR_Restore(level);
	}
	return ret;
}

u32 __lwp_sema_seize(lwp_sema *sema,u32 id,u32 wait,u64 timeout)
{
	u32 level;
	lwp_cntrl *exec;
	
	exec = _thr_executing;
	exec->wait.ret_code = LWP_SEMA_SUCCESSFUL;

	_CPU_ISR_Disable(level);
	if(sema->count!=0) {
		--sema->count;
		_CPU_ISR_Restore(level);
		return LWP_SEMA_SUCCESSFUL;
	}

	if(!wait) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = LWP_SEMA_UNSATISFIED_NOWAIT;
		return LWP_SEMA_UNSATISFIED_NOWAIT;
	}

	__lwp_threadqueue_csenter(&sema->wait_queue);
	exec->wait.queue = &sema->wait_queue;
	exec->wait.id = id;
	_CPU_ISR_Restore(level);
	
	__lwp_threadqueue_enqueue(&sema->wait_queue,timeout);
	return LWP_SEMA_SUCCESSFUL;
}

void __lwp_sema_flush(lwp_sema *sema,u32 status)
{
	__lwp_threadqueue_flush(&sema->wait_queue,status);
}
