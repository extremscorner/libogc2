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

#ifndef __OGC_LWP_MESSAGES_H__
#define __OGC_LWP_MESSAGES_H__

#include <gctypes.h>
#include <limits.h>
#include <string.h>
#include <lwp_threadq.h>

//#define _LWPMQ_DEBUG

#define LWP_MQ_FIFO							0
#define LWP_MQ_PRIORITY						1

#define LWP_MQ_STATUS_SUCCESSFUL			0
#define LWP_MQ_STATUS_INVALID_SIZE			1
#define LWP_MQ_STATUS_TOO_MANY				2
#define LWP_MQ_STATUS_UNSATISFIED			3
#define LWP_MQ_STATUS_UNSATISFIED_NOWAIT	4
#define LWP_MQ_STATUS_DELETED				5
#define LWP_MQ_STATUS_TIMEOUT				6
#define LWP_MQ_STATUS_UNSATISFIED_WAIT		7

#define LWP_MQ_SEND_REQUEST					INT_MAX
#define LWP_MQ_SEND_URGENT					INT_MIN

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mq_notifyhandler)(void *);

typedef struct _mqbuffer {
	u32 size;
	u32 buffer[1];
} mq_buffer;

typedef struct _mqbuffercntrl {
	lwp_node node;
	u32 prio;
	mq_buffer contents;
} mq_buffercntrl;

//the following struct is extensible
typedef struct _mqattr {
	u32 mode;
} mq_attr;

typedef struct _mqcntrl {
	lwp_thrqueue wait_queue;
	mq_attr attr;
	u32 max_pendingmsgs;
	u32 num_pendingmsgs;
	u32 max_msgsize;
	lwp_queue pending_msgs;
	mq_buffer *msq_buffers;
	mq_notifyhandler notify_handler;
	void *notify_arg;
	lwp_queue inactive_msgs;
} mq_cntrl;

u32 __lwpmq_initialize(mq_cntrl *mqueue,mq_attr *attrs,u32 max_pendingmsgs,u32 max_msgsize);
void __lwpmq_close(mq_cntrl *mqueue,u32 status);
u32 __lwpmq_seize(mq_cntrl *mqueue,u32 id,void *buffer,u32 *size,u32 wait,s64 timeout);
u32 __lwpmq_submit(mq_cntrl *mqueue,u32 id,void *buffer,u32 size,u32 type,u32 wait,s64 timeout);
u32 __lwpmq_broadcast(mq_cntrl *mqueue,void *buffer,u32 size,u32 id,u32 *count);
void __lwpmq_msg_insert(mq_cntrl *mqueue,mq_buffercntrl *msg,u32 type);
u32 __lwpmq_flush(mq_cntrl *mqueue);
u32 __lwpmq_flush_support(mq_cntrl *mqueue);
void __lwpmq_flush_waitthreads(mq_cntrl *mqueue);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_messages.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
