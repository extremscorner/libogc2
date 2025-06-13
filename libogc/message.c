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

message.c -- Thread subsystem II

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
#include <lwp_messages.h>
#include <lwp_objmgr.h>
#include <lwp_config.h>
#include "message.h"

#define LWP_OBJTYPE_MBOX			6

#define LWP_CHECK_MBOX(hndl)		\
{									\
	if(((hndl)==MQ_BOX_NULL) || (LWP_OBJTYPE(hndl)!=LWP_OBJTYPE_MBOX))	\
		return NULL;				\
}

typedef struct _mqbox_st
{
	lwp_obj object;
	mq_cntrl mqueue;
} mqbox_st;

lwp_objinfo _lwp_mqbox_objects;

void __lwp_mqbox_init(void)
{
	__lwp_objmgr_initinfo(&_lwp_mqbox_objects,LWP_MAX_MQUEUES,sizeof(mqbox_st));
}

static __inline__ mqbox_st* __lwp_mqbox_open(mqbox_t mbox)
{
	LWP_CHECK_MBOX(mbox);
	return (mqbox_st*)__lwp_objmgr_get(&_lwp_mqbox_objects,LWP_OBJMASKID(mbox));
}

static __inline__ void __lwp_mqbox_free(mqbox_st *mqbox)
{
	__lwp_objmgr_close(&_lwp_mqbox_objects,&mqbox->object);
	__lwp_objmgr_free(&_lwp_mqbox_objects,&mqbox->object);
}

static mqbox_st* __lwp_mqbox_allocate(void)
{
	mqbox_st *mqbox;

	__lwp_thread_dispatchdisable();
	mqbox = (mqbox_st*)__lwp_objmgr_allocate(&_lwp_mqbox_objects);
	if(mqbox) {
		__lwp_objmgr_open(&_lwp_mqbox_objects,&mqbox->object);
		return mqbox;
	}
	__lwp_thread_dispatchenable();
	return NULL;
}

static BOOL __lwp_mqbox_sendsupp(mqbox_t mqbox,mqmsg_t msg,u32 type,u32 wait_status,s64 timeout)
{
	u32 status,size = sizeof(mqmsg_t);
	mqbox_st *mbox;

	mbox = __lwp_mqbox_open(mqbox);
	if(!mbox) return FALSE;

	status = __lwpmq_submit(&mbox->mqueue,mbox->object.id,(void*)&msg,size,type,wait_status,timeout);
	__lwp_thread_dispatchenable();

	if(status==LWP_MQ_STATUS_UNSATISFIED_WAIT)
		status = _thr_executing->wait.ret_code;
	return (status==LWP_MQ_STATUS_SUCCESSFUL);
}

static BOOL __lwp_mqbox_recvsupp(mqbox_t mqbox,mqmsg_t *msg,u32 wait_status,s64 timeout)
{
	u32 status,size;
	mqbox_st *mbox;

	mbox = __lwp_mqbox_open(mqbox);
	if(!mbox) return FALSE;

	__lwpmq_seize(&mbox->mqueue,mbox->object.id,(void*)msg,&size,wait_status,timeout);
	__lwp_thread_dispatchenable();

	status = _thr_executing->wait.ret_code;
	return (status==LWP_MQ_STATUS_SUCCESSFUL);
}

s32 MQ_Init(mqbox_t *mqbox,u32 count)
{
	mq_attr attr;
	mqbox_st *ret = NULL;

	if(!mqbox) return EINVAL;

	ret = __lwp_mqbox_allocate();
	if(!ret) return ENOSPC;

	attr.mode = LWP_MQ_FIFO;
	if(!__lwpmq_initialize(&ret->mqueue,&attr,count,sizeof(mqmsg_t))) {
		__lwp_mqbox_free(ret);
		__lwp_thread_dispatchenable();
		return ENOSPC;
	}

	*mqbox = (mqbox_t)(LWP_OBJMASKTYPE(LWP_OBJTYPE_MBOX)|LWP_OBJMASKID(ret->object.id));
	__lwp_thread_dispatchenable();
	return 0;
}

s32 MQ_Close(mqbox_t mqbox)
{
	mqbox_st *mbox;

	mbox = __lwp_mqbox_open(mqbox);
	if(!mbox) return EINVAL;

	__lwpmq_close(&mbox->mqueue,LWP_MQ_STATUS_DELETED);
	__lwp_thread_dispatchenable();

	__lwp_mqbox_free(mbox);
	return 0;
}

BOOL MQ_Send(mqbox_t mqbox,mqmsg_t msg,u32 flags)
{
	u32 wait_status = (flags==MQ_MSG_BLOCK)?LWP_MQ_STATUS_SUCCESSFUL:LWP_MQ_STATUS_TOO_MANY;

	return __lwp_mqbox_sendsupp(mqbox,msg,LWP_MQ_SEND_REQUEST,wait_status,LWP_THREADQ_NOTIMEOUT);
}

BOOL MQ_TimedSend(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime)
{
	u32 wait_status = LWP_MQ_STATUS_SUCCESSFUL;
	s64 timeout = LWP_THREADQ_NOTIMEOUT;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return FALSE;
		timeout = __lwp_wd_calc_ticks(reltime);
		if(timeout<=0) wait_status = LWP_MQ_STATUS_TIMEOUT;
	}
	return __lwp_mqbox_sendsupp(mqbox,msg,LWP_MQ_SEND_REQUEST,wait_status,timeout);
}

BOOL MQ_Jam(mqbox_t mqbox,mqmsg_t msg,u32 flags)
{
	u32 wait_status = (flags==MQ_MSG_BLOCK)?LWP_MQ_STATUS_SUCCESSFUL:LWP_MQ_STATUS_TOO_MANY;

	return __lwp_mqbox_sendsupp(mqbox,msg,LWP_MQ_SEND_URGENT,wait_status,LWP_THREADQ_NOTIMEOUT);
}

BOOL MQ_TimedJam(mqbox_t mqbox,mqmsg_t msg,const struct timespec *reltime)
{
	u32 wait_status = LWP_MQ_STATUS_SUCCESSFUL;
	s64 timeout = LWP_THREADQ_NOTIMEOUT;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return FALSE;
		timeout = __lwp_wd_calc_ticks(reltime);
		if(timeout<=0) wait_status = LWP_MQ_STATUS_TIMEOUT;
	}
	return __lwp_mqbox_sendsupp(mqbox,msg,LWP_MQ_SEND_URGENT,wait_status,timeout);
}

BOOL MQ_Receive(mqbox_t mqbox,mqmsg_t *msg,u32 flags)
{
	u32 wait_status = (flags==MQ_MSG_BLOCK)?LWP_MQ_STATUS_SUCCESSFUL:LWP_MQ_STATUS_UNSATISFIED_NOWAIT;

	return __lwp_mqbox_recvsupp(mqbox,msg,wait_status,LWP_THREADQ_NOTIMEOUT);
}

BOOL MQ_TimedReceive(mqbox_t mqbox,mqmsg_t *msg,const struct timespec *reltime)
{
	u32 wait_status = LWP_MQ_STATUS_SUCCESSFUL;
	s64 timeout = LWP_THREADQ_NOTIMEOUT;

	if(reltime) {
		if(!__lwp_wd_timespec_valid(reltime)) return FALSE;
		timeout = __lwp_wd_calc_ticks(reltime);
		if(timeout<=0) wait_status = LWP_MQ_STATUS_TIMEOUT;
	}
	return __lwp_mqbox_recvsupp(mqbox,msg,wait_status,timeout);
}
