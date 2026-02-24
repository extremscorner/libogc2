/*-------------------------------------------------------------

lock_supp.c -- devkitPPC lock support

Copyright (C) 2026 Extrems' Corner.org

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

#include <gctypes.h>
#include <ogc/cond.h>
#include <ogc/mutex.h>
#include <ogc/timesupp.h>
#include <sys/iosupport.h>
#include <sys/lock.h>

__LOCK_INIT_RECURSIVE(static, __malloc_recursive_mutex);

static mutex_t GetMutex(_LOCK_T *lock, bool recursive)
{
	if (*lock == __LOCK_INITIALIZER)
		LWP_MutexInit(lock, recursive);
	return *lock;
}

static cond_t GetCond(_COND_T *cond)
{
	if (*cond == __COND_INITIALIZER)
		LWP_CondInit(cond);
	return *cond;
}

void __SYSCALL(lock_acquire)(_LOCK_T *lock)
{
	LWP_MutexLock(GetMutex(lock, false));
}

int __SYSCALL(lock_try_acquire)(_LOCK_T *lock)
{
	return LWP_MutexTryLock(GetMutex(lock, false));
}

void __SYSCALL(lock_release)(_LOCK_T *lock)
{
	LWP_MutexUnlock(GetMutex(lock, false));
}

void __SYSCALL(lock_close)(_LOCK_T *lock)
{
	if (!LWP_MutexDestroy(*lock))
		*lock = LWP_MUTEX_NULL;
}

void __SYSCALL(lock_acquire_recursive)(_LOCK_RECURSIVE_T *lock)
{
	LWP_MutexLock(GetMutex(&lock->lock, true));
}

int __SYSCALL(lock_try_acquire_recursive)(_LOCK_RECURSIVE_T *lock)
{
	return LWP_MutexTryLock(GetMutex(&lock->lock, true));
}

void __SYSCALL(lock_release_recursive)(_LOCK_RECURSIVE_T *lock)
{
	LWP_MutexUnlock(GetMutex(&lock->lock, true));
}

void __SYSCALL(lock_close_recursive)(_LOCK_RECURSIVE_T *lock)
{
	__SYSCALL(lock_close)(&lock->lock);
}

int __SYSCALL(cond_signal)(_COND_T *cond)
{
	return LWP_CondSignal(GetCond(cond));
}

int __SYSCALL(cond_broadcast)(_COND_T *cond)
{
	return LWP_CondBroadcast(GetCond(cond));
}

int __SYSCALL(cond_wait)(_COND_T *cond, _LOCK_T *lock, uint64_t timeout_ns)
{
	struct timespec tv;

	if (timeout_ns == UINT64_MAX)
		return LWP_CondWait(GetCond(cond), GetMutex(lock, false));

	tv.tv_sec  = timeout_ns / TB_NSPERSEC;
	tv.tv_nsec = timeout_ns % TB_NSPERSEC;

	return LWP_CondTimedWait(GetCond(cond), GetMutex(lock, false), &tv);
}

void __SYSCALL(cond_close)(_COND_T *cond)
{
	if (!LWP_CondDestroy(*cond))
		*cond = LWP_COND_NULL;
}

int __SYSCALL(cond_wait_recursive)(_COND_T *cond, _LOCK_RECURSIVE_T *lock, uint64_t timeout_ns)
{
	struct timespec tv;

	if (timeout_ns == UINT64_MAX)
		return LWP_CondWait(GetCond(cond), GetMutex(&lock->lock, true));

	tv.tv_sec  = timeout_ns / TB_NSPERSEC;
	tv.tv_nsec = timeout_ns % TB_NSPERSEC;

	return LWP_CondTimedWait(GetCond(cond), GetMutex(&lock->lock, true), &tv);
}

void __SYSCALL(malloc_lock)(struct _reent *ptr)
{
	__SYSCALL(lock_acquire_recursive)(&__malloc_recursive_mutex);
}

void __SYSCALL(malloc_unlock)(struct _reent *ptr)
{
	__SYSCALL(lock_release_recursive)(&__malloc_recursive_mutex);
}
