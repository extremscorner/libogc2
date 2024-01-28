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

#ifndef __OGC_LWP_MESSAGES_INL__
#define __OGC_LWP_MESSAGES_INL__

static __inline__ void __lwpmq_set_notify(mq_cntrl *mqueue,mq_notifyhandler handler,void *arg)
{
	mqueue->notify_handler = handler;
	mqueue->notify_arg = arg;
}

static __inline__ u32 __lwpmq_is_priority(mq_attr *attr)
{
	return (attr->mode==LWP_MQ_PRIORITY);
}

static __inline__ mq_buffercntrl* __lwpmq_allocate_msg(mq_cntrl *mqueue)
{
	return (mq_buffercntrl*)__lwp_queue_get(&mqueue->inactive_msgs);
}

static __inline__ void __lwpmq_free_msg(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
	__lwp_queue_append(&mqueue->inactive_msgs,&msg->node);
}

static __inline__ void __lwpmq_msg_append(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_msq_append(%p,%p,%p)\n",mqueue,&mqueue->inactive_msgs,msg);
#endif
	__lwp_queue_append(&mqueue->pending_msgs,&msg->node);
}

static __inline__ void __lwpmq_msg_prepend(mq_cntrl *mqueue,mq_buffercntrl *msg)
{
#ifdef _LWPMQ_DEBUG
	printf("__lwpmq_msq_prepend(%p,%p,%p)\n",mqueue,&mqueue->inactive_msgs,msg);
#endif
	__lwp_queue_prepend(&mqueue->pending_msgs,&msg->node);
}

static __inline__ u32 __lwpmq_send(mq_cntrl *mqueue,u32 id,void *buffer,u32 size,u32 wait,u32 timeout)
{
	return __lwpmq_submit(mqueue,id,buffer,size,LWP_MQ_SEND_REQUEST,wait,timeout);
}

static __inline__ u32 __lwpmq_urgent(mq_cntrl *mqueue,void *buffer,u32 size,u32 id,u32 wait,u32 timeout)
{
	return __lwpmq_submit(mqueue,id,buffer,size,LWP_MQ_SEND_URGENT,wait,timeout);
}

static __inline__ mq_buffercntrl* __lwpmq_get_pendingmsg(mq_cntrl *mqueue)
{
	return (mq_buffercntrl*)__lwp_queue_getI(&mqueue->pending_msgs);
}

static __inline__ void __lwpmq_buffer_copy(void *dest,const void *src,u32 size)
{
	if(size==sizeof(u32)) *(u32*)dest = *(u32*)src;
	else memcpy(dest,src,size);
}

#endif
