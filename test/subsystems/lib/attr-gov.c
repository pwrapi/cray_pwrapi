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

#define EC_GOV_COMPARE			64

#define CONTEXT_NAME		"test_attr_gov"

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
	uint64_t gov;
	uint64_t initial;
	uint64_t current;
	PWR_Time tspec;

	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "test_role", &context,
			PWR_RET_SUCCESS);

	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	get_ht_obj(context, entry_point, &ht_obj);

	// Get initial value of governor so we can set it back later.
	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_GOV, &initial, &tspec,
			PWR_RET_SUCCESS);

	// Switch governor to USERSPACE for a single HT object.
	gov = PWR_GOV_LINUX_USERSPACE;
	TST_ObjAttrSetValue(ht_obj, PWR_ATTR_GOV, &gov, PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_GOV, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify current governor is LINUX_USERSPACE: ");
	check_int_equal(current, gov, EC_GOV_COMPARE);

	// Using the entry_point object, switch governor to POWERSAVE for
	// all HT objects.
	gov = PWR_GOV_LINUX_POWERSAVE;
	TST_ObjAttrSetValue(entry_point, PWR_ATTR_GOV, &gov, PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_GOV, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify current governor is LINUX_POWERSAVE: ");
	check_int_equal(current, gov, EC_GOV_COMPARE);

	// Using the entry_point object, switch governor back to initial
	// value for all HT objects.
	TST_ObjAttrSetValue(entry_point, PWR_ATTR_GOV, &initial,
			PWR_RET_SUCCESS);

	TST_ObjAttrGetValue(ht_obj, PWR_ATTR_GOV, &current, &tspec,
			PWR_RET_SUCCESS);

	printf("Verify current governor is back to initial: ");
	check_int_equal(current, initial, EC_GOV_COMPARE);

	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}

