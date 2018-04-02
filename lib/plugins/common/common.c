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
 * This file contains common functions for plugin functions.
 */

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include <cray-powerapi/types.h>
#include <log.h>

#include "common.h"

/*
 * read_val_from_buf - Converts a single value in a character buffer to the
 *		       specified type.
 *
 * Argument(s):
 *
 *	buf - The buffer containing string
 *	val - Target memory to hold value
 *	type - Target type to convert to
 *	tspec - Target memory to hold timestamp of when data sample is taken.
 *		If NULL, no timestamp is taken.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
read_val_from_buf(const char *buf, void *val, val_type_t type,
		struct timespec *tspec)
{
	int retval = PWR_RET_FAILURE;
	gchar *tmp_buf = NULL;

	TRACE2_ENTER("buf = %p, val = %p, type = %d, tspec = %p",
			buf, val, type, tspec);

	/*
	 * Grab timestamp.  If NULL no timestamp is taken.  Timestamp is
	 * nanoseconds since the Epoch.
	 */
	if (tspec != NULL) {
		if (clock_gettime(CLOCK_REALTIME, tspec)) {
			LOG_FAULT("clock_gettime() failed: %m");
			goto done;
		}
	}

	/*
	 * Copy buffer so we can modify it.
	 */
	tmp_buf = g_strdup(buf);
	if (tmp_buf == NULL) {
		LOG_FAULT("g_strdup() failed: %m");
		goto done;
	}

	/*
	 * Get rid of '\n' character if it exists.
	 */
	g_strstrip(tmp_buf);

	/*
	 * Convert from string.
	 */
	switch (type) {
	case TYPE_UINT64:
		*(uint64_t *)val = strtoul(tmp_buf, NULL, 0);
		if (*(uint64_t *)val == LLONG_MIN
				|| *(uint64_t *)val == LLONG_MAX) {
			LOG_FAULT("'%s' out of range", tmp_buf);
			goto done;
		}
		break;
	case TYPE_DOUBLE:
		*(double *)val = strtod(tmp_buf, NULL);
		if (*(double *)val == HUGE_VAL
				|| *(double *)val == -HUGE_VAL) {
			LOG_FAULT("'%s' out of range", tmp_buf);
			goto done;
		}
		break;
	case TYPE_STRING:
		*(char **)val = (char *)g_strdup(tmp_buf);
		if (*(char **)val == NULL) {
			LOG_FAULT("g_strdup() failed: %m");
			goto done;
		}
		break;
	default:
		LOG_FAULT("Invalid type %d", type);
		goto done;
	}

	retval = PWR_RET_SUCCESS;

done:
	/*
	 * Clean up and return.
	 */
	if (tmp_buf)
		g_free(tmp_buf);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
convert_double_to_uint64(const double *dvalue, uint64_t *ivalue)
{
	int retval = PWR_RET_FAILURE;
	double dval = 0.0;
	uint64_t ival = 0;

	TRACE2_ENTER("dvalue = %p, ivalue = %p", dvalue, ivalue);

	dval = *dvalue;
	ival = dval;

	if ((double)ival == dval) {
		retval = PWR_RET_SUCCESS;
		*ivalue = ival;
	}

	TRACE2_EXIT("retval = %d, dval = %lf, ival = %lu", retval, dval, ival);

	return retval;
}
