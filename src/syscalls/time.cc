/* © 2010 David Given.
 * LBW is licensed under the MIT open source license. See the COPYING
 * file in this distribution for the full text.
 */

#include "globals.h"
#include "syscalls.h"
#include <sys/times.h>
#include <sys/time.h>
#include <time.h>

#define LINUX_CLOCK_REALTIME  0
#define LINUX_CLOCK_MONOTONIC 1

/* compat_timeval is compatible with Interix */
SYSCALL(compat_sys_gettimeofday)
{
	struct timeval& lt = *(struct timeval*) arg.a0.p;

	int result = gettimeofday(&lt, NULL);
	CheckError(result);
	return 0;
}

SYSCALL(compat_sys_time)
{
	u_int32_t* tp = (u_int32_t*) arg.a0.p;

	time_t t = time(NULL);
	if (tp)
		*tp = t;
	return t;
}

/* compat_timespec is compatible with Interix */
SYSCALL(compat_sys_clock_gettime)
{
	int which_clock = arg.a0.s;
	struct timespec& lt = *(struct timespec*) arg.a1.p;

	switch (which_clock)
	{
		case LINUX_CLOCK_REALTIME:
		case LINUX_CLOCK_MONOTONIC:
		{
			struct timeval it;
			int result = gettimeofday(&it, NULL);
			if (result == -1)
				throw EINVAL;

			lt.tv_sec = it.tv_sec;
			lt.tv_nsec = it.tv_usec * 1000;
			break;
		}

		default:
			throw EINVAL;
	}

	return 0;
}

SYSCALL(compat_sys_clock_getres)
{
	int which_clock = arg.a0.s;
	struct timespec& lt = *(struct timespec*) arg.a1.p;

	switch (which_clock)
	{
		case LINUX_CLOCK_REALTIME:
		case LINUX_CLOCK_MONOTONIC:
		{
			lt.tv_sec = 0;
			lt.tv_nsec = 1000000000LL / CLOCKS_PER_SEC;
			break;
		}

		default:
			throw EINVAL;
	}

	return 0;
}

/* struct itermval is compatible; timer names are compatible */
SYSCALL(compat_sys_setitimer)
{
	int which = arg.a0.s;
	const struct itimerval* value = (const struct itimerval*) arg.a1.p;
	struct itimerval* ovalue = (struct itimerval*) arg.a2.p;

	int i = setitimer(which, value, ovalue);
	if (i == -1)
		throw errno;
	return 0;
}

SYSCALL(compat_sys_nanosleep)
{
	const struct timespec* req = (const struct timespec*) arg.a0.p;
	struct timespec* rem = (struct timespec*) arg.a1.p;

	struct timeval start;
	gettimeofday(&start, NULL);

	struct timeval tv;
	tv.tv_sec = req->tv_sec;
	tv.tv_usec = req->tv_nsec / 1000;

	int result = select(0, NULL, NULL, NULL, &tv);
	if (rem)
	{
		u64 start_us = (u64)start.tv_sec * 1000000LL + (u64)start.tv_usec;

		gettimeofday(&tv, NULL);
		u64 now_us = (u64)tv.tv_sec * 1000000LL + (u64)tv.tv_usec;
		u64 delta_us = now_us - start_us;

		rem->tv_sec = delta_us / 1000000;
		rem->tv_nsec = (delta_us % 1000000) * 1000;
	}

	if (result == -1)
		throw errno;
	return 0;
}

SYSCALL(sys_alarm)
{
	unsigned int t = arg.a0.u;

	int i = alarm(t);
	CheckError(i);
	return 0;
}

/* compat_tms is compatible. */
SYSCALL(compat_sys_times)
{
	struct tms* tms = (struct tms*) arg.a0.p;

	clock_t t = times(tms);
	CheckError(t);
	return t;
}
