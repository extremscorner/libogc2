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

#ifndef __OGC_LWP_PRIORITY_INL__
#define __OGC_LWP_PRIORITY_INL__

static __inline__ void __lwp_priomap_init(prio_cntrl *theprio,u32 prio)
{
	u32 major,minor,mask;
	
	major = prio/16;
	minor = prio%16;
	
	theprio->minor = &_prio_bitmap[major];
	
	mask = 0x80000000>>major;
	theprio->ready_major = mask;
	theprio->block_major = ~mask;
	
	mask = 0x80000000>>minor;
	theprio->ready_minor = mask;
	theprio->block_minor = ~mask;
#ifdef _LWPPRIO_DEBUG
	printf("__lwp_priomap_init(%p,%d,%p,%d,%d,%d,%d)\n",theprio,prio,theprio->minor,theprio->ready_major,theprio->ready_minor,theprio->block_major,theprio->block_minor);
#endif
}

static __inline__ void __lwp_priomap_addto(prio_cntrl *theprio)
{
	*theprio->minor |= theprio->ready_minor;
	_prio_major_bitmap |= theprio->ready_major;
}

static __inline__ void __lwp_priomap_removefrom(prio_cntrl *theprio)
{
	*theprio->minor &= theprio->block_minor;
	if(*theprio->minor==0)
		_prio_major_bitmap &= theprio->block_major;
}

static __inline__ u32 __lwp_priomap_highest(void)
{
	u32 major,minor;
	major = cntlzw(_prio_major_bitmap);
	minor = cntlzw(_prio_bitmap[major]);
#ifdef _LWPPRIO_DEBUG
	printf("__lwp_priomap_highest(%d)\n",((major<<4)+minor));
#endif
	return ((major<<4)+minor);
}

#endif
