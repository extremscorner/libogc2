/*-------------------------------------------------------------

threads_supp.c -- devkitPPC threads support

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
#include <ogc/lwp.h>
#include <sched.h>
#include <sys/iosupport.h>

int __SYSCALL(thread_create)(struct __pthread_t **thread, void *(*func)(void *), void *arg, void *stack_addr, size_t stack_size)
{
	if (stack_size == 0)
		stack_size = LWP_GetThreadStackSize(LWP_THREAD_NULL);

	return LWP_CreateThread((lwp_t *)thread, func, arg, stack_addr, stack_size, LWP_GetThreadPriority(LWP_THREAD_NULL));
}

void *__SYSCALL(thread_join)(struct __pthread_t *thread)
{
	void *value = NULL;
	LWP_JoinThread((lwp_t)thread, &value);
	return value;
}

int __SYSCALL(thread_detach)(struct __pthread_t *thread)
{
	return LWP_DetachThread((lwp_t)thread);
}

void __SYSCALL(thread_exit)(void *value)
{
	LWP_ExitThread(value);
}

struct __pthread_t *__SYSCALL(thread_self)(void)
{
	return (struct __pthread_t *)LWP_GetSelf();
}

int sched_yield(void)
{
	LWP_YieldThread();
	return 0;
}
