/*
 * Copyright (c) 2016-2018, Cray Inc.
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
 * This file contains the common use functions for the x86 implementation
 * of power objects.
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
//			Common Functions				//
//----------------------------------------------------------------------//

static inline void
cpuid(uint32_t eax_in, uint32_t ecx_in, uint32_t *eax_out, uint32_t *ebx_out,
	uint32_t *ecx_out, uint32_t *edx_out)
{
	__asm__("cpuid" : "=a" (*eax_out), "=b" (*ebx_out), "=c" (*ecx_out),
			"=d" (*edx_out) : "a" (eax_in), "c" (ecx_in));
}

static inline uint32_t
bit_val(uint32_t reg, int fbit, int nbits)
{
	return (reg >> fbit) & ((1 << nbits) - 1);
}

double
x86_cpu_power_factor(void)
{
	uint32_t eax, ebx, ecx, edx;
	int family, model;
	double factor = 1.0;

	cpuid(1, 0, &eax, &ebx, &ecx, &edx);

	family = bit_val(eax, 20, 4) + bit_val(eax, 8, 4);
	model = (bit_val(eax, 16, 4) << 4) | bit_val(eax, 4, 4);

	if (family == 0x6 && model == 0x57) { // KNL
		// KNL nodes appear to run 10% higher than the
		// specified limit. Use a factor of 90% to
		// account for that.
		factor = 0.9;
	}

	return factor;
}

int
x86_get_power(const char *path, PWR_Time window, double *value,
		struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
	struct timespec ts1, ts2;
	uint64_t energy1 = 0, energy2 = 0;
	double energy = 0.0;

	TRACE2_ENTER("path = '%s', window = %lu, value = %p, ts = %p",
			path, window, value, ts);

	retval = read_uint64_from_file(path, &energy1, &ts1);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = pwr_nanosleep(window);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &energy2, &ts2);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	// The energy counter is 32 bits. Handle rollover.
	if (energy2 < energy1) {
		energy2 += 1UL << 32;
	}

	// Calculate energy used and convert to Joules.
	energy = (energy2 - energy1) * 1.0e-6;

	// Convert from energy (Joules) to power (Watts = J / s).
	*value = energy / pwr_tspec_diff(&ts2, &ts1);

	if (ts != NULL) {
		*ts = ts2;
	}

failure_return:

	TRACE2_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

int
x86_get_time_unit(uint64_t ht_id, uint64_t *value, struct timespec *ts)
{
#ifdef USE_RDMSR

	int status = PWR_RET_SUCCESS;
	char *command = NULL;
	uint64_t time_unit = 0;

	TRACE2_ENTER("ht_id = %lu, value = %p, ts = %p",
			ht_id, value, ts);

	//
	// Now read the MSR containing the time unit
	//
	command = g_strdup_printf(RDMSR_COMMAND, ht_id, MSR_PKG_POWER_SKU_UNIT);
	if (!command) {
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	status = read_uint64_from_command(command, &time_unit, status);
	if (status) {
		goto error_return;
	}

	//
	// Pull the time unit field out of the MSR contents
	//
	*value = (time_unit >> MSR_FIELD_TIME_UNIT_SHIFT)
			& MSR_FIELD_TIME_UNIT_MASK;

error_return:
	g_free(command);

	TRACE2_EXIT("status = %d, *value = %lu", status, *value);

	return status;

#else

	int status = PWR_RET_SUCCESS;
	char *path = NULL;
	uint64_t time_unit = 0;

	TRACE2_ENTER("ht_id = %lu, value = %p, ts = %p",
			ht_id, value, ts);

	//
	// Now read the MSR containing the time unit
	//
	path = g_strdup_printf(MSR_PATH, ht_id, MSR_PKG_POWER_SKU_UNIT);
	if (!path) {
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	status = read_uint64_from_file(path, &time_unit, ts);
	if (status) {
		goto error_return;
	}

	//
	// Pull the time unit field out of the MSR contents
	//
	*value = (time_unit >> MSR_FIELD_TIME_UNIT_SHIFT)
			& MSR_FIELD_TIME_UNIT_MASK;

error_return:
	g_free(path);

	TRACE2_EXIT("status = %d, *value = %lu", status, *value);

	return status;

#endif
}

int
x86_get_throttled_time(int msr, uint64_t ht_id, uint64_t *value,
		struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;
#ifdef USE_RDMSR
	char *command = NULL;
#else
	char *path = NULL;
#endif
	uint64_t counter = 0, time_unit = 0;

	TRACE2_ENTER("msr = 0x%x, ht_id = %lu, value = %p, ts = %p",
			msr, ht_id, value, ts);

	//
	// Read the MSR containing the throttle counter
	//
#ifdef USE_RDMSR
	command = g_strdup_printf(RDMSR_COMMAND, ht_id, msr);
	if (!command) {
		goto failure_return;
	}

	retval = read_uint64_from_command(command, &counter, ts);
#else
	path = g_strdup_printf(MSR_PATH, ht_id, msr);
	if (!path) {
		goto failure_return;
	}

	retval = read_uint64_from_file(path, &counter, ts);
#endif
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	retval = PWR_RET_FAILURE;

	//
	// Pull the throttle counter field out of the MSR contents
	//
	counter = (counter >> MSR_FIELD_PKG_THROTTLE_CNTR_SHIFT)
			& MSR_FIELD_PKG_THROTTLE_CNTR_MASK;

	// Read the time unit in use
	retval = x86_get_time_unit(ht_id, &time_unit, ts);
	if (retval != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	//
	// Now calculate the total throttle time duration, in seconds.
	// The Intel EDS gives the following formula:
	//
	//	Total time = counter * 1s * 1 / (2^(time_unit))
	//
	*value = counter / (1UL << time_unit);

failure_return:
#ifdef USE_RDMSR
	g_free(command);
#else
	g_free(path);
#endif

	TRACE2_EXIT("retval = %d, *value = %lu", retval, *value);

	return retval;
}

static int
x86_find_rapl_pkg_id(uint64_t socket_id, uint64_t *rapl_pkg_id)
{
	int error = 1;
	uint64_t i;
	char *name = NULL;
	char *path = NULL;
	char *string = NULL;

	TRACE3_ENTER("socket_id = %ld, rapl_pkg_id = %p",
			socket_id, rapl_pkg_id);

	name = g_strdup_printf("package-%lu\n", socket_id);
	if (!name) {
		goto failure_return;
	}

	//
	// Discover the correct RAPL domain ID for this socket by iterating
	// over every package name file in every domain looking for a match
	//
	for (i = 0; error; i++) {
		//
		// Construct the pathname to the package name file for
		// this RAPL domain
		//
		path = g_strdup_printf(RAPL_PKG_NAME_PATH, i);
		if (!path) {
			goto failure_return;
		}

		//
		// Ensure this package name file exists.  If not then we know
		// we've already checked all domains and there was no match
		//
		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			goto failure_return;
		}

		//
		// Read the package name
		//
		if (!g_file_get_contents(path, &string, NULL, NULL)) {
			goto failure_return;
		}

		g_free(path);
		path = NULL;

		//
		// Do we have a match?
		//
		if (strcmp(string, name) == 0) {
			//
			// Yup.  We're done.
			//
			*rapl_pkg_id = i;
			error = 0;
		}

		g_free(string);
		string = NULL;
	}

failure_return:
	g_free(name);
	g_free(path);
	g_free(string);

	TRACE3_EXIT("error = %d, *rapl_pkg_id = %ld", error, *rapl_pkg_id);

	return error;
}

static int
x86_find_rapl_mem_id(uint64_t rapl_pkg_id, uint64_t *rapl_mem_id)
{
	int error = 1;
	uint64_t i;
	char *path = NULL;
	char *string = NULL;

	TRACE3_ENTER("rapl_pkg_id = %ld, rapl_mem_id = %p",
			rapl_pkg_id, rapl_mem_id);

	for (i = 0; error; i++) {
		path = g_strdup_printf(RAPL_SUB_NAME_PATH,
					rapl_pkg_id, rapl_pkg_id, i);
		if (!path) {
			goto failure_return;
		}

		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			goto failure_return;
		}

		if (!g_file_get_contents(path, &string, NULL, NULL)) {
			goto failure_return;
		}

		g_free(path);
		path = NULL;

		if (strcmp(string, "dram\n") == 0) {
			*rapl_mem_id = i;
			error = 0;
		}

		g_free(string);
		string = NULL;
	}

failure_return:
	g_free(path);
	g_free(string);

	TRACE3_EXIT("error = %d, *rapl_mem_id = %ld", error, *rapl_mem_id);

	return error;
}

int
x86_find_rapl_id(uint64_t socket_id, uint64_t *rapl_pkg_id,
		uint64_t *rapl_mem_id)
{
	int error = 1;

	TRACE3_ENTER("socket_id = %ld, rapl_pkg_id = %p, rapl_mem_id = %p",
			socket_id, rapl_pkg_id, rapl_mem_id);

	error = x86_find_rapl_pkg_id(socket_id, rapl_pkg_id);
	if (error) {
		LOG_DBG("x86_find_rapl_pkg_id failed");
		goto failure_return;
	}

	error = x86_find_rapl_mem_id(*rapl_pkg_id, rapl_mem_id);
	if (error) {
		LOG_DBG("x86_find_rapl_mem_id failed");
		goto failure_return;
	}

failure_return:

	TRACE3_EXIT("error = %d, *rapl_pkg_id = %ld, *rapl_mem_id = %ld",
			error, *rapl_pkg_id, *rapl_mem_id);

	return error;
}

static int
x86_os_id_get_meta(obj_t *obj, PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, meta = %d, value = %p", obj, meta, value);

	switch (meta) {
	case PWR_MD_NUM:
		*(uint64_t *)value = 0; // not enumerable
		break;
	case PWR_MD_MIN:
	case PWR_MD_MAX:
		*(uint64_t *)value = obj->os_id;
		break;
	case PWR_MD_PRECISION:
	case PWR_MD_ACCURACY:
	case PWR_MD_UPDATE_RATE:
	case PWR_MD_SAMPLE_RATE:
	case PWR_MD_TIME_WINDOW:
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
	case PWR_MD_VALUE_LEN:
	case PWR_MD_MEASURE_METHOD:
		status = PWR_RET_NO_META;
		break;
	default:
		// If metadata doesn't match one of the above
		// cases, it should have been handled as one
		// of the common cases, else it is an out of
		// range value.
		LOG_FAULT("Unexpected metadata value: %d", meta);
		status = PWR_RET_FAILURE;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static const char *
x86_obj_vendor_info(obj_t *obj)
{
	const char *vendor_info = NULL;

	switch (obj->type) {
	case PWR_OBJ_NODE:
	case PWR_OBJ_POWER_PLANE:
		vendor_info = x86_metadata.node_vendor_info;
		break;
	case PWR_OBJ_SOCKET:
	case PWR_OBJ_MEM:
	case PWR_OBJ_CORE:
	case PWR_OBJ_HT:
		vendor_info = x86_metadata.socket_vendor_info;
		break;
	default:
		vendor_info = "<invalid object type>";
		break;
	}

	return vendor_info;
}

static int
x86_get_meta(obj_t *obj, PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, meta = %d, value = %p", obj, meta, value);

	switch (meta) {
	case PWR_MD_VENDOR_INFO_LEN:
		*(uint64_t *)value = strlen(x86_obj_vendor_info(obj)) + 1;
		break;
	case PWR_MD_VENDOR_INFO:
		strcpy(value, x86_obj_vendor_info(obj));
		break;
	default:
		status = PWR_RET_NO_META;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_obj_get_meta(obj_t *obj, PWR_AttrName attr, PWR_MetaName meta, void *value)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, attr = %d, meta = %d, value = %p",
			obj, attr, meta, value);

	switch (attr) {
	case PWR_ATTR_NOT_SPECIFIED:
		status = x86_get_meta(obj, meta, value);
		break;
	case PWR_ATTR_OS_ID:
		status = x86_os_id_get_meta(obj, meta, value);
		break;
	default:
		status = PWR_RET_NO_ATTRIB;
		break;
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

int
x86_obj_get_meta_at_index(obj_t *obj, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str)
{
	int status = PWR_RET_NO_ATTRIB;

	TRACE2_ENTER("obj = %p, attr = %d, index = %u, value = %p, value_str = %p",
			obj, attr, index, value, value_str);

// No generic enumerable attributes

	TRACE2_EXIT("status = %d", status);

	return status;
}
