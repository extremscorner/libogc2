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

#ifndef __OGC_SYS_STATE_INL__
#define __OGC_SYS_STATE_INL__

static __inline__ void __sys_state_init(void)
{
	_sys_state_curr = SYS_STATE_BEFORE_INIT;
}

static __inline__ void __sys_state_set(u32 sys_state)
{
	_sys_state_curr = sys_state;
}

static __inline__ u32 __sys_state_get(void)
{
	return _sys_state_curr;
}

static __inline__ u32 __sys_state_beforeinit(u32 statecode)
{
	return (statecode==SYS_STATE_BEFORE_INIT);
}

static __inline__ u32 __sys_state_beforemultitasking(u32 statecode)
{
	return (statecode==SYS_STATE_BEFORE_MT);
}

static __inline__ u32 __sys_state_beginmultitasking(u32 statecode)
{
	return (statecode==SYS_STATE_BEGIN_MT);
}

static __inline__ u32 __sys_state_up(u32 statecode)
{
	return (statecode==SYS_STATE_UP);
}

static __inline__ u32 __sys_state_failed(u32 statecode)
{
	return (statecode==SYS_STATE_FAILED);
}

#endif
