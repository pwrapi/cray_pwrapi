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
 * This file contains the functions to implement memory (mem) objects
 * for the x86 architecture.
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
//		Plugin Memory Object Functions				//
//----------------------------------------------------------------------//

void
x86_del_mem(mem_t *mem)
{
	x86_mem_t *x86_mem = NULL;

	if (!mem) {
		return;
	}

	x86_mem = mem->plugin_data;
	if (x86_mem) {
		g_free(x86_mem);
	}

	mem->plugin_data = NULL;
}

int
x86_new_mem(mem_t *mem)
{
	int status = PWR_RET_SUCCESS;
	x86_mem_t *x86_mem = NULL;

	x86_mem = g_new0(x86_mem_t, 1);
	if (!x86_mem) {
		LOG_FAULT("Failed to alloc x86_mem for %s", mem->obj.name);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	mem->plugin_data = x86_mem;

	x86_mem->power_time_window_meta = x86_metadata.pm_counters_time_window;

status_return:
	return status;
}

// Attribute Functions -----------------------------------------------//
int
x86_mem_get_throttled_time(mem_t *mem, uint64_t *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;

	TRACE2_ENTER("mem = %p, value = %p, ts = %p", mem, value, ts);

	retval = x86_get_throttled_time(MSR_DDR_RAPL_PERF_STATUS,
					mem->ht_id, value, ts);

	TRACE2_EXIT("retval = %d, *value = %lu", retval, *value);

	return retval;
}

int
x86_mem_get_power(mem_t *mem, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	x86_mem_t *x86_mem = mem->plugin_data;
	char *path = NULL;

	TRACE2_ENTER("mem = %p, value = %p, ts = %p", mem, value, ts);

	path = g_strdup_printf(RAPL_SUB_ENERGY_PATH,
			x86_mem->rapl_pkg_id, x86_mem->rapl_pkg_id,
			x86_mem->rapl_mem_id);
	if (!path) {
		goto failure_return;
	}

	retval = x86_get_power(path, x86_mem->power_time_window_meta, value,
			ts);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_mem_get_power_limit_max(mem_t *mem, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	x86_mem_t *x86_mem = mem->plugin_data;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("mem = %p, value = %p, ts = %p", mem, value, ts);

	path = g_strdup_printf(RAPL_SUB_POWER_LIMIT_PATH,
			x86_mem->rapl_pkg_id, x86_mem->rapl_pkg_id,
			x86_mem->rapl_mem_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = ivalue * 1.0e-6; // convert from uw to w

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_mem_set_power_limit_max(mem_t *mem, ipc_t *ipc, const double *value)
{
	int retval = PWR_RET_FAILURE;
	x86_mem_t *x86_mem = mem->plugin_data;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("mem = %p, ipc = %p, value = %p", mem, ipc, value);

	path = g_strdup_printf(RAPL_SUB_POWER_LIMIT_PATH,
			x86_mem->rapl_pkg_id, x86_mem->rapl_pkg_id,
			x86_mem->rapl_mem_id);
	if (!path) {
		goto failure_return;
	}

	ivalue = *value * 1.0e6; // convert from w to uw

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_MEM,
			PWR_ATTR_POWER_LIMIT_MAX, PWR_MD_NOT_SPECIFIED,
			&ivalue, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
x86_mem_get_energy(mem_t *mem, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	x86_mem_t *x86_mem = mem->plugin_data;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("mem = %p, value = %p, ts = %p", mem, value, ts);

	path = g_strdup_printf(RAPL_SUB_ENERGY_PATH,
			x86_mem->rapl_pkg_id, x86_mem->rapl_pkg_id,
			x86_mem->rapl_mem_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);

	*value = ivalue * 1.0e-6; // convert from uj to j

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

// Metadata Functions -----------------------------------------------//

static int
time_mem_get_u64_op(mem_t *mem,
		int (*get_u64_op)(mem_t *, uint64_t *, struct timespec *),
		PWR_Time *timing)
{
	int status = PWR_RET_SUCCESS;
	struct timespec ts = { 0 };
	PWR_Time beg = 0;
	uint64_t dummy = 0;

	if (clock_gettime(CLOCK_REALTIME, &ts)) {
		status = PWR_RET_FAILURE;
		goto error_return;
	}
	beg = pwr_tspec_to_nsec(&ts);

	status = get_u64_op(mem, &dummy, NULL);
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
time_mem_get_dbl_op(mem_t *mem,
		int (*get_dbl_op)(mem_t *, double *, struct timespec *),
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

	status = get_dbl_op(mem, &dummy, NULL);
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
x86_mem_power_get_meta(mem_t *mem, PWR_MetaName meta,
		void *value)
{
	int status = PWR_RET_SUCCESS;
	x86_mem_t *x86_mem = mem->plugin_data;

	TRACE2_ENTER("mem = %p, meta = %d, value = %p", mem, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = x86_metadata.pm_counters_update_rate;
		break;
	case PWR_MD_TIME_WINDOW:
		*(PWR_Time *)value = x86_mem->power_time_window_meta;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_mem_get_dbl_op(mem, x86_mem_get_power,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_mem_power_limit_max_get_meta(mem_t *mem,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, meta = %d, value = %p", mem, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_MIN:
		*(double *)value = 0.0;
		break;
	case PWR_MD_MAX:
		{
			uint64_t ival = 0;
			x86_mem_t *x86_mem = (x86_mem_t *)mem->plugin_data;
			char *path = g_strdup_printf(
					RAPL_SUB_POWER_LIMIT_MAX_PATH,
					x86_mem->rapl_pkg_id,
					x86_mem->rapl_pkg_id,
					x86_mem->rapl_mem_id);

			status = read_uint64_from_file(path, &ival, NULL);
			if (status == PWR_RET_SUCCESS) {
				// Convert from uw to w
				*(double *)value = ival * 1.0e-6;
			}
			g_free(path);
			break;
		}
	case PWR_MD_TIME_WINDOW:
		{
			uint64_t ival = 0;
			PWR_Time tval = 0;
			x86_mem_t *x86_mem = (x86_mem_t *)mem->plugin_data;
			char *path = g_strdup_printf(RAPL_SUB_TIME_WINDOW_PATH,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_mem_id);

			status = read_uint64_from_file(path, &ival, NULL);
			if (status == PWR_RET_SUCCESS) {
				// Convert from us to ns, guard against
				// overflow.
				if (ival > USEC_MAX) {
					LOG_WARN("Time in usec read from %s would overflow nsec, "
							"forcing to max allowed value",
							path);
					tval = NSEC_MAX;
				} else {
					tval = pwr_usec_to_nsec(ival);
				}
				*(PWR_Time *)value = tval;
			}
			g_free(path);
			break;
		}
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_mem_get_dbl_op(mem, x86_mem_get_power_limit_max,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_mem_energy_get_meta(mem_t *mem,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, meta = %d, value = %p", mem, meta, value);

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
			x86_mem_t *x86_mem = (x86_mem_t *)mem->plugin_data;
			char *path =
				g_strdup_printf(RAPL_SUB_ENERGY_MAX_PATH,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_mem_id);

			status = read_uint64_from_file(path, &ivalue, NULL);
			if (status == PWR_RET_SUCCESS) {
				// Convert from uj to j
				*(double *)value = ivalue * 1.0e-6;
			}
			g_free(path);
			break;
		}
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_mem_get_dbl_op(mem, x86_mem_get_energy,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_mem_throttled_time_get_meta(mem_t *mem,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_NOT_IMPLEMENTED;

	TRACE2_ENTER("mem = %p, meta = %d, value = %p", mem, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_MIN:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_MAX:
		// Intel currently limits throttle time to 32bits
		// interpreted as microseconds.
		//
		// Compute as 2^32 us converted to ns
		*(PWR_Time *)value = pwr_usec_to_nsec(1UL << 32);
		break;
	case PWR_MD_UPDATE_RATE:
		{
			uint64_t time_unit = 0;

			status = x86_get_time_unit(mem->ht_id, &time_unit,
					NULL);
			if (status == PWR_RET_SUCCESS) {
				*(double *)value = 1.0 / (1 << time_unit);
			}
			break;
		}
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_mem_get_u64_op(mem, x86_mem_get_throttled_time,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_mem_get_meta(mem_t *mem, PWR_AttrName attr,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, attr = v%d, meta = %d, value = %p",
			mem, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
	case PWR_ATTR_OS_ID:
		status = x86_obj_get_meta(to_obj(mem), attr, meta, value);
		break;
	case PWR_ATTR_POWER:
		status = x86_mem_power_get_meta(mem, meta, value);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		status = x86_mem_power_limit_max_get_meta(mem, meta, value);
		break;
	case PWR_ATTR_ENERGY:
		status = x86_mem_energy_get_meta(mem, meta, value);
		break;
	case PWR_ATTR_THROTTLED_TIME:
		status = x86_mem_throttled_time_get_meta(mem, meta, value);
		break;
	default:
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_mem_power_set_meta(mem_t *mem, ipc_t *ipc,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, ipc = %p, meta = %d, value = %p",
			mem, ipc, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
	case PWR_MD_MIN:
	case PWR_MD_MAX:
		status = PWR_RET_READ_ONLY;
		break;
	case PWR_MD_TIME_WINDOW:
		{
			PWR_Time tval = *(PWR_Time *)value;
			PWR_Time min_val = x86_metadata.pm_counters_time_window;
			PWR_Time max_val = min_val * MD_TIME_WINDOW_MULTIPLE_MAX;
			x86_mem_t *x86_mem = mem->plugin_data;

			// Round request time to nearest multiple of min_val
			PWR_Time rval =
				((tval + min_val / 2) / min_val) * min_val;

			LOG_FAULT("Specified time window %lu, rounded to %lu",
						tval, rval);

			// Check for time window in the allowed range
			if (rval < min_val || rval > max_val) {
				LOG_FAULT("Specified time window %lu, rounded to %lu, "
						"is out of range [%lu, %lu]",
						tval, rval, min_val, max_val);
				status = PWR_RET_OUT_OF_RANGE;
				break; // out of switch
			}

			x86_mem->power_time_window_meta = rval;
			break;
		}
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_VENDOR_INFO:
	case PWR_MD_MEASURE_METHOD:
		status = PWR_RET_READ_ONLY;
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_mem_power_limit_max_set_meta(mem_t *mem, ipc_t *ipc,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, ipc = %p, meta = %d, value = %p",
			mem, ipc, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
	case PWR_MD_MIN:
	case PWR_MD_MAX:
		status = PWR_RET_READ_ONLY;
		break;
	case PWR_MD_TIME_WINDOW:
		{
			uint64_t ival = pwr_nsec_to_usec(*(uint64_t *)value);
			x86_mem_t *x86_mem =
				(x86_mem_t *)mem->plugin_data;
			char *path = g_strdup_printf(RAPL_SUB_TIME_WINDOW_PATH,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_pkg_id,
						x86_mem->rapl_mem_id);
			if (!path) {
				LOG_FAULT("Failed to allocate path to set metadata for %s",
						mem->obj.name);
				status = PWR_RET_FAILURE;
				break; // break out of switch & return
			}

			// Request powerapid set the value
			status = ipc->ops->set_uint64(ipc, PWR_OBJ_MEM,
					PWR_ATTR_POWER_LIMIT_MAX, meta, &ival,
					path);
			g_free(path);
			break;
		}
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_VENDOR_INFO:
	case PWR_MD_MEASURE_METHOD:
		status = PWR_RET_READ_ONLY;
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for mem objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_mem_set_meta(mem_t *mem, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			mem, ipc, attr, meta, value);

	// Caller checks for attribute support by the object, so
	// getting here assumes the object supports the attribute.
	// Now the question is can the metadata for the attribute
	// be modified?
	switch (attr) {
	case PWR_ATTR_POWER:
		status = x86_mem_power_set_meta(mem, ipc, meta, value);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		status = x86_mem_power_limit_max_set_meta(mem, ipc, meta, value);
		break;
	default:
		status = PWR_RET_READ_ONLY;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_mem_get_meta_at_index(mem_t *mem, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, attr = %d, index = %u, value = %p, value_str = %p",
			mem, attr, index, value, value_str);

	status = x86_obj_get_meta_at_index(to_obj(mem), attr,
			index, value, value_str);

	TRACE2_EXIT("status = %d", status);

	return status;
}
