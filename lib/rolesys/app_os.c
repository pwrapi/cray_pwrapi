/*
 * Copyright (c) 2015-2018, Cray Inc.
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
 * This file contains functions for the Application, Operating System Interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include <glib.h>

#include "app_os.h"
#include "hints/hint.h"
#include "../plugins/cpudev/cstate.h"
#include "../plugins/cpudev/freq.h"

//----------------------------------------------------------------------//
//              External APP/OS Interfaces                              //
//----------------------------------------------------------------------//

/**
 * Sort comparator function for context name list.
 *
 * @param v1 - pointer to (char *) string
 * @param v2 - pointer to (char *) string
 * @param data - NULL
 *
 * @return int - -1, 0, or 1 sort comparison
 */
static int
_sortnames(const void *v1, const void *v2, void *data)
{
	char *s1 = (char *)v1;
	char *s2 = (char *)v2;
	return strcmp(s1, s2);
}

/**
 * Sort comparator function for hint list.
 *
 * @param v1 - pointer to (hint_t *) structure
 * @param v2 - pointer to (hint_t *) structure
 * @param data - NULL
 *
 * @return int - -1, 0, or 1 sort comparison
 */
static int
_sorthints(const void *v1, const void *v2, void *data)
{
	hint_t *h1 = (hint_t *)v1;
	hint_t *h2 = (hint_t *)v2;
	return strcmp(h1->name, h2->name);
}

/**
 * Delete a hint.
 *
 * There are three references to each hint_t object.
 *
 * One reference is in the global opaque_map, which maps a random hash to
 * &hint.opaque == &hint.
 *
 * One reference is in the context hintnames sorted b-tree, which contains
 * hint.name, which is a pointer to heap memory for the hint name.
 *
 * One reference -- the primary reference -- is in the object hints sorted
 * b-tree, which contains &hint.
 *
 * This routine is used as the cleanup pointer for removal from the object hints
 * b-tree: so the "correct" way to delete a hint is to simply remove it from the
 * object hints, and this routine will do everything EXCEPT removing it from
 * this list. This function also gets called directly as cleanup of partial
 * construction below, and should be safe.
 *
 * @param hintptr - hint pointer
 */
static void
_del_hint(void *hintptr)
{
	hint_t *hint = (hint_t *)hintptr;
	TRACE2_ENTER("hint = %p", hint);

	if (hint) {
		GSequenceIter *iter;
		// Remove the context name, if it exists
		iter = g_sequence_lookup(hint->ctxptr->hintnames,
				hint->name, _sortnames, NULL);
		if (iter)
			g_sequence_remove(iter);
		// Remove the opaque key, if it exists
		if (hint->opaque.key)
			opaque_map_remove(opaque_map, hint->opaque.key);
		// Free the name
		free(hint->name);
		// Free the hint
		free(hint);
	}

	TRACE2_EXIT("");
}

/**
 * Create a new hint and create the opaque key.
 *
 * This does no other initialization.
 *
 * @return hint_t* - new hint pointer
 */
static hint_t *
_new_hint(void)
{
	hint_t *hint;

	TRACE2_ENTER("");

	// Allocate space
	hint = g_new0(hint_t, 1);
	if (hint) {
		// Insert into the opaque map
		if (!opaque_map_insert(opaque_map, OPAQUE_HINT, &hint->opaque)) {
			LOG_FAULT("Unable to insert hint into opaque_map");
			// Clean up partial creation
			_del_hint(hint);
			hint = NULL;
		}
	}

	TRACE2_EXIT("hint = %p", hint);
	return hint;
}

/**
 * Initialize a sequence (b-tree) structure for an object, registering the
 * _del_hint() cleanup routine. This should ONLY be called when initializing a
 * new object against which a new hint can be created.
 *
 * If this fails, it returns NULL, and the caller should post an error message.
 *
 * @param void
 *
 * @return GSequence* - sequence (b-tree) pointer for empty tree
 */
GSequence *
init_hints(void)
{
	GSequence *hints;
	TRACE2_ENTER("");
	hints = g_sequence_new(_del_hint);
	TRACE2_EXIT("hints = %p", hints);
	return hints;
}

/**
 * Clean up a sequence (b-tree) structure for an object. This will automatically
 * call _del_hint() on every node in the tree, and then deletes the tree itself.
 *
 * @param obj_hints - hint sequence for an object
 */
void
destroy_hints(GSequence *obj_hints)
{
	TRACE2_ENTER("obj_hints = %p", obj_hints);
	if (obj_hints)
		g_sequence_free(obj_hints);
	TRACE2_EXIT("");
}

/**
 * Create a new hint region.
 *
 * A valid object must be specified.
 *
 * The name of the new region can be specified, in which case it must be
 * globally unique within the context. It can also be passed as NULL, in which
 * case a new (unique) name will be generated.
 *
 * The opaque reference for use in other AppHint commands is returned through
 * the hint_region_id pointer.
 *
 * @param obj - opaque reference to object associated with hint
 * @param hint_region_name - unique region name, or NULL
 * @param hint_region_id - returned opaque hint region ID
 * @param hint - hint
 * @param level - hint intensity
 *
 * @return int - status code
 */
int
PWR_AppHintCreate(PWR_Obj obj, const char *hint_region_name,
		      uint64_t *hint_region_id, PWR_RegionHint hint,
		      PWR_RegionIntensity level)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(obj);
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(obj);
	context_t *ctxptr = NULL;
	obj_t *objptr = NULL;
	hint_t *hintptr = NULL;
	char localname[128];

	TRACE1_ENTER("obj = %p, hint_region_name = %p '%s', "
			"hint_region_id = %p, hint = %d, level = %d",
			obj, hint_region_name, hint_region_name,
			hint_region_id, hint, level);

	// Sanity checks
	if (!hint_region_id) {
		LOG_FAULT("hint_region_id pointer required");
		goto done;
	}
	if (hint < 0 || hint >= PWR_NUM_REGION_HINTS) {
		LOG_FAULT("invalid hint = %d", hint);
		goto done;
	}
	if (level < 0 || level >= PWR_NUM_REGION_INTENSITIES) {
		LOG_FAULT("invalid intensity = %d", level);
		goto done;
	}

	// Find the object
	objptr = opaque_map_lookup_object(opaque_map, obj_key);
	if (!objptr) {
		LOG_FAULT("object not found!");
		goto done;
	}

	// Find the object context
	ctxptr = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctxptr) {
		LOG_FAULT("object context not found!");
		goto done;
	}

	// Check the region name
	if (!hint_region_name) {
		// Auto-generate a unique name
		do {
			snprintf(localname, sizeof(localname), "hint.%s.%lu",
					objptr->name, ++ctxptr->hintunique);
		} while (g_sequence_lookup(ctxptr->hintnames, localname,
				_sortnames, NULL));
		hint_region_name = localname;
	} else if (g_sequence_lookup(ctxptr->hintnames, (char *)hint_region_name,
			_sortnames, NULL)) {
		// Specified name already exists, fail
		LOG_FAULT("hint name '%s' already exists!", hint_region_name);
		goto done;
	}

	// Create the hint
	hintptr = _new_hint();
	if (!hintptr) {
		LOG_FAULT("unable to create new hint!");
		goto done;
	}
	hintptr->objptr = objptr;
	hintptr->ctxptr = ctxptr;
	hintptr->object = obj;
	hintptr->name = strdup(hint_region_name);
	hintptr->hint = hint;
	hintptr->level = level;

	// Add the name to the context list, for subsequent uniqueness check
	if (!g_sequence_insert_sorted(ctxptr->hintnames, hintptr->name,
			_sortnames, NULL)) {
		LOG_FAULT("unable to insert hint name into context sequence");
		goto done;
	}

	/*
	 * Add the hint itself to the object. This must be the last thing done,
	 * so that if it fails, we can simply call del_hint() directly.
	 */
	if (!g_sequence_insert_sorted(objptr->hints, hintptr,
			_sorthints, NULL)) {
		LOG_FAULT("unable to insert hint into object sequence");
		goto done;
	}

	// Provide opaque_key to the caller
	*hint_region_id = (uint64_t)OPAQUE_GENERATE(
			ctxptr->opaque.key, hintptr->opaque.key);

	status = PWR_RET_SUCCESS;

done:
	/*
	 * On failure, unmake the hint. Note that since the last thing done is
	 * to link it to the object, a failure means it wasn't linked to the
	 * object. So we don't need to unlink, we can just destroy it.
	 */
	if (status != PWR_RET_SUCCESS) {
		// Clean up partial creation
		_del_hint(hintptr);
	}

	TRACE1_EXIT("status = %d", status);
	return status;
}

/**
 * Destroy a hint region.
 *
 * @param hint_region_id - opaque reference to hint region
 *
 * @return int - status code
 */
int PWR_AppHintDestroy(uint64_t hint_region_id)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t hint_key = OPAQUE_GET_DATA_KEY(hint_region_id);
	hint_t *hintptr = NULL;
	apphint_t apphint;
	GSequenceIter *iter;

	TRACE1_ENTER("hint_region_id = %lu", hint_region_id);

	// Find the hint object
	hintptr = opaque_map_lookup_hint(opaque_map, hint_key);
	if (!hintptr) {
		LOG_FAULT("hint not found!");
		goto done;
	}

	// Allow the plugin to deal with active hints being destroyed
	memset(&apphint, 0, sizeof(apphint_t));
	apphint.object = hintptr->object;
	apphint.name = hintptr->name;
	apphint.hint = hintptr->hint;
	apphint.level = hintptr->level;
	if (app_hint_destroy(&apphint) != PWR_RET_SUCCESS) {
		LOG_FAULT("hint cannot be destroyed at this time");
		goto done;
	}

	// Find the hint in the object list
	iter = g_sequence_lookup(hintptr->objptr->hints, hintptr,
			_sorthints, NULL);
	if (!iter) {
		/*
		 * This is a weird error. It should not be possible to have a
		 * hint survive without being attached to an object. Creating
		 * the hint links to the object, or destroys the hint completely
		 * if something goes wrong. Destroying the object destroys all
		 * hints attached to the object. So there should never be
		 * free-floating hints.
		 */
		LOG_FAULT("hint found, but not linked to object");
		_del_hint(hintptr);
		goto done;
	}

	/*
	 * Remove the hint from the object list. This will call the registered
	 * del_hint() function on the hint as it removes it from the list.
	 */
	g_sequence_remove(iter);
	status = PWR_RET_SUCCESS;

done:
	TRACE1_EXIT("status = %d", status);
	return status;
}

/**
 * Activate a hint region.
 *
 * @param hint_region_id - opaque reference to hint region
 *
 * @return int - status code
 */
int PWR_AppHintStart(uint64_t hint_region_id)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t hint_key = OPAQUE_GET_DATA_KEY(hint_region_id);
	hint_t *hintptr = NULL;
	apphint_t apphint;

	TRACE1_ENTER("hint_region_id = %lu", hint_region_id);

	// Find the hint object
	hintptr = opaque_map_lookup_hint(opaque_map, hint_key);
	if (!hintptr) {
		LOG_FAULT("hint not found!");
		goto done;
	}

	// call plugin with hintptr
	memset(&apphint, 0, sizeof(apphint_t));
	apphint.object = hintptr->object;
	apphint.name = hintptr->name;
	apphint.hint = hintptr->hint;
	apphint.level = hintptr->level;
	status = app_hint_start(&apphint);

done:
	TRACE1_EXIT("status = %d", status);
	return status;
}

/**
 * Deactivate a hint region.
 *
 * @param hint_region_id - opaque reference to hint region
 *
 * @return int - status code
 */
int PWR_AppHintStop(uint64_t hint_region_id)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t hint_key = OPAQUE_GET_DATA_KEY(hint_region_id);
	hint_t *hintptr = NULL;
	apphint_t apphint;

	TRACE1_ENTER("hint_region_id = %lu", hint_region_id);

	// Find the hint object
	hintptr = opaque_map_lookup_hint(opaque_map, hint_key);
	if (!hintptr) {
		LOG_FAULT("hint not found!");
		goto done;
	}

	// call plugin with hintptr
	memset(&apphint, 0, sizeof(apphint_t));
	apphint.object = hintptr->object;
	apphint.name = hintptr->name;
	apphint.hint = hintptr->hint;
	apphint.level = hintptr->level;
	status = app_hint_stop(&apphint);

done:
	TRACE1_EXIT("status = %d", status);
	return status;
}

/**
 * Advise the hint plugin of progress within a hint region, for predictive power
 * management changes.
 *
 * @param hint_region_id - opaque reference to hint region
 * @param progress_fraction - progress fraction (0.0 to 1.0)
 *
 * @return int - status code
 */
int PWR_AppHintProgress(uint64_t hint_region_id, double progress_fraction)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t hint_key = OPAQUE_GET_DATA_KEY(hint_region_id);
	hint_t *hintptr = NULL;
	apphint_t apphint;

	TRACE1_ENTER("hint_region_id = %lu, progress_fraction = %lf",
			hint_region_id, progress_fraction);

	// Find the hint object
	hintptr = opaque_map_lookup_hint(opaque_map, hint_key);
	if (!hintptr) {
		LOG_FAULT("hint not found!");
		goto done;
	}

	// call plugin with hintptr, progress_fraction
	memset(&apphint, 0, sizeof(apphint_t));
	apphint.object = hintptr->object;
	apphint.name = hintptr->name;
	apphint.hint = hintptr->hint;
	apphint.level = hintptr->level;
	status = app_hint_progress(&apphint, progress_fraction);

done:
	TRACE1_EXIT("status = %d", status);
	return status;
}

int PWR_GetSleepState(PWR_Obj obj, PWR_SleepState *sstate)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_cstates = -1;
	int64_t *latencies = NULL;
	uint64_t cstate = 0;

	TRACE1_ENTER("obj = %p, sstate = %p", obj, sstate);

	tmpret = init_cstate_limits(obj, &num_cstates, &latencies);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine c-state information!");
		goto error_handling;
	}

	status = PWR_ObjAttrGetValue(obj, PWR_ATTR_CSTATE_LIMIT, &cstate, NULL);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Can't get the c-state limit!  status = %d", status);
		goto error_handling;
	}

	tmpret = map_cs_to_ss(cstate);
	if (tmpret == -1) {
		LOG_FAULT("Error mapping c-state limit(%lu) to SleepState.",
				cstate);
		goto error_handling;
	}

	*sstate = tmpret;
	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int PWR_SetSleepStateLimit(PWR_Obj obj, PWR_SleepState sstate)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_cstates = -1;
	int64_t *latencies = NULL;
	uint64_t cstate = 0;

	TRACE1_ENTER("obj = %p, sstate = %d", obj, sstate);

	tmpret = init_cstate_limits(obj, &num_cstates, &latencies);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine c-state information!");
		goto error_handling;
	}

	cstate = map_ss_to_cs(sstate);
	if (cstate == -1) {
		LOG_FAULT("Error mapping SleepState(%d) to c-state limit.",
				sstate);
		goto error_handling;
	}

	status = PWR_ObjAttrSetValue(obj, PWR_ATTR_CSTATE_LIMIT, &cstate);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Can't set the c-state limit!  status = %d", status);
		goto error_handling;
	}

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int PWR_WakeUpLatency(PWR_Obj obj, PWR_SleepState sstate, PWR_Time *latency)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_cstates = -1;
	int64_t *latencies = NULL;
	uint64_t cstate = 0;

	TRACE1_ENTER("obj = %p, sstate = %d, latency = %p", obj, sstate, latency);

	tmpret = init_cstate_limits(obj, &num_cstates, &latencies);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine c-state information!");
		goto error_handling;
	}

	cstate = map_ss_to_cs(sstate);
	if (cstate == -1) {
		LOG_FAULT("Error mapping SleepState(%d) to c-state limit.",
				sstate);
		goto error_handling;
	}

	*latency = latencies[cstate];
	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("");

	return status;
}

int PWR_RecommendSleepState(PWR_Obj obj, PWR_Time latency,
			    PWR_SleepState *sstate)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_cstates = -1;
	int64_t *latencies = NULL;
	uint64_t cstate = 0;
	int i = 0;

	TRACE1_ENTER("obj = %p, latency = %lu", obj, latency);

	tmpret = init_cstate_limits(obj, &num_cstates, &latencies);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine c-state information!");
		goto error_handling;
	}

	// Find the sleep state that has the largest latency that is less than
	// or equal to the requested latency.
	for (i = PWR_NUM_SLEEP_STATES - 1;i >= PWR_SLEEP_NO;--i) {
		cstate = map_ss_to_cs(i);
		if (cstate == -1) {
			continue;
		}

		if (latencies[cstate] <= latency)
			break;
	}

	*sstate = i;
	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("");

	return status;
}

int PWR_GetPerfState(PWR_Obj obj, PWR_PerfState *pstate)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_freqs = -1;
	int freq_idx = -1;
	double *freqs = NULL;
	double freq = 0.0;
	int i = 0;

	TRACE1_ENTER("obj = %p, pstate = %p", obj, pstate);

	tmpret = init_freqs(obj, &num_freqs, &freqs);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine CPU frequency information!");
		goto error_handling;
	}

	status = PWR_ObjAttrGetValue(obj, PWR_ATTR_FREQ, &freq, NULL);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Can't get the current CPU frequency!  status = %d",
				status);
		goto error_handling;
	}

	for (i = 0;i < num_freqs;++i) {
		if (freq == freqs[i]) {
			freq_idx = i;
			break;
		}
	}

	tmpret = map_freq_to_ps(freq_idx);
	if (tmpret == -1) {
		LOG_FAULT("Error mapping CPU frequency to PerfState.");
		goto error_handling;
	}

	*pstate = tmpret;
	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int PWR_SetPerfState(PWR_Obj obj, PWR_PerfState pstate)
{
	int status = PWR_RET_FAILURE;
	int tmpret = 0;
	int num_freqs = -1;
	int freq_idx = -1;
	double *freqs = NULL;
	double freq = 0.0;
	uint64_t gov = 0;

	TRACE1_ENTER("obj = %p, pstate = %d", obj, pstate);

	tmpret = init_freqs(obj, &num_freqs, &freqs);
	if (tmpret != PWR_RET_SUCCESS) {
		LOG_FAULT("Unable to determine CPU frequency information!");
		goto error_handling;
	}

	freq_idx = map_ps_to_freq(pstate);
	if (freq_idx == -1) {
		LOG_FAULT("Error mapping PerfState(%d) to a CPU frequency.",
				pstate);
		goto error_handling;
	}

	gov = PWR_GOV_LINUX_USERSPACE;
	status = PWR_ObjAttrSetValue(obj, PWR_ATTR_GOV, &gov);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Can't set the CPU frequency governor!  status = %d",
				status);
		goto error_handling;
	}

	freq = freqs[freq_idx];
	status = PWR_ObjAttrSetValue(obj, PWR_ATTR_FREQ_REQ, &freq);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Can't set the current CPU frequency!  status = %d",
				status);
		goto error_handling;
	}

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}
