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
 * Functions for managing powerapid's permissions file.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "permissions.h"

static int
file_copy(const char *frompath, const char *topath)
{
	int retval = 0;
	FILE *from = NULL, *to = NULL;
	char line[32];

	TRACE2_ENTER("frompath = '%s', topath = '%s'", frompath, topath);

	from = fopen(frompath, "r");
	if (from == NULL) {
		LOG_FAULT("unable to open source file %s: %m", frompath);
		retval = 1;
		goto error_handling;
	}

	// need to do anything with file permissions here?
	to = fopen(topath, "w");
	if (to == NULL) {
		LOG_FAULT("unable to create destination file %s: %m", topath);
		retval = 2;
		goto error_handling;
	}

	while (fgets(line, sizeof(line), from))
		fputs(line, to);

error_handling:
	if (from)
		fclose(from);
	if (to)
		fclose(to);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
file_search_uint(const char *filepath, const unsigned int key)
{
	int retval = 1; // initialize to unsuccessful
	FILE *fp = NULL;
	unsigned int current = 0;

	TRACE2_ENTER("filepath = %s, key = %u", filepath, key);

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		LOG_FAULT("unable to open file %s: %m", filepath);
		retval = 2;
		goto error_handling;
	}

	while (fscanf(fp, "%u", &current) == 1) {
		if (key == current) {
			retval = 0;
			break;
		}
	}

error_handling:
	if (fp)
		fclose(fp);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// truncate the current permissions file
int
clear_permissions_file(void)
{
	int retval = 0;
	FILE *fp = NULL;

	TRACE2_ENTER("");

	fp = fopen(CURR_PERMISSIONS_FILE, "w");
	if (!fp) {
		LOG_FAULT("unable to clear file %s: %m",
				CURR_PERMISSIONS_FILE);
		retval = 1;
		goto error_handling;
	}

error_handling:
	if (fp)
		fclose(fp);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// copy the boot-time permissions file to the current permissions file
int
restore_permissions_file(void)
{
	int retval = 0;

	TRACE2_ENTER("");

	retval = file_copy(BOOT_PERMISSIONS_FILE, CURR_PERMISSIONS_FILE);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// search through current permissions file, an unsorted file of integers,
// for uid.  Return 0 if uid is found, non-zero otherwise.
int
check_permissions_file(const unsigned int uid)
{
	int retval = 0;

	TRACE2_ENTER("uid = %u", uid);

	retval = file_search_uint(CURR_PERMISSIONS_FILE, uid);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// add uid to current permissions file if it's not already there
int
add_uid_permissions_file(const unsigned int uid)
{
	int retval = 1;
	FILE *fp = NULL;

	TRACE2_ENTER("");

	if (file_search_uint(CURR_PERMISSIONS_FILE, uid) == 0) {
		retval = 0;
		goto error_handling;
	}

	fp = fopen(CURR_PERMISSIONS_FILE, "a");
	if (fp == NULL) {
		LOG_FAULT("unable to open file %s: %m",
				CURR_PERMISSIONS_FILE);
		goto error_handling;
	}

	fprintf(fp, "%u\n", uid);

	retval = 0;

error_handling:
	if (fp)
		fclose(fp);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

// delete uid from current permissions file if it's there
int
del_uid_permissions_file(const unsigned int uid)
{
	int retval = 1;
	FILE *fp1 = NULL, *fp2 = NULL;
	unsigned int current = 0;

	TRACE2_ENTER("");

	if (file_search_uint(CURR_PERMISSIONS_FILE, uid) != 0) {
		retval = 0;
		goto error_handling;
	}

	fp1 = fopen(CURR_PERMISSIONS_FILE, "r");
	if (fp1 == NULL) {
		LOG_FAULT("unable to open file %s: %m",
				CURR_PERMISSIONS_FILE);
		goto error_handling;
	}

	fp2 = fopen(TEMP_PERMISSIONS_FILE, "w");
	if (fp2 == NULL) {
		LOG_FAULT("unable to open file %s: %m",
				TEMP_PERMISSIONS_FILE);
		goto error_handling;
	}

	// Note: no error if uid wasn't found
	while (fscanf(fp1, "%u", &current) == 1) {
		if (uid == current)
			continue;
		fprintf(fp2, "%u\n", current);
	}

	fclose(fp1);
	fclose(fp2);
	fp1 = fp2 = NULL;

	if (remove(CURR_PERMISSIONS_FILE) != 0) {
		LOG_FAULT("unable to remove file %s: %m",
				CURR_PERMISSIONS_FILE);
		goto error_handling;
	}

	if (rename(TEMP_PERMISSIONS_FILE, CURR_PERMISSIONS_FILE) != 0) {
		LOG_FAULT("unable to rename file %s to %s: %m",
				TEMP_PERMISSIONS_FILE, CURR_PERMISSIONS_FILE);
		goto error_handling;
	}

	retval = 0;

error_handling:
	if (fp1)
		fclose(fp1);
	if (fp2)
		fclose(fp2);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}
