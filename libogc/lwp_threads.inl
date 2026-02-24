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

#ifndef __OGC_LWP_THREADS_INL__
#define __OGC_LWP_THREADS_INL__

static __inline__ u32 __lwp_thread_isexec(lwp_cntrl *thethread)
{
	return (thethread==_thr_executing);
}

static __inline__ u32 __lwp_thread_isheir(lwp_cntrl *thethread)
{
	return (thethread==_thr_heir);
}

static __inline__ void __lwp_thread_calcheir(void)
{
	_thr_heir = (lwp_cntrl*)_lwp_thr_ready[__lwp_priomap_highest()].first;
#ifdef _LWPTHREADS_DEBUG
	printf("__lwp_thread_calcheir(%p)\n",_thr_heir);
#endif
}

static __inline__ u32 __lwp_thread_isallocatedfp(lwp_cntrl *thethread)
{
	return (thethread==_thr_allocated_fp);
}

static __inline__ void __lwp_thread_deallocatefp(void)
{
	_thr_allocated_fp = NULL;
}

static __inline__ void __lwp_thread_dispatchinitialize(void)
{
	_thread_dispatch_disable_level = 1;
}

static __inline__ void __lwp_thread_dispatchenable(void)
{
	if((--_thread_dispatch_disable_level)==0)
		__thread_dispatch();
}

static __inline__ void __lwp_thread_dispatchdisable(void)
{
	++_thread_dispatch_disable_level;
}

static __inline__ void __lwp_thread_dispatchunnest(void)
{
	--_thread_dispatch_disable_level;
}

static __inline__ void __lwp_thread_unblock(lwp_cntrl *thethread)
{
	__lwp_thread_clearstate(thethread,LWP_STATES_BLOCKED);
}

static __inline__ bool __lwp_thread_isswitchwant(void)
{

	return _context_switch_want;
}

static __inline__ bool __lwp_thread_isdispatchenabled(void)
{
	return (_thread_dispatch_disable_level==0);
}

static __inline__ void __lwp_thread_inittimeslice(void)
{
	__lwp_wd_initialize(&_lwp_wd_timeslice,__lwp_thread_tickle_timeslice,LWP_TIMESLICE_TIMER_ID,NULL);
}

static __inline__ void __lwp_thread_starttimeslice(void)
{
	__lwp_wd_insert_ticks(&_lwp_wd_timeslice,millisecs_to_ticks(1));
}

static __inline__ void __lwp_thread_stoptimeslice(void)
{
	__lwp_wd_remove_ticks(&_lwp_wd_timeslice);
}

#endif
