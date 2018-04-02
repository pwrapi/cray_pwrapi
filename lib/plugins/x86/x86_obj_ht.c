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
 * This file contains the functions to implement hardware thread (ht)
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

#include "../common/common.h"
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
//		Plugin Hardware Thread Object Functions			//
//----------------------------------------------------------------------//

void
x86_del_ht(ht_t *ht)
{
	return;
}

int
x86_new_ht(ht_t *ht)
{
	return 0;
}

// Attribute Functions -----------------------------------------------//
int
x86_ht_get_freq(ht_t *ht, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	path = g_strdup_printf(HT_FREQ_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = ivalue;

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_ht_get_freq_req(ht_t *ht, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	path = g_strdup_printf(HT_FREQ_REQ_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = ivalue;

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_ht_set_freq_req(ht_t *ht, ipc_t *ipc, const double *value)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, ipc = %p, value = %p", ht, ipc, value);

	path = g_strdup_printf(HT_FREQ_REQ_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = convert_double_to_uint64(value, &ivalue);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_HT, PWR_ATTR_FREQ_REQ,
			PWR_MD_NOT_SPECIFIED, &ivalue, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
x86_ht_get_freq_limit_min(ht_t *ht, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	path = g_strdup_printf(HT_FREQ_LIMIT_MIN_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = ivalue;

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_ht_set_freq_limit_min(ht_t *ht, ipc_t *ipc, const double *value)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, ipc = %p, value = %p", ht, ipc, value);

	path = g_strdup_printf(HT_FREQ_LIMIT_MIN_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = convert_double_to_uint64(value, &ivalue);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_HT, PWR_ATTR_FREQ_LIMIT_MIN,
			PWR_MD_NOT_SPECIFIED, &ivalue, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
x86_ht_get_freq_limit_max(ht_t *ht, double *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	path = g_strdup_printf(HT_FREQ_LIMIT_MAX_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &ivalue, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = ivalue;

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_ht_set_freq_limit_max(ht_t *ht, ipc_t *ipc, const double *value)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	uint64_t ivalue = 0;

	TRACE2_ENTER("ht = %p, ipc = %p, value = %p", ht, ipc, value);

	path = g_strdup_printf(HT_FREQ_LIMIT_MAX_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = convert_double_to_uint64(value, &ivalue);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_HT, PWR_ATTR_FREQ_LIMIT_MAX,
			PWR_MD_NOT_SPECIFIED, &ivalue, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
x86_ht_get_governor(ht_t *ht, uint64_t *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;
	char *buf = NULL;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	path = g_strdup_printf(HT_GOVERNOR_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = read_string_from_file(path, &buf, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	*value = pwr_string_to_gov(buf);

failure_return:
	g_free(path);
	g_free(buf);

	TRACE2_EXIT("retval = %d, *value = %ld", retval, *value);

	return retval;
}

int
x86_ht_set_governor(ht_t *ht, ipc_t *ipc, const uint64_t *value)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;

	TRACE2_ENTER("ht = %p, ipc = %p, value = %p", ht, ipc, value);

	path = g_strdup_printf(HT_GOVERNOR_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_HT, PWR_ATTR_GOV,
			PWR_MD_NOT_SPECIFIED, value, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
x86_ht_get_cstate_limit(ht_t *ht, uint64_t *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	int num_cstates = 0;
	uint64_t disable = 0;
	uint64_t i = 0;
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	char *path = NULL;

	TRACE2_ENTER("ht = %p, value = %p, ts = %p", ht, value, ts);

	// Count the number of state[0-N] subdirectories under the
	// HT_CSTATE_PATH directory.

	path = g_strdup_printf(HT_CSTATE_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	dp = opendir(path);
	if (!dp) {
		goto failure_return;
	}

	while ((entry = readdir(dp)) != NULL) {
		if (entry->d_type == DT_DIR &&
				strncmp(entry->d_name, "state", 5) == 0) {
			num_cstates += 1;
		}
	}

	closedir(dp);

	// Read through the states, starting with state1, until finding
	// a state disabled. The limit is then the highest number state
	// not disabled.
	for (i = 1; i < num_cstates; ++i) {
		g_free(path);
		path = g_strdup_printf(HT_CSTATE_LIMIT_PATH, ht->obj.os_id, i);
		if (!path) {
			retval = PWR_RET_FAILURE;
			goto failure_return;
		}

		retval = read_uint64_from_file(path, &disable, ts);
		if (retval != PWR_RET_SUCCESS)
			goto failure_return;

		if (disable != 0)
			break;
	}

	*value = i - 1;

	retval = PWR_RET_SUCCESS;

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d, *value = %ld", retval, *value);

	return retval;
}

int
x86_ht_set_cstate_limit(ht_t *ht, ipc_t *ipc, const uint64_t *value)
{
	int retval = PWR_RET_FAILURE;
	char *path = NULL;

	TRACE2_ENTER("ht = %p, ipc = %p, value = %p", ht, ipc, value);

	path = g_strdup_printf(HT_CSTATE_PATH, ht->obj.os_id);
	if (!path) {
		goto failure_return;
	}

	retval = ipc->ops->set_uint64(ipc, PWR_OBJ_HT, PWR_ATTR_CSTATE_LIMIT,
			PWR_MD_NOT_SPECIFIED, value, path);

failure_return:
	g_free(path);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// Metadata Functions -----------------------------------------------//

static int
time_ht_get_u64_op(ht_t *ht,
		int (*get_u64_op)(ht_t *, uint64_t *, struct timespec *),
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

	status = get_u64_op(ht, &dummy, NULL);
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
time_ht_get_dbl_op(ht_t *ht,
		int (*get_dbl_op)(ht_t *, double *, struct timespec *),
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

	status = get_dbl_op(ht, &dummy, NULL);
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
x86_ht_cstate_limit_get_meta(ht_t *ht,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_cstate.num;
		break;
	case PWR_MD_MIN:
		*(uint64_t *)value = x86_metadata.ht_cstate.min;
		break;
	case PWR_MD_MAX:
		*(uint64_t *)value = x86_metadata.ht_cstate.max;
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = 0.0;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_u64_op(ht, x86_ht_get_cstate_limit,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_cstate.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_ht_freq_get_meta(ht_t *ht, PWR_MetaName meta,
		void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_freq.num;
		break;
	case PWR_MD_MIN:
		*(double *)value = x86_metadata.ht_freq.min;
		break;
	case PWR_MD_MAX:
		*(double *)value = x86_metadata.ht_freq.max;
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = 0.0;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_dbl_op(ht, x86_ht_get_freq,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_freq.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_ht_freq_req_get_meta(ht_t *ht, PWR_MetaName meta,
		void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_freq.num;
		break;
	case PWR_MD_MIN:
		*(double *)value = x86_metadata.ht_freq.min;
		break;
	case PWR_MD_MAX:
		*(double *)value = x86_metadata.ht_freq.max;
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = 0.0;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_dbl_op(ht, x86_ht_get_freq_req,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_freq.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_ht_freq_limit_min_get_meta(ht_t *ht,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_freq.num;
		break;
	case PWR_MD_MIN:
		*(double *)value = x86_metadata.ht_freq.min;
		break;
	case PWR_MD_MAX:
		*(double *)value = x86_metadata.ht_freq.max;
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = 0.0;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_dbl_op(ht, x86_ht_get_freq_limit_min,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_freq.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_ht_freq_limit_max_get_meta(ht_t *ht,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_freq.num;
		break;
	case PWR_MD_MIN:
		*(double *)value = x86_metadata.ht_freq.min;
		break;
	case PWR_MD_MAX:
		*(double *)value = x86_metadata.ht_freq.max;
		break;
	case PWR_MD_UPDATE_RATE:
		*(double *)value = 0.0;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_dbl_op(ht, x86_ht_get_freq_limit_max,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_freq.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_ht_gov_get_meta(ht_t *ht, PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, meta = %d, value = %p", ht, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = x86_metadata.ht_gov.num;
		break;
	case PWR_MD_TS_LATENCY:
		*(PWR_Time *)value = 0;
		break;
	case PWR_MD_TS_ACCURACY:
		status = time_ht_get_u64_op(ht, x86_ht_get_governor,
				(PWR_Time *)value);
		break;
	case PWR_MD_VALUE_LEN:
		*(uint64_t *)value = x86_metadata.ht_gov.value_len;
		break;
	case PWR_MD_MEASURE_METHOD:
		*(uint64_t *)value = 0; // measured, not modeled
		break;
	default:
		// Fell through all previous metadata checks, so requested
		// metadata must not be supported for ht objects.
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_ht_get_meta(ht_t *ht, PWR_AttrName attr,
		PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, attr = %d, meta = %d, value = %p",
			ht, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
	case PWR_ATTR_OS_ID:
		status = x86_obj_get_meta(to_obj(ht), attr, meta, value);
		break;
	case PWR_ATTR_CSTATE_LIMIT:
		status = x86_ht_cstate_limit_get_meta(ht, meta, value);
		break;
	case PWR_ATTR_FREQ:
		status = x86_ht_freq_get_meta(ht, meta, value);
		break;
	case PWR_ATTR_FREQ_REQ:
		status = x86_ht_freq_req_get_meta(ht, meta, value);
		break;
	case PWR_ATTR_FREQ_LIMIT_MIN:
		status = x86_ht_freq_limit_min_get_meta(ht, meta, value);
		break;
	case PWR_ATTR_FREQ_LIMIT_MAX:
		status = x86_ht_freq_limit_max_get_meta(ht, meta, value);
		break;
	case PWR_ATTR_GOV:
		status = x86_ht_gov_get_meta(ht, meta, value);
		break;
	default:
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_ht_set_meta(ht_t *ht, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int status = PWR_RET_READ_ONLY;

	TRACE2_ENTER("ht = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			ht, ipc, attr, meta, value);

// No settable metadata

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_ht_get_meta_at_index(ht_t *ht, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;
	pwr_list_uint64_t *ilist = NULL;
	pwr_list_double_t *flist = NULL;
	pwr_list_string_t *slist = NULL;

	TRACE2_ENTER("ht = %p, attr = %d, index = %u, value = %p, value_str = %p",
			ht, attr, index, value, value_str);

	switch (attr) {
	case PWR_ATTR_CSTATE_LIMIT:
		ilist = &x86_metadata.ht_cstate;
		status = pwr_list_value_at_index_uint64(ilist,
				index, value, value_str);
		break;

	case PWR_ATTR_FREQ:
	case PWR_ATTR_FREQ_REQ:
	case PWR_ATTR_FREQ_LIMIT_MIN:
	case PWR_ATTR_FREQ_LIMIT_MAX:
		flist = &x86_metadata.ht_freq;
		status = pwr_list_value_at_index_double(flist,
				index, value, value_str);
		break;

	case PWR_ATTR_GOV:
		slist = &x86_metadata.ht_gov;
		status = pwr_list_value_at_index_string(slist,
				index, value, value_str,
				pwr_string_to_gov);
		break;

	default:
		// see if generic function can handle it
		status = x86_obj_get_meta_at_index(to_obj(ht), attr,
				index, value, value_str);
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}
