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

#ifndef __OGC_LWP_MUTEX_H__
#define __OGC_LWP_MUTEX_H__

#include <gctypes.h>
#include <lwp_threadq.h>

#define LWP_MUTEX_LOCKED				0
#define LWP_MUTEX_UNLOCKED				1

#define LWP_MUTEX_NEST_ACQUIRE			0
#define LWP_MUTEX_NEST_ERROR			1
#define LWP_MUTEX_NEST_BLOCK			2

#define LWP_MUTEX_FIFO					0
#define LWP_MUTEX_PRIORITY				1
#define LWP_MUTEX_INHERITPRIO			2
#define LWP_MUTEX_PRIORITYCEIL			3

#define LWP_MUTEX_SUCCESSFUL			0
#define LWP_MUTEX_UNSATISFIED_NOWAIT	1
#define LWP_MUTEX_NEST_NOTALLOWED		2
#define LWP_MUTEX_NOTOWNER				3
#define LWP_MUTEX_DELETED				4
#define LWP_MUTEX_TIMEOUT				5
#define LWP_MUTEX_CEILINGVIOL			6

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwpmutexattr {
	u32 mode;
	u32 nest_behavior;
	u8 prioceil,onlyownerrelease;
} lwp_mutex_attr;

typedef struct _lwpmutex {
	lwp_thrqueue wait_queue;
	lwp_mutex_attr atrrs;
	u32 lock,nest_cnt,blocked_cnt;
	lwp_cntrl *holder;
} lwp_mutex;

void __lwp_mutex_initialize(lwp_mutex *mutex,lwp_mutex_attr *attrs,u32 init_lock);
u32 __lwp_mutex_surrender(lwp_mutex *mutex);
void __lwp_mutex_seize_irq_blocking(lwp_mutex *mutex,u64 timeout);
void __lwp_mutex_flush(lwp_mutex *mutex,u32 status);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_mutex.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
