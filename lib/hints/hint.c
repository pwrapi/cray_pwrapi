/*
 * Copyright (c) 2017-2018, Cray Inc.
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
 * This file contains functions for accessing frequency related attributes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include <cray-powerapi/types.h>
#include <cray-powerapi/api.h>
#include <log.h>
#include "hint.h"

// Implementation simply logs to a file
static void *logctx = NULL;
static int _initcount = 0;

// The implementation -- yep, that's it
#define	LOG(fmt, arg...)	if (logctx) pmlog_message_ctx(logctx, LOG_TYPE_MESSAGE, fmt "\n", ## arg)

/**
 * Must be called any time a PWR_Cntxt is destroyed. When the last context is
 * destroyed, this will shut down the logging system for this implementation.
 *
 * This will also sync with the logging system any time ANY PWRCntxt is
 * destroyed, meaning that we can be sure that all logged hint messages have
 * been flushed to the logging file.
 */
void
app_hint_term(void)
{
	// Always sync logging, if enabled
	if (logctx)
		pmlog_sync_ctx(logctx);

	// Do nothing until the last termination
	if (_initcount == 0 || --_initcount > 0)
		return;

	// Terminate the logging on the last termination
	if (logctx) {
		pmlog_term_ctx(logctx);
		logctx = NULL;
	}
}

/**
 * Must be called any time a new PWR_Cntxt is created. This starts up the
 * logging system for this implementation.
 */
void
app_hint_init(void)
{
	char *path = NULL;
	int64_t max_size = 0;           // use default
	int64_t max_files = 0;          // use default
	int64_t num_rings = -1;         // only MESSAGES allowed
	int64_t ring_size = -1;         // ignored
	int64_t val;

	TRACE3_ENTER("");

	// Keep track of nested initializations
	_initcount++;

	// If already initialized, do nothing
	if (logctx)
		goto done;

	// If no log path is specified, do nothing
	if (!(path = getenv("PWR_HINT_LOG_PATH")))
		goto done;

	// Override default log rotation values
	if ((val = getenvzero("PWR_HINT_MAX_FILE_SIZE")))
		max_size = val;
	if ((val = getenvzero("PWR_HINT_MAX_FILE_COUNT")))
		max_files = val;

	// Start the logging (logctx == NULL if this fails)
	logctx = pmlog_init_new(path, max_size, max_files, num_rings, ring_size);

done:
	TRACE3_EXIT("");
}

/**
 * A hint is about to be destroyed. Deal with it.
 *
 * @param apphint - apphint structure
 *
 * @return int - status value
 */
int
app_hint_destroy(apphint_t *apphint)
{
	TRACE2_ENTER("apphint = %p", apphint);
	LOG("apphint destroy %s", apphint->name);
	TRACE2_EXIT("");
	return PWR_RET_SUCCESS;
}

/**
 * Apply a hint.
 *
 * @param apphint - apphint structure
 *
 * @return int - status value
 */
int
app_hint_start(apphint_t *apphint)
{
	TRACE2_ENTER("apphint = %p", apphint);
	LOG("apphint start %s hint=%d level=%d", apphint->name,
			apphint->hint, apphint->level);
	TRACE2_EXIT("");
	return PWR_RET_SUCCESS;
}

/**
 * Unapply a hint.
 *
 * @param apphint - apphint structure
 *
 * @return int - status value
 */
int
app_hint_stop(apphint_t *apphint)
{
	TRACE2_ENTER("apphint = %p", apphint);
	LOG("apphint stop %s", apphint->name);
	TRACE2_EXIT("");
	return PWR_RET_SUCCESS;
}

/**
 * Make use of progress information.
 *
 * @param apphint - apphint structure
 * @param progress_fraction - progress fraction (0.0 to 1.0)
 *
 * @return int - status value
 */
int
app_hint_progress(apphint_t *apphint, double progress_fraction)
{
	TRACE2_ENTER("apphint = %p", apphint);
	LOG("apphint progress %s=%04.2f", apphint->name, progress_fraction);
	TRACE2_EXIT("");
	return PWR_RET_SUCCESS;
}

/**
 * EXAMPLE CODE
 *
 * This is not intended to make any sense as an implementation, but only as an
 * example to exercise some of the features that could be implemented.
 *
 * @param apphint - apphint structure
 *
 * @return int - status value
 */
int
app_hint_start_example(apphint_t *apphint)
{
	int status;
	PWR_Obj obj;
	PWR_ObjType otype;
	uint64_t cstate;

	obj = apphint->object;

	// Example of getting the object type
	status = PWR_ObjGetType(obj, &otype);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("PWR_ObjGetType() failed");
		goto done;
	}

	// Example of walking up the hierarchy
	if (otype == PWR_OBJ_HT) {
		status = PWR_ObjGetParent(obj, &obj);
		if (status != PWR_RET_SUCCESS) {
			LOG_FAULT("PWR_ObjGetParent() failed");
			goto done;
		}
	}

	// Example of getting the current state
	status = PWR_ObjAttrGetValue(obj, PWR_ATTR_CSTATE, &cstate, NULL);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("PWR_ObjAttrGetValue() failed");
		goto done;
	}

	// Example of using the hint to change an attribute
	switch (apphint->hint) {
	case PWR_REGION_COMPUTE:
		switch (apphint->level) {
		case PWR_REGION_INT_HIGHEST:
			cstate = 0;     // some value
			break;
		default:
			cstate = 0;     // some value
			break;
		}
		break;
	default:
		cstate = 0;     // some value
		break;
	}

	status = PWR_ObjAttrSetValue(obj, PWR_ATTR_CSTATE, &cstate);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("PWR_ObjAttrSetValue() failed");
		goto done;
	}

done:
	TRACE2_EXIT("status = %d", status);
	return status;
}

