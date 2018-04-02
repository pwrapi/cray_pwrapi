/*
 * Copyright (c) 2017-2018, Cray Inc.
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
 * This file contains the functions to implement power plane (pplane)
 * objects for the x86 architecture.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <dirent.h>

#define _GNU_SOURCE // Enable GNU extensions

#include <glib.h>

#include <cray-powerapi/types.h>
#include <common.h>
#include <log.h>

#include "../common/file.h"
#include "../common/command.h"
#include "typedefs.h"
#include "hierarchy.h"
#include "context.h"
#include "attributes.h"
#include "timer.h"
#include "x86_obj.h"
#include "x86_paths.h"

//----------------------------------------------------------------------//
//		Plugin Power Plane Object Functions			//
//----------------------------------------------------------------------//

void
x86_del_pplane(pplane_t *pplane)
{
	return;
}

int
x86_new_pplane(pplane_t *pplane)
{
	return 0;
}

// Attribute Functions -----------------------------------------------//
int
x86_pplane_get_power(pplane_t *pplane, double *value, struct timespec *ts)
{
	int retval = PWR_RET_NOT_IMPLEMENTED;
	const char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("pplane = %p, value = %p, ts = %p", pplane, value, ts);

	//
	// The data source depends on the sub-object type
	//
	if (pplane->sub_type == PWR_OBJ_SOCKET) {
		path = NODE_CPU_POWER_PATH;
	} else if (pplane->sub_type == PWR_OBJ_MEM) {
		path = NODE_MEM_POWER_PATH;
	} else {
		goto failure_return;
	}

	//
	// Older node types don't have this data.  Ensure the data is present
	// by checking the existence of the pm_counters file
	//
	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		goto failure_return;
	}

	//
	// Read and return the data
	//
	retval = read_uint64_from_file(path, &ivalue, ts);

	*value = ivalue;

failure_return:

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_pplane_get_energy(pplane_t *pplane, double *value, struct timespec *ts)
{
	int retval = PWR_RET_NOT_IMPLEMENTED;
	const char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("pplane = %p, value = %p, ts = %p", pplane, value, ts);

	//
	// The data source depends on the sub-object type
	//
	if (pplane->sub_type == PWR_OBJ_SOCKET) {
		path = NODE_CPU_ENERGY_PATH;
	} else if (pplane->sub_type == PWR_OBJ_MEM) {
		path = NODE_MEM_ENERGY_PATH;
	} else {
		goto failure_return;
	}

	//
	// Older node types don't have this data.  Ensure the data is present
	// by checking the existence of the pm_counters file
	//
	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		goto failure_return;
	}

	//
	// Read and return the data
	//
	retval = read_uint64_from_file(path, &ivalue, ts);

	*value = ivalue;

failure_return:

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}


// Metadata Functions -----------------------------------------------//

static int
time_pplane_get_dbl_op(pplane_t *pplane,
		int (*get_dbl_op)(pplane_t *, double *, struct timespec *),
		PWR_Time *timing)
{
	int status = PWR_RET_SUCCESS;
	struct timespec ts = { 0 };
	PWR_Time beg = 0;
	double dummy = 0.0;

	if (clock_gettime(CLOCK_REALTIME, &ts)) {
		status = PWR_RET_FAILURE;
		goto error_return;
	}
	beg = pwr_tspec_to_nsec(&ts);

	status = get_dbl_op(pplane, &dummy, NULL);
	if (status) {
		goto error_return;
	}

	if (clock_gettime(CLOCK_REALTIME, &ts)) {
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	*timing = pwr_tspec_to_nsec(&ts) - beg;

error_return:
	return status;
}

static int
x86_pplane_power_get_meta(pplane_t *pplane,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, meta = %d, value = %p", pplane, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = x86_metadata.pm_counters_update_rate;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_pplane_get_dbl_op(pplane, x86_pplane_get_power,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for x86_pplane objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_pplane_energy_get_meta(pplane_t *pplane,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, meta = %d, value = %p", pplane, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = x86_metadata.pm_counters_update_rate;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_pplane_get_dbl_op(pplane, x86_pplane_get_power,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for x86_pplane objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_pplane_get_meta(pplane_t *pplane, PWR_AttrName attr,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, attr = %d, meta = %d, value = %p",
			pplane, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
	case PWR_ATTR_OS_ID:
		status = x86_obj_get_meta(to_obj(pplane), attr, meta, value);
		break;
	case PWR_ATTR_POWER:
		status = x86_pplane_power_get_meta(pplane, meta, value);
		break;
	case PWR_ATTR_ENERGY:
		status = x86_pplane_energy_get_meta(pplane, meta, value);
		break;
	default:
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_pplane_set_meta(pplane_t *pplane, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_READ_ONLY;

	TRACE2_ENTER("pplane = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			pplane, ipc, attr, meta, value);

// No settable metadata

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_pplane_get_meta_at_index(pplane_t *pplane, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, attr = %d, index = %u, value = %p, value_str = %p",
			pplane, attr, index, value, value_str);

	status = x86_obj_get_meta_at_index(to_obj(pplane), attr,
			index, value, value_str);

	TRACE2_EXIT("status = %d", status);

	return status;
}
