/*
 * Copyright (c) 2015-2018, Cray Inc.
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
 * This file contains source code to determine the number of c-states on the
 * compute node, and the latencies involved for each of the c-states to
 * return to C0.  There are also helper functions to translate between
 * c-states and PWR_SleepStates.
 */

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include <cray-powerapi/api.h>
#include <cray-powerapi/types.h>
#include <log.h>

#include "cstate.h"
#include "../common/file.h"
#include "../common/paths.h"

/*
 * ss_to_cs - Because the underlying processor can have an arbitrary number
 *	      of c-states, we need to be able to map between them and
 *	      our statically sized PWR_SleepState.
 */
static int ss_to_cs[PWR_NUM_SLEEP_STATES] = { 0 };

static void
init_ss_to_cs(int num_cstates)
{
	int jump = 0;
	int i = 0;

	TRACE1_ENTER("num_cstates = %d", num_cstates);

	for (i = 0; i < PWR_NUM_SLEEP_STATES; ++i) {
		ss_to_cs[i] = 0;
	}

	// map SLEEP_NO to the lowest c-state, and SLEEP_DEEPEST to the
	// highest c-state.
	ss_to_cs[PWR_SLEEP_NO] = 0;
	if (num_cstates > 1) {
		ss_to_cs[PWR_SLEEP_DEEPEST] = num_cstates - 1;
	} else {
		// all sleep states will map to c-state C0
		return;
	}

	// map SLEEP_SHALLOW, SLEEP_MEDIUM and SLEEP_DEEP evenly into the
	// remaining c-states.
	jump = (num_cstates - 2) / 3;
	if (jump < 1) {
		// There aren't 3 c-states remaining
		for (i = PWR_SLEEP_SHALLOW; i < PWR_SLEEP_DEEPEST; ++i) {
			ss_to_cs[i] = 1;
		}
		if (num_cstates == 4) {
			ss_to_cs[PWR_SLEEP_DEEP] = 2;
		}
	} else {
		ss_to_cs[PWR_SLEEP_SHALLOW] = jump;
		ss_to_cs[PWR_SLEEP_MEDIUM] = jump * 2;
		ss_to_cs[PWR_SLEEP_DEEP] = jump * 3;
	}

	TRACE1_EXIT("");
}

// map a PWR_SleepState to an underlying c-state
int
map_ss_to_cs(PWR_SleepState sstate)
{
	int ret = -1;

	TRACE1_ENTER("sstate = %d", sstate);

	ret = ss_to_cs[sstate];
	if (ret == -1) {
		LOG_FAULT("Invalid SleepState(%d) specified for this ht.",
				sstate);
	}

	TRACE1_EXIT("ret = %d", ret);

	return ret;
}

// map a c-state to a PWR_SleepState
int
map_cs_to_ss(int cstate)
{
	int ret = -1;
	int i = 0;

	TRACE1_ENTER("cstate = %d", cstate);

	for (i = 0; i < PWR_NUM_SLEEP_STATES; ++i) {
		if (ss_to_cs[i] >= cstate) {
			ret = i;
			break;
		}
	}

	if (ret == -1) {
		LOG_FAULT("Invalid c-state(%d) specified for this ht.",
				cstate);
	}

	TRACE1_EXIT("ret = %d", ret);

	return ret;
}

static int64_t	*latencies = NULL;
static int	num_cstates = 0;

/*
 * init_cstate_limits - C-state attribute initialization.
 *
 * Argument(s):
 *
 *	num_vals - Will hold the number of discrete C-States discovered
 *	lats     - Will hold an array of latencies for each c-state to
 *	           return to C0
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
init_cstate_limits(PWR_Obj obj, int *num_vals, int64_t **lats)
{
	int retval = PWR_RET_FAILURE;
	uint64_t val = 0;
	int i = 0;

	TRACE2_ENTER("num_vals = %p, lats = %p", num_vals, lats);

	if (latencies != NULL)
		goto success;

	/*
	 * First find out how many C-States there are.
	 */
	retval = PWR_ObjAttrGetMeta(obj, PWR_ATTR_CSTATE_LIMIT, PWR_MD_NUM, &val);
	if (retval != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine the number of processor"
				" c-states, retval = %d.", retval);
		goto done;
	}
	num_cstates = val;
	if (num_cstates <= 0) {
		LOG_FAULT("Invalid number of c-states(%d).", num_cstates);
		goto done;
	}

	init_ss_to_cs(num_cstates);

	/*
	 * Allocate enough memory for all of the cstates' latencies.
	 */
	latencies = g_malloc0(sizeof(*latencies) * num_cstates);
	if (latencies == NULL) {
		LOG_FAULT("Unable to allocate memory for c-state latencies.");
		goto done;
	}

	/*
	 * Iterate through each C-State, recording its latency.
	 */
	for (i = 0; i < num_cstates; ++i) {
		char path[80];

		snprintf(path, sizeof(path), CSTATE_LATENCY_PATH, i);
		retval = read_val_from_file(path, &val, TYPE_UINT64, NULL);
		if (retval != PWR_RET_SUCCESS) {
			LOG_FAULT("Unable to read c-state latency from %s.",
					path);
			goto done;
		}
		latencies[i] = val;
	}

success:
	/*
	 * Point return pointers to correct data.
	 */
	*lats = latencies;
	*num_vals = num_cstates;
	retval = PWR_RET_SUCCESS;

done:
	if (retval != PWR_RET_SUCCESS) {
		g_free(latencies);
		latencies = NULL;
		num_cstates = 0;
	}

	TRACE2_EXIT("retval = %d, *num_vals = %d, *lats = %p",
			retval, *num_vals, *lats);

	return retval;
}
