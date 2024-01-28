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

#ifndef __OGC_LWP_WATCHDOG_INL__
#define __OGC_LWP_WATCHDOG_INL__

static __inline__ void __lwp_wd_initialize(wd_cntrl *wd,wd_service_routine routine,u32 id,void *usr_data)
{
	wd->state = LWP_WD_INACTIVE;
	wd->id = id;
	wd->routine = routine;
	wd->usr_data = usr_data;
}

static __inline__ wd_cntrl* __lwp_wd_first(lwp_queue *queue)
{
	return (wd_cntrl*)queue->first;
}

static __inline__ wd_cntrl* __lwp_wd_last(lwp_queue *queue)
{
	return (wd_cntrl*)queue->last;
}

static __inline__ wd_cntrl* __lwp_wd_next(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.next;
}

static __inline__ wd_cntrl* __lwp_wd_prev(wd_cntrl *wd)
{
	return (wd_cntrl*)wd->node.prev;
}

static __inline__ void __lwp_wd_activate(wd_cntrl *wd)
{
	wd->state = LWP_WD_ACTIVE;
}

static __inline__ void __lwp_wd_deactivate(wd_cntrl *wd)
{
	wd->state = LWP_WD_REMOVE;
}

static __inline__ u32 __lwp_wd_isactive(wd_cntrl *wd)
{
	return (wd->state==LWP_WD_ACTIVE);
}

static __inline__ u64 __lwp_wd_calc_ticks(const struct timespec *time)
{
	u64 ticks;

	ticks = secs_to_ticks(time->tv_sec);
	ticks += nanosecs_to_ticks(time->tv_nsec);

	return ticks;
}

static __inline__ void __lwp_wd_tickle_ticks(void)
{
	__lwp_wd_tickle(&_wd_ticks_queue);
}

static __inline__ void __lwp_wd_insert_ticks(wd_cntrl *wd,s64 interval)
{
	wd->start = __SYS_GetSystemTime();
	wd->fire = (wd->start+LWP_WD_ABS(interval));
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}

static __inline__ void __lwp_wd_adjust_ticks(u32 dir,s64 interval)
{
	__lwp_wd_adjust(&_wd_ticks_queue,dir,interval);
}

static __inline__ void __lwp_wd_remove_ticks(wd_cntrl *wd)
{
	__lwp_wd_remove(&_wd_ticks_queue,wd);
}

static __inline__ void __lwp_wd_reset(wd_cntrl *wd)
{
	__lwp_wd_remove(&_wd_ticks_queue,wd);
	__lwp_wd_insert(&_wd_ticks_queue,wd);
}
#endif
