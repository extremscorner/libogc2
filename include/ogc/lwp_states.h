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

#ifndef __OGC_LWP_STATES_H__
#define __OGC_LWP_STATES_H__

#include <gctypes.h>

#define LWP_STATES_READY					0x00000000
#define LWP_STATES_DORMANT					0x00000001
#define LWP_STATES_SUSPENDED				0x00000002
#define LWP_STATES_TRANSIENT				0x00000004
#define LWP_STATES_DELAYING					0x00000008
#define LWP_STATES_WAITING_FOR_TIME			0x00000010
#define LWP_STATES_WAITING_FOR_BUFFER		0x00000020
#define LWP_STATES_WAITING_FOR_SEGMENT		0x00000040
#define LWP_STATES_WAITING_FOR_MESSAGE		0x00000080
#define LWP_STATES_WAITING_FOR_EVENT		0x00000100
#define LWP_STATES_WAITING_FOR_MUTEX		0x00000200
#define LWP_STATES_WAITING_FOR_SEMAPHORE	0x00000400
#define LWP_STATES_WAITING_FOR_CONDVAR		0x00000800
#define LWP_STATES_WAITING_FOR_JOIN			0x00001000
#define LWP_STATES_WAITING_FOR_RPCREPLY		0x00002000
#define LWP_STATES_WAITING_FOR_PERIOD		0x00004000
#define LWP_STATES_WAITING_FOR_SIGNAL		0x00008000
#define LWP_STATES_WAITING_FOR_JOINATEXIT	0x00010000
#define LWP_STATES_INTERRUPTIBLE_BY_SIGNAL	0x10000000

#define LWP_STATES_LOCALLY_BLOCKED			(LWP_STATES_WAITING_FOR_BUFFER | LWP_STATES_WAITING_FOR_SEGMENT |	\
											 LWP_STATES_WAITING_FOR_MESSAGE | LWP_STATES_WAITING_FOR_SEMAPHORE |	\
											 LWP_STATES_WAITING_FOR_MUTEX | LWP_STATES_WAITING_FOR_CONDVAR |	\
											 LWP_STATES_WAITING_FOR_JOIN | LWP_STATES_WAITING_FOR_SIGNAL)

#define LWP_STATES_WAITING_ON_THREADQ		(LWP_STATES_LOCALLY_BLOCKED | LWP_STATES_WAITING_FOR_RPCREPLY)

#define LWP_STATES_BLOCKED					(LWP_STATES_DELAYING | LWP_STATES_WAITING_FOR_TIME |	\
											 LWP_STATES_WAITING_FOR_PERIOD | LWP_STATES_WAITING_FOR_EVENT |	\
											 LWP_STATES_WAITING_ON_THREADQ | LWP_STATES_INTERRUPTIBLE_BY_SIGNAL)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_states.inl>
#endif
	
#ifdef __cplusplus
	}
#endif

#endif
