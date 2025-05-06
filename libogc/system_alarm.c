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

#include <stdlib.h>
#include <errno.h>
#include "asm.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"
#include "lwp_objmgr.h"
#include "lwp_config.h"
#include "system.h"

#define LWP_OBJTYPE_SYSWD			7

#define LWP_CHECK_SYSWD(hndl)		\
{									\
	if(((hndl)==SYS_WD_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_SYSWD))	\
		return NULL;				\
}

typedef struct _alarm_st
{
	lwp_obj object;
	wd_cntrl alarm;
	u64 ticks;
	u64 periodic;
	u64 start_per;
	alarmcallback alarmhandler;
	void *cb_arg;
} alarm_st;

static lwp_objinfo sys_alarm_objects;

void __lwp_syswd_init(void)
{
	__lwp_objmgr_initinfo(&sys_alarm_objects,LWP_MAX_WATCHDOGS,sizeof(alarm_st));
}

static __inline__ alarm_st* __lwp_syswd_open(syswd_t wd)
{
	LWP_CHECK_SYSWD(wd);
	return (alarm_st*)__lwp_objmgr_get(&sys_alarm_objects,LWP_OBJMASKID(wd));
}

static __inline__ void __lwp_syswd_free(alarm_st *alarm)
{
	__lwp_objmgr_close(&sys_alarm_objects,&alarm->object);
	__lwp_objmgr_free(&sys_alarm_objects,&alarm->object);
}

static alarm_st* __lwp_syswd_allocate(void)
{
	alarm_st *alarm;

	__lwp_thread_dispatchdisable();
	alarm = (alarm_st*)__lwp_objmgr_allocate(&sys_alarm_objects);
	if(alarm) {
		__lwp_objmgr_open(&sys_alarm_objects,&alarm->object);
		return alarm;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

static void __sys_alarmhandler(void *arg)
{
	alarm_st *alarm;
	syswd_t thealarm = (syswd_t)arg;

	if(thealarm==SYS_WD_NULL || LWP_OBJTYPE(thealarm)!=LWP_OBJTYPE_SYSWD) return;

	__lwp_thread_dispatchdisable();
	alarm = (alarm_st*)__lwp_objmgr_getnoprotection(&sys_alarm_objects,LWP_OBJMASKID(thealarm));
	if(alarm) {
		if(alarm->periodic) __lwp_wd_insert_ticks(&alarm->alarm,alarm->periodic);
		if(alarm->alarmhandler) alarm->alarmhandler(thealarm,alarm->cb_arg);
	}
	__lwp_thread_dispatchunnest();
}

s32 SYS_CreateAlarm(syswd_t *thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_allocate();
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->start_per = 0;
	alarm->periodic = 0;

	*thealarm = (LWP_OBJMASKTYPE(LWP_OBJTYPE_SYSWD)|LWP_OBJMASKID(alarm->object.id));
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_SetAlarm(syswd_t thealarm,const struct timespec *tp,alarmcallback cb,void *cbarg)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->cb_arg = cbarg;
	alarm->alarmhandler = cb;
	alarm->ticks = __lwp_wd_calc_ticks(tp);

	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_initialize(&alarm->alarm,__sys_alarmhandler,alarm->object.id,(void*)thealarm);
	__lwp_wd_insert_ticks(&alarm->alarm,alarm->ticks);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_SetPeriodicAlarm(syswd_t thealarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb,void *cbarg)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->start_per = __lwp_wd_calc_ticks(tp_start);
	alarm->periodic = __lwp_wd_calc_ticks(tp_period);
	alarm->alarmhandler = cb;
	alarm->cb_arg = cbarg;

	alarm->ticks = 0;

	__lwp_wd_initialize(&alarm->alarm,__sys_alarmhandler,alarm->object.id,(void*)thealarm);
	__lwp_wd_insert_ticks(&alarm->alarm,alarm->start_per);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_RemoveAlarm(syswd_t thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_remove_ticks(&alarm->alarm);
	__lwp_syswd_free(alarm);
	__lwp_thread_dispatchenable();
	return 0;
}

s32 SYS_CancelAlarm(syswd_t thealarm)
{
	alarm_st *alarm;

	alarm = __lwp_syswd_open(thealarm);
	if(!alarm) return -1;

	alarm->alarmhandler = NULL;
	alarm->ticks = 0;
	alarm->periodic = 0;
	alarm->start_per = 0;

	__lwp_wd_remove_ticks(&alarm->alarm);
	__lwp_thread_dispatchenable();
	return 0;
}
