/*
 * Copyright (c) 2017, Cray Inc.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <cray-powerapi/types.h>
#include <log.h>

#include "pwr_list.h"

void
pwr_list_init_uint64(pwr_list_uint64_t *list)
{
	TRACE3_ENTER("list = %p", list);

	memset(list, 0, sizeof(*list));

	TRACE3_EXIT("");
}

void
pwr_list_init_double(pwr_list_double_t *list)
{
	TRACE3_ENTER("list = %p", list);

	memset(list, 0, sizeof(*list));

	TRACE3_EXIT("");
}

void
pwr_list_init_string(pwr_list_string_t *list)
{
	TRACE3_ENTER("list = %p", list);

	memset(list, 0, sizeof(*list));

	TRACE3_EXIT("");
}

void
pwr_list_free_uint64(pwr_list_uint64_t *list)
{
	TRACE3_ENTER("list = %p", list);

	g_free(list->list);

	pwr_list_init_uint64(list);

	TRACE3_EXIT("");
}

void
pwr_list_free_double(pwr_list_double_t *list)
{
	TRACE3_ENTER("list = %p", list);

	g_free(list->list);

	pwr_list_init_double(list);

	TRACE3_EXIT("");
}

void
pwr_list_free_string(pwr_list_string_t *list)
{
	size_t idx = 0;

	TRACE3_ENTER("list = %p", list);

	for (idx = 0; idx < list->num; idx++) {
		g_free(list->list[idx]);
	}

	g_free(list->list);

	pwr_list_init_string(list);

	TRACE3_EXIT("");
}

int
pwr_list_add_uint64(pwr_list_uint64_t *list, uint64_t val)
{
	int status = PWR_RET_SUCCESS;
	size_t len = 0;

	TRACE3_ENTER("list = %p, val = %lu", list, val);

	if (list->num >= list->alloc) {
		size_t num = list->alloc + 20;
		void *ptr;

		ptr = g_realloc_n(list->list, num, sizeof(*list->list));
		if (!ptr) {
			LOG_FAULT("Failed to alloc list array, num = %zu", num);
			status = PWR_RET_FAILURE;
			goto status_return;
		}

		list->alloc = num;
		list->list = ptr;
	}

	if (list->num == 0) {
		list->min = list->max = val;
	} else if (val < list->min) {
		list->min = val;
	} else if (val > list->max) {
		list->max = val;
	}

	len = snprintf(NULL, 0, "%lu", val);
	if (len >= list->value_len) {
		list->value_len = len + 1;
	}

	list->list[list->num] = val;
	list->num += 1;

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_add_double(pwr_list_double_t *list, double val)
{
	int status = PWR_RET_SUCCESS;
	size_t len = 0;

	TRACE3_ENTER("list = %p, val = %lf", list, val);

	if (list->num >= list->alloc) {
		size_t num = list->alloc + 20;
		void *ptr;

		ptr = g_realloc_n(list->list, num, sizeof(*list->list));
		if (!ptr) {
			LOG_FAULT("Failed to alloc list array, num = %zu", num);
			status = PWR_RET_FAILURE;
			goto status_return;
		}

		list->alloc = num;
		list->list = ptr;
	}

	if (list->num == 0) {
		list->min = list->max = val;
	} else if (val < list->min) {
		list->min = val;
	} else if (val > list->max) {
		list->max = val;
	}

	len = snprintf(NULL, 0, "%lf", val);
	if (len >= list->value_len) {
		list->value_len = len + 1;
	}

	list->list[list->num] = val;
	list->num += 1;

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_add_string(pwr_list_string_t *list, const char *str)
{
	int status = PWR_RET_SUCCESS;
	char *dup = NULL;
	size_t len = 0;

	TRACE3_ENTER("list = %p, str = '%s'", list, str);

	if (list->num >= list->alloc) {
		size_t num = list->alloc + 20;
		void *ptr;

		ptr = g_realloc_n(list->list, num, sizeof(*list->list));
		if (!ptr) {
			LOG_FAULT("Failed to alloc list array, num = %zu", num);
			status = PWR_RET_FAILURE;
			goto status_return;
		}

		list->alloc = num;
		list->list = ptr;
	}

	dup = g_strdup(str);
	if (!dup) {
		LOG_FAULT("Failed to dup string '%s'", str);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	len = strlen(dup);
	if (len >= list->value_len) {
		list->value_len = len + 1;
	}

	list->list[list->num] = dup;
	list->num += 1;

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_add_str_uint64(pwr_list_uint64_t *list, const char *str)
{
	int status = PWR_RET_SUCCESS;
	uint64_t val = 0;
	char *ep = NULL;

	TRACE3_ENTER("list = %p, str = '%s'", list, str);

	errno = 0;
	val = g_ascii_strtoull(str, &ep, 10);
	if (ep == str || *ep != '\0' || errno != 0) {
		LOG_FAULT("Failed to convert string '%s' to integer: %m", str);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	status = pwr_list_add_uint64(list, val);

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_add_str_double(pwr_list_double_t *list, const char *str)
{
	int status = PWR_RET_SUCCESS;
	double val = 0.0;
	char *ep = NULL;

	TRACE3_ENTER("list = %p, str = '%s'", list, str);

	errno = 0;
	val = g_strtod(str, &ep);
	if (ep == str || *ep != '\0' || errno != 0) {
		LOG_FAULT("Failed to convert string '%s' to double: %m", str);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	status = pwr_list_add_double(list, val);

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}


#define PWR_COMPARE(TYPE, A, B)			\
	do {					\
		const TYPE *a = A;		\
		const TYPE *b = B;		\
						\
		if (*a < *b) return -1;		\
		if (*a > *b) return 1;		\
		/*  *a == *b  */		\
		return 0;			\
	} while (0)


static int
pwr_compare_uint64(const void *v1, const void *v2)
{
	PWR_COMPARE(uint64_t, v1, v2);
}

static int
pwr_compare_double(const void *v1, const void *v2)
{
	PWR_COMPARE(double, v1, v2);
}

void
pwr_list_sort_uint64(pwr_list_uint64_t *list)
{
	TRACE3_ENTER("list = %p", list);

	qsort(list->list, list->num, sizeof(uint64_t), pwr_compare_uint64);

	TRACE3_EXIT("");
}

void
pwr_list_sort_double(pwr_list_double_t *list)
{
	TRACE3_ENTER("list = %p", list);

	qsort(list->list, list->num, sizeof(double), pwr_compare_double);

	TRACE3_EXIT("");
}

int
pwr_list_value_at_index_uint64(pwr_list_uint64_t *list, unsigned int index,
		uint64_t *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;
	uint64_t ival = 0;

	TRACE3_ENTER("list = %p, index = %u, value = %p, value_str = %p",
			list, index, value, value_str);

	if (index >= list->num) {
		status = PWR_RET_BAD_INDEX;
		goto status_return;
	}

	ival = list->list[index];

	if (value) {
		*value = ival;
	}

	if (value_str) {
		if (sprintf(value_str, "%lu", ival) < 0) {
			LOG_FAULT("Failed to write value to value_str: %m");
			status = PWR_RET_FAILURE;
			goto status_return;
		}
	}

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_value_at_index_double(pwr_list_double_t *list, unsigned int index,
		double *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;
	double fval = 0.0;

	TRACE3_ENTER("list = %p, index = %u, value = %p, value_str = %p",
			list, index, value, value_str);

	if (index >= list->num) {
		status = PWR_RET_BAD_INDEX;
		goto status_return;
	}

	fval = list->list[index];

	if (value) {
		*value = fval;
	}

	if (value_str) {
		if (sprintf(value_str, "%lf", fval) < 0) {
			LOG_FAULT("Failed to write value to value_str: %m");
			status = PWR_RET_FAILURE;
			goto status_return;
		}
	}

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}

int
pwr_list_value_at_index_string(pwr_list_string_t *list, unsigned int index,
		uint64_t *value, char *value_str,
		int (*string_to_value)(const char *))
{
	int status = PWR_RET_SUCCESS;
	const char *str = NULL;

	TRACE3_ENTER("list = %p, index = %u, value = %p, value_str = %p, string_to_value = %p",
			list, index, value, value_str, string_to_value);

	if (index >= list->num) {
		status = PWR_RET_BAD_INDEX;
		goto status_return;
	}

	str = list->list[index];

	if (value) {
		if (!string_to_value) {
			LOG_FAULT("string_to_value() function is NULL!");
			status = PWR_RET_FAILURE;
			goto status_return;
		}
		*value = string_to_value(str);
	}

	if (value_str) {
		strcpy(value_str, str);
	}

status_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}
