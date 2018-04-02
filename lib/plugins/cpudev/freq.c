/*
 * Copyright (c) 2015-2017, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
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
 * This file contains functions for accessing frequency related attributes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include <cray-powerapi/api.h>
#include <cray-powerapi/types.h>
#include <log.h>

#include "freq.h"
#include "../common/file.h"
#include "../common/paths.h"

/*
 * ps_to_freq - Because the underlying processor can have an arbitrary number
 *	        of frequencies, we need to be able to map between them and
 *	        our statically sized PWR_PerfState.
 */
static int ps_to_freq[PWR_NUM_PERF_STATES] = { 0 };

static void
init_ps_to_freq(int num_freqs)
{
	int jump = 0;
	int i = 0;

	TRACE1_ENTER("num_freqs = %d", num_freqs);

	for (i = 0; i < PWR_NUM_PERF_STATES; ++i) {
		ps_to_freq[i] = 0;
	}

	// map PERF_FASTEST to the highest frequency's index, and PERF_SLOWEST
	// to the lowest frequency's index.
	ps_to_freq[PWR_PERF_FASTEST] = 0;
	if (num_freqs > 1) {
		ps_to_freq[PWR_PERF_SLOWEST] = num_freqs - 1;
	} else {
		// all performance states will map to the same frequency
		return;
	}

	// map PERF_FAST, PERF_MEDIUM and PERF_SLOW evenly into the
	// remaining frequencies.
	jump = (num_freqs - 2) / 3;
	if (jump < 1) {
		// There aren't 3 frequencies remaining
		for (i = PWR_PERF_FAST; i < PWR_PERF_SLOWEST; ++i) {
			ps_to_freq[i] = 1;
		}
		if (num_freqs == 4) {
			ps_to_freq[PWR_PERF_SLOW] = 2;
		}
	} else {
		ps_to_freq[PWR_PERF_FAST] = jump;
		ps_to_freq[PWR_PERF_MEDIUM] = jump * 2;
		ps_to_freq[PWR_PERF_SLOW] = jump * 3;
	}

	TRACE1_EXIT("");
}

// map a PWR_PerfState to an underlying frequency
int
map_ps_to_freq(PWR_PerfState pstate)
{
	int ret = -1;

	TRACE1_ENTER("pstate = %d", pstate);

	ret = ps_to_freq[pstate];
	if (ret == -1) {
		LOG_FAULT("Invalid PerfState(%d) specified for this ht.",
				pstate);
	}

	TRACE1_EXIT("ret = %d", ret);

	return ret;
}

// map a frequency to a PWR_PerfState
int
map_freq_to_ps(int freq_idx)
{
	int ret = -1;
	int i = 0;

	TRACE1_ENTER("freq_idx = %d", freq_idx);

	for (i = 0; i < PWR_NUM_PERF_STATES; ++i) {
		if (ps_to_freq[i] >= freq_idx) {
			ret = i;
			break;
		}
	}

	if (ret == -1) {
		LOG_FAULT("Invalid frequency index(%d) specified for this ht.",
				freq_idx);
	}

	TRACE1_EXIT("ret = %d", ret);

	return ret;
}

static double *freqs = NULL;
static int num_freqs = 0;

/*
 * init_freqs - Frequency attribute initialization.
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
init_freqs(PWR_Obj obj, int *num_vals, double **vals)
{
	int retval = PWR_RET_FAILURE;
	uint64_t val = 0;
	int i = 0;

	TRACE2_ENTER("num_vals = %p, vals = %p", num_vals, vals);

	/*
	 * PWR_ATTR_FREQ and PWR_ATTR_FREQ_LIMIT_{MIN|MAX} all point to
	 * the same data.
	 */
	if (freqs != NULL)
		goto success;

	/*
	 * First find out how many frequencies there are.
	 */
	retval = PWR_ObjAttrGetMeta(obj, PWR_ATTR_FREQ_REQ, PWR_MD_NUM, &val);
	if (retval != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine the number of processor"
				" frequencies, retval = %d.", retval);
		goto done;
	}
	num_freqs = val;
	if (num_freqs <= 0) {
		LOG_FAULT("Invalid number of processor frequencies(%d).",
				num_freqs);
		goto done;
	}

	init_ps_to_freq(num_freqs);

	/*
	 * Allocate enough memory for all of the frequencies.
	 */
	freqs = g_malloc0(sizeof(*freqs) * num_freqs);
	if (freqs == NULL) {
		LOG_FAULT("Unable to allocate memory for processor frequencies.");
		goto done;
	}

	/*
	 * Iterate, recording each frequency.  Store them in reverse order so
	 * largest frequencies are first.
	 */
	for (i = 0; i < num_freqs; ++i) {
		double dval = 0.0;

		retval = PWR_MetaValueAtIndex(obj, PWR_ATTR_FREQ_REQ,
				num_freqs - i - 1, &dval, NULL);
		if (retval != PWR_RET_SUCCESS) {
			LOG_FAULT("Unable to read frequency at index %d,"
				       " retval = %d.", i, retval);
			goto done;
		}
		freqs[i] = dval;
	}

success:
	/*
	 * Point return pointers to correct data.
	 */
	*vals = freqs;
	*num_vals = num_freqs;
	retval = PWR_RET_SUCCESS;

done:
	if (retval != PWR_RET_SUCCESS) {
		g_free(freqs);
		freqs = NULL;
		num_freqs = 0;
	}

	TRACE2_EXIT("retval = %d, *num_vals = %d, *vals = %p",
			retval, *num_vals, *vals);

	return retval;
}
