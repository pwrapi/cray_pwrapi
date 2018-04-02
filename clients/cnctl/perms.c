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
 * Wrapper functions for the permissions file functions which translate the
 * result into json.
 *
 */

#include <stdio.h>

#include <permissions.h>

#include "log.h"
#include "perms.h"
#include "io.h"

int64_t specified_uid = -1;

void
perms_add(void)
{
	int retval;

	TRACE2_ENTER("specified_uid = %lu", specified_uid);

	retval = add_uid_permissions_file(specified_uid);

	if (retval)
		set_json_ret_code(retval);

	TRACE2_EXIT("retval = %d", retval);
}

void
perms_remove(void)
{
	int retval;

	TRACE2_ENTER("specified_uid = %lu", specified_uid);

	retval = del_uid_permissions_file(specified_uid);

	if (retval)
		set_json_ret_code(retval);

	TRACE2_EXIT("retval = %d", retval);
}

void
perms_clear(void)
{
	int retval;

	TRACE2_ENTER("");

	retval = clear_permissions_file();

	if (retval)
		set_json_ret_code(retval);

	TRACE2_EXIT("retval = %d", retval);
}

void
perms_restore(void)
{
	int retval;

	TRACE2_ENTER("");

	retval = restore_permissions_file();

	if (retval)
		set_json_ret_code(retval);

	TRACE2_EXIT("retval = %d", retval);
}

void
perms_list(void)
{
	TRACE2_ENTER("");

	print_perms_list(CURR_PERMISSIONS_FILE);

	TRACE2_EXIT("");
}
