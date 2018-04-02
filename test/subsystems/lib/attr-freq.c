/*
 * Copyright (c) 2016, Cray Inc.
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
#include <string.h>
#include <glib.h>

#include <cray-powerapi/api.h>

#include "../common/common.h"

#define EC_FREQ_COMPARE			64

#define CONTEXT_NAME		"test_attr_freq"

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
	PWR_Cntxt context;
	PWR_Obj entry_point;
	PWR_Obj ht_obj;
	double freq_min;
	double freq_max;
	double current;
	PWR_Time tspec;

	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "test_role", &context,
			PWR_RET_SUCCESS);

	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	get_ht_obj(context, entry_point, &ht_obj);

	// Get initial value of frequency min so we can set it back later.
	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MIN, &freq_min, &tspec,
			PWR_RET_SUCCESS);

	// Get initial value of frequency max so we can set it back later.
	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MAX, &freq_max, &tspec,
			PWR_RET_SUCCESS);

	// Start of FREQ_LIMIT_MIN test.
	// Set it to freq limit max, which we know is a valid value.

	// Set freq limit min for a single HT object.
	TST_ObjAttrSetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MIN, &freq_max,
			PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MIN, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify freq limit min was set: ");
	check_double_equal(current, freq_max, EC_FREQ_COMPARE);

	// Using the entry_point object, switch freq limit min back to
	// initial value for all HT objects.
	TST_ObjAttrSetValue(entry_point, PWR_ATTR_FREQ_LIMIT_MIN, &freq_min,
			PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MIN, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify current freq limit min is back to initial: ");
	check_double_equal(current, freq_min, EC_FREQ_COMPARE);

	// End of FREQ_LIMIT_MIN test.

	// Start of FREQ_LIMIT_MAX test.
	// Set it to freq limit min, which we know is a valid value.

	// Set freq limit max for a single HT object.
	TST_ObjAttrSetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MAX, &freq_min,
			PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MAX, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify freq limit max was set: ");
	check_double_equal(current, freq_min, EC_FREQ_COMPARE);

	// Using the entry_point object, switch freq limit max back to
	// initial value for all HT objects.
	TST_ObjAttrSetValue(entry_point, PWR_ATTR_FREQ_LIMIT_MAX, &freq_max,
			PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_FREQ_LIMIT_MAX, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify current freq limit max is back to initial: ");
	check_double_equal(current, freq_max, EC_FREQ_COMPARE);

	// End of FREQ_LIMIT_MAX test.

	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}

