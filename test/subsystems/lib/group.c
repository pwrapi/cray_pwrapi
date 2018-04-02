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

#define CONTEXT_NAME		"test_group"

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
	PWR_Obj entry_point = NULL;
	PWR_Grp nodegrp = NULL, childrengrp = NULL,
		nodechildrengrp = NULL, tmpgrp = NULL;
	unsigned int num_objects = 0;

	//
	// Create a context for this test
	//
	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, CONTEXT_NAME,
		      &context, PWR_RET_SUCCESS);

	//
	// Create a new group
	//
	TST_GrpCreate(context, &nodegrp, PWR_RET_SUCCESS);

	//
	// We need an object to add to the group so get our entry point
	//
	TST_CntxtGetEntryPoint(context, &entry_point, PWR_RET_SUCCESS);

	//
	// Add it to the group
	//
	TST_GrpAddObj(nodegrp, entry_point, PWR_RET_SUCCESS);

	//
	// Group count should be 1
	//
	TST_GrpGetNumObjs(nodegrp, &num_objects, PWR_RET_SUCCESS);
	printf("Verify object count is 1: ");
	check_int_equal(num_objects, 1, EC_GROUP_NUM_OBJS_COMPARE);

	//
	// Get a second group with the children of the node object
	//
	TST_ObjGetChildren(entry_point, &childrengrp, PWR_RET_SUCCESS);

	//
	// Duplicate the children group and add the node object to it
	//
	TST_GrpDuplicate(childrengrp, &nodechildrengrp, PWR_RET_SUCCESS);
	TST_GrpAddObj(nodechildrengrp, entry_point, PWR_RET_SUCCESS);

	//
	// Duplicate the node group
	//
	TST_GrpDuplicate(nodegrp, &tmpgrp, PWR_RET_SUCCESS);
	//
	// Remove the entry point from the group
	//
	TST_GrpRemoveObj(tmpgrp, entry_point, PWR_RET_SUCCESS);

	//
	// Group count should now be 0
	//
	TST_GrpGetNumObjs(tmpgrp, &num_objects, PWR_RET_SUCCESS);
	printf("Verify object count is 0: ");
	check_int_equal(num_objects, 0, EC_GROUP_NUM_OBJS_COMPARE);

	//
	// Try to remove a non-existent object from the group
	//
	TST_GrpRemoveObj(tmpgrp, entry_point, PWR_RET_SUCCESS);

	//
	// Destroy the group
	//
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	//
	// Check that named groups are present and non-zero length
	//
	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_SOCKETS, &tmpgrp,
			      PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_CORES, &tmpgrp,
			      PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_MEMS, &tmpgrp,
			      PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_HTS, &tmpgrp,
			      PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_CntxtGetGrpByName(context, "Bad_group", &tmpgrp,
			      PWR_RET_WARN_NO_GRP_BY_NAME);

	//
	// Try the various set operations, just verify that they return success
	// for now
	//
	TST_GrpUnion(nodegrp, childrengrp, &tmpgrp, PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_GrpIntersection(nodegrp, nodechildrengrp, &tmpgrp,
			    PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_GrpDifference(nodechildrengrp, childrengrp, &tmpgrp,
			  PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	TST_GrpSymDifference(childrengrp, nodechildrengrp, &tmpgrp,
			     PWR_RET_SUCCESS);
	TST_GrpDestroy(tmpgrp, PWR_RET_SUCCESS);

	//
	// Destroy our context
	//
	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}
