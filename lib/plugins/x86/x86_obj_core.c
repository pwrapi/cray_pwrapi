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
 * This file contains the functions to implement core objects for
 * the x86 architecture.
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
//		Plugin Core Object Functions				//
//----------------------------------------------------------------------//

void
x86_del_core(core_t *core)
{
	x86_core_t *x86_core = NULL;

	if (!core) {
		return;
	}

	x86_core = core->plugin_data;
	if (x86_core) {
		g_free(x86_core->temp_input);
		g_free(x86_core->temp_max);
		g_free(x86_core);
	}

	core->plugin_data = NULL;
}

int
x86_new_core(core_t *core)
{
	int status = PWR_RET_SUCCESS;
	x86_core_t *x86_core = NULL;

	x86_core = g_new0(x86_core_t, 1);
	if (!x86_core) {
		LOG_FAULT("Failed to alloc x86_core for %s", core->obj.name);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	core->plugin_data = x86_core;

status_return:
	return status;
}

// Attribute Functions -----------------------------------------------//
int
x86_core_get_temp(core_t *core, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	x86_core_t *x86_core = core->plugin_data;
	uint64_t millidegree = 0;

	TRACE2_ENTER("core = %p, value = %p, ts = %p", core, value, ts);

	retval = read_uint64_from_file(x86_core->temp_input, &millidegree, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = (double)millidegree / 1000.0;

failure_return:
	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

// Metadata Functions -----------------------------------------------//

static int
time_core_get_dbl_op(core_t *core,
		int (*get_dbl_op)(core_t *, double *, struct timespec *),
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

	status = get_dbl_op(core, &dummy, NULL);
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
x86_core_temp_get_meta(core_t *core, PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("core = %p, meta = %d, value = %p", core, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_MIN:
		*(double *)value = 0.0;
		break;
	case PWR_MD_MAX:
		{
			uint64_t ivalue = 0;
			x86_core_t *x86_core = (x86_core_t *)core->plugin_data;

			status = read_uint64_from_file(x86_core->temp_max, &ivalue,
					NULL);
			if (status == PWR_RET_SUCCESS) {
				// Convert from mC to C
				*(double *)value = ivalue * 1.0e-3;
			}
			break;
		}
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_core_get_dbl_op(core, x86_core_get_temp,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for core objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_core_get_meta(core_t *core, PWR_AttrName attr,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("core = %p, attr = %d, meta = %d, value = %p",
			core, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
	case PWR_ATTR_OS_ID:
		status = x86_obj_get_meta(to_obj(core), attr, meta, value);
		break;
	case PWR_ATTR_TEMP:
		status = x86_core_temp_get_meta(core, meta, value);
		break;
	default:
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_core_set_meta(core_t *core, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_READ_ONLY;

	TRACE2_ENTER("core = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			core, ipc, attr, meta, value);

// No settable metadata

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_core_get_meta_at_index(core_t *core, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("core = %p, attr = %d, index = %u, value = %p, value_str = %p",
			core, attr, index, value, value_str);

	status = x86_obj_get_meta_at_index(to_obj(core), attr,
			index, value, value_str);

	TRACE2_EXIT("status = %d", status);

	return status;
}
