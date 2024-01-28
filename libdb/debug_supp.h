/*
 *    This software is Copyright (C) 1998 by T.sqware - all rights limited
 *    It is provided in to the public domain "as is", can be freely modified
 *    as far as this copyight notice is kept unchanged, but does not imply
 *    an endorsement by T.sqware of the product in which it is included.
 *
 *     Copyright (C) 2000 Quality Quorum, Inc.
 *
 *     All Rights Reserved
 *
 *     Permission to use, copy, modify, and distribute this software and its
 *     documentation for any purpose and without fee is hereby granted.
 *
 *     QQI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 *     ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 *     QQI BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 *     ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *     WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 *     ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 *     SOFTWARE.
 */

/*-------------------------------------------------------------

Copyright (C) 2008 - 2025
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

#ifndef __DEBUG_SUPP_H__
#define __DEBUG_SUPP_H__

#include <gctypes.h>

#define QM_MAXTHREADS			(20)

struct gdbstub_threadinfo {
	char display[256];
	char more_display[256];
	char name[256];
};

s32 gdbstub_getcurrentthread(void);
s32 hstr2nibble(const char *buf,s32 *nibble);
char* int2vhstr(char *buf,s32 val);
char* mem2hstr(char *buf,const char *mem,s32 count);
char* thread2vhstr(char *buf,s32 thread);
const char* vhstr2thread(const char *buf,s32 *thread);
lwp_cntrl* gdbstub_indextoid(s32 thread);
s32 gdbstub_getoffsets(char **textaddr,char **dataaddr,char **bssaddr);
s32 parsezbreak(const char *in,s32 *type,char **addr,s32 *len);
s32 gdbstub_getthreadinfo(s32 thread,struct gdbstub_threadinfo *info);
s32 parseqp(const char *in,s32 *mask,s32 *thread);
void packqq(char *out,s32 mask,s32 thread,struct gdbstub_threadinfo *info);
char* reserve_qmheader(char *out);
s32 parseql(const char *in,s32 *first,s32 *max_cnt,s32 *athread);
s32 gdbstub_getnextthread(s32 athread);
char* packqmthread(char *out,s32 thread);
void packqmheader(char *out,s32 count,s32 done,s32 athread);

#endif
