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
 * This file contains the functions to implement node objects
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
//		Plugin Node Object Functions and Data			//
//----------------------------------------------------------------------//

void
x86_del_node(node_t *node)
{
	x86_node_t *x86_node = NULL;

	if (!node) {
		return;
	}

	x86_node = node->plugin_data;
	if (x86_node) {
		g_free(x86_node);
	}

	node->plugin_data = NULL;
}

int
x86_new_node(node_t *node)
{
	int status = PWR_RET_SUCCESS;
	x86_node_t *x86_node = NULL;

	TRACE2_ENTER("node = %p", node);

	x86_node = g_new0(x86_node_t, 1);
	if (!x86_node) {
		LOG_FAULT("Failed to alloc x86_node for %s", node->obj.name);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Setup was successful. Set the reference from the node to the x86
	// plugin data
	node->plugin_data = x86_node;

status_return:
	// Error cleanup
	if (status) {
		x86_del_node(node);
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

// Attribute Functions -----------------------------------------------//
int
x86_node_get_power(node_t *node, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	uint64_t ivalue = 0;

	TRACE2_ENTER("node = %p, value = %p, ts = %p", node, value, ts);

	retval = read_uint64_from_file(NODE_POWER_PATH, &ivalue, ts);

	*value = ivalue;

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_node_get_power_limit_max(node_t *node, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	uint64_t ivalue = 0;

	TRACE2_ENTER("node = %p, value = %p, ts = %p", node, value, ts);

	retval = read_uint64_from_file(NODE_POWER_CAP_PATH, &ivalue, ts);

	*value = ivalue;

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_node_get_energy(node_t *node, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	uint64_t ivalue = 0;

	TRACE2_ENTER("node = %p, value = %p, ts = %p", node, value, ts);

	retval = read_uint64_from_file(NODE_ENERGY_PATH, &ivalue, ts);

	*value = ivalue;

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

// Metadata Functions -----------------------------------------------//

static int
time_node_get_dbl_op(node_t *node,
		int (*get_dbl_op)(node_t *, double *, struct timespec *),
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

	status = get_dbl_op(node, &dummy, NULL);
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
x86_node_power_get_meta(node_t *node,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, meta = %d, value = %p", node, meta, value);

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
		status = time_node_get_dbl_op(node, x86_node_get_power,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for node objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_node_power_limit_max_get_meta(node_t *node,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, meta = %d, value = %p", node, meta, value);

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
		status = time_node_get_dbl_op(node,
				x86_node_get_power_limit_max,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for node objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_node_energy_get_meta(node_t *node,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, meta = %d, value = %p", node, meta, value);

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
		status = time_node_get_dbl_op(node, x86_node_get_energy,
				(PWR_Time *)value);
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for node objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_node_get_meta(node_t *node, PWR_AttrName attr,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, attr = %d, meta = %d, value = %p",
			node, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
	case PWR_ATTR_OS_ID:
		status = x86_obj_get_meta(to_obj(node), attr, meta, value);
		break;
	case PWR_ATTR_POWER:
		status = x86_node_power_get_meta(node, meta, value);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		status = x86_node_power_limit_max_get_meta(node, meta, value);
		break;
	case PWR_ATTR_ENERGY:
		status = x86_node_energy_get_meta(node, meta, value);
		break;
	default:
		LOG_DBG("Request for unsupported attribute %d from %s", attr,
				node->obj.name);
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_node_set_meta(node_t *node, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_READ_ONLY;

	TRACE2_ENTER("node = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			node, ipc, attr, meta, value);

// No settable metadata

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_node_get_meta_at_index(node_t *node, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, attr = %d, index = %u, value = %p, value_str = %p",
			node, attr, index, value, value_str);

	status = x86_obj_get_meta_at_index(to_obj(node), attr,
			index, value, value_str);

	TRACE2_EXIT("status = %d", status);

	return status;
}
