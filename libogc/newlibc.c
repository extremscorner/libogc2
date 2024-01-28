/*
 *  COPYRIGHT (c) 1994 by Division Incorporated
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *      for any purpose is hereby granted without fee, provided that
 *      the above copyright notice and this notice appears in all
 *      copies, and that the name of Division Incorporated not be
 *      used in advertising or publicity pertaining to distribution
 *      of the software without specific, written prior permission.
 *      Division Incorporated makes no representations about the
 *      suitability of this software for any purpose.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include "sys_state.h"
#include "lwp_threads.h"

int libc_reentrant;
struct _reent libc_globl_reent;

extern void _reclaim_reent(struct _reent *);

int __libc_create_hook(lwp_cntrl *curr_thr,lwp_cntrl *create_thr)
{
	create_thr->libc_reent = NULL;
	return 1;
}

int __libc_start_hook(lwp_cntrl *curr_thr,lwp_cntrl *start_thr)
{
	struct _reent *ptr;

	ptr = (struct _reent*)calloc(1,sizeof(struct _reent));
	if(!ptr) abort();

	_REENT_INIT_PTR((ptr));

	start_thr->libc_reent = ptr;
	return 1;
}

int __libc_delete_hook(lwp_cntrl *curr_thr, lwp_cntrl *delete_thr)
{
	struct _reent *ptr;

	if(curr_thr==delete_thr)
		ptr = _impure_ptr;
	else
		ptr = (struct _reent*)delete_thr->libc_reent;

	if(ptr && ptr!=&libc_globl_reent) {
		_reclaim_reent(ptr);
		free(ptr);
	}
	delete_thr->libc_reent = NULL;

	if(curr_thr==delete_thr) _impure_ptr = NULL;

	return 1;
}

void __libc_init(int reentrant)
{
	libc_globl_reent = (struct _reent)_REENT_INIT((libc_globl_reent));
	_impure_ptr = &libc_globl_reent;

	if(reentrant) {
		__lwp_thread_setlibcreent((void*)&_impure_ptr);
		libc_reentrant = reentrant;
	}
}
