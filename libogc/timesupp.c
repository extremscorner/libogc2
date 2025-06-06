#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>

#include "asm.h"
#include "processor.h"
#include "lwp_threads.h"
#include "lwp_watchdog.h"

#include <sys/time.h>

u64 gettime(void)
{
	u32 tmp;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	__asm__ __volatile__(
		"1:	mftbu	%0\n\
		    mftb	%1\n\
		    mftbu	%2\n\
			cmpw	%0,%2\n\
			bne		1b\n"
		: "=r" (v.ul[0]), "=r" (v.ul[1]), "=&r" (tmp)
	);
	return v.ull;
}

void settime(u64 t)
{
	u32 tmp;
	union uulc {
		u64 ull;
		u32 ul[2];
	} v;

	v.ull = t;
	__asm__ __volatile__ (
		"li		%0,0\n\
		 mttbl  %0\n\
		 mttbu  %1\n\
		 mttbl  %2\n"
		 : "=&r" (tmp)
		 : "r" (v.ul[0]), "r" (v.ul[1])
	);
}

u32 diff_sec(u64 start,u64 end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_secs(diff);
}

u32 diff_msec(u64 start,u64 end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_millisecs(diff);
}

u32 diff_usec(u64 start,u64 end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_microsecs(diff);
}

u32 diff_nsec(u64 start,u64 end)
{
	u64 diff;

	diff = diff_ticks(start,end);
	return ticks_to_nanosecs(diff);
}

int __syscall_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	u64 now;
	switch (clock_id) {
		case CLOCK_REALTIME:
			now = gettime();
			tp->tv_sec = ticks_to_secs(now) + 946684800;
			tp->tv_nsec = tick_nanosecs(now);
			return 0;
		case CLOCK_MONOTONIC:
			now = __SYS_GetSystemTime();
			tp->tv_sec = ticks_to_secs(now);
			tp->tv_nsec = tick_nanosecs(now);
			return 0;
		default:
			errno = EINVAL;
			return -1;
	}
}

int __syscall_clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	u64 now;
	switch (clock_id) {
		case CLOCK_REALTIME:
			now = secs_to_ticks(tp->tv_sec - 946684800);
			now += nanosecs_to_ticks(tp->tv_nsec);
			__SYS_SetTime(now);
			return 0;
		default:
			errno = EINVAL;
			return -1;
	}
}

int __syscall_clock_getres(clockid_t clock_id, struct timespec *res)
{
	switch (clock_id) {
		case CLOCK_REALTIME:
		case CLOCK_MONOTONIC:
			res->tv_sec = ticks_to_secs(1);
			res->tv_nsec = tick_nanosecs(1);
			return 0;
		default:
			errno = EINVAL;
			return -1;
	}
}

// this function spins till timeout is reached
void udelay(u32 usec)
{
	u64 start, end;
	start = gettime();
	while (1)
	{
		end = gettime();
		if (diff_usec(start,end) >= usec)
			break;
	}
}

int __syscall_nanosleep(const struct timespec *tb, struct timespec *rem)
{
	s64 timeout;

	if (!__lwp_wd_timespec_valid(tb)) {
		errno = EINVAL;
		return -1;
	}

	timeout = __lwp_wd_calc_ticks(tb);
	if (timeout <= 0) {
		__lwp_thread_dispatchdisable();
		__lwp_thread_yield();
		__lwp_thread_dispatchenable();
		return 0;
	}

	__lwp_thread_dispatchdisable();
	__lwp_thread_setstate(_thr_executing,LWP_STATES_DELAYING|LWP_STATES_INTERRUPTIBLE_BY_SIGNAL);
	__lwp_wd_initialize(&_thr_executing->timer,__lwp_thread_delayended,_thr_executing->object.id,_thr_executing);
	__lwp_wd_insert_ticks(&_thr_executing->timer,timeout);
	__lwp_thread_dispatchenable();
	return 0;
}

int __syscall_gettod_r(struct _reent *ptr, struct timeval *tp, struct timezone *tz)
{
	u64 now;
	if (tp != NULL) {
		now = gettime();
		tp->tv_sec = ticks_to_secs(now) + 946684800;
		tp->tv_usec = tick_microsecs(now);
	}
	if (tz != NULL) {
		tz->tz_minuteswest = 0;
		tz->tz_dsttime = 0;
	}
	return 0;
}
