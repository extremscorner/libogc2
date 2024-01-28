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

#ifndef __OGC_LWP_SEMA_H__
#define __OGC_LWP_SEMA_H__

#include <gctypes.h>
#include <lwp_threadq.h>

#define LWP_SEMA_MODEFIFO				0
#define LWP_SEMA_MODEPRIORITY			1

#define LWP_SEMA_SUCCESSFUL				0
#define LWP_SEMA_UNSATISFIED_NOWAIT		1
#define LWP_SEMA_DELETED				2
#define LWP_SEMA_TIMEOUT				3
#define LWP_SEMA_MAXCNT_EXCEEDED		4

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwpsemattr {
	u32 max_cnt;
	u32 mode;
} lwp_semattr;

typedef struct _lwpsema {
	lwp_thrqueue wait_queue;
	lwp_semattr	attrs;
	u32 count;
} lwp_sema;

void __lwp_sema_initialize(lwp_sema *sema,lwp_semattr *attrs,u32 init_count);
u32 __lwp_sema_surrender(lwp_sema *sema,u32 id);
u32 __lwp_sema_seize(lwp_sema *sema,u32 id,u32 wait,u64 timeout);
void __lwp_sema_flush(lwp_sema *sema,u32 status);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_sema.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
