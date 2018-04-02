/*
 * Copyright (c) 2015-2017, Cray Inc.
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
 * This file contains the functions necessary for the statistics interface.
 */

#include <stdio.h>
#include <math.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "statistics.h"
#include "context.h"
#include "timer.h"
#include "utility.h"

stat_t *
new_stat(void)
{
	stat_t *stat = NULL;
	int error_state = 0;

	TRACE2_ENTER("");

	// A new statistics structure would be nice
	stat = g_new0(stat_t, 1);
	if (!stat) {
		++error_state;
		goto error_handling;
	}
	g_mutex_init(&stat->val_lock);

	// Since statistics get returned to users, they need to
	// go into the opaque map so they have an opaque key.
	if (!opaque_map_insert(opaque_map, OPAQUE_STAT, &stat->opaque)) {
		++error_state;
		goto error_handling;
	}

error_handling:
	// If any errors were encountered, clean up
	if (error_state) {
		del_stat(stat);
		stat = NULL;
	}

	TRACE2_EXIT("statistic = %p", stat);
	return stat;
}

int stop_thread(stat_t *);

void
del_stat(stat_t *stat)
{
	TRACE2_ENTER("stat = %p", stat);

	if (stat) {
		if (stat->thread) {
			stop_thread(stat);
		}
		if (stat->opaque.key) {
			opaque_map_remove(opaque_map, stat->opaque.key);
		}
		g_free(stat->values);
		g_free(stat->instants);
		g_free(stat);
	}

	TRACE2_EXIT("");
}

void
stat_destroy_callback(gpointer data)
{
	stat_t *stat = (stat_t *)data;

	TRACE3_ENTER("data = %p", data);

	del_stat(stat);

	TRACE3_EXIT("");
}

void
stat_invalidate_callback(gpointer data)
{
	stat_t *stat = (stat_t *)data;

	TRACE3_ENTER("data = %p", data);

	if (stat) {
		if (stat->thread) {
			stop_thread(stat);
		}

		stat->obj = stat->grp = NULL;
	}

	TRACE3_EXIT("");
}

//
// This function will run in its own glib thread to continuously calculate
// the requested statistic until the thread is killed.
//
gpointer
calculate_stat(gpointer statptr)
{
	stat_t *stat = (stat_t *)statptr;
	PWR_Time window = NSEC_PER_SEC / stat->sample_rate;
	double *reading = NULL;
	double *roll_mean = NULL;
	double *roll_sum = NULL;
	double avgcnt = 0;
	double prev_mean = 0;
	PWR_Time *readtime = NULL;
	int i = 0;

	TRACE2_ENTER("statptr = %p", statptr);

	// For MIN, MAX and AVG, we only need to keep track of the min or max
	// reading seen, or the cumulative sum of the readings and the number
	// of readings.
	reading    = g_new0(double, stat->objcount);
	readtime   = g_new0(PWR_Time, stat->objcount);

	// For STDEV, we need to keep track of two values (Welford method, 1962)
	if (stat->stat == PWR_ATTR_STAT_STDEV) {
		roll_mean = g_new0(double, stat->objcount);
		roll_sum = g_new0(double, stat->objcount);
	}

	// Statistics validation and initialization
	for (i = 0; i < stat->objcount; ++i) {
		// Set initial value of the statistic to INFINITY for MIN, and
		// -INFINITY for MAX, and 0 for AVG.
		switch (stat->stat) {
		case PWR_ATTR_STAT_MIN:
			stat->values[i] = INFINITY;
			break;
		case PWR_ATTR_STAT_MAX:
			stat->values[i] = -INFINITY;
			break;
		case PWR_ATTR_STAT_AVG:
			stat->values[i] = 0;
			break;
		case PWR_ATTR_STAT_STDEV:
			stat->values[i] = 0;
			roll_mean[i] = 0;
			roll_sum[i] = 0;
			break;
		default:
			// should never get here..
			LOG_FAULT("Unsupported PWR_AttrStat = %d", stat->stat);
			goto done;
			break;	// keep linters happy
		}
	}

	while (stat->die == false) {
		int retval = 0;

		if (stat->obj) {
			retval = PWR_ObjAttrGetValue(stat->obj, stat->attr,
					reading, readtime);
		} else {
			retval = PWR_GrpAttrGetValue(stat->grp, stat->attr,
					reading, readtime, NULL);
		}
		if (retval != PWR_RET_SUCCESS) {
			// Exit thread?
			LOG_FAULT("Can't get value! %d", retval);
			pwr_nanosleep(window);
			continue;
		}
		avgcnt += 1;

		g_mutex_lock(&stat->val_lock);
		for (i = 0; i < stat->objcount; ++i) {
			switch (stat->stat) {
			case PWR_ATTR_STAT_MIN:
				if (reading[i] < stat->values[i]) {
					stat->values[i] = reading[i];
					stat->instants[i] = readtime[i];
				}
				break;
			case PWR_ATTR_STAT_MAX:
				if (reading[i] > stat->values[i]) {
					stat->values[i] = reading[i];
					stat->instants[i] = readtime[i];
				}
				break;
			case PWR_ATTR_STAT_AVG:
				stat->values[i] = stat->values[i] +
					(reading[i] - stat->values[i])/avgcnt;
				break;
			case PWR_ATTR_STAT_STDEV:
				prev_mean = roll_mean[i];
				roll_mean[i] = roll_mean[i] +
					(reading[i] - roll_mean[i])/avgcnt;
				roll_sum[i] = roll_sum[i] +
					(reading[i] - roll_mean[i]) *
					(reading[i] - prev_mean);
				stat->values[i] =
					sqrt(roll_sum[i]/(avgcnt - 1));
				break;
			default:
				// should never get here..
				LOG_FAULT("Unsupported PWR_AttrStat = %d",
					  stat->stat);
				goto done;
				break;	// keep linters happy
			}
		}
		g_mutex_unlock(&stat->val_lock);

		pwr_nanosleep(window);
	}

done:
	g_free(reading);
	g_free(readtime);
	if (roll_mean != NULL) {
		g_free(roll_mean);
	}
	if (roll_sum != NULL) {
		g_free(roll_sum);
	}
	TRACE2_EXIT("");

	return statptr;
}

int
stop_thread(stat_t *stat)
{
	int status = PWR_RET_SUCCESS;

	TRACE2_ENTER("stat = %p", stat);

	if (stat->thread) {
		stat->die = true;
		g_thread_join(stat->thread);
		stat->thread = NULL;
		stat->die = false;
	}

	TRACE2_EXIT("");

	return status;
}

int
start_thread(stat_t *stat)
{
	int status = PWR_RET_FAILURE;

	TRACE2_ENTER("stat = %p", stat);

	if (stat->thread) {
		if (stop_thread(stat) != PWR_RET_SUCCESS) {
			goto error_handling;
		}
	}

	stat->thread = g_thread_try_new(NULL, calculate_stat, stat, NULL);
	if (!stat->thread) {
		LOG_FAULT("unable to start statistics monitoring thread!");
		goto error_handling;
	}

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
validate_attribute(PWR_AttrName name)
{
	int status = PWR_RET_SUCCESS;

	switch (name) {
	case PWR_ATTR_POWER:
	case PWR_ATTR_ENERGY:
	case PWR_ATTR_TEMP:
		break;
	case PWR_NUM_ATTR_NAMES:
	case PWR_ATTR_INVALID:
	case PWR_ATTR_NOT_SPECIFIED:
		LOG_FAULT("invalid attribute (%d)!", name);
		status = PWR_RET_FAILURE;
		break;
	default:
		status = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	return status;
}

static int
validate_statistic(PWR_AttrStat statistic)
{
	int status = PWR_RET_SUCCESS;

	switch (statistic) {
	case PWR_ATTR_STAT_MIN:
	case PWR_ATTR_STAT_MAX:
	case PWR_ATTR_STAT_AVG:
	case PWR_ATTR_STAT_STDEV:
		break;
	case PWR_NUM_ATTR_STATS:
	case PWR_ATTR_STAT_INVALID:
	case PWR_ATTR_STAT_NOT_SPECIFIED:
		LOG_FAULT("invalid statistic (%d)!", statistic);
		status = PWR_RET_FAILURE;
		break;
	default:
		status = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	return status;
}

//----------------------------------------------------------------------//
//              External Statistics Interfaces                          //
//----------------------------------------------------------------------//

int
PWR_ObjCreateStat(PWR_Obj object, PWR_AttrName name, PWR_AttrStat statistic,
		PWR_Stat *statObj)
{
	int status = PWR_RET_FAILURE;
	int tmp_status = 0;
	context_t *ctx = NULL;
	obj_t *obj = NULL;
	stat_t *stat = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(object);
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, name = %d, statistic = %d, statObj = %p",
			object, name, statistic, statObj);

	if (!statObj) {
		LOG_FAULT("NULL statistics pointer");
		goto error_handling;
	}

	tmp_status = validate_attribute(name);
	if (tmp_status) {
		LOG_FAULT("Invalid attribute (%d).", name);
		status = tmp_status;
		goto error_handling;
	}

	tmp_status = validate_statistic(statistic);
	if (tmp_status) {
		LOG_FAULT("Invalid statistic requested (%d).", statistic);
		status = tmp_status;
		goto error_handling;
	}

	// Find the object
	obj = opaque_map_lookup_object(opaque_map, obj_key);
	if (!obj) {
		LOG_FAULT("object not found!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Check if object/attribute can be monitored
	{
		double reading = 0.0;
		PWR_Time readtime = 0;
		int retval = 0;

		retval = PWR_ObjAttrGetValue(object, name,
				&reading, &readtime);
		if (retval != PWR_RET_SUCCESS) {
			LOG_FAULT("Unable to monitor object attribute, "
					"statistic not created!");
			status = retval;

			goto error_handling;
		}
	}

	// Have context create the statistic
	stat = context_new_statistic(ctx);
	if (!stat) {
		LOG_FAULT("unable to create new statistic!");
		goto error_handling;
	}

	stat->obj	= object;
	stat->attr	= name;
	stat->stat	= statistic;
	stat->objcount	= 1;
	stat->values	= g_new0(double, 1);
	stat->instants	= g_new0(PWR_Time, 1);
	if (!stat->values || !stat->instants) {
		LOG_FAULT("unable to allocate space for statistics!");
		goto error_handling;
	}

	tmp_status = PWR_ObjAttrGetMeta(object, name, PWR_MD_UPDATE_RATE,
			&stat->sample_rate);
	if (tmp_status == PWR_RET_NO_META) {
		stat->sample_rate = 10; // 10Hz default
	} else if (tmp_status != PWR_RET_SUCCESS || stat->sample_rate <= 0) {
		LOG_FAULT("unable to get update_rate for statistic!");
		goto error_handling;
	}

	// Provide opaque key to the caller
	*statObj = OPAQUE_GENERATE(ctx->opaque.key, stat->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if (status != PWR_RET_SUCCESS) {
		if (stat) {
			context_del_statistic(ctx, stat);
		}
	}

	TRACE1_EXIT("status = %d, *statObj = %p", status, *statObj);

	return status;
}

int
PWR_GrpCreateStat(PWR_Grp group, PWR_AttrName name, PWR_AttrStat statistic,
		PWR_Stat *statObj)
{
	int status = PWR_RET_FAILURE;
	int tmp_status = 0;
	context_t *ctx = NULL;
	group_t *grp = NULL;
	stat_t *stat = NULL;
	int grplen = 0;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(group);
	opaque_key_t grp_key = OPAQUE_GET_DATA_KEY(group);

	TRACE1_ENTER("group = %p, name = %d, statistic = %d, statObj = %p",
			group, name, statistic, statObj);

	if (!statObj) {
		LOG_FAULT("NULL statistics pointer");
		goto error_handling;
	}

	tmp_status = validate_attribute(name);
	if (tmp_status) {
		LOG_FAULT("Invalid attribute (%d).", name);
		status = tmp_status;
		goto error_handling;
	}

	tmp_status = validate_statistic(statistic);
	if (tmp_status) {
		LOG_FAULT("Invalid statistic requested (%d).", statistic);
		status = tmp_status;
		goto error_handling;
	}

	// Find the group
	grp = opaque_map_lookup_group(opaque_map, grp_key);
	if (!grp) {
		LOG_FAULT("group not found!");
		goto error_handling;
	}

	grplen = PWR_GrpGetNumObjs(group);
	if (grplen <= 0) {
		LOG_FAULT("invalid group!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Check if group/attribute can be monitored
	{
		double *reading = NULL;
		PWR_Time *readtime = NULL;
		int retval = 0;

		reading = g_new0(double, grplen);
		readtime = g_new0(PWR_Time, grplen);
		retval = PWR_GrpAttrGetValue(group, name,
				reading, readtime, NULL);
		g_free(reading);
		g_free(readtime);
		if (retval != PWR_RET_SUCCESS) {
			LOG_FAULT("Unable to monitor group attribute, "
					"statistic not created!");
			status = retval;

			goto error_handling;
		}
	}

	// Have group create the statistic so it can track the
	// statistics that are monitoring it.
	stat = group_new_statistic(grp);
	if (!stat) {
		LOG_FAULT("unable to create new statistic!");
		goto error_handling;
	}

	stat->grp	= group;
	stat->attr	= name;
	stat->stat	= statistic;
	stat->objcount	= grplen;
	stat->values	= g_new0(double, grplen);
	stat->instants	= g_new0(PWR_Time, grplen);
	if (!stat->values || !stat->instants) {
		LOG_FAULT("unable to allocate space for statistics!");
		goto error_handling;
	}

	PWR_Obj object;
	if (PWR_GrpGetObjByIndx(group, 0, &object) != PWR_RET_SUCCESS) {
		LOG_FAULT("unable to access first object in group!");
		goto error_handling;
	}

	tmp_status = PWR_ObjAttrGetMeta(object, name, PWR_MD_UPDATE_RATE,
			&stat->sample_rate);
	if (tmp_status == PWR_RET_NO_META) {
		stat->sample_rate = 10; // 10Hz default
	} else if (tmp_status != PWR_RET_SUCCESS || stat->sample_rate <= 0) {
		LOG_FAULT("unable to get update_rate for statistic!");
		goto error_handling;
	}

	// Provide opaque key to the caller
	*statObj = OPAQUE_GENERATE(ctx->opaque.key, stat->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if (status != PWR_RET_SUCCESS) {
		if (stat) {
			group_del_statistic(grp, stat);
		}
	}

	TRACE1_EXIT("status = %d, *statObj = %p", status, *statObj);

	return status;
}

int
PWR_StatDestroy(PWR_Stat statObj)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	context_t *ctx = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(statObj);

	TRACE1_ENTER("statObj = %p", statObj);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	if (stat->grp) {
		opaque_key_t grp_key = OPAQUE_GET_DATA_KEY(stat->grp);
		group_t *grp = NULL;

		// Find the parent group
		grp = opaque_map_lookup_group(opaque_map, grp_key);
		if (!grp) {
			LOG_FAULT("group not found!");
			goto error_handling;
		}
		group_del_statistic(grp, stat);
	} else {
		// It's an object stat or it's been invalidated
		context_del_statistic(ctx, stat);
	}

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d", status);

	return status;
}

static PWR_Time
get_current_time(void)
{
	struct timespec ts = { };

	if (clock_gettime(CLOCK_REALTIME, &ts))
		return 0;
	else
		return pwr_tspec_to_nsec(&ts);
}

static bool
is_invalid(stat_t *stat)
{
	return (!stat->obj && !stat->grp);
}

int
PWR_StatStart(PWR_Stat statObj)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);

	TRACE1_ENTER("statObj = %p", statObj);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	stat->start = get_current_time();
	if (!stat->start) {
		goto error_handling;
	}
	stat->stop = 0;
	start_thread(stat);

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_StatStop(PWR_Stat statObj)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);

	TRACE1_ENTER("statObj = %p", statObj);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	stat->stop = get_current_time();
	if (!stat->stop) {
		goto error_handling;
	}

	stop_thread(stat);

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_StatClear(PWR_Stat statObj)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);

	TRACE1_ENTER("statObj = %p", statObj);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	if (stat->thread)
		stop_thread(stat);
	stat->start = get_current_time();
	if (!stat->start) {
		goto error_handling;
	}
	stat->stop = 0;
	start_thread(stat);

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_StatGetValue(PWR_Stat statObj, double *value, PWR_TimePeriod *statTimes)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);

	TRACE1_ENTER("statObj = %p, value = %p, statTimes = %p",
			statObj, value, statTimes);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		// Shouldn't ever get here since be don't remove hierarchy
		// objects, and this function should be called with single
		// object statistics.
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	g_mutex_lock(&stat->val_lock);
	*value = stat->values[0];
	statTimes->instant = stat->instants[0];
	g_mutex_unlock(&stat->val_lock);
	statTimes->start = stat->start;
	statTimes->stop  = stat->stop;
	if (stat->stop == 0)
		statTimes->stop  = get_current_time();

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_StatGetValues(PWR_Stat statObj, double values[], PWR_TimePeriod statTimes[])
{
	int i = 0;
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);

	TRACE1_ENTER("statObj = %p, values = %p, statTimes = %p",
			statObj, values, statTimes);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	g_mutex_lock(&stat->val_lock);
	for (i = 0; i < stat->objcount; ++i) {
		values[i] = stat->values[i];
		statTimes[i].instant = stat->instants[i];
	}
	g_mutex_unlock(&stat->val_lock);

	{
		PWR_Time tmpstop = stat->stop;

		if (tmpstop == 0) {
			tmpstop = get_current_time();
		}
		for (i = 0; i < stat->objcount; ++i) {
			statTimes[i].start = stat->start;
			statTimes[i].stop  = tmpstop;
		}
	}

	status = PWR_RET_SUCCESS;

error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_StatGetReduce(PWR_Stat statObj, PWR_AttrStat reduceOp,
		int *index, double *result, PWR_Time *instant)
{
	int status = PWR_RET_FAILURE;
	stat_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(statObj);
	int i = 0;
	double avgcnt = 0;
	double roll_mean = 0;
	double roll_sum = 0;
	double prev_mean = 0;

	TRACE1_ENTER("statObj = %p, reduceOp = %d, index = %p, result = %p, "
			"instant = %p",
			statObj, reduceOp, index, result, instant);

	// Find the statistic
	stat = opaque_map_lookup_stat(opaque_map, stat_key);
	if (!stat) {
		LOG_FAULT("statistic not found!");
		goto error_handling;
	}

	if (is_invalid(stat)) {
		LOG_FAULT("statistic no longer has a group to monitor!");
		status = PWR_RET_INVALID;

		goto error_handling;
	}

	switch (reduceOp) {
	case PWR_ATTR_STAT_MIN:
		*result = INFINITY;
		break;
	case PWR_ATTR_STAT_MAX:
		*result = -INFINITY;
		break;
	case PWR_ATTR_STAT_AVG:
		*result = 0;
		break;
	case PWR_ATTR_STAT_STDEV:
		roll_mean = 0;
		roll_sum = 0;
		*result = 0;
		break;
	default:
		LOG_FAULT("Invalid reduce operation.");
		goto error_handling;
	}

	g_mutex_lock(&stat->val_lock);
	for (i = 0; i < stat->objcount; ++i) {
		switch (reduceOp) {
		case PWR_ATTR_STAT_MIN:
			if (stat->values[i] < *result) {
				*result = stat->values[i];
				*index  = i;
				*instant = stat->instants[i];
			}
			break;
		case PWR_ATTR_STAT_MAX:
			if (stat->values[i] > *result) {
				*result = stat->values[i];
				*index  = i;
				*instant = stat->instants[i];
			}
			break;
		case PWR_ATTR_STAT_AVG:
			avgcnt = (double)i+1;
			*result = *result + (stat->values[i] - *result)/avgcnt;
			break;
		case PWR_ATTR_STAT_STDEV:
			avgcnt = (double)i+1;
			prev_mean = roll_mean;
			roll_mean = roll_mean +
				(stat->values[i] - roll_mean)/avgcnt;
			roll_sum = roll_sum + (stat->values[i] - roll_mean) *
				(stat->values[i] - prev_mean);
		default:
			break;
		}
	}
	// Compute final standard deviation
	if (stat->stat == PWR_ATTR_STAT_STDEV) {
		*result = sqrt(roll_sum/(avgcnt - 1));
	}
	g_mutex_unlock(&stat->val_lock);

	status = PWR_RET_SUCCESS;
error_handling:

	TRACE1_EXIT("status = %d", status);

	return status;
}

//
// The following three functions are not currently implemented.  They are
// used for historic statistics which we do not currently support.
//
int
PWR_ObjGetStat(PWR_Obj object, PWR_AttrName name, PWR_AttrStat statistic,
		PWR_TimePeriod *statTime, double *value)
{
	TRACE1_ENTER("object = %p, name = %d, statistic = %d, statTime = %p, "
			"value = %p",
			object, name, statistic, statTime, value);

	TRACE1_EXIT("");

	return PWR_RET_NOT_IMPLEMENTED;
}

int
PWR_GrpGetStats(PWR_Grp group, PWR_AttrName name, PWR_AttrStat statistic,
		PWR_TimePeriod *statTime, double values[],
		PWR_TimePeriod statTimes[])
{
	TRACE1_ENTER("group = %p, name = %d, statistic = %d, statTime = %p, "
			"values = %p, statTimes = %p",
			group, name, statistic, statTime, values, statTimes);

	TRACE1_EXIT("");

	return PWR_RET_NOT_IMPLEMENTED;
}

int
PWR_GrpGetReduce(PWR_Grp group, PWR_AttrName name, PWR_AttrStat statistic,
		PWR_AttrStat reduceOp, PWR_TimePeriod statTime,
		int *index, double *result, PWR_TimePeriod *resultTime)
{
	TRACE1_ENTER("group = %p, name = %d, statistic = %d, reduceOp = %d, "
			"statTime = %p, index = %p, result = %p, "
			"resultTime = %p",
			group, name, statistic, reduceOp,
			&statTime, index, result, resultTime);

	TRACE1_EXIT("");

	return PWR_RET_NOT_IMPLEMENTED;
}

