/*
 * Copyright (c) 2016-2017, Cray Inc.
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
 * Helper functions to read and set values in sysfs control files for
 * the powerapi daemon.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <cray-powerapi/powerapid.h>
#include <common.h>
#include <log.h>

#include "powerapid.h"
#include "pwrapi_set.h"
#include "pwrapi_worker.h"

static int
file_read_uint64(const char *filepath, uint64_t *value)
{
	FILE *fp = NULL;
	uint64_t local_value = 0;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', value = %p", filepath, value);

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	if (fscanf(fp, "%ld", &local_value) != 1) {
		LOG_FAULT("Error reading value from %s", filepath);
		goto done;
	}

	*value = local_value;
	retval = 0;

done:
	if (fp)
		fclose(fp);

	TRACE1_EXIT("retval = %d, *value = %ld", retval, *value);

	return retval;
}

static int
file_read_double(const char *filepath, double *value)
{
	FILE *fp = NULL;
	double local_value = 0;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', value = %p", filepath, value);

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	if (fscanf(fp, "%lf", &local_value) != 1) {
		LOG_FAULT("Error reading value from %s", filepath);
		goto done;
	}

	*value = local_value;
	retval = 0;

done:
	if (fp)
		fclose(fp);
	TRACE1_EXIT("retval = %d, *value = %lf", retval, *value);

	return retval;
}

static int
file_read_string(const char *filepath, char **value)
{
	FILE *fp = NULL;
	char *str = NULL;
	size_t size = 0;
	ssize_t ret;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', value = %p", filepath, value);

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	ret = getline(&str, &size, fp);
	if (ret < 0) {
		LOG_FAULT("Error reading value from %s", filepath);
		goto done;
	}

	if (ret > 0 && str[ret - 1] == '\n') {
		str[ret - 1] = '\0';
	}

	*value = str;
	retval = 0;

done:
	if (fp)
		fclose(fp);

	TRACE1_EXIT("retval = %d, *value = %p '%s'", retval, *value, *value);

	return retval;
}

static int
file_write_uint64(const char *filepath, uint64_t value)
{
	FILE *fp = NULL;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', value = %ld", filepath, value);

	fp = fopen(filepath, "w");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	if (fprintf(fp, "%ld", value) < 0) {
		LOG_FAULT("Error writing value (%ld) to %s", value, filepath);
		goto done;
	}

	if (fflush(fp) != 0) {
		LOG_FAULT("Error flushing value (%ld) to %s: %m", value, filepath);
		goto done;
	}

	retval = 0;

done:
	if (fp)
		fclose(fp);

	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static int
file_write_double(const char *filepath, double value)
{
	FILE *fp = NULL;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', value = %lf", filepath, value);

	fp = fopen(filepath, "w");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	if (fprintf(fp, "%lf", value) < 0) {
		LOG_FAULT("Error writing value (%lf) to %s", value, filepath);
		goto done;
	}

	if (fflush(fp) != 0) {
		LOG_FAULT("Error flushing value (%lf) to %s: %m", value, filepath);
		goto done;
	}

	retval = 0;

done:
	if (fp)
		fclose(fp);

	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static int
file_write_string(const char *filepath, const char *value)
{
	FILE *fp = NULL;
	int retval = 1;

	TRACE1_ENTER("filepath = '%s', str = %p '%s'", filepath, value, value);

	fp = fopen(filepath, "w");
	if (fp == NULL) {
		LOG_FAULT("Unable to open %s: %m", filepath);
		goto done;
	}

	if (fprintf(fp, "%s", value) < 0) {
		LOG_FAULT("Error writing value '%s' to %s", value, filepath);
		goto done;
	}

	if (fflush(fp) != 0) {
		LOG_FAULT("Error flushing value '%s' to %s: %m", value, filepath);
		goto done;
	}

	retval = 0;

done:
	if (fp)
		fclose(fp);

	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static int
get_cstates_count(const char *path)
{
	int states = 0;
	DIR *dp = NULL;
	struct dirent *entry = NULL;

	TRACE2_ENTER("path = '%s'", path);

	// Count the number of state[0-N] subdirectories under the
	// HT_CSTATE_PATH directory.
	dp = opendir(path);
	if (!dp) {
		LOG_FAULT("unable to open directory %s: %m", path);
		goto done;
	}

	while ((entry = readdir(dp)) != NULL) {
		if (entry->d_type == DT_DIR && strncmp(entry->d_name, "state", 5) == 0)
			states += 1;
	}

	closedir(dp);

done:
	TRACE2_EXIT("states = %d", states);

	return states;
}

static int
read_cstate_limit(const char *path, uint64_t *ivalue)
{
	int retval = 0;
	int num_cstates = get_cstates_count(path);
	int i;

	TRACE2_ENTER("path = '%s', ivalue = %p", path, ivalue);

	for (i = 1; i < num_cstates; i++) {
		char *filepath;
		uint64_t disable_value = 0;

		filepath = g_strdup_printf("%s/state%d/disable", path, i);
		if (!filepath) {
			LOG_CRIT(MEM_ERROR_EXIT);
			exit(1);
		}

		retval = file_read_uint64(filepath, &disable_value);

		g_free(filepath);

		if (retval) {
			break;
		}
		if (disable_value > 0) {
			break;
		}
	}
	if (retval) {
		goto done;
	}

	*ivalue = i - 1;
	LOG_DBG("Read value of %s is %ld", path, *ivalue);

done:
	TRACE2_EXIT("retval = %d, *ivalue = %ld", retval, *ivalue);

	return retval;
}

static int
read_governor(const char *path, uint64_t *ivalue)
{
	int retval = 0;
	char *buf = NULL;

	TRACE2_ENTER("path = '%s', ivalue = %p", path, ivalue);

	retval = file_read_string(path, &buf);
	if (retval) {
		goto done;
	}

	*ivalue = pwr_string_to_gov(buf);
	LOG_DBG("Read value of %s is '%s' (%ld)", path, buf, *ivalue);

done:
	g_free(buf);

	TRACE2_EXIT("retval = %d, *ivalue = %ld", retval, *ivalue);

	return retval;
}

static int
read_value_by_type(const char *path, PWR_AttrDataType data_type,
		type_union_t *value)
{
	int retval = 1;

	TRACE2_ENTER("path = '%s', data_type = %d, value = %p",
			path, data_type, value);

	switch (data_type) {
	case PWR_ATTR_DATA_UINT64:
		retval = file_read_uint64(path, &value->ivalue);
		if (retval) {
			break;
		}
		LOG_DBG("Read value of %s is %ld", path, value->ivalue);
		break;
	case PWR_ATTR_DATA_DOUBLE:
		retval = file_read_double(path, &value->fvalue);
		if (retval) {
			break;
		}
		LOG_DBG("Read value of %s is %lf", path, value->fvalue);
		break;
	default:
		LOG_FAULT("Value of %s has unknown data type %d", path, data_type);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
read_attr_value(set_info_t *setp)
{
	powerapi_setreq_t *setreq;
	const char *path;
	type_union_t *value;
	int retval = 1;

	TRACE1_ENTER("setp = %p", setp);

	setreq = &setp->setreq;
	path = setreq->path;
	value = &setreq->value;

	switch (setreq->attribute) {
	case PWR_ATTR_CSTATE_LIMIT:
		retval = read_cstate_limit(path, &value->ivalue);
		break;
	case PWR_ATTR_GOV:
		retval = read_governor(path, &value->ivalue);
		break;
	default:
		retval = read_value_by_type(path, setreq->data_type, value);
		break;
	}

	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static int
write_cstate_limit(const char *path, uint64_t ivalue)
{
	int retval = 0;
	int num_cstates = get_cstates_count(path);
	int i;

	TRACE2_ENTER("path = '%s', ivalue = %ld", path, ivalue);

	LOG_DBG("Write value of %s is %ld", path, ivalue);

	if (ivalue < 0 || ivalue >= num_cstates) {
		retval = 1;
		goto done;
	}

	for (i = 1; i < num_cstates; i++) {
		char *filepath;
		uint64_t disable_value = (i > ivalue) ? 1 : 0;

		filepath = g_strdup_printf("%s/state%d/disable", path, i);
		if (!filepath) {
			LOG_CRIT(MEM_ERROR_EXIT);
			exit(1);
		}

		retval = file_write_uint64(filepath, disable_value);

		g_free(filepath);

		if (retval) {
			break;
		}
	}

done:
	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
write_governor(const char *path, uint64_t ivalue)
{
	int retval = 0;
	const char *buf = pwr_gov_to_string(ivalue);

	TRACE2_ENTER("path = '%s', ivalue = %ld", path, ivalue);

	LOG_DBG("Write value of %s is %ld '%s'", path, ivalue, buf);

	retval = file_write_string(path, buf);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
write_value_by_type(const char *path, PWR_AttrDataType data_type,
		const type_union_t *value)
{
	int retval = 1;

	TRACE2_ENTER("path = '%s', data_type = %d, value = %p",
			path, data_type, value);

	switch (data_type) {
	case PWR_ATTR_DATA_UINT64:
		LOG_DBG("Write value of %s is %ld", path, value->ivalue);
		retval = file_write_uint64(path, value->ivalue);
		break;
	case PWR_ATTR_DATA_DOUBLE:
		LOG_DBG("Write value of %s is %lf", path, value->fvalue);
		retval = file_write_double(path, value->fvalue);
		break;
	default:
		LOG_FAULT("Value of %s has unknown data type %d", path, data_type);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
write_attr_value(const set_info_t *setp)
{
	const powerapi_setreq_t *setreq;
	const char *path;
	const type_union_t *value;
	int retval = 1;

	TRACE1_ENTER("setp = %p", setp);

	setreq = &setp->setreq;
	path = setreq->path;
	value = &setreq->value;

	switch (setreq->attribute) {
	case PWR_ATTR_CSTATE_LIMIT:
		retval = write_cstate_limit(path, value->ivalue);
		break;
	case PWR_ATTR_GOV:
		retval = write_governor(path, value->ivalue);
		break;
	default:
		retval = write_value_by_type(path, setreq->data_type, value);
		break;
	}

	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static void
worker_process_item(set_info_t *newset)
{
	int retval = PWR_RET_SUCCESS;
	char *path;
	socket_info_t *skinfo;
	set_info_t *defset;
	int persist;
	set_info_t *oldset;

	TRACE1_ENTER("newset = %p", newset);

	path = newset->setreq.path;
	skinfo = newset->skinfo;

	defset = g_hash_table_lookup(def_values, path);
	persist = is_persistent(skinfo);

	if (defset == NULL || persist) {
		powerapi_setreq_t *setreq;
		type_union_t *value;

		if (defset == NULL) {
			defset = set_create_item(&newset->setreq, NULL);
		} else {
			set_remove(defset, def_values);
		}

		setreq = &defset->setreq;
		value = &setreq->value;

		if (persist) {
			*value = newset->setreq.value;
		} else {
			if (read_attr_value(defset) != 0) {
				LOG_FAULT("Unable to read default value for %s!", path);
				send_ret_code_response(skinfo, PWR_RET_FAILURE);
				set_destroy(defset);
				set_destroy(newset);
				TRACE1_EXIT("");
				return;
			}
		}

		switch (setreq->attribute) {
		case PWR_ATTR_GOV:
			LOG_DBG("Setting default value for %s to %ld (%s)", path, value->ivalue,
					pwr_gov_to_string(value->ivalue));
			break;
		default:
			// print the value according to the data type
			switch (setreq->data_type) {
			case PWR_ATTR_DATA_UINT64:
				LOG_DBG("Setting default value for %s to %ld", path, value->ivalue);
				break;
			case PWR_ATTR_DATA_DOUBLE:
				LOG_DBG("Setting default value for %s to %lf", path, value->fvalue);
				break;
			default:
				// don't know how to print the value
				break;
			}
			break;
		}

		defset->timestamp = g_get_real_time();

		set_insert(defset, def_values);
	}

	// Check if this attribute has already been set from this socket
	// (my_changes). If it has, remove it so we can update it.
	oldset = g_hash_table_lookup(skinfo->my_changes, path);
	if (oldset != NULL) {
		// set request is present in my_changes, remove it from
		// my_changes and all_changes
		set_remove(oldset, skinfo->my_changes);
		set_destroy(oldset);
	}

	newset->timestamp = g_get_real_time();

	set_insert(newset, skinfo->my_changes);

	// Find the top priority value for this attribute in all_changes
	GSList *slist = g_hash_table_lookup(all_changes, path);
	set_info_t *top = slist->data;
	if (attr_value_comp(newset, top) == 0) {
		// new set request is the highest priority value, make the change
		if (write_attr_value(newset) != 0) {
			retval = PWR_RET_FAILURE;
		}
	}

	send_ret_code_response(skinfo, retval);

	TRACE1_EXIT("retval = %d", retval);
}

gpointer
worker_process_items(gpointer data)
{
	TRACE1_ENTER("data = %p", data);

	g_async_queue_ref(work_queue);

	while (daemon_run) {
		set_info_t *setp = NULL;

		setp = g_async_queue_pop(work_queue);
		if (setp->skinfo == NULL) {
			// Must be a request to exit.
			continue;
		}

		LOG_DBG("work item arrived: %s", setp->setreq.path);

		worker_process_item(setp);
	}

	g_async_queue_unref(work_queue);

	TRACE1_EXIT("");

	return NULL;
}
