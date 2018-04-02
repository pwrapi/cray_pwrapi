/*
 * Copyright (c) 2015-2016, 2018, Cray Inc.
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

#include <sys/types.h>
#include <sys/wait.h>

#include <cray-powerapi/types.h>
#include <log.h>

#include "file.h"

/*
 * read_val_from_file - Reads the contents of the specified file and converts
 *		        it to the specified type.  Assumes file contains a
 *		        single value but can be of any type.
 *
 * Argument(s):
 *
 *	path - Path to file to read
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
read_val_from_file(const char *path, void *val, val_type_t type,
		struct timespec *tspec)
{
	int retval = PWR_RET_FAILURE;
	gchar *buf = NULL;

	TRACE2_ENTER("path = '%s', val = %p, type = %d, tspec = %p",
			path, val, type, tspec);

	/*
	 * Target file contains a single value.
	 */
	if (!g_file_get_contents(path, &buf, NULL, NULL)) {
		LOG_FAULT("File '%s' read failed", path);
		goto done;
	}

	/*
	 * Read value out of buffer
	 */
	retval = read_val_from_buf(buf, val, type, tspec);

done:
	/*
	 * Clean up and return.
	 */
	if (buf)
		g_free(buf);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

/*
 * read_line_from_file - Reads the contents of the specified file and returns
 *			 the requested line.
 *
 * Argument(s):
 *
 *	path - Path to file to read
 *	num  - line number to read
 *	line - return pointer to copy of the line
 *	tspec - Target memory to hold timestamp of when data sample is taken.
 *		If NULL, no timestamp is taken.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
read_line_from_file(const char *path, unsigned int num, char **line,
		struct timespec *tspec)
{
	int status = PWR_RET_SUCCESS;
	gchar *buf = NULL;
	gchar **lines = NULL;
	int i = 0;

	TRACE2_ENTER("path = '%s', num = %u, line = %p, tspec = %p",
			path, num, line, tspec);

	/*
	 * Target file contains a single value.
	 */
	if (!g_file_get_contents(path, &buf, NULL, NULL)) {
		LOG_FAULT("Failed to read file %s", path);
		status = PWR_RET_FAILURE;
		goto error_handling;
	}

	/*
	 * Grab timestamp.  If NULL no timestamp is taken.  Timestamp is
	 * nanoseconds since the Epoch.
	 */
	if (tspec != NULL) {
		if (clock_gettime(CLOCK_REALTIME, tspec)) {
			LOG_FAULT("Failed to get timestamp");
			status = PWR_RET_FAILURE;
			goto error_handling;
		}
	}

	/*
	 * Tokenize the file contents with \n as the separator.
	 */
	lines = g_strsplit(buf, "\n", -1);

	/*
	 * Try to walk the vector to the requested line, it is an error
	 * if the line doesn't exist.
	 */
	for (i = 0; i <= num; i++) {
		if (lines[i] == NULL) {
			status = PWR_RET_FAILURE;
			goto error_handling;
		}
	}

	/* duplicate the line to return to caller */
	*line = g_strdup(lines[num]);


error_handling:
	/*
	 * Clean up and return.
	 */
	g_free(buf);
	g_strfreev(lines);

	TRACE2_EXIT("status = %d", status);

	return status;
}
