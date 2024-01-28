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

#ifndef __OGC_LWP_OBJMGR_H__
#define __OGC_LWP_OBJMGR_H__

#include <gctypes.h>
#include "lwp_queue.h"

#define LWP_OBJMASKTYPE(type)		((type)<<16)
#define LWP_OBJMASKID(id)			((id)&0xffff)
#define LWP_OBJTYPE(id)				((id)>>16)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwp_objinfo lwp_objinfo;

typedef struct _lwp_obj {
	lwp_node node;
	s32 id;
	lwp_objinfo *information;
} lwp_obj;

struct _lwp_objinfo {
	u32 min_id;
	u32 max_id;
	u32 max_nodes;
	u32 node_size;
	lwp_obj **local_table;
	void *obj_blocks;
	lwp_queue inactives;
	u32 inactives_cnt;
};

void __lwp_objmgr_initinfo(lwp_objinfo *info,u32 max_nodes,u32 node_size);
void __lwp_objmgr_free(lwp_objinfo *info,lwp_obj *object);
lwp_obj* __lwp_objmgr_allocate(lwp_objinfo *info);
lwp_obj* __lwp_objmgr_get(lwp_objinfo *info,u32 id);
lwp_obj* __lwp_objmgr_getisrdisable(lwp_objinfo *info,u32 id,u32 *p_level);
lwp_obj* __lwp_objmgr_getnoprotection(lwp_objinfo *info,u32 id);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_objmgr.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
