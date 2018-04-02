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
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#include <glib.h>

#include <cray-powerapi/api.h>

#include "../common/common.h"

#define CONTEXT_NAME		"test_hierarchy"

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
	PWR_ObjType entry_point_type;

	//
	// Create a context for this test
	//
	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "test_hierarchy",
		      &context, PWR_RET_SUCCESS);

	//
	// Get an entry point
	//
	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	//
	// Get the type for the entry point
	//
	TST_ObjGetType(entry_point, &entry_point_type, PWR_RET_SUCCESS);

	//
	// In our implementation, the entry point is always the node
	//
	printf("Verify entry point is a PWR_OBJ_NODE: ");
	check_int_equal(entry_point_type, PWR_OBJ_NODE, EC_OBJ_TYPE_COMPARE);

	//
	// Destroy our context
	//
	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}
