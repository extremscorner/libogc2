#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "mutex.h"

void __syscall_lock_init(_LOCK_T *lock)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;

	if(!lock) return;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,false);
	if(ret==0) *lock = (_LOCK_T)retlck;
}

void __syscall_lock_init_recursive(_LOCK_RECURSIVE_T *lock)
{
	s32 ret;
	mutex_t retlck = LWP_MUTEX_NULL;

	if(!lock) return;

	*lock = 0;
	ret = LWP_MutexInit(&retlck,true);
	if(ret==0) *lock = (_LOCK_RECURSIVE_T)retlck;
}

void __syscall_lock_close(_LOCK_T *lock)
{
	s32 ret;
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	ret = LWP_MutexDestroy(plock);
	if(ret==0) *lock = 0;
}

void __syscall_lock_acquire(_LOCK_T *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	LWP_MutexLock(plock);
}

int __syscall_lock_try_acquire(_LOCK_T *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return -1;

	plock = (mutex_t)*lock;
	return LWP_MutexTryLock(plock);
}

void __syscall_lock_release(_LOCK_T *lock)
{
	mutex_t plock;

	if(!lock || *lock==0) return;

	plock = (mutex_t)*lock;
	LWP_MutexUnlock(plock);
}

void flockfile(FILE *fp)
{
	__lock_acquire_recursive(fp->_lock);
}

int ftrylockfile(FILE *fp)
{
	return ({ __lock_try_acquire_recursive(fp->_lock); });
}

void funlockfile(FILE *fp)
{
	__lock_release_recursive(fp->_lock);
}
