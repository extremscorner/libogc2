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

#ifndef __OGC_LWP_TQDATA_H__
#define __OGC_LWP_TQDATA_H__

#define LWP_THREADQ_NUM_PRIOHEADERS		4
#define LWP_THREADQ_PRIOPERHEADER		64
#define LWP_THREADQ_REVERSESEARCHMASK	0x20

#define LWP_THREADQ_SYNCHRONIZED		0
#define LWP_THREADQ_NOTHINGHAPPEND		1
#define LWP_THREADQ_TIMEOUT				2
#define LWP_THREADQ_SATISFIED			3

#define LWP_THREADQ_MODEFIFO			0
#define LWP_THREADQ_MODEPRIORITY		1

#ifdef __cplusplus
extern "C" {
#endif

#include "lwp_queue.h"
#include "lwp_priority.h"

typedef struct _lwpthrqueue {
	union {
		lwp_queue fifo;
		lwp_queue priority[LWP_THREADQ_NUM_PRIOHEADERS];
	} queues;
	u32 sync_state;
	u32 mode;
	u32 state;
	u32 timeout_state;
} lwp_thrqueue;

#ifdef __cplusplus
	}
#endif

#endif
