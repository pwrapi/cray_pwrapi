/*
 * Copyright (c) 2016, Cray Inc.
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

#include "command.h"

/*
 * read_val_from_command - Executes a command and converts a single output
 *			   value to the specified type.
 *
 * Argument(s):
 *
 *	command - The command to execute
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
read_val_from_command(const char *command, void *val, val_type_t type,
		struct timespec *tspec)
{
	int retval = PWR_RET_FAILURE;
	gchar *buf = NULL;
	int spawn_status = 0;
	int exit_status = 0;

	TRACE2_ENTER("command = '%s', val = %p, type = %d, tspec = %p",
			command, val, type, tspec);

	/*
	 * Error check arguments.
	 */
	if (command == NULL || val == NULL)
		goto done;

	/*
	 * Command should output a single value to stdout.
	 */
	spawn_status = g_spawn_command_line_sync(command, &buf, NULL, &exit_status,
			NULL);
	if ((spawn_status == FALSE) || (WEXITSTATUS(exit_status) != 0))
		goto done;

	/*
	 * Read value out of buffer
	 */
	retval = read_val_from_buf(buf, val, type, tspec);

done:
	/*
	 * Clean up and return.
	 */
	g_free(buf);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}
