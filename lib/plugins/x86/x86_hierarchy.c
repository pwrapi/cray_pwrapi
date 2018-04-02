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
 * This file contains the functions necessary for hierarchy navigation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <cray-powerapi/types.h>
#include <log.h>

#define _GNU_SOURCE // Enable GNU extensions

#include <glib.h>

#include "hierarchy.h"
#include "x86_hierarchy.h"
#include "x86_obj.h"
#include "x86_paths.h"

#include "../common/file.h"

// Declare kernel interface to determine
// current CPU for executing thread
int sched_getcpu(void);


typedef struct {
	uint32_t  cpus_max;             // Max CPUs supported by kernel

	bitmask_t *cpu_mask_possible;
	bitmask_t *cpu_mask_present;
	bitmask_t *cpu_mask_online;
} info_t;


x86_metadata_t x86_metadata;


static const char *
x86_string_parse_token(const char *str, int sep)
{
	if (str)
		str = strchr(str, sep);
	if (str)
		++str;

	return str;
}

/***
 * Parses a list of cpus and sets corresponding bits in bitmask
 * for cpus present in the list.
 *
 * List expressed as BNF:
 *
 *	LIST  = RANGE [ ',' RANGE ] <EOL>
 *	RANGE = CPU [ '-' CPU [ ':' INC ] ]
 *	CPU   = NUM
 *	INC   = NUM
 *	NUM   = [1-9][0-9]*
 *
 * Tokens
 * 	LIST		CPU list string.
 * 	RANGE		Range of CPUs, must be from low to high, or a single
 * 			CPU.
 *      CPU             A logical CPU number.
 *      INC		Increment value for subsequent values in a range.
 * 	-		Range token, separates low and high values.
 * 	,		End-of-range token (a single value is a range of 1)
 * 	:		Range increment specifier token
 *
 */
static int
x86_list_parse_bitmask(const char *list, bitmask_t *bitmask)
{
	int retval = 1;
	int r = 0;              // tokens read
	const char *range;      // Range substring

	TRACE3_ENTER("list = %p, bitmask = %p", list, bitmask);

	BITMASK_CLEARALL(bitmask);

	for (range = list; range; range = x86_string_parse_token(range, ',')) {
		uint32_t beg;   // begining value in range
		uint32_t end;   // ending value in range
		uint32_t inc;   // increment value for range
		char stop;              // token that stops read of int
		const char *tok1;
		const char *tok2;

		// Read in start of a range. Handle single value as
		// range of 1.
		if ((r = sscanf(range, "%u%c", &beg, &stop)) < 1)
			goto done;
		end = beg;
		inc = 1;

		// Find next CPU token after range and end-of-value tokens
		tok1 = x86_string_parse_token(range, '-');
		tok2 = x86_string_parse_token(range, ',');

		// If tok1 has something it is the end of a range.
		if (tok1 != NULL && (tok2 == NULL || tok1 < tok2)) {

			// Read end of range value.
			if ((r = sscanf(tok1, "%u%c", &end, &stop)) < 1)
				goto done;

			// Check if range contains an increment value
			// and read it if present.
			tok1 = x86_string_parse_token(tok1, ':');
			if (tok1 != NULL && (tok2 == NULL || tok1 < tok2)) {
				if ((r = sscanf(tok1, "%u%c", &inc, &stop)) < 1)
					goto done;
				if (inc == 0)
					goto done;
			}
		}

		if (!(beg <= end))
			goto done;
		while (beg <= end) {
			if (beg >= bitmask->used)
				goto done;
			BITMASK_SET(bitmask, beg);
			beg += inc;
		}
	}

	if (r == 2)
		goto done;

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

static int
x86_read_line(const char *path, char **buf, size_t *len)
{
	int retval = -1;
	ssize_t bytes = 0;
	FILE *file;

	TRACE3_ENTER("path = '%s', buf = %p, len = %p", path, buf, len);

	file = fopen(path, "r");
	if (!file) {
		LOG_FAULT("fopen(%s): %m", path);
		goto done;
	}

	bytes = getline(buf, len, file);

	fclose(file);

	if (bytes < 0) {
		LOG_FAULT("getline(%s): %m", path);
		goto done;
	}

	// Clean up end of string
	if (bytes > 0) {
		char *ep = (*buf) + bytes - 1;
		if (*ep == '\n') {
			*ep = '\0';
		}
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d, bytes = %zd, *buf = %p '%s', *len = %zu",
			retval, bytes, *buf, *buf, *len);

	return retval;
}

static int
x86_read_uint32(const char *path, uint32_t *value)
{
	int retval = -1;
	char *buf = NULL;
	size_t len = 0;

	TRACE3_ENTER("path = '%s', value = %p", path, value);

	if (x86_read_line(path, &buf, &len) < 0)
		goto done;

	if (sscanf(buf, "%u", value) != 1) {
		LOG_FAULT("sscanf('%s', '%%u') failed: %m", buf);
		goto done;
	}

	retval = 0;

done:
	free(buf);
	TRACE3_EXIT("retval = %d, *value = %u", retval, *value);

	return retval;
}

static int
x86_read_uint64(const char *path, uint64_t *value)
{
	int retval = -1;
	char *buf = NULL;
	size_t len = 0;

	TRACE3_ENTER("path = '%s', value = %p", path, value);

	if (x86_read_line(path, &buf, &len) < 0)
		goto done;

	if (sscanf(buf, "%lu", value) != 1) {
		LOG_FAULT("sscanf('%s', '%%lu') failed: %m", buf);
		goto done;
	}

	retval = 0;

done:
	free(buf);
	TRACE3_EXIT("retval = %d, *value = %lu", retval, *value);

	return retval;
}

static int
x86_read_socket_metadata(uint64_t socket_id, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	gboolean success = TRUE;
	gsize buflen = 0;
	gchar *buf = NULL;      // will contain allocated string
	gchar *beg = NULL;
	gchar *end = NULL;
	GError *err = NULL;     // may contain allocated error struct

#define CPU_MODEL_KEY "model name"
#define CPU_MODEL_UNK "CPU vendor/model unknown"
#define KEY_VALUE_SEP  ":"

	TRACE2_ENTER("socket_id = %lu, hierarchy = %p", socket_id, hierarchy);

	// Read contents of procfs cpuinfo file
	success = g_file_get_contents(PROCFS_CPUINFO, &buf, &buflen, &err);
	if (!success) {
		// Log the problem reading the cpuinfo, set the metadata
		// to indicate value is unknown. Call can determine if this
		// failure is a stop condition.
		LOG_FAULT("Failed to read socket metadata from %s: %s",
				PROCFS_CPUINFO, err->message);
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	// Find first instance of string "model name" as that is the key
	// for the key-value entry that describes the CPU vendor & model info.
	beg = g_strstr_len(buf, -1, CPU_MODEL_KEY);
	if (!beg) {
		// cpuinfo keys must have changed!
		LOG_FAULT("Failed to find key '%s' in %s", CPU_MODEL_KEY,
				PROCFS_CPUINFO);
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	// Find the key-value field separator
	beg = g_strstr_len(beg, -1, KEY_VALUE_SEP);
	if (!beg) {
		// cpuinfo keys must have changed!
		LOG_FAULT("Failed to find key-value field separator '%s' in %s",
				KEY_VALUE_SEP, PROCFS_CPUINFO);
		status = PWR_RET_FAILURE;
		goto error_return;
	}

	// Skip any leading whitespace to first non-ws character
	// to find the start of the value string
	beg += 1;       // advance forward from field-separator
	while (isspace(*beg)) {
		beg += 1;

		// Check for unexpected end-of-line or end-of-string
		if (*beg == '\n' || *beg == '\0') {
			LOG_FAULT("Missing value for key '%s' in %s",
					CPU_MODEL_KEY, PROCFS_CPUINFO);
			status = PWR_RET_FAILURE;
			goto error_return;
		}
	}

	// Find end-of-line after value string
	end = beg + 1;
	while (*end != '\n' && *end != '\0') {
		end += 1;
	}

	// Save the value string
	x86_metadata.socket_vendor_info = g_strndup(beg, end - beg);

error_return:
	// Check for error status
	if (status) {
		// Save string to indicate CPU model is unknown
		x86_metadata.socket_vendor_info = g_strdup(CPU_MODEL_UNK);
	}

	if (buf)
		g_free(buf);
	if (err)
		g_error_free(err);

	TRACE2_EXIT("status = %d", status);

	return status;
}


static socket_t *
x86_find_socket(hierarchy_t *hierarchy, uint64_t socket_id, uint64_t ht_id)
{
	int error = PWR_RET_SUCCESS;
	char *socket_name = NULL;
	socket_t *socket = NULL;
	socket_t *found = NULL;
	mem_t *mem = NULL;
	uint64_t rapl_pkg_id = 0, rapl_mem_id = 0;

	TRACE2_ENTER("hierarchy = %p, socket_id = %lu, ht_id = %lu",
			hierarchy, socket_id, ht_id);

	socket_name = g_strdup_printf("socket.%lu", socket_id);
	if (!socket_name) {
		LOG_FAULT("Failed to alloc socket name for search");
		error = PWR_RET_FAILURE;
		goto error_return;
	}
	found = g_hash_table_lookup(hierarchy->map, socket_name);

	// If socket_name not found create a new socket_t object to add
	// this socket to the hierarchy tree and and name map.
	if (!found) {
		x86_socket_t *x86_socket = NULL;
		x86_mem_t *x86_mem = NULL;

		socket = new_socket(socket_id, socket_name);
		if (!socket) {
			LOG_FAULT("Failed to alloc socket object %lu",
					socket_id);
			error = PWR_RET_FAILURE;
			goto error_return;
		}

		mem = new_mem(socket_id, "mem.%lu", socket_id);
		if (!mem) {
			LOG_FAULT("Failed to alloc mem object %lu", socket_id);
			error = PWR_RET_FAILURE;
			goto error_return;
		}

		//
		// Many operations require working through an OS interface
		// on one of the ht's underneath it.  Record any one of the
		// ht IDs here.
		//
		socket->ht_id = ht_id;
		mem->ht_id = ht_id;

		//
		// Find the RAPL domain.  Will be the same for both the
		// socket and memory objects.
		//
		error = x86_find_rapl_id(socket_id, &rapl_pkg_id, &rapl_mem_id);
		if (error) {
			LOG_FAULT("x86_find_rapl_id error");
			goto error_return;
		}

		x86_socket = socket->plugin_data;
		x86_socket->rapl_pkg_id = rapl_pkg_id;

		x86_mem = mem->plugin_data;
		x86_mem->rapl_pkg_id = rapl_pkg_id;
		x86_mem->rapl_mem_id = rapl_mem_id;

		error = hierarchy_insert(hierarchy, hierarchy->tree,
				&socket->obj);
		if (error) {
			// If the socket couldn't be added to the hierarchy
			// delete the socket object.
			LOG_FAULT("Failed to add %s to hierarchy",
					socket->obj.name);
			goto error_return;
		}

		// Object inserted into hierarchy, grab a reference to the
		// tree node to use as a parent, but drop the reference to
		// the object.
		found = socket;
		socket = NULL;

		error = hierarchy_insert(hierarchy, found->obj.gnode, &mem->obj);
		if (error) {
			// If the mem couldn't be added to the hierarchy
			// delete the socket object.
			LOG_FAULT("Failed to add %s to hierarchy",
					mem->obj.name);
			goto error_return;
		}

		// Object inserted into hierarchy, drop the reference
		mem = NULL;

		// Only read the metadata for socket 0. On a cray node
		// all sockets will have the same metadata.
		if (socket_id == 0)
			x86_read_socket_metadata(socket_id, hierarchy);
	}

error_return:
	// If there was an error, clean up any objects not
	// inserted into the hierarchy.
	if (error) {
		if (socket)
			del_socket(socket);
		if (mem)
			del_mem(mem);
	}

	g_free(socket_name);

	TRACE2_EXIT("found = %p", found);

	return found;
}

static core_t *
x86_find_core(hierarchy_t *hierarchy, uint64_t core_id, uint64_t socket_id,
		uint64_t ht_id)
{
	int error = PWR_RET_SUCCESS;
	char *core_name = NULL;
	core_t *core = NULL;
	core_t *found = NULL;

	TRACE2_ENTER("hierarchy = %p, core_id = %lu, socket_id = %lu, "
			"ht_id = %lu", hierarchy, core_id, socket_id, ht_id);

	core_name = g_strdup_printf("core.%lu.%lu", socket_id, core_id);
	if (!core_name) {
		LOG_FAULT("Failed to alloc core name for search");
		error = PWR_RET_FAILURE;
		goto error_return;
	}
	found = g_hash_table_lookup(hierarchy->map, core_name);

	// If core_name not found create a new core_t object to add
	// this core to the hierarchy tree and name map.
	if (!found) {
		socket_t *socket = NULL;
		socket = x86_find_socket(hierarchy, socket_id, ht_id);
		if (!socket) {
			LOG_FAULT("Failed to find socket.%lu", socket_id);
			error = PWR_RET_FAILURE;
			goto error_return;
		}

		core = new_core(core_id, core_name);
		if (!core) {
			LOG_FAULT("Failed to alloc core.%lu", core_id);
			error = PWR_RET_FAILURE;
			goto error_return;
		}

		core->socket_id = socket_id;

		error = hierarchy_insert(hierarchy, socket->obj.gnode,
				&core->obj);
		if (error) {
			LOG_FAULT("Failed to add %s to hierarchy",
					core->obj.name);
			goto error_return;
		}

		// Object inserted into hierarchy, drop the reference
		found = core;
		core = NULL;
	}

error_return:
	// If there was an error, clean up any objects not
	// inserted into the hierarchy.
	if (error) {
		if (core) {
			del_core(core);
		}
	}

	g_free(core_name);

	TRACE2_EXIT("found = %p", found);

	return found;
}

static ht_t *
x86_find_ht(hierarchy_t *hierarchy, uint64_t ht_id)
{
	int error = PWR_RET_SUCCESS;
	GString *path = NULL;
	ht_t *ht = NULL;
	ht_t *found = NULL;
	uint64_t core_id = 0;
	uint64_t socket_id = 0;
	core_t *core = NULL;

	TRACE2_ENTER("hierarchy = %p, ht_id = %lu", hierarchy, ht_id);

	path = g_string_new("");
	if (!path) {
		LOG_FAULT("Failed alloc path name");
		error = PWR_RET_FAILURE;
		goto error_return;
	}

	// read /sys/devices/system/cpu/cpu{ht_id}/topology/physical_package_id
	g_string_printf(path, TOPOLOGY_PATH, ht_id, "physical_package_id");
	x86_read_uint64(path->str, &socket_id);

	// read /sys/devices/system/cpu/cpu{ht_id}/topology/core_id
	g_string_printf(path, TOPOLOGY_PATH, ht_id, "core_id");
	x86_read_uint64(path->str, &core_id);

	core = x86_find_core(hierarchy, core_id, socket_id, ht_id);
	if (!core) {
		LOG_FAULT("Failed to find core.%lu.%lu", socket_id, core_id);
		error = PWR_RET_FAILURE;
		goto error_return;
	}

	ht = new_ht(ht_id, "ht.%lu", ht_id);
	if (!ht) {
		LOG_FAULT("Failed to alloc ht %lu", ht_id);
		error = PWR_RET_FAILURE;
		goto error_return;
	}

	error = hierarchy_insert(hierarchy, core->obj.gnode, &ht->obj);
	if (error) {
		LOG_FAULT("Failed to add %s to hierarchy", ht->obj.name);
		goto error_return;
	}

	// Object inserted into hierarchy, drop the reference
	found = ht;
	ht = NULL;

error_return:
	// If there was an error, clean up any objects not
	// inserted into the hierarchy.
	if (error) {
		if (ht)
			del_ht(ht);
	}

	g_string_free(path, TRUE);

	TRACE2_EXIT("found = %p", found);

	return found;
}

static int
x86_read_ht_cstate_metadata(uint64_t ht_id, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	char *path = NULL;
	pwr_list_uint64_t list = PWR_LIST_INIT;

	TRACE2_ENTER("ht_id = %lu, hierarchy = %p", ht_id, hierarchy);

	path = g_strdup_printf(HT_CSTATE_PATH, ht_id);
	if (!path) {
		LOG_FAULT("Error creating path string for %s: %m",
				HT_CSTATE_PATH);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	dp = opendir(path);
	if (!dp) {
		LOG_FAULT("Error opening directory %s: %m", path);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Read the directory entries and save the cstate numbers
	while ((entry = readdir(dp)) != NULL) {
		if (entry->d_type != DT_DIR) {
			continue;
		}
		if (strncmp(entry->d_name, "state", 5) != 0) {
			continue;
		}
		status = pwr_list_add_str_uint64(&list, entry->d_name + 5);
		if (status) {
			goto status_return;
		}
	}

	// Sort the list
	pwr_list_sort_uint64(&list);

	x86_metadata.ht_cstate = list;

	pwr_list_init_uint64(&list); // release reference, list has been saved

status_return:
	if (dp)
		closedir(dp);
	if (path)
		g_free(path);
	pwr_list_free_uint64(&list);

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_read_ht_freq_metadata(uint64_t ht_id, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	char *path = NULL;
	char *line = NULL;
	char **strlist = NULL;
	uint64_t num = 0;
	pwr_list_double_t list = PWR_LIST_INIT;

	TRACE2_ENTER("ht_id = %lu, hierarchy = %p", ht_id, hierarchy);

	// Construct path to sysfs file giving the list of frequency
	// values, then read the list line from the file.
	path = g_strdup_printf(HT_FREQ_LIMIT_LIST_PATH, ht_id);
	if (!path) {
		LOG_FAULT("Error creating path string for %s: %m",
				HT_CSTATE_PATH);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	status = read_line_from_file(path, 0, &line, NULL);
	if (status) {
		LOG_FAULT("Error reading line 0 from %s", path);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	// Done with the path string, clean it up.
	g_free(path);
	path = NULL;

	// strip leading/trailing whitespace
	g_strstrip(line);

	// Split the list line into the frequency value strings.
	strlist = g_strsplit(line, " ", -1);
	if (strlist == NULL) {
		LOG_FAULT("Failed to allocate frequency list");
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	g_free(line);
	line = NULL;

	// Add strings to list
	for (num = 0; strlist[num]; num++) {
		status = pwr_list_add_str_double(&list, strlist[num]);
		if (status) {
			goto status_return;
		}
	}

	// Sort the list from smallest to largest value.
	pwr_list_sort_double(&list);

	// Save metadata
	x86_metadata.ht_freq = list;

	pwr_list_init_double(&list); // release reference, list has been saved

status_return:
	if (path)
		g_free(path);
	if (line)
		g_free(line);
	if (strlist)
		g_strfreev(strlist);
	pwr_list_free_double(&list);

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_read_ht_gov_metadata(uint64_t ht_id, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	char *path = NULL;
	char *line = NULL;
	char **strlist = NULL;
	uint64_t num = 0;
	pwr_list_string_t list = PWR_LIST_INIT;

	TRACE2_ENTER("ht_id = %lu, hierarchy = %p", ht_id, hierarchy);

	// Construct path to sysfs file giving the list of governors,
	// then read the list line from the file.
	path = g_strdup_printf(HT_GOVERNOR_LIST_PATH, ht_id);
	if (!path) {
		LOG_FAULT("Error creating path string for %s: %m",
				HT_GOVERNOR_LIST_PATH);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	status = read_line_from_file(path, 0, &line, NULL);
	if (status) {
		LOG_FAULT("Error reading line 0 from %s", path);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	// Done with the path string, clean it up.
	g_free(path);
	path = NULL;

	// Split the list line into the governor value strings.
	g_strstrip(line);
	strlist = g_strsplit(line, " ", -1);
	if (!strlist) {
		LOG_FAULT("Failed to allocate governor list");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Add strings to list
	for (num = 0; strlist[num]; num++) {
		status = pwr_list_add_string(&list, strlist[num]);
		if (status) {
			goto status_return;
		}
	}

	// Save metadata
	x86_metadata.ht_gov = list;

	pwr_list_init_string(&list); // release reference, list has been saved

status_return:
	if (path)
		g_free(path);
	if (line)
		g_free(line);
	if (strlist)
		g_strfreev(strlist);
	pwr_list_free_string(&list);

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_read_ht_metadata(uint64_t ht_id, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht_id = %lu, hierarchy = %p", ht_id, hierarchy);

	// Gather HT cstate metadata: number of states supported,
	// lowest cstate, highest cstate.
	status = x86_read_ht_cstate_metadata(ht_id, hierarchy);
	if (status) {
		// Read function reports reason
		goto error_return;
	}

	// Gather HT frequency metadata: number of frequencies supported,
	// lowest frequency, highest frequency.
	status = x86_read_ht_freq_metadata(ht_id, hierarchy);
	if (status) {
		// Read function reports reason
		goto error_return;
	}

	// Gather count of HT governors supported.
	status = x86_read_ht_gov_metadata(ht_id, hierarchy);
	if (status) {
		// Read function reports reason
		goto error_return;
	}

error_return:
	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_read_topology(info_t *info, hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	uint64_t ht_id = 0;
	int cpu = 0;
	int i;

	TRACE2_ENTER("info = %p, hierarchy = %p", info, hierarchy);

	// Get number of the current CPU where this thread is
	// executing and use that as the hardware thread ID
	// to read metadata. This should be done before reading
	// the topology as creation of objects may need metadata
	// values.
	cpu = sched_getcpu();
	if (cpu < 0) {
		LOG_FAULT("Failed to determine current CPU number: %m");
		status = PWR_RET_FAILURE;
	} else {
		status = x86_read_ht_metadata((uint64_t)cpu, hierarchy);
		if (status) {
			LOG_FAULT("Failed to read HT metadata using cpu %d",
					cpu);
		}
	}

	// Walk the cpuset of possible cpus & cross-reference
	// to the cpuset of online cpus. Create a new hardware
	// thread (ht) object for the CPU and add it to the
	// hierarchy tree and name map.
	for (i = 0; i < info->cpu_mask_possible->used; i++) {
		if (BITMASK_TEST(info->cpu_mask_possible, i)) {
			if (BITMASK_TEST(info->cpu_mask_online, i)
					&& !x86_find_ht(hierarchy, ht_id)) {
				LOG_FAULT("Failed to find ht.%lu", ht_id);
				status = PWR_RET_FAILURE;
				break;  // exit the for loop
			}
			ht_id++;
		}
	}

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
x86_read_info(info_t *info)
{
	int retval = -1;
	int ret = 0;
	size_t size = 0;
	char *cpu_list = NULL;

	TRACE2_ENTER("info = %p", info);

	// Read in the maximum CPU number supported by the kernel.
	// This gives an upper-bound on possible CPU numbers.
	ret = x86_read_uint32(KERNEL_MAX_PATH, &info->cpus_max);
	if (ret < 0)
		goto done;

	// Get the list of possible CPUs.
	ret = x86_read_line(CPU_POSSIBLE_PATH, &cpu_list, &size);
	if (ret < 0)
		goto done;

	info->cpu_mask_possible = new_bitmask(info->cpus_max);
	x86_list_parse_bitmask(cpu_list, info->cpu_mask_possible);

	// Get the list of present CPUs.
	ret = x86_read_line(CPU_PRESENT_PATH, &cpu_list, &size);
	if (ret < 0)
		goto done;

	info->cpu_mask_present = new_bitmask(info->cpus_max);
	x86_list_parse_bitmask(cpu_list, info->cpu_mask_present);

	// Get the list of online CPUs.
	ret = x86_read_line(CPU_ONLINE_PATH, &cpu_list, &size);
	if (ret < 0)
		goto done;

	info->cpu_mask_online = new_bitmask(info->cpus_max);
	x86_list_parse_bitmask(cpu_list, info->cpu_mask_online);

	retval = 0;

done:
	free(cpu_list);

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Search for an object in the hierarchy hash table.
 *
 * Example:
 *   core = x86_lookup_object(hierarchy, "temp5_label", "core.%ld.%ld", 0, 3);
 * returns a pointer to the core object for socket 0, core 3. If there is no
 * such object, this returns NULL, and generates a warning message.
 *
 * This function primarily manages memory cleanup.
 *
 * @param text - identifying text for the error message
 * @param fmt - format to generate the object name
 *
 * @return void* - pointer returned by the hash lookup
 */
static void *
x86_lookup_object(hierarchy_t *hierarchy, const char *text,
		const char *fmt, ...)
{
	void *obj = NULL;
	char *name = NULL;
	va_list args;

	TRACE2_ENTER("hierarchy = %p, text = '%s', fmt = '%s', ...",
			hierarchy, text, fmt);

	va_start(args, fmt);
	name = g_strdup_vprintf(fmt, args);
	va_end(args);

	obj = g_hash_table_lookup(hierarchy->map, name);
	if (!obj) {
		LOG_FAULT("%s: '%s' not found", text, name);
	}

	g_free(name);

	TRACE2_EXIT("obj = %p", obj);

	return obj;
}

/**
 * GNodeTraverseFunc callback to walk each node of the hierarchy, and check that
 * it has a valid temp_id value (valid means > 0).
 *
 * We added the temperature IDs by scanning the HWMON directory for temperature
 * files, found the associated core/socket ID, then looked up the appropriate
 * core/socket objects through the hash table. So we're guaranteed that we
 * didn't add a temperature sensor for an object not in the hierarchy tree (if
 * we tried, the hash lookup would have failed), but there is no guarantee that
 * there were not missing temperature sensors. Though this should not happen in
 * a well-behaved Linux system.
 *
 * So as a final check, we walk the hierarchy and see that we assigned every
 * core and socket a valid temperature sensor ID values, and print warnings if
 * we missed any. This will certainly help diagnose the problem if we start
 * seeing cores without temperature values.
 *
 * @param node - current node being traversed
 * @param data - user data == &error
 *
 * @return gboolean - return true to stop the traversal
 */
static gboolean
x86_check_temp_id(GNode *node, gpointer data)
{
	obj_t *obj = (obj_t *)node->data;
	int *error = data;

	TRACE3_ENTER("node = %p, data = %p", node, data);

	if (obj->type == PWR_OBJ_SOCKET) {
		socket_t *socket = to_socket(obj);
		x86_socket_t *x86_socket = socket->plugin_data;

		LOG_DBG("%s temp_id %ld temp_input '%s'",
				obj->name, x86_socket->temp_id,
				x86_socket->temp_input);
		if (!x86_socket->temp_input) {
			LOG_FAULT("%s has no temperature ID", obj->name);
			*error = 1;
		}
	} else if (obj->type == PWR_OBJ_CORE) {
		core_t *core = to_core(obj);
		x86_core_t *x86_core = core->plugin_data;

		LOG_DBG("%s temp_id %ld temp_input '%s'",
				obj->name, x86_core->temp_id,
				x86_core->temp_input);
		if (!x86_core->temp_input) {
			LOG_FAULT("%s has no temperature ID", obj->name);
			*error = 1;
		}
	}

	TRACE3_EXIT("*error = %d", *error);

	return false;
}

static int
x86_str_to_val(const char *str, const char *prefix, uint64_t *val,
		const char *suffix)
{
	int retval = 1;
	size_t len = strlen(prefix);
	char *ep = NULL;

	TRACE3_ENTER("str = '%s', prefix = '%s', val = %p, suffix = '%s'",
			str, prefix, val, suffix);

	if (strncmp(str, prefix, len) != 0) {
		LOG_DBG("prefix mismatch");
		goto done;
	}
	str += len;

	*val = strtoul(str, &ep, 10);

	if (ep == str || strcmp(ep, suffix) != 0) {
		LOG_DBG("suffix mismatch");
		goto done;
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d, *val = %lu", retval, *val);

	return retval;
}

static int
x86_scan_temp_dir(hierarchy_t *hierarchy, socket_t *socket,
		const char *temp_dir_path)
{
	GDir *dirp = NULL;
	const char *name = NULL;
	int found = 0;

	TRACE2_ENTER("hierarchy = %p, socket = %p, temp_dir_path = '%s'",
			hierarchy, socket, temp_dir_path);

	LOG_DBG("open directory '%s'", temp_dir_path);
	dirp = g_dir_open(temp_dir_path, 0, NULL);
	if (!dirp) {
		LOG_FAULT("g_dir_open(%s): %m", temp_dir_path);
		goto done;
	}

	while ((name = g_dir_read_name(dirp))) {
		char *temp_label_path = NULL;
		char *temp_input_path = NULL;
		char *temp_max_path = NULL;
		uint64_t temp_id = -1;
		uint64_t id = -1;
		char *buf = NULL;
		size_t len = 0;

		LOG_DBG("name = '%s'", name);
		// skip if not tempN_label, N == temp_id
		if (x86_str_to_val(name, "temp", &temp_id, "_label") != 0) {
			continue;
		}
		LOG_DBG("temp_id = %ld", temp_id);
		found++;

		// read the label from the file
		temp_label_path = g_strdup_printf("%s/%s",
				temp_dir_path, name);
		if (!temp_label_path) {
			LOG_FAULT("g_strdup_printf(%s/%s) failed",
					temp_dir_path, name);
			goto next;
		}
		LOG_DBG("temp label path '%s'", temp_label_path);

		temp_input_path = g_strdup_printf("%s/temp%lu_input",
				temp_dir_path, temp_id);
		if (!temp_input_path) {
			LOG_FAULT("g_strdup_printf(%s/temp%lu_input) failed",
					temp_dir_path, temp_id);
			goto next;
		}
		LOG_DBG("temp input path '%s'", temp_input_path);

		temp_max_path = g_strdup_printf("%s/temp%lu_crit",
				temp_dir_path, temp_id);
		if (!temp_max_path) {
			LOG_FAULT("g_strdup_printf(%s/temp%lu_crit) failed",
					temp_dir_path, temp_id);
			goto next;
		}
		LOG_DBG("temp max path '%s'", temp_max_path);

		if (x86_read_line(temp_label_path, &buf, &len) != 0) {
			goto next;
		}

		// check to see if this is 'Core N', N == core id
		if (x86_str_to_val(buf, "Core ", &id, "") == 0) {
			core_t *core = NULL;

			// should be a core in the hierarchy already
			LOG_DBG("found core %lu temperature file", id);
			core = x86_lookup_object(hierarchy, temp_label_path,
					"core.%ld.%ld", socket->obj.os_id, id);
			if (core) {
				x86_core_t *x86_core = core->plugin_data;

				x86_core->temp_id = temp_id;
				x86_core->temp_input = temp_input_path;
				x86_core->temp_max = temp_max_path;
				temp_input_path = NULL;
				temp_max_path = NULL;
			}
			goto next;
		}

		// check to see if this is 'Physical id N', N == socket number
		if (x86_str_to_val(buf, "Physical id ", &id, "") == 0) {
			// already have the socket pointer
			LOG_DBG("found socket %lu temperature file", id);
			if (socket->obj.os_id == id) {
				x86_socket_t *x86_socket = socket->plugin_data;

				x86_socket->temp_id = temp_id;
				x86_socket->temp_input = temp_input_path;
				x86_socket->temp_max = temp_max_path;
				temp_input_path = NULL;
				temp_max_path = NULL;
			}
			goto next;
		}

		// label not recognized
		LOG_FAULT("%s: unexpected label: '%s'", temp_label_path, buf);

	next:
		g_free(temp_label_path);
		g_free(temp_input_path);
		g_free(buf);
	}

done:
	if (dirp) {
		LOG_DBG("close directory '%s'", temp_dir_path);
		g_dir_close(dirp);
	}
	TRACE2_EXIT("found = %d", found);

	return found;
}

/**
 * Fill in the hierarchy 'temp_id' fields for socket and core.
 *
 * @param hierarchy - fully-populated hierarchy
 */
static int
x86_read_temp_ids(hierarchy_t *hierarchy)
{
	const char *hwmon_dir_path = SYSFS_HWMON;
	GDir *dirp = NULL;
	const char *name = NULL;
	int error = 0;

	TRACE2_ENTER("hierarchy = %p", hierarchy);

	LOG_DBG("open directory '%s'", hwmon_dir_path);
	dirp = g_dir_open(hwmon_dir_path, 0, NULL);
	if (!dirp) {
		LOG_FAULT("g_dir_open(%s): %m", hwmon_dir_path);
		error = 1;
		goto done;
	}

	// Default error == 0, because we want to scan all the names before failing
	while ((name = g_dir_read_name(dirp))) {
		uint64_t socket_id = -1;
		socket_t *socket = NULL;
		char *temp_dir_path = NULL;
		int found = 0;

		LOG_DBG("name = '%s'", name);
		// skip names that don't look like hwmonN
		if (x86_str_to_val(name, "hwmon", &socket_id, "") != 0) {
			continue;
		}
		LOG_DBG("socket_id = %ld", socket_id);

		// do a hash lookup of this socket O(N)
		socket = x86_lookup_object(hierarchy, hwmon_dir_path,
				"socket.%ld", socket_id);
		if (!socket) {
			continue;
		}

		// Scan for the temp files.

#if 1 // Remove when SLES12[SP0] support is no longer needed.
		// Try the SLES12 location.
		temp_dir_path = g_strdup_printf("%s/%s/device",
				hwmon_dir_path, name);
		found = x86_scan_temp_dir(hierarchy, socket, temp_dir_path);
		g_free(temp_dir_path);
		if (found) {
			continue;
		}
#endif

		// Try the SLES12SP2 location.
		temp_dir_path = g_strdup_printf("%s/%s",
				hwmon_dir_path, name);
		found = x86_scan_temp_dir(hierarchy, socket, temp_dir_path);
		g_free(temp_dir_path);
		if (found) {
			continue;
		}

		LOG_FAULT("unable to find temperature files for socket %lu",
				socket_id);
		error = 1;
	}
	if (error != 0)
		goto done;

	// Perform a sanity check for all items
	g_node_traverse(hierarchy->tree,
			G_IN_ORDER, G_TRAVERSE_ALL, -1,
			x86_check_temp_id, &error);

done:
	if (dirp) {
		LOG_DBG("close directory '%s'", hwmon_dir_path);
		g_dir_close(dirp);
	}
	TRACE2_EXIT("error = %d", error);

	return error;
}

int
x86_read_hierarchy(hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	info_t info = { .cpus_max = 0 };

	TRACE2_ENTER("hierarchy = %p", hierarchy);

	if (x86_read_info(&info)) {
		status = PWR_RET_FAILURE;
	} else if (x86_read_topology(&info, hierarchy)) {
		status = PWR_RET_FAILURE;
	} else if (x86_read_temp_ids(hierarchy)) {
		status = PWR_RET_FAILURE;
	}

	del_bitmask(info.cpu_mask_possible);
	del_bitmask(info.cpu_mask_present);
	del_bitmask(info.cpu_mask_online);

	TRACE2_EXIT("status = %d", status);

	return status;
}
