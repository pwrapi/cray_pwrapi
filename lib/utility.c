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

#define _GNU_SOURCE // Enable GNU extensions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <log.h>

#include "hierarchy.h"
#include "opaque.h"
#include "utility.h"
#include "typedefs.h"


//----------------------------------------------------------------------//
// 			GLOBAL DATA INIT 				//
//----------------------------------------------------------------------//

bool
global_init(void)
{
	static bool init = false;

	TRACE2_ENTER("init = %d", init);

	if (init) {
		TRACE2_EXIT("");
		return true;
	}

	// Initialize the global opaque map
	opaque_map = opaque_map_new();
	if (!opaque_map) {
		LOG_FAULT("Unable to allocate opaque context map");
		goto failure_return;
	}

	// Initialize the architecture plugin
	plugin = new_plugin();
	if (!plugin) {
		LOG_FAULT("Unable to allocate plugin data");
		goto failure_return;
	}

	// Modify the sysfile structure once during global initialization
	configure_sysfiles();

	init = true;

failure_return:
	// If init failed, cleanup anything that was allocated
	if (!init) {
		if (plugin) {
			del_plugin(plugin);
			plugin = NULL;
		}

		if (opaque_map) {
			opaque_map_free(opaque_map);
			opaque_map = NULL;
		}
	}

	TRACE2_EXIT("init = %d", init);

	return init;
}

/**
 * Local 'whitespace' inline to indicate space, tab, or LF.
 *
 * @param c - character to evaluate
 *
 * @return bool - true if c is space, tab, or LF
 */
static inline bool
_whitespace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n');
}

/**
 * Local advance over whitespace.
 *
 * @param p - pointer to string
 *
 * @return char* - pointer to first non-whitespace (may be NUL)
 */
static inline char *
_skipspace(char *p)
{
	while (*p && _whitespace(*p))
		p++;
	return p;
}

/**
 * Compare a string with a reference string, and make sure that all of the
 * printf format specifiers match. This is limited to the format specifiers used
 * in the path names, which is a subset of all possible specifiers.
 *
 * @param ref - reference string
 * @param str - string to be tested
 *
 * @return bool - true if strings are compatible, false otherwise
 */
static bool
_fmtmatch(const char *ref, const char *str)
{
	// Scan until one or both strings are exhausted
	while (*ref && *str) {
		// Advance each string independently to '%'
		while (*ref && *ref != '%')
			ref++;
		while (*str && *str != '%')
			str++;
		// Compare for equality
		while (*ref && *str) {
			// Fatal mismatch
			if (*ref != *str)
				return false;
			// Format descriptor terminators
			if (*ref == 'd' ||
					*ref == 'u' ||
					*ref == 'x' ||
					*ref == 'f' ||
					*ref == 's')
				break;
			// Advance
			ref++;
			str++;
		}
	}
	// Both strings must be empty, or mismatch
	return (!*ref && !*str);
}

/**
 * Read the sysfile configuration file, and modify the sysfile structure.
 *
 * The generic sysfile structure is a collection of sysentry_t structures, which
 * can be viewed either as a structure (with named fields), or as an array of
 * sysentry_t structures. Since the structure contains the name of the field,
 * we can either cast the structure and dereference the field, OR we can search
 * through the array structure for the field name. In this generic routine, we
 * do not need to dereference the fields.
 *
 * The structure is statically allocated and initialized in the architecture
 * specific plugin code when the plugin is initialized, so all of the values are
 * static strings. If the configuration file wants us to change these, we use a
 * dynamic string, allocated on the heap. To do cleanup properly, we keep all of
 * the allocated string pointers on an unordered list, and to clean up, we
 * simply free the list and its contents, while setting all of the value
 * pointers in the structure to NULL.
 *
 * We keep a backup structure with all of the original (static) values, and we
 * can use this to restore any NULL values.
 *
 * So the basic algorithm is:
 *
 * - set all value pointers to NULL, and delete all dynamic strings
 * - read the file, create dynamic strings as directed, and set value pointers
 * - replace all remaining NULL value pointers with the original static values.
 */
void
configure_sysfiles(void)
{
	static sysentry_t *backup = NULL;
	static GList *strings = NULL;
	bool error = true;
	int lineno = 0;
	FILE *fid = NULL;
	char *buf = NULL;
	size_t bufsiz = 0;
	char *root = NULL;
	char *tmp;
	sysentry_t *s;
	sysentry_t *t;

	TRACE2_ENTER("");

	// Sanity check
	if (!plugin || !plugin->sysfile_catalog) {
		LOG_FAULT("plugin not initialized");
		goto quit;
	}

	// On the first run of this in a process, copy the catalog to backup
	if (!backup) {
		size_t siz = sizeof(sysentry_t);        // trailing EOD
		LOG_DBG("Initializing backup");
		for (s = plugin->sysfile_catalog; s->key != NULL; s++)
			siz += sizeof(sysentry_t);
		if (!(backup = malloc(siz))) {
			LOG_FAULT("Cannot allocate sysfile_catalog backup");
			goto quit;
		}
		memcpy(backup, plugin->sysfile_catalog, siz);
		LOG_DBG("Initialized backup");
	}

	// Clear all of the values in the catalog, and release any held memory
	for (s = plugin->sysfile_catalog; s->key != NULL; s++)
		s->val = NULL;
	g_list_free_full(strings, free);
	strings = NULL;
	LOG_DBG("Cleared sysfile_catalog");

	// If we can't open the catalog file, it's okay - file is optional
	if (!(fid = fopen(POWERAPI_SYSFILE_CFG_PATH, "r"))) {
		LOG_DBG("no file, done");
		error = false;
		goto done;
	}

	// Read the sysfile configuration file
	while (getline(&buf, &bufsiz, fid) != -1) {
		char *key;
		char *val;
		char *p;

		// Keep track of lines for errors
		lineno++;

		// Trim whitespace and LF from end of line
		p = buf + strlen(buf);
		while (--p >= buf && _whitespace(*p))
			*p = 0;

		// Trim whitespace from beginning of line
		p = _skipspace(buf);
		key = p;

		// Ignore empty line or a comment
		if (*p == '#' || *p == 0)
			continue;

		// Advance to the next whitespace, or the = sign
		while (*p && *p != '=' && !_whitespace(*p))
			p++;

		// Clobber all whitespace and '='
		while (_whitespace(*p))
			*p++ = 0;

		// We expect '='
		if (*p != '=') {
			LOG_FAULT("%s:%d malformed line, no '=' found",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}

		// Clobber the '='
		*p++ = 0;

		// Clobber remaining whitespace
		while (_whitespace(*p))
			*p++ = 0;

		// There needs to be an absolute path
		if (*p != '/') {
			LOG_FAULT("%s:%d malformed line, no '/' found on path",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}

		// Just for coding clarity
		val = p;

		// If this is a 'chroot' directive, record the value
		if (!strcmp("chroot", key)) {
			// Trim trailing slashes
			p = val + strlen(val);
			while (--p >= val && *p == '/')
				*p = 0;
			root = strdup(val);
			LOG_DBG("chroot to '%s'", root);
			continue;
		}

		// Otherwise, see if we can find the key in our structure
		for (s = plugin->sysfile_catalog, t = backup; s->key != NULL; s++, t++) {
			if (!strcmp(s->key, key))
				break;
		}

		// If we can't find the key, it's a file error
		if (s->key == NULL) {
			LOG_FAULT("%s:%d key '%s' not recognized",
					POWERAPI_SYSFILE_CFG_PATH, lineno, key);
			goto done;
		}

		// If the value is already set, duplicate in file, error
		if (s->val) {
			LOG_FAULT("%s:%d key '%s' is a duplicate",
					POWERAPI_SYSFILE_CFG_PATH, lineno, key);
			goto done;
		}

		// Evaluate the val for printf descriptors
		if (!_fmtmatch(t->val, val)) {
			LOG_FAULT("%s:%d '%s' format not compatible with '%s'",
					POWERAPI_SYSFILE_CFG_PATH, lineno, val, t->val);
			goto done;
		}

		// Value is usable, duplicate into persistent storage
		if (!(tmp = strdup(val))) {
			LOG_FAULT("%s:%d strdup failed",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}
		// Store the duplicate on the list of duplicates
		if (!(strings = g_list_prepend(strings, tmp))) {
			LOG_FAULT("%s:%d g_list_prepend failed",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}
		// Set the value of our sysfile catalog
		s->val = tmp;
		LOG_DBG("sysfile %s = '%s'", s->key, s->val);
	}

	// If we're not doing a chroot, we're finished
	if (root == NULL) {
		LOG_DBG("no chroot, done");
		error = false;
		goto done;
	}

	// We're doing a chroot, so we need to fill in all the rest of the values
	for (s = plugin->sysfile_catalog, t = backup; s->key != NULL; s++, t++) {
		// Already assigned, skip
		if (s->val != NULL)
			continue;
		// Prepend the chroot to the default value
		if (asprintf(&tmp, "%s%s", root, t->val) == -1) {
			LOG_FAULT("%s:%d asprintf failed",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}
		// Store the duplicate on the list of duplicates
		if (!(strings = g_list_prepend(strings, tmp))) {
			LOG_FAULT("%s:%d g_list_prepend failed",
					POWERAPI_SYSFILE_CFG_PATH, lineno);
			goto done;
		}
		// Set the value of our sysfile catalog
		s->val = tmp;
		LOG_DBG("sysfile %s = '%s' (chroot)", s->key, s->val);
	}

	// Success!
	error = false;

done:
	// Close the file, if open
	if (fid)
		fclose(fid);

	// Free memory
	free(root);
	free(buf);

	// If we got here with errors, erase everything done
	if (error) {
		LOG_FAULT("clean up after error, restore defaults");
		for (s = plugin->sysfile_catalog; s->key != NULL; s++)
			s->val = NULL;
		g_list_free_full(strings, free);
		strings = NULL;
	}

	// We expect there to be holes from the above, fill them in
	for (s = plugin->sysfile_catalog, t = backup; s->key != NULL; s++, t++) {
		if (s->val == NULL) {
			s->val = t->val;
			LOG_DBG("sysfile %s = '%s' (default)", s->key, s->val);
		}
	}

quit:
	TRACE2_EXIT("");
}
