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

#ifndef __OGC_LWP_STATES_INL__
#define __OGC_LWP_STATES_INL__

static __inline__ u32 __lwp_setstate(u32 curr_state,u32 stateset)
{
	return (curr_state|stateset);
}

static __inline__ u32 __lwp_clearstate(u32 curr_state,u32 stateclear)
{
	return (curr_state&~stateclear);
}

static __inline__ u32 __lwp_stateready(u32 curr_state)
{
	return (curr_state==LWP_STATES_READY);
}

static __inline__ u32 __lwp_stateonlydormant(u32 curr_state)
{
	return (curr_state==LWP_STATES_DORMANT);
}

static __inline__ u32 __lwp_statedormant(u32 curr_state)
{
	return (curr_state&LWP_STATES_DORMANT);
}

static __inline__ u32 __lwp_statesuspended(u32 curr_state)
{
	return (curr_state&LWP_STATES_SUSPENDED);
}

static __inline__ u32 __lwp_statetransient(u32 curr_state)
{
	return (curr_state&LWP_STATES_TRANSIENT);
}

static __inline__ u32 __lwp_statedelaying(u32 curr_state)
{
	return (curr_state&LWP_STATES_DELAYING);
}

static __inline__ u32 __lwp_statewaittime(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_TIME);
}

static __inline__ u32 __lwp_statewaitbuffer(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_BUFFER);
}

static __inline__ u32 __lwp_statewaitsegment(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_SEGMENT);
}

static __inline__ u32 __lwp_statewaitmessage(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_MESSAGE);
}

static __inline__ u32 __lwp_statewaitevent(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_EVENT);
}

static __inline__ u32 __lwp_statewaitmutex(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_MUTEX);
}

static __inline__ u32 __lwp_statewaitsemaphore(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_SEMAPHORE);
}

static __inline__ u32 __lwp_statewaitrpcreply(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_RPCREPLY);
}

static __inline__ u32 __lwp_statewaitperiod(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_PERIOD);
}

static __inline__ u32 __lwp_statewaitjoinatexit(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_FOR_JOINATEXIT);
}

static __inline__ u32 __lwp_statewaitlocallyblocked(u32 curr_state)
{
	return (curr_state&LWP_STATES_LOCALLY_BLOCKED);
}

static __inline__ u32 __lwp_statewaitthreadqueue(u32 curr_state)
{
	return (curr_state&LWP_STATES_WAITING_ON_THREADQ);
}

static __inline__ u32 __lwp_stateblocked(u32 curr_state)
{
	return (curr_state&LWP_STATES_BLOCKED);
}

static __inline__ u32 __lwp_statesset(u32 curr_state,u32 mask)
{
	return ((curr_state&mask)!=LWP_STATES_READY);
}

#endif
