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

#include <processor.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <lwp_threads.h>
#include <lwp_wkspace.h>
#include <lwp_config.h>

#include "lwp_objmgr.h"

static u32 _lwp_objmgr_memsize = 0;
static lwp_obj *null_local_table = NULL;

u32 __lwp_objmgr_memsize(void)
{
	return _lwp_objmgr_memsize;
}

void __lwp_objmgr_initinfo(lwp_objinfo *info,u32 max_nodes,u32 node_size)
{
	u32 idx,i,size;
	lwp_obj *object;
	lwp_queue inactives;
	void **local_table;
	
	info->min_id = 0;
	info->max_id = 0;
	info->inactives_cnt = 0;
	info->node_size = node_size;
	info->max_nodes = max_nodes;
	info->obj_blocks = NULL;
	info->local_table = &null_local_table;

	__lwp_queue_init_empty(&info->inactives);

	size = ((info->max_nodes*sizeof(lwp_obj*))+(info->max_nodes*info->node_size));
	local_table = (void**)__lwp_wkspace_allocate(info->max_nodes*sizeof(lwp_obj*));
	if(!local_table) return;

	info->local_table = (lwp_obj**)local_table;
	for(i=0;i<info->max_nodes;i++) {
		local_table[i] = NULL;
	}

	info->obj_blocks = __lwp_wkspace_allocate(info->max_nodes*info->node_size);
	if(!info->obj_blocks) {
		__lwp_wkspace_free(local_table);
		return;
	}

	__lwp_queue_initialize(&inactives,info->obj_blocks,info->max_nodes,info->node_size);

	idx = info->min_id;
	while((object=(lwp_obj*)__lwp_queue_get(&inactives))!=NULL) {
		object->id = idx;
		object->information = NULL;
		__lwp_queue_append(&info->inactives,&object->node);
		idx++;
	}

	info->max_id += info->max_nodes;
	info->inactives_cnt += info->max_nodes;
	_lwp_objmgr_memsize += size;
}

lwp_obj* __lwp_objmgr_getisrdisable(lwp_objinfo *info,u32 id,u32 *p_level)
{
	u32 level;
	lwp_obj *object = NULL;

	_CPU_ISR_Disable(level);
	if(info->max_id>=id) {
		if((object=info->local_table[id])!=NULL) {
			*p_level = level;
			return object;
		}
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

lwp_obj* __lwp_objmgr_getnoprotection(lwp_objinfo *info,u32 id)
{
	lwp_obj *object = NULL;

	if(info->max_id>=id) {
		if((object=info->local_table[id])!=NULL) return object;
	}
	return NULL;
}

lwp_obj* __lwp_objmgr_get(lwp_objinfo *info,u32 id)
{
	lwp_obj *object = NULL;

	if(info->max_id>=id) {
		__lwp_thread_dispatchdisable();
		if((object=info->local_table[id])!=NULL) return object;
		__lwp_thread_dispatchenable();
	}
	return NULL;
}

lwp_obj* __lwp_objmgr_allocate(lwp_objinfo *info)
{
	u32 level;
	lwp_obj* object;

	_CPU_ISR_Disable(level);
	 object = (lwp_obj*)__lwp_queue_getI(&info->inactives);
	 if(object) {
		 object->information = info;
		 info->inactives_cnt--;
	 }
	_CPU_ISR_Restore(level);

	return object;
}

void __lwp_objmgr_free(lwp_objinfo *info,lwp_obj *object)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(&info->inactives,&object->node);
	object->information	= NULL;
	info->inactives_cnt++;
	_CPU_ISR_Restore(level);
}
