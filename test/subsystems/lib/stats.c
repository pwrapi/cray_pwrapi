/*
 * Copyright (c) 2016-2017, Cray Inc.
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
#include <stdbool.h>
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <glib.h>

#include <cray-powerapi/api.h>

#include "../common/common.h"

#define CONTEXT_NAME		"test_stats"

static void
check_stat_value(double value, int exit_code)
{
	printf("Check for valid stat value: ");
	check_double_greater_than(value, 0.0, exit_code);
}

static void
check_time_period(PWR_TimePeriod *tp, bool check_instant, int exit_code)
{
	printf("Check for valid time period: ");

	// Do some basic logic checks on the time period
	if (tp->start == 0) {
		printf("FAIL (start time=%"PRIu64" == 0)\n", tp->start);
		exit(exit_code);
	}
	if (tp->stop == 0) {
		printf("FAIL (stop time=%"PRIu64" == 0)\n", tp->stop);
		exit(exit_code);
	}

	if (tp->start > tp->stop) {
		printf("FAIL (start time=%"PRIu64" > stop time=%"PRIu64")\n",
			tp->start, tp->stop);
		exit(exit_code);
	}

	if (check_instant) {
		if (tp->instant == 0) {
			printf("FAIL (instant time=%"PRIu64" == 0)\n",
				tp->instant);
			exit(exit_code);
		}
		if ((tp->instant < tp->start) ||
		    (tp->instant > tp->stop)) {
			printf("FAIL (instant time=%"PRIu64" should be between"
				" start time=%"PRIu64" and stop time=%"PRIu64
				")\n", tp->instant, tp->start, tp->stop);
			exit(exit_code);
		}
	} else {
		if (tp->instant != 0) {
			printf("FAIL (instant time=%"PRIu64" != 0)\n",
				tp->instant);
			exit(exit_code);
		}
	}

	printf("PASS\n");
}

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
	PWR_Obj entry_point = NULL, sock_obj = NULL;
	PWR_Stat stat1 = NULL,
		 stat2 = NULL,
		 stat3 = NULL,
		 stat4 = NULL,
		 stat5 = NULL,
		 statx = NULL;
	PWR_TimePeriod times1 = {};
	double value1 = 0;

	//
	// Create a context for this test
	//
	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, CONTEXT_NAME,
		      &context, PWR_RET_SUCCESS);

	//
	// We need an object to create a statistic so get our entry point
	//
	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	get_socket_obj(context, entry_point, &sock_obj);

	//
	// Statistics are only supported on POWER, ENERGY and TEMP attributes.
	// Statistics only support MIN, MAX, AVG, and STDEV.
	//
	TST_StatCreateObj(sock_obj, PWR_ATTR_FREQ, PWR_ATTR_STAT_MAX,
			  &statx, PWR_RET_NOT_IMPLEMENTED);

	TST_StatCreateObj(sock_obj, PWR_ATTR_POWER, PWR_ATTR_STAT_CV,
			  &statx, PWR_RET_NOT_IMPLEMENTED);

	//
	// Create a new statistic based on an object.
	//
	TST_StatCreateObj(sock_obj, PWR_ATTR_TEMP, PWR_ATTR_STAT_MAX,
			  &stat1, PWR_RET_SUCCESS);

	TST_StatCreateObj(sock_obj, PWR_ATTR_POWER, PWR_ATTR_STAT_MIN,
			  &stat2, PWR_RET_SUCCESS);

	TST_StatCreateObj(sock_obj, PWR_ATTR_POWER, PWR_ATTR_STAT_AVG,
			  &stat3, PWR_RET_SUCCESS);

	TST_StatCreateObj(sock_obj, PWR_ATTR_ENERGY, PWR_ATTR_STAT_AVG,
			  &stat4, PWR_RET_SUCCESS);

	TST_StatCreateObj(sock_obj, PWR_ATTR_POWER, PWR_ATTR_STAT_STDEV,
			  &stat5, PWR_RET_SUCCESS);

	TST_StatStart(stat1, PWR_RET_SUCCESS);
	TST_StatStart(stat2, PWR_RET_SUCCESS);
	TST_StatStart(stat3, PWR_RET_SUCCESS);
	TST_StatStart(stat4, PWR_RET_SUCCESS);
	TST_StatStart(stat5, PWR_RET_SUCCESS);
	sleep(3);

	TST_StatStop(stat1, PWR_RET_SUCCESS);

	TST_StatGetValue(stat1, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, true, EC_STAT_GET_VALUE);

	TST_StatGetValue(stat2, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, true, EC_STAT_GET_VALUE);

	TST_StatGetValue(stat3, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, false, EC_STAT_GET_VALUE);

	TST_StatGetValue(stat4, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, false, EC_STAT_GET_VALUE);

	TST_StatGetValue(stat5, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, false, EC_STAT_GET_VALUE);

	TST_StatClear(stat1, PWR_RET_SUCCESS);
	sleep(2);

	TST_StatGetValue(stat1, &value1, &times1, PWR_RET_SUCCESS);
	check_stat_value(value1, EC_STAT_GET_VALUE);
	check_time_period(&times1, true, EC_STAT_GET_VALUE);

	TST_StatDestroy(stat1, PWR_RET_SUCCESS);
	TST_StatDestroy(stat2, PWR_RET_SUCCESS);
	TST_StatDestroy(stat3, PWR_RET_SUCCESS);
	TST_StatDestroy(stat4, PWR_RET_SUCCESS);
	// Save stat5 for the context destroy
	//TST_StatDestroy(stat5, PWR_RET_SUCCESS);

	PWR_Stat gstat1 = NULL;
	PWR_TimePeriod *times2 = NULL;
	double *values2 = NULL;
	PWR_Grp all_cores = NULL;
	unsigned int num_cores = 0;
	int index = 0;
	double result = 0.0;
	PWR_Time instant = 0;

	//
	// Create a new statistic based on a group.
	//
	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_CORES, &all_cores,
			      PWR_RET_SUCCESS);
	TST_StatCreateGrp(all_cores, PWR_ATTR_TEMP, PWR_ATTR_STAT_MAX,
			  &gstat1, PWR_RET_SUCCESS);

	TST_GrpGetNumObjs(all_cores, &num_cores, PWR_RET_SUCCESS);
	printf("Check for more than 1 core: ");
	check_int_greater_than(num_cores, 1, EC_STAT_GET_VALUE);

	TST_StatStart(gstat1, PWR_RET_SUCCESS);
	sleep(3);

	values2 = g_new0(double, num_cores);
	times2  = g_new0(PWR_TimePeriod, num_cores);
	TST_StatGetValues(gstat1, values2, times2, PWR_RET_SUCCESS);
	check_stat_value(values2[0], EC_STAT_GET_VALUES);
	check_stat_value(values2[1], EC_STAT_GET_VALUES);
	check_time_period(&times2[0], true, EC_STAT_GET_VALUES);
	check_time_period(&times2[1], true, EC_STAT_GET_VALUES);

	TST_StatGetReduce(gstat1, PWR_ATTR_STAT_MAX, &index, &result, &instant,
			  PWR_RET_SUCCESS);
	check_stat_value(result, EC_STAT_GET_REDUCE);
	TST_StatGetReduce(gstat1, PWR_ATTR_STAT_MIN, &index, &result, &instant,
			  PWR_RET_SUCCESS);
	check_stat_value(result, EC_STAT_GET_REDUCE);
	TST_StatGetReduce(gstat1, PWR_ATTR_STAT_AVG, &index, &result, &instant,
			  PWR_RET_SUCCESS);
	check_stat_value(result, EC_STAT_GET_REDUCE);

	TST_GrpDestroy(all_cores, PWR_RET_SUCCESS);

	TST_StatGetValues(gstat1, values2, times2, PWR_RET_INVALID);

	g_free(values2);
	g_free(times2);

	//
	// Destroy our context
	//
	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}
