/*
 * Copyright (c) 2017, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.

 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _PWR_LIB_TIMER_H
#define _PWR_LIB_TIMER_H

#include <time.h>

static inline void
pwr_tspec_add(struct timespec *a, struct timespec *b, struct timespec *res)
{
	res->tv_sec = a->tv_sec + b->tv_sec;
	res->tv_nsec = a->tv_nsec + b->tv_nsec;
	if (res->tv_nsec > NSEC_PER_SEC) {
		res->tv_sec++;
		res->tv_nsec -= NSEC_PER_SEC;
	}
}

static inline void
pwr_tspec_sub(struct timespec *a, struct timespec *b, struct timespec *res)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (a->tv_nsec < b->tv_nsec) {
		res->tv_sec--;
		res->tv_nsec += NSEC_PER_SEC;
	}
}

static inline double
pwr_tspec_to_sec(struct timespec *ts)
{
	return ts->tv_sec + (ts->tv_nsec / (double)NSEC_PER_SEC);
}

static inline PWR_Time
pwr_tspec_to_nsec(struct timespec *ts)
{
	return (ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

static inline double
pwr_tspec_diff(struct timespec *a, struct timespec *b)
{
	struct timespec res;

	pwr_tspec_sub(a, b, &res);

	return pwr_tspec_to_sec(&res);
}

int pwr_nanosleep(PWR_Time sleep_time);

static inline PWR_Time
pwr_usec_to_nsec(uint64_t usec)
{
	return (usec * NSEC_PER_USEC);
}

static inline uint64_t
pwr_nsec_to_usec(PWR_Time nsec)
{
	return (nsec / NSEC_PER_USEC);
}


#endif /* _PWR_LIB_TIMER_H */
