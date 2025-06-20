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

#ifndef __OGC_LWP_SEMA_INL__
#define __OGC_LWP_SEMA_INL__

static __inline__ u32 __lwp_sema_ispriority(lwp_semattr *attr)
{
	return (attr->mode==LWP_SEMA_MODEPRIORITY);
}

static __inline__ u32 __lwp_sema_getcount(lwp_sema *sema)
{
	return sema->count;
}

static __inline__ void __lwp_sema_seize_isrdisable(lwp_sema *sema,u32 id,u32 wait_status,u32 *isrlevel)
{
	lwp_cntrl *exec;
	u32 level = *isrlevel;

	exec = _thr_executing;
	exec->wait.ret_code = LWP_SEMA_SUCCESSFUL;
	if(sema->count!=0) {
		--sema->count;
		_CPU_ISR_Restore(level);
		return;
	}

	if(wait_status) {
		_CPU_ISR_Restore(level);
		exec->wait.ret_code = wait_status;
		return;
	}

	__lwp_thread_dispatchdisable();
	__lwp_threadqueue_csenter(&sema->wait_queue);
	exec->wait.queue = &sema->wait_queue;
	exec->wait.id = id;
	_CPU_ISR_Restore(level);

	__lwp_threadqueue_enqueue(&sema->wait_queue,LWP_THREADQ_NOTIMEOUT);
	__lwp_thread_dispatchenable();
}

#endif
