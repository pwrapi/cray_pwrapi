/*
 * Copyright (c) 2017, Cray Inc.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cray-powerapi/api.h>

#include "../common/common.h"

#define CONTEXT_NAME		"test_appos"

//
// main - Main entry point.
//
// Argument(s):
//
//	argc - Number of arguments
//	argv - Arguments
//
// Return Code(s):
//
//	int - Zero for success, non-zero for failure
//
int
main(int argc, char **argv)
{
	PWR_Cntxt context = NULL;
	PWR_Obj entry_point = NULL, ht_obj = NULL;
	PWR_SleepState sstate = 0;
	PWR_PerfState pstate = 0;
	PWR_Time latency = 0;
	PWR_Time latencies[PWR_NUM_SLEEP_STATES];
	int i = 0;

	//
	// Create a context for this test
	//
	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, CONTEXT_NAME,
		      &context, PWR_RET_SUCCESS);

	//
	// We need an object so get our entry point
	//
	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	get_ht_obj(context, entry_point, &ht_obj);

	for (i = PWR_SLEEP_NO;i < PWR_NUM_SLEEP_STATES;++i) {
		TST_SetSleepStateLimit(ht_obj, i, PWR_RET_SUCCESS);
		TST_GetSleepState(ht_obj, &sstate, PWR_RET_SUCCESS);
		// Removing the check below because multiple sleep states can
		// map to the same c-state on machines with few c-states to
		// choose from.
//		printf("Check that current Sleep State(%d) "
//			"equals the requested Sleep State(%d): ", sstate, i);
//		check_int_equal(sstate, i, EC_APPOS_SET_SLEEP_STATE_LIMIT);

		TST_WakeUpLatency(ht_obj, i, &latency, PWR_RET_SUCCESS);
		latencies[i] = latency;
	}

	for (latency = 0;latency <= 150;latency += 8) {
		TST_RecommendSleepState(ht_obj, latency, &sstate,
				PWR_RET_SUCCESS);
		printf("Check that the recommended sleep state(%d) has a lower "
			"latency(%lu) than the requested latency(%lu): ",
			sstate, latencies[sstate], latency);
		check_int_greater_than_equal(latency, latencies[sstate],
				EC_APPOS_RECOMMEND_SLEEP_STATE);
	}

	for (i = PWR_PERF_FASTEST;i < PWR_NUM_PERF_STATES;++i) {
		TST_SetPerfState(ht_obj, i, PWR_RET_SUCCESS);
		TST_GetPerfState(ht_obj, &pstate, PWR_RET_SUCCESS);
		// Removing the check below because multiple performance states
		// can map to the same frequency on machines with few
		// frequencies to choose from.
//		printf("Check that current Perf State(%d) "
//			"equals the set Perf State(%d): ", pstate, i);
//		check_int_equal(pstate, i, EC_APPOS_SET_PERF_STATE);
	}

	//
	// Destroy our context
	//
	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}
