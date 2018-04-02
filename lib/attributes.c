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
 * This file contains functions for accessing attributes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <cray-powerapi/types.h>
#include <cray-powerapi/api.h>
#include <log.h>

#include "attributes.h"
#include "hierarchy.h"
#include "context.h"
#include "timer.h"
#include "utility.h"

/**
 * Create a status_t object.
 *
 * @return status_t* - objected created
 */
status_t *
new_status(void)
{
	status_t *stat = NULL;
	int errcnt = 0;
	TRACE2_ENTER("");

	// Allocate the status object
	stat = g_new0(status_t, 1);
	if (! stat) {
		errcnt++;
		goto error_handling;
	}

	// And a list to hold the PWR_AttrAccessError objects in the status
	stat->list = g_queue_new();
	if (!stat->list) {
		errcnt++;
		goto error_handling;
	}

	// Since status gets returned to library users, it needs to
	// go into the opaque_map so it has an opaque key.
	if (!opaque_map_insert(opaque_map, OPAQUE_STATUS, &stat->opaque)) {
		errcnt++;
		goto error_handling;
	}

error_handling:
	if (errcnt) {
		del_status(stat);
		stat = NULL;
	}
	TRACE2_EXIT("stat = %p", stat);
	return stat;
}

/**
 * Delete a status_t object. This accepts a NULL pointer.
 *
 * @param stat - object to delete
 */
void
del_status(status_t *stat)
{
	TRACE2_ENTER("stat = %p", stat);
	if (stat) {
		if (stat->opaque.key) {
			opaque_map_remove(opaque_map, stat->opaque.key);
		}
		if (stat->list) {
			g_queue_free_full(stat->list, free);
		}
		g_free(stat);
	}
	TRACE2_EXIT("");
}

/**
 * Callback alias for del_status, used when destroying the containing context.
 *
 * @param data - status object to destroy
 */
void
status_destroy_callback(gpointer data)
{
	status_t *status = (status_t *)data;

	TRACE3_ENTER("data = %p", data);

	del_status(status);

	TRACE3_EXIT("");
}

/**
 * Look up a status_t object in the hash table, using an opaque key.
 *
 * This accepts 0 as an 'invalid' key, and returns a NULL pointer.
 *
 * @param status - opaque key to look up, or 0
 *
 * @return status_t* - found object, or NULL
 */
static status_t *
find_status_by_opaque(PWR_Status status)
{
	status_t *stat = NULL;
	opaque_key_t stat_key = OPAQUE_GET_DATA_KEY(status);

	TRACE2_ENTER("status = %p, stat_key = %p",
		    status, stat_key);

	// Find the status
	stat = opaque_map_lookup_status(opaque_map, stat_key);

	TRACE2_EXIT("stat = %p", stat);

	return stat;
}

/**
 * Check the status context against the context of the object it is being used
 * with. If they match, this returns true, else it returns false.
 *
 * @param status - opaque key for status
 * @param obj_or_grp - opaque key for an object or group
 *
 * @return bool - true if contexts match, else false
 */
static bool
check_status_context(PWR_Status status, void *obj_or_grp)
{
	bool retval = false;
	opaque_key_t stat_ctx_key = OPAQUE_GET_CONTEXT_KEY(status);
	opaque_key_t obj_ctx_key = OPAQUE_GET_CONTEXT_KEY(obj_or_grp);

	TRACE2_ENTER("status = %p, obj_or_grp = %p",
		    status, obj_or_grp);

	retval = (stat_ctx_key == obj_ctx_key);

	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Push an error onto the status object. This accepts a NULL status_t pointer as
 * a no-op.
 *
 * @param stat - object to push onto, or NULL for no-op
 * @param obj - object associated with error
 * @param name - attribute name associated with error
 * @param index - return array index associated with error
 * @param errcode - error code
 *
 * @return bool - true on success, false on failure
 */
static bool
push_status_error(status_t *stat, PWR_Obj obj, PWR_AttrName name, int index, int errcode)
{
	PWR_AttrAccessError *errdup = NULL;
	bool retval = false;

	TRACE2_ENTER("stat = %p, obj = %p, name = %d, index = %d, errcode = %d",
		    stat, obj, name, index, errcode);

	// A NULL status_t is valid
	if (!stat || !stat->list) {
		retval = true;
		goto error_handling;
	}
	// We need to push persistent memory
	errdup = malloc(sizeof(PWR_AttrAccessError));
	if (!errdup) {
		goto error_handling;
	}
	errdup->obj = obj;
	errdup->name = name;
	errdup->index = index;
	errdup->error = errcode;
	g_queue_push_tail(stat->list, errdup);
	retval = true;

error_handling:
	if (!retval) {
		free(errdup);
	}
	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Clear the status_t object of any prior errors. This accepts a NULL pointer.
 *
 * @param stat
 */
static void
clear_status(status_t *stat)
{
	TRACE2_ENTER("stat = %p", stat);
	if (stat && stat->list) {
		PWR_AttrAccessError *errdup;
		while ((errdup = g_queue_pop_head(stat->list))) {
			free(errdup);
		}
	}
	TRACE2_EXIT("");
}

/*
 * PWR_ObjAttrGetValue - Get the value of a single specified attribute from
 *			 a single specified object. The time-stamp returned
 *			 should accurately represent when the value was
 *			 measured.
 *
 * Argument(s):
 *
 *	obj   - The target object
 *	attr  - The target attribute
 *	value - Pointer to caller-allocated storage, of 8 bytes, to hold the
 *		value read from the attribute
 *	ts    - Pointer to caller-allocated storage to hold the timestamp of
 *		when the value was read from the attribute.  Pass in NULL if
 *		the timestamp is not needed.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS 	- Upon SUCCESS
 *	PWR_RET_FAILURE 	- Upon FAILURE
 *	PWR_RET_NOT_IMPLEMENTED - The requested attribute is not supported
 *				  for the target object.
 */
int
PWR_ObjAttrGetValue(PWR_Obj object, PWR_AttrName attr, void *value,
		    PWR_Time *ts)
{
	obj_t *obj = NULL;
	struct timespec tspec;
	int retval = PWR_RET_FAILURE;
	opaque_key_t data_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, attr = %d, value = %p, ts = %p",
			object, attr, value, ts);

	// Validate that the opaque key references a hierarchy object
	obj = opaque_map_lookup_object(opaque_map, data_key);
	if (!obj) {
		LOG_FAULT("Invalid PWR_Obj reference %p", object);
		goto error_handling;
	}

	// All cases set retval to final value
	switch (obj->type) {
	case PWR_OBJ_NODE:
		retval = node_attr_get_value(to_node(obj), attr, value,
				&tspec);
		break;
	case PWR_OBJ_SOCKET:
		retval = socket_attr_get_value(to_socket(obj), attr, value,
				&tspec);
		break;
	case PWR_OBJ_CORE:
		retval = core_attr_get_value(to_core(obj), attr, value,
				&tspec);
		break;
	case PWR_OBJ_POWER_PLANE:
		retval = pplane_attr_get_value(to_pplane(obj), attr, value,
				&tspec);
		break;
	case PWR_OBJ_MEM:
		retval = mem_attr_get_value(to_mem(obj), attr, value,
				&tspec);
		break;
	case PWR_OBJ_HT:
		retval = ht_attr_get_value(to_ht(obj), attr, value,
				&tspec);
		break;
	default:
		LOG_FAULT("Invalid PWR_Obj type %u", obj->type);
		retval = PWR_RET_FAILURE;
		break;
	}

	// Return if not successful.
	if (retval != PWR_RET_SUCCESS) {
		goto error_handling;
	}

	// Convert timestamp from timespec to PWR_Time.
	if (ts != NULL) {
		*ts = pwr_tspec_to_nsec(&tspec);
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/*
 * PWR_ObjAttrSetValue - Set the value of a single specified attribute of a
 *			 single specified object.
 *
 * Argument(s):
 *
 *	obj   - The target object
 *	attr  - The target attribute
 *	value - Pointer to the 8 byte value to write to the attribute
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS 	- Upon SUCCESS
 *	PWR_RET_FAILURE 	- Upon FAILURE
 *	PWR_RET_NOT_IMPLEMENTED - The requested attribute is not supported
 *				  for the target object
 *	PWR_RET_BAD_VALUE	- The value was not appropriate for the
 *				  target attribute
 *	PWR_RET_OUT_OF_RANGE	- The value was out of range for the target
 *				  attribute
 */
int
PWR_ObjAttrSetValue(PWR_Obj object, PWR_AttrName attr, const void *value)
{
	context_t *context = NULL;
	obj_t *obj = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(object);
	opaque_key_t data_key = OPAQUE_GET_DATA_KEY(object);
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("object = %p, attr = %d, value = %p",
			object, attr, value);

	// Validate that the opaque key references a context object
	context = opaque_map_lookup_context(opaque_map, context_key);
	if (!context) {
		LOG_FAULT("Invalid PWR_Obj context reference %p", context_key);
		goto error_handling;
	}

	// Validate that the opaque key references a hierarchy object
	obj = opaque_map_lookup_object(opaque_map, data_key);
	if (!obj) {
		LOG_FAULT("Invalid PWR_Obj data reference %p", data_key);
		goto error_handling;
	}

	// All cases set retval to final value
	switch (obj->type) {
	case PWR_OBJ_NODE:
		retval = node_attr_set_value(to_node(obj), context->ipc,
				attr, value);
		break;
	case PWR_OBJ_SOCKET:
		retval = socket_attr_set_value(to_socket(obj), context->ipc,
				attr, value);
		break;
	case PWR_OBJ_CORE:
		retval = core_attr_set_value(to_core(obj), context->ipc,
				attr, value);
		break;
	case PWR_OBJ_POWER_PLANE:
		retval = pplane_attr_set_value(to_pplane(obj), context->ipc,
				attr, value);
		break;
	case PWR_OBJ_MEM:
		retval = mem_attr_set_value(to_mem(obj), context->ipc,
				attr, value);
		break;
	case PWR_OBJ_HT:
		retval = ht_attr_set_value(to_ht(obj), context->ipc,
				attr, value);
		break;
	default:
		LOG_FAULT("Invalid PWR_Obj type %u", obj->type);
		retval = PWR_RET_FAILURE;
		break;
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this gets a collection of attributes for a single object,
 * and returns the attribute values through an array, and the timestamps through
 * a different array.
 *
 * If any errors occur, this returns failure. However, it will still return the
 * entire array of values, and timestamps, and successful attempts will be valid
 * in this array. Specific errors are reported through the status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * caller does not care to determine which values are valid and which are not.
 *
 * 'values' must point to memory of at least (count*8) bytes. Each returned
 * value will appear at address (values + 8*index).
 *
 * @param object - object for which attributes are desired
 * @param count - count of attributes (>= 0)
 * @param attrs - pointer to a list of count attributes
 * @param values - array of return values
 * @param ts - array of return timestamps
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 */
int
PWR_ObjAttrGetValues(PWR_Obj object, int count, const PWR_AttrName attrs[],
		     void *values, PWR_Time ts[], PWR_Status status)
{
	status_t *stat = NULL;
	int i;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("object = %p, count = %d, attrs = %p, values = %p, "
			"ts = %p, status = %p",
			object, count, attrs, values, ts, status);

	// Sanity checks
	if (count < 0) {
		LOG_FAULT("count == %d, < 0", count);
		goto error_handling;
	}
	if (attrs == NULL) {
		LOG_FAULT("NULL attrs pointer");
		goto error_handling;
	}
	if (values == NULL) {
		LOG_FAULT("NULL values pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, object)) {
		LOG_FAULT("Status ctx != Object ctx");
		goto error_handling;
	}
	clear_status(stat);

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	for (i = 0; i < count; i++) {
		PWR_Time *tsp = (ts) ? &ts[i] : NULL;
		int errcode;

		// Get the attribute value and timestamp
		errcode = PWR_ObjAttrGetValue(object, attrs[i], values+8*i, tsp);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, object, attrs[i], i, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this sets a collection of attributes for a single object.
 *
 * If any errors occur, this returns failure. However, it will still attempt to
 * set the entire array of values. Specific errors are reported through the
 * status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * status information is of no interest to the caller.
 *
 * Note that groups are implemented in a balanced binary tree with an
 * unspecified sort criterion.
 *
 * @param object - object for which attributes are desired
 * @param count - count of attributes (>= 0)
 * @param attrs - pointer to a list of count attributes
 * @param values - array of values to set
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 */
int
PWR_ObjAttrSetValues(PWR_Obj object, int count, const PWR_AttrName attrs[],
		     const void *values, PWR_Status status)
{
	status_t *stat = NULL;
	int i;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("object = %p, count = %d, attrs = %p, values = %p, "
			"status = %p",
			object, count, attrs, values, status);

	// Sanity checks
	if (count < 0) {
		LOG_FAULT("count == %d, < 0", count);
		goto error_handling;
	}
	if (attrs == NULL) {
		LOG_FAULT("NULL attrs pointer");
		goto error_handling;
	}
	if (values == NULL) {
		LOG_FAULT("NULL values pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, object)) {
		LOG_FAULT("Status ctx != Object ctx");
		goto error_handling;
	}
	clear_status(stat);

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	for (i = 0; i < count; i++) {
		int errcode;

		// Set the attribute
		errcode = PWR_ObjAttrSetValue(object, attrs[i], values+8*i);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, object, attrs[i], 0, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this gets a specific attribute for all objects in a
 * specified group, and returns the attribute value through an array, and the
 * timestamps through a different array.
 *
 * If any errors occur, this returns failure. However, it will still return the
 * entire array of values, and timestamps, and successful attempts will be valid
 * in this array. Specific errors are reported through the status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * caller does not care to determine which values are valid and which are not.
 *
 * 'values' must point to memory of at least (group_size*8) bytes. Each returned
 * value will appear at address (values + 8*index).
 *
 * Note that groups are implemented in a balanced binary tree with an
 * unspecified sort criterion. To determine which return value belongs to which
 * object in the group, the caller must call PWR_GrpGetObjByIndx() for the index
 * of each return value.
 *
 * @param group - group object
 * @param attr - attribute to return
 * @param values - array of return values
 * @param ts - array of return timestamps
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 *   PWR_RET_INVALID = bad group, all attributes failed, status is empty
 */
int
PWR_GrpAttrGetValue(PWR_Grp group, PWR_AttrName attr, void *values,
		    PWR_Time ts[], PWR_Status status)
{
	status_t *stat = NULL;
	int num_objs;
	int i;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("group = %p, attr = %d, values = %p, ts = %p, status = %p",
			group, attr, values, ts, status);

	// Sanity checks
	if (values == NULL) {
		LOG_FAULT("NULL values pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, group)) {
		LOG_FAULT("Status ctx != Group ctx");
		goto error_handling;
	}
	clear_status(stat);

	// We will iterate over elements in the group
	// An empty group is valid
	num_objs = PWR_GrpGetNumObjs(group);
	if (num_objs < 0) {
		LOG_FAULT("group object count < 0");
		retval = PWR_RET_INVALID;
		goto error_handling;
	}

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	for (i = 0; i < num_objs; i++) {
		PWR_Time *tsp = (ts) ? &ts[i] : NULL;
		PWR_Obj obj;
		int errcode;

		// Find the group object
		errcode = PWR_GrpGetObjByIndx(group, i, &obj);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, obj, attr, i, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}

		// Get the attribute value and timestamp
		errcode = PWR_ObjAttrGetValue(obj, attr, values+8*i, tsp);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, obj, attr, i, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this sets a specific attribute for all objects in a
 * specified group. The attribute is set to the same value for all objects.
 *
 * If any errors occur, this returns failure. Specific errors are reported
 * through the status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * status information is of no interest to the caller.
 *
 * Note that groups are implemented in a balanced binary tree with an
 * unspecified sort criterion.
 *
 * @param group - group object
 * @param attr - attribute to set
 * @param value - value to set
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 *   PWR_RET_INVALID = bad group, all attributes failed, status is empty
 */
int
PWR_GrpAttrSetValue(PWR_Grp group, PWR_AttrName attr, const void *value,
		    PWR_Status status)
{
	status_t *stat = NULL;
	int num_objs;
	int i;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("group = %p, attr = %d, value = %p, status = %p",
			group, attr, value, status);

	// Sanity checks
	if (value == NULL) {
		LOG_FAULT("NULL value pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, group)) {
		LOG_FAULT("Status ctx != Group ctx");
		goto error_handling;
	}
	clear_status(stat);

	// We will iterate over elements in the group
	// An empty group is valid
	num_objs = PWR_GrpGetNumObjs(group);
	if (num_objs < 0) {
		LOG_FAULT("group object count < 0");
		retval = PWR_RET_INVALID;
		goto error_handling;
	}

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	for (i = 0; i < num_objs; i++) {
		PWR_Obj obj;
		int errcode;

		// Find the group object
		errcode = PWR_GrpGetObjByIndx(group, i, &obj);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, obj, attr, 0, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}

		// Set the attribute
		errcode = PWR_ObjAttrSetValue(obj, attr, value);
		if (errcode != PWR_RET_SUCCESS) {
			push_status_error(stat, obj, attr, 0, errcode);
			retval = PWR_RET_FAILURE;
			continue;
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this gets a collection of attributes for all objects in a
 * specified group, and returns the attribute values through an array, and the
 * timestamps through a different array.
 *
 * If any errors occur, this returns failure. However, it will still return the
 * entire array of values, and timestamps, and successful attempts will be valid
 * in this array. Specific errors are reported through the status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * caller does not care to determine which values are valid and which are not.
 *
 * 'values' must point to memory of at least (group_size*count*8) bytes. Each
 * returned value will appear at address (values + (count*grpindex) +
 * 8*attrindex).
 *
 * Note that groups are implemented in a balanced binary tree with an
 * unspecified sort criterion. To determine which return value belongs to which
 * object in the group, the caller must call PWR_GrpGetObjByIndx() for the index
 * of each return value.
 *
 * @param group - group object
 * @param count - count of attributes (>= 0)
 * @param attrs - attributes to return
 * @param values - array of return values
 * @param ts - array of return timestamps
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 *   PWR_RET_INVALID = bad group, all attributes failed, status is empty
 */
int
PWR_GrpAttrGetValues(PWR_Grp group, int count, const PWR_AttrName attrs[],
		     void *values, PWR_Time ts[], PWR_Status status)
{
	status_t *stat = NULL;
	int num_objs;
	int i, j, offset;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("group = %p, count = %d, attrs = %p, values = %p, "
			"ts = %p, status = %p",
			group, count, attrs, values, ts, status);

	// Sanity checks
	if (count < 0) {
		LOG_FAULT("count < 0");
		goto error_handling;
	}
	if (attrs == NULL) {
		LOG_FAULT("NULL attrs pointer");
		goto error_handling;
	}
	if (values == NULL) {
		LOG_FAULT("NULL values pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, group)) {
		LOG_FAULT("Status ctx != Group ctx");
		goto error_handling;
	}
	clear_status(stat);

	// We will iterate over elements in the group
	// An empty group is valid
	num_objs = PWR_GrpGetNumObjs(group);
	if (num_objs < 0) {
		LOG_FAULT("group object count < 0");
		retval = PWR_RET_INVALID;
		goto error_handling;
	}

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	offset = 0;				// incremented in the inner loop
	for (i = 0; i < num_objs; i++) {	// outer loop over objects in group
		PWR_Obj obj;
		int grperrcode = PWR_RET_SUCCESS;

		// Find the group object
		grperrcode = PWR_GrpGetObjByIndx(group, i, &obj);

		// Iterate over all of the attributes
		for (j = 0; j < count; j++, offset++) {	// inner loop over attributes
			PWR_Time *tsp = (ts) ? &ts[offset] : NULL;
			int errcode;

			// If the object is invalid, push status
			if (grperrcode != PWR_RET_SUCCESS) {
				push_status_error(stat, NULL, attrs[j], offset, grperrcode);
				retval = PWR_RET_FAILURE;
				continue;
			}

			// Get the attribute value and timestamp
			errcode = PWR_ObjAttrGetValue(obj, attrs[j], values+offset*8, tsp);
			if (errcode != PWR_RET_SUCCESS) {
				push_status_error(stat, obj, attrs[j], offset, errcode);
				retval = PWR_RET_FAILURE;
				continue;
			}
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Per specification, this sets a collection of attributes for all objects in a
 * specified group.
 *
 * If any errors occur, this returns failure. Specific errors are reported
 * through the status object.
 *
 * If a status object is supplied, the errors will be pushed onto the status
 * object. A NULL value for the status object is valid, and means that the
 * status information is of no interest to the caller.
 *
 * Note that groups are implemented in a balanced binary tree with an
 * unspecified sort criterion.
 *
 * @param group - group object
 * @param count - count of attributes (>= 0)
 * @param attrs - attributes to set
 * @param values - array of values to set
 * @param status - pre-created PWR_Status object, or NULL
 *
 * @return int - return code
 *   PWR_RET_SUCCESS = all attributes returned successfully
 *   PWR_RET_FAILURE = one or more attributes failed, status contains details
 *   PWR_RET_INVALID = bad group, all attributes failed, status is empty
 */
int
PWR_GrpAttrSetValues(PWR_Grp group, int count, const PWR_AttrName attrs[],
		     const void *values, PWR_Status status)
{
	status_t *stat = NULL;
	int num_objs;
	int i, j;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("group = %p, count = %d, attrs = %p, values = %p, "
			"status = %p",
			group, count, attrs, values, status);

	// Sanity checks
	if (count < 0) {
		LOG_FAULT("count < 0");
		goto error_handling;
	}
	if (attrs == NULL) {
		LOG_FAULT("NULL attrs pointer");
		goto error_handling;
	}
	if (values == NULL) {
		LOG_FAULT("NULL values pointer");
		goto error_handling;
	}

	// A returned stat == NULL is valid, and safe
	stat = find_status_by_opaque(status);
	if (stat && !check_status_context(status, group)) {
		LOG_FAULT("Status ctx != Group ctx");
		goto error_handling;
	}
	clear_status(stat);

	// We will iterate over elements in the group
	// An empty group is valid
	num_objs = PWR_GrpGetNumObjs(group);
	if (num_objs < 0) {
		LOG_FAULT("group object count < 0");
		retval = PWR_RET_INVALID;
		goto error_handling;
	}

	// Any failure results in call failure
	retval = PWR_RET_SUCCESS;
	for (i = 0; i < num_objs; i++) {	// outer loop over objects in group
		PWR_Obj obj;
		int grperrcode = PWR_RET_SUCCESS;

		// Find the group object
		grperrcode = PWR_GrpGetObjByIndx(group, i, &obj);

		// Iterate over all of the attributes
		for (j = 0; j < count; j++) {	// inner loop over attributes
			int errcode;

			// If the object is invalid, push status
			if (grperrcode != PWR_RET_SUCCESS) {
				push_status_error(stat, NULL, attrs[j], j, grperrcode);
				retval = PWR_RET_FAILURE;
				continue;
			}

			// Set the attribute value
			errcode = PWR_ObjAttrSetValue(obj, attrs[j], values+j*8);
			if (errcode != PWR_RET_SUCCESS) {
				push_status_error(stat, obj, attrs[j], j, errcode);
				retval = PWR_RET_FAILURE;
				continue;
			}
		}
	}

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Create a status object. This object is contextless, and can be reused in any
 * context.
 *
 * Formally, it should be cleared before reusing, but the implementation
 * performs an automatic clear when a new command is started, since there is no
 * reason to append new errors (which reference index locations in a call
 * pointer, and would therefore be stale if returned in a second call).
 *
 * @param status - returned opaque handle to status_t object
 *
 * @return int - PWR_RET_SUCCESS on success, PWR_RET_FAILURE on failure
 */
int
PWR_StatusCreate(PWR_Cntxt context, PWR_Status *status)
{
	int retval = PWR_RET_FAILURE;	// only permitted failure return value
	context_t *ctx = NULL;
	status_t *stat = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(context);

	TRACE1_ENTER("context = %p, status = %p", context, status);

	// Sanity checks
	if (!status) {
		LOG_FAULT("no status pointer");
		goto error_handling;
	}
	if (ctx_key != OPAQUE_GET_DATA_KEY(context)) {
		LOG_FAULT("context keys don't match!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the status
	stat = context_new_status(ctx);
	if (!stat) {
		LOG_FAULT("unable to create new status!");
		goto error_handling;
	}

	// Successful, return the opaque key
	*status = OPAQUE_GENERATE(ctx->opaque.key, stat->opaque.key);
	retval = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("retval = %d, opaque = %p", retval,
		   (status) ? *status : NULL);

	return retval;
}

/**
 * Destroy a status object. Passing an invalid opaque handle (e.g. duplicate
 * release) is safe, but reports failure.
 *
 * @param status - opaque handle to status_t object
 *
 * @return int - PWR_RET_SUCCESS on success, PWR_RET_FAILURE on failure
 */
int
PWR_StatusDestroy(PWR_Status status)
{
	int retval = PWR_RET_FAILURE;	// only permitted failure return value
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(status);
	status_t *stat = NULL;
	context_t *ctx = NULL;

	TRACE1_ENTER("status = %p", status);

	// Failure to find this typically means a double-release
	stat = find_status_by_opaque(status);
	if (!stat) {
		LOG_FAULT("status not found!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context delete the status
	context_del_status(ctx, stat);
	retval = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/**
 * Pop a single error off an existing status_t object, and optionally return its
 * content through the 'error' structure.
 *
 * This fails with PWR_RET_FAILURE if the status handle is invalid.
 *
 * This fails with PWR_RET_EMPTY if the status is empty (all errors popped).
 *
 * @param status - opaque handle to status_t object
 * @param error - pointer to error return structure, or NULL to discard
 *
 * @return int - return code:
 *      PWR_RET_FAILURE status opaque handle is invalid
 *      PWR_RET_SUCCESS error popped
 *      PWR_RET_EMPTY no more errors to pop
 */
int
PWR_StatusPopError(PWR_Status status, PWR_AttrAccessError *error)
{
	PWR_AttrAccessError *errdup = NULL;
	status_t *stat = NULL;
	int retval = PWR_RET_FAILURE;

	TRACE1_ENTER("status = %p, error = %p", status, error);

	stat = find_status_by_opaque(status);
	if (!stat || !stat->list) {
		goto error_handling;
	}

	errdup = g_queue_pop_head(stat->list);
	if (!errdup) {
		retval = PWR_RET_EMPTY;
		goto error_handling;
	}

	if (error) {
		memcpy(error, errdup, sizeof(PWR_AttrAccessError));
	}
	free(errdup);
	retval = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

