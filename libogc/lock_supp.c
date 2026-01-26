#include <gctypes.h>
#include <ogc/mutex.h>
#include <stdio.h>
#include <sys/iosupport.h>
#include <sys/lock.h>

static mutex_t GetMutex(_LOCK_T *lock, bool recursive)
{
	if (*lock == __LOCK_INITIALIZER)
		LWP_MutexInit(lock, recursive);
	return *lock;
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
	mutex_t mutex = *lock;
	*lock = __LOCK_INITIALIZER;
	LWP_MutexDestroy(mutex);
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

void flockfile(FILE *fp)
{
	__lock_acquire_recursive(fp->_lock);
}

int ftrylockfile(FILE *fp)
{
	return __lock_try_acquire_recursive(fp->_lock);
}

void funlockfile(FILE *fp)
{
	__lock_release_recursive(fp->_lock);
}
