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
#include <string.h>

#define _GNU_SOURCE // Enable GNU extensions

#include <glib.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "group.h"
#include "object.h"
#include "context.h"
#include "attributes.h"
#include "rolesys/app_os.h"

// PWR_ObjType values mapped to name strings
const char *obj_name[PWR_NUM_OBJ_TYPES] = {
	[ PWR_OBJ_PLATFORM	] = "PWR_OBJ_PLATFORM",
	[ PWR_OBJ_CABINET	] = "PWR_OBJ_CABINET",
	[ PWR_OBJ_CHASSIS	] = "PWR_OBJ_CHASSIS",
	[ PWR_OBJ_BOARD		] = "PWR_OBJ_BOARD",
	[ PWR_OBJ_NODE		] = "PWR_OBJ_NODE",
	[ PWR_OBJ_SOCKET	] = "PWR_OBJ_SOCKET",
	[ PWR_OBJ_CORE		] = "PWR_OBJ_CORE",
	[ PWR_OBJ_POWER_PLANE	] = "PWR_OBJ_POWER_PLANE",
	[ PWR_OBJ_MEM		] = "PWR_OBJ_MEM",
	[ PWR_OBJ_NIC		] = "PWR_OBJ_NIC",
	[ PWR_OBJ_HT		] = "PWR_OBJ_HT"
};

// PWR_ObjType values mapped to description strings
const char *obj_desc[PWR_NUM_OBJ_TYPES] = {
	[ PWR_OBJ_PLATFORM	] = "Unsupported by implementation",
	[ PWR_OBJ_CABINET	] = "Unsupported by implementation",
	[ PWR_OBJ_CHASSIS	] = "Unsupported by implementation",
	[ PWR_OBJ_BOARD		] = "Unsupported by implementation",
	[ PWR_OBJ_NODE		] = "Compute node",
	[ PWR_OBJ_SOCKET	] = "Integrated ciruit package installed in socket",
	[ PWR_OBJ_CORE		] = "Processor core in an integrated circuit package",
	[ PWR_OBJ_POWER_PLANE	] = "A collection of components with the same power source",
	[ PWR_OBJ_MEM		] = "Memory attached to a socket",
	[ PWR_OBJ_NIC		] = "Unsupported by implementation",
	[ PWR_OBJ_HT		] = "Hardware thread in a processor core"
};

// PWR_AttrName values mapped to name strings
const char *attr_name[PWR_NUM_ATTR_NAMES] = {
	[ PWR_ATTR_PSTATE		] = "PWR_ATTR_PSTATE",
	[ PWR_ATTR_CSTATE		] = "PWR_ATTR_CSTATE",
	[ PWR_ATTR_CSTATE_LIMIT		] = "PWR_ATTR_CSTATE_LIMIT",
	[ PWR_ATTR_SSTATE		] = "PWR_ATTR_SSTATE",
	[ PWR_ATTR_CURRENT		] = "PWR_ATTR_CURRENT",
	[ PWR_ATTR_VOLTAGE		] = "PWR_ATTR_VOLTAGE",
	[ PWR_ATTR_POWER		] = "PWR_ATTR_POWER",
	[ PWR_ATTR_POWER_LIMIT_MIN	] = "PWR_ATTR_POWER_LIMIT_MIN",
	[ PWR_ATTR_POWER_LIMIT_MAX	] = "PWR_ATTR_POWER_LIMIT_MAX",
	[ PWR_ATTR_FREQ			] = "PWR_ATTR_FREQ",
	[ PWR_ATTR_FREQ_REQ		] = "PWR_ATTR_FREQ_REQ",
	[ PWR_ATTR_FREQ_LIMIT_MIN	] = "PWR_ATTR_FREQ_LIMIT_MIN",
	[ PWR_ATTR_FREQ_LIMIT_MAX	] = "PWR_ATTR_FREQ_LIMIT_MAX",
	[ PWR_ATTR_ENERGY		] = "PWR_ATTR_ENERGY",
	[ PWR_ATTR_TEMP			] = "PWR_ATTR_TEMP",
	[ PWR_ATTR_OS_ID		] = "PWR_ATTR_OS_ID",
	[ PWR_ATTR_THROTTLED_TIME	] = "PWR_ATTR_THROTTLED_TIME",
	[ PWR_ATTR_THROTTLED_COUNT	] = "PWR_ATTR_THROTTLED_COUNT",
	[ PWR_ATTR_GOV			] = "PWR_ATTR_GOV"
};

// PWR_AttrName values mapped to description strings
const char *attr_desc[PWR_NUM_ATTR_NAMES] = {
	[ PWR_ATTR_PSTATE		] =
	"Current P-state for the %s object",
	[ PWR_ATTR_CSTATE		] =
	"current C-state for the %s object",
	[ PWR_ATTR_CSTATE_LIMIT		] =
	"Lowest C-state allowed for the %s object",
	[ PWR_ATTR_SSTATE		] =
	"Current S-state for the %s object",
	[ PWR_ATTR_CURRENT		] =
	"Discrete current value in amps for the %s object",
	[ PWR_ATTR_VOLTAGE		] =
	"Discrete voltage value in volts for the %s object",
	[ PWR_ATTR_POWER		] =
	"Discrete power value in watts for the %s object",
	[ PWR_ATTR_POWER_LIMIT_MIN	] =
	"Minimum power limit, lower bound, in watts for the %s object",
	[ PWR_ATTR_POWER_LIMIT_MAX	] =
	"Maximum power limit, upper bound, in watts for the %s object",
	[ PWR_ATTR_FREQ			] =
	"Current operating frequency value, in Hz, for the %s object",
	[ PWR_ATTR_FREQ_REQ		] =
	"Requested operating frequency value, in Hz, for the %s object",
	[ PWR_ATTR_FREQ_LIMIT_MIN	] =
	"Minimum operating frequency limit, in Hz, for the %s object",
	[ PWR_ATTR_FREQ_LIMIT_MAX	] =
	"Maximum operating frequency limit, in Hz, for the %s object",
	[ PWR_ATTR_ENERGY		] =
	"Cumulative energy used, in joules, for the %s object",
	[ PWR_ATTR_TEMP			] =
	"Current temperature, in degrees Celsius, for the %s object",
	[ PWR_ATTR_OS_ID		] =
	"Operating system identifier for the %s object",
	[ PWR_ATTR_THROTTLED_TIME	] =
	"The cumulative time, in nanoseconds, that performance of the"
	" %s object was purposefully slowed in order to meet"
	" some constraint, such as a power limit",
	[ PWR_ATTR_THROTTLED_COUNT	] =
	"The cumulative count of the number of times that performance"
	" of the %s object was purposefully slowed in order to"
	" meet some constraint, such as a power limit",
	[ PWR_ATTR_GOV			] =
	"Power related frequency governer capability exposed through"
	" the operating system interface for the %s object."
};

// Macro to test a bit in a bitmask and assign a powerapi return value
// for the result. PWR_RET_SUCCESS if bit is set, PWR_RET_NO_ATTRIB if
// bit is not set.
#define TEST_OBJ_ATTR(bitmask, attr) \
	(BITMASK_TEST((bitmask), (attr)) ? PWR_RET_SUCCESS : PWR_RET_NO_ATTRIB)

// Node attribute support bitmask
const DECLARE_BITMASK_AUTO(node_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_POWER)|
		BITBLOCK_MASK(PWR_ATTR_POWER_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_ENERGY)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID));

// Socket attribute support bitmask
const DECLARE_BITMASK_AUTO(socket_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_POWER)|
		BITBLOCK_MASK(PWR_ATTR_POWER_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_ENERGY)|
		BITBLOCK_MASK(PWR_ATTR_TEMP)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID)|
		BITBLOCK_MASK(PWR_ATTR_THROTTLED_TIME));

// Memory attribute support bitmask
const DECLARE_BITMASK_AUTO(mem_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_POWER)|
		BITBLOCK_MASK(PWR_ATTR_POWER_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_ENERGY)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID)|
		BITBLOCK_MASK(PWR_ATTR_THROTTLED_TIME));

// Power Plane attribute support bitmask
const DECLARE_BITMASK_AUTO(pplane_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_POWER)|
		BITBLOCK_MASK(PWR_ATTR_ENERGY)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID));

// Core attribute support bitmask
const DECLARE_BITMASK_AUTO(core_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_TEMP)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID));

// Hardware Thread attribute support bitmask
const DECLARE_BITMASK_AUTO(ht_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_CSTATE_LIMIT)|
		BITBLOCK_MASK(PWR_ATTR_FREQ)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_REQ)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_LIMIT_MIN)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID)|
		BITBLOCK_MASK(PWR_ATTR_GOV));

// Bitmask of all supported attributes
const DECLARE_BITMASK_AUTO(supported_attr_bitmask, PWR_NUM_ATTR_NAMES,
		BITBLOCK_MASK(PWR_ATTR_POWER)|
		BITBLOCK_MASK(PWR_ATTR_POWER_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_CSTATE_LIMIT)|
		BITBLOCK_MASK(PWR_ATTR_FREQ)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_REQ)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_LIMIT_MIN)|
		BITBLOCK_MASK(PWR_ATTR_FREQ_LIMIT_MAX)|
		BITBLOCK_MASK(PWR_ATTR_ENERGY)|
		BITBLOCK_MASK(PWR_ATTR_TEMP)|
		BITBLOCK_MASK(PWR_ATTR_OS_ID)|
		BITBLOCK_MASK(PWR_ATTR_THROTTLED_TIME)|
		BITBLOCK_MASK(PWR_ATTR_GOV));


//----------------------------------------------------------------------//
//			HIERARCHY OBJECT FUNCTIONS			//
//----------------------------------------------------------------------//


// Macro defines a function to construct a specified hierarchy
// object type; used as a generic function template.
#define NEW_OBJ(TYPE, PWR_TYPE)						\
	TYPE##_t *new_##TYPE(uint64_t id,				\
			const char *name_fmt, ...)			\
	{								\
		va_list args;						\
		TYPE##_t *ptr;						\
									\
		TRACE3_ENTER("id = %#lx, name_fmt = '%s'", id, name_fmt); \
									\
		ptr = g_new0(TYPE##_t, 1);				\
		if (!ptr) {						\
			LOG_FAULT("Alloc for " #TYPE " object failed"); \
			goto error_return;				\
		}							\
		ptr->obj.os_id = id;					\
		ptr->obj.type = PWR_TYPE;				\
		va_start(args, name_fmt);				\
		ptr->obj.name = g_strdup_vprintf(name_fmt, args);	\
		va_end(args);						\
		if (!ptr->obj.name) {					\
			LOG_FAULT("Failed to alloc object name");	\
			goto error_return;				\
		}							\
		if (!plugin) {						\
			LOG_FAULT("Plugin not configured!");		\
			goto error_return;				\
		}							\
		if (plugin->construct_##TYPE(ptr) != 0) {		\
			LOG_FAULT("plugin construct_" #TYPE " fail");	\
			goto error_return;				\
		}							\
		if (!(ptr->obj.hints = init_hints())) {			\
			LOG_FAULT("init_hints fail");			\
			goto error_return;				\
		}							\
		if (!opaque_map_insert(opaque_map, OPAQUE_OBJECT,	\
					&ptr->obj.opaque)) {		\
			LOG_FAULT("opaque map insert fail");		\
			goto error_return;				\
		}							\
									\
		TRACE3_EXIT("ptr = %p", ptr);				\
		return ptr;						\
									\
error_return:								\
		TRACE3_EXIT("alloc failed, returning NULL");		\
		del_##TYPE(ptr);					\
		return NULL;						\
	}

// Macro defines a function to destruct a specified hierarchy
// object type; used as a generic function template
#define DEL_OBJ(TYPE)							\
	void del_##TYPE(TYPE##_t *ptr)					\
	{								\
		TRACE3_ENTER("ptr = %p", ptr);				\
									\
		if (!ptr) {						\
			TRACE3_EXIT("null pointer");			\
			return;						\
		}							\
									\
		destroy_hints(ptr->obj.hints);				\
		opaque_map_remove(opaque_map, ptr->obj.opaque.key);	\
		if (plugin) {						\
			plugin->destruct_##TYPE(ptr);			\
		}							\
		g_free(ptr->obj.name);					\
		g_free(ptr);						\
									\
		TRACE3_EXIT("");					\
	}

// Define constructor/destructor for ht_t type
DEL_OBJ(ht)
NEW_OBJ(ht, PWR_OBJ_HT)

// Define constructor/destructor for core_t type
DEL_OBJ(core)
NEW_OBJ(core, PWR_OBJ_CORE)

// Define constructor/destructor for pplane_t type
DEL_OBJ(pplane)
NEW_OBJ(pplane, PWR_OBJ_POWER_PLANE)

// Define constructor/destructor for mem_t type
DEL_OBJ(mem)
NEW_OBJ(mem, PWR_OBJ_MEM)

// Define constructor/destructor for socket_t type
DEL_OBJ(socket)
NEW_OBJ(socket, PWR_OBJ_SOCKET)

// Define constructor/destructor for node_t type
DEL_OBJ(node)
NEW_OBJ(node, PWR_OBJ_NODE)

static void
obj_del_container(obj_t *obj)
{
	TRACE3_ENTER("obj = %p", obj);

	if (!obj) {
		TRACE3_EXIT("");
		return;
	}

	switch (obj->type) {
	case PWR_OBJ_NODE:
		del_node(to_node(obj));
		break;
	case PWR_OBJ_SOCKET:
		del_socket(to_socket(obj));
		break;
	case PWR_OBJ_CORE:
		del_core(to_core(obj));
		break;
	case PWR_OBJ_POWER_PLANE:
		del_pplane(to_pplane(obj));
		break;
	case PWR_OBJ_MEM:
		del_mem(to_mem(obj));
		break;
	case PWR_OBJ_HT:
		del_ht(to_ht(obj));
		break;
	default:
		LOG_FAULT("Attempt to delete unkown object %d", obj->type);
		break;
	}

	TRACE3_EXIT("");
}

void
obj_destroy_callback(gpointer data)
{
	TRACE3_ENTER("data = %p", data);

	obj_del_container(to_obj(data));

	TRACE3_EXIT("");
}

static int
obj_attr_get_meta_plugin(obj_t *obj, PWR_AttrName attr, PWR_MetaName meta,
		void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, attr = %d, meta = %d, value = %p",
			obj, attr, meta, value);

	// Call the plugin get_meta() function for the specific object type.
	switch (obj->type) {
	case PWR_OBJ_NODE:
		retval = to_node(obj)->ops->get_meta(to_node(obj),
				attr, meta, value);
		break;
	case PWR_OBJ_SOCKET:
		retval = to_socket(obj)->ops->get_meta(to_socket(obj),
				attr, meta, value);
		break;
	case PWR_OBJ_CORE:
		retval = to_core(obj)->ops->get_meta(to_core(obj),
				attr, meta, value);
		break;
	case PWR_OBJ_POWER_PLANE:
		retval = to_pplane(obj)->ops->get_meta(to_pplane(obj),
				attr, meta, value);
		break;
	case PWR_OBJ_MEM:
		retval = to_mem(obj)->ops->get_meta(to_mem(obj),
				attr, meta, value);
		break;
	case PWR_OBJ_HT:
		retval = to_ht(obj)->ops->get_meta(to_ht(obj),
				attr, meta, value);
		break;
	default:
		LOG_FAULT("Unexpected object type: %d", obj->type);
		retval = PWR_RET_BAD_VALUE;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
obj_attr_set_meta_plugin(obj_t *obj, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, ipc = %p, attr = %d, meta = %d, value = %p",
			obj, ipc, attr, meta, value);

	// Call the plugin set_meta() function for the specific object type.
	switch (obj->type) {
	case PWR_OBJ_NODE:
		retval = to_node(obj)->ops->set_meta(to_node(obj), ipc,
				attr, meta, value);
		break;
	case PWR_OBJ_SOCKET:
		retval = to_socket(obj)->ops->set_meta(to_socket(obj), ipc,
				attr, meta, value);
		break;
	case PWR_OBJ_CORE:
		retval = to_core(obj)->ops->set_meta(to_core(obj), ipc,
				attr, meta, value);
		break;
	case PWR_OBJ_POWER_PLANE:
		retval = to_pplane(obj)->ops->set_meta(to_pplane(obj), ipc,
				attr, meta, value);
		break;
	case PWR_OBJ_MEM:
		retval = to_mem(obj)->ops->set_meta(to_mem(obj), ipc,
				attr, meta, value);
		break;
	case PWR_OBJ_HT:
		retval = to_ht(obj)->ops->set_meta(to_ht(obj), ipc,
				attr, meta, value);
		break;
	default:
		LOG_FAULT("Unexpected object type: %d", obj->type);
		retval = PWR_RET_BAD_VALUE;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
obj_get_meta(obj_t *obj, PWR_MetaName meta, void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, meta = %d, value = %p", obj, meta, value);

	// Common object metadata can be supplied here, but unique
	// object metadata will require a call to the plugin interface.
	switch (meta) {
	case PWR_MD_NUM:
	case PWR_MD_MIN:
	case PWR_MD_MAX:
	case PWR_MD_PRECISION:
	case PWR_MD_ACCURACY:
	case PWR_MD_UPDATE_RATE:
	case PWR_MD_SAMPLE_RATE:
	case PWR_MD_TIME_WINDOW:
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
		// No such metadata for objects
		retval = PWR_RET_NO_META;
		break;
	case PWR_MD_NAME_LEN:
		// Length of the object name string
		*(uint64_t *)value = strlen(obj_name[obj->type]) + 1;
		break;
	case PWR_MD_NAME:
		// Copy of the attribute name string
		strcpy((char *)value, obj_name[obj->type]);
		break;
	case PWR_MD_DESC_LEN:
		// Length of the object description string
		*(uint64_t *)value = strlen(obj_desc[obj->type]) + 1;
		break;
	case PWR_MD_DESC:
		// Length of the object description string
		strcpy((char *)value, obj_desc[obj->type]);
		break;
	case PWR_MD_VALUE_LEN:
		// No such metadata for objects
		retval = PWR_RET_NO_META;
		break;
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_VENDOR_INFO:
		// This metadata is not attribute specific
		// Let the plugin handle it
		retval = obj_attr_get_meta_plugin(obj,
				PWR_ATTR_NOT_SPECIFIED, meta, value);
		break;
	case PWR_MD_MEASURE_METHOD:
		// No such metadata for objects
		retval = PWR_RET_NO_META;
		break;
	default:
		// If metadata doesn't match one of the above
		// cases, it should have been handled as one
		// of the common cases, else it is an out of
		// range value.
		LOG_FAULT("Unexpected metadata value: %d", meta);
		retval = PWR_RET_FAILURE;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
obj_attr_get_meta(obj_t *obj, PWR_AttrName attr, PWR_MetaName meta,
		void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("obj = %p, attr = %d, meta = %d, value = %p",
			obj, attr, meta, value);

	// Common attribute metadata can be supplied here, but other
	// object/attribute metadata will require a call to the
	// plugin interface.
	switch (meta) {
	case PWR_MD_NUM:
	case PWR_MD_MIN:
	case PWR_MD_MAX:
	case PWR_MD_PRECISION:
	case PWR_MD_ACCURACY:
	case PWR_MD_UPDATE_RATE:
	case PWR_MD_SAMPLE_RATE:
	case PWR_MD_TIME_WINDOW:
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
		// Let the plugin handle it
		retval = obj_attr_get_meta_plugin(obj, attr, meta, value);
		break;
	case PWR_MD_NAME_LEN:
		// Length of the attribute name string
		*(uint64_t *)value = strlen(attr_name[attr]) + 1;
		break;
	case PWR_MD_NAME:
		// Copy of the attribute name string
		strcpy((char *)value, attr_name[attr]);
		break;
	case PWR_MD_DESC_LEN:
		// Compute description string length: description format
		// string length, minus conversion characters, plus object
		// name length, plus null byte.
		*(uint64_t *)value = strlen(attr_desc[attr]) - 2 +
				strlen(obj_name[obj->type]) + 1;
		break;
	case PWR_MD_DESC:
		// Construct the description string.
		{
			int retcode = 0;
			retcode = sprintf((char *)value, attr_desc[attr],
					obj_name[obj->type]);
			if (retcode < 0) {
				LOG_FAULT("Failed to generate description: %m");
				retval = PWR_RET_FAILURE;
			}
		}
		break;
	case PWR_MD_VALUE_LEN:
	case PWR_MD_MEASURE_METHOD:
		// Let the plugin handle it
		retval = obj_attr_get_meta_plugin(obj, attr, meta, value);
		break;
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_VENDOR_INFO:
		// This metadata is not attribute specific
		// Let the plugin handle it
		retval = obj_attr_get_meta_plugin(obj,
				PWR_ATTR_NOT_SPECIFIED, meta, value);
		break;
	default:
		// If metadata doesn't match one of the above
		// cases, it should have been handled as one
		// of the common cases, else it is an out of
		// range value.
		LOG_FAULT("Unexpected metadata value: %d", meta);
		retval = PWR_RET_FAILURE;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
get_os_id(uint64_t id, uint64_t *value, struct timespec *ts)
{
	int retval = PWR_RET_FAILURE;

	TRACE2_ENTER("id = %#lx, value = %p, ts = %p", id, value, ts);

	/*
	 * Must grab timestamp as close to the data sample as possible.
	 * Timestamp is nanoseconds since the Epoch.
	 */
	if (clock_gettime(CLOCK_REALTIME, ts))
		goto done;

	*value = id;
	retval = PWR_RET_SUCCESS;

done:
	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
forward_attr_set_value(obj_t *obj, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;
	int implemented = 0;
	GNode *gnode = NULL;

	TRACE2_ENTER("obj = %p, ipc = %p, attr = %d, value = %p",
			obj, ipc, attr, value);

	for (gnode = g_node_first_child(obj->gnode);
			gnode != NULL;
			gnode = g_node_next_sibling(gnode)) {
		int status = PWR_RET_SUCCESS;
		obj_t *child_obj = to_obj(gnode->data);

		switch (child_obj->type) {
		case PWR_OBJ_NODE:
			status = node_attr_set_value(to_node(child_obj),
					ipc, attr, value);
			break;
		case PWR_OBJ_SOCKET:
			status = socket_attr_set_value(to_socket(child_obj),
					ipc, attr, value);
			break;
		case PWR_OBJ_CORE:
			status = core_attr_set_value(to_core(child_obj),
					ipc, attr, value);
			break;
		case PWR_OBJ_POWER_PLANE:
			status = pplane_attr_set_value(to_pplane(child_obj),
					ipc, attr, value);
			break;
		case PWR_OBJ_MEM:
			status = mem_attr_set_value(to_mem(child_obj),
					ipc, attr, value);
			break;
		case PWR_OBJ_HT:
			status = ht_attr_set_value(to_ht(child_obj),
					ipc, attr, value);
			break;
		default:
			status = PWR_RET_NOT_IMPLEMENTED;
			break;
		}

		if (status == PWR_RET_NOT_IMPLEMENTED) {
			continue;
		}

		implemented++;

		/*
		 * Handling errors/warnings is a little weird since
		 * warnings are positive, errors are negative, and
		 * success is zero.
		 *
		 * Here is what we do:
		 * First, warnings are ignored.
		 * Second, if this is the first error, record it.
		 * Third, if this error is different from the
		 *   previous one, set error to the generic
		 *   PWR_RET_FAILURE.
		 * Finally, if there are no errors success is
		 *   returned.
		 */
		if (retval == PWR_RET_SUCCESS) {
			retval = status;
		}
		if (status != retval && status < PWR_RET_SUCCESS) {
			retval = PWR_RET_FAILURE;
		}
	}

	/*
	 * If none of our children implement the attribute, say that
	 * it is not implemented.
	 */
	if (!implemented) {
		retval = PWR_RET_NOT_IMPLEMENTED;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
node_attr_get_value(node_t *node, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, attr = %d, value = %p, ts = %p",
			node, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(node->obj.os_id, value, ts);
		break;
	case PWR_ATTR_POWER:
		retval = node->ops->get_power(node, value, ts);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		retval = node->ops->get_power_limit_max(node, value, ts);
		break;
	case PWR_ATTR_ENERGY:
		retval = node->ops->get_energy(node, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
node_attr_set_value(node_t *node, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("node = %p, ipc = %p, attr = %d, value = %p",
			node, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_POWER:
	case PWR_ATTR_ENERGY:
		retval = PWR_RET_READ_ONLY;
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(node), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
socket_attr_get_value(socket_t *socket, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("socket = %p, attr = %d, value = %p, ts = %p",
			socket, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(socket->obj.os_id, value, ts);
		break;
	case PWR_ATTR_POWER:
		retval = socket->ops->get_power(socket, value, ts);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		retval = socket->ops->get_power_limit_max(socket, value, ts);
		break;
	case PWR_ATTR_ENERGY:
		retval = socket->ops->get_energy(socket, value, ts);
		break;
	case PWR_ATTR_THROTTLED_TIME:
		retval = socket->ops->get_throttled_time(socket, value, ts);
		break;
	case PWR_ATTR_TEMP:
		retval = socket->ops->get_temp(socket, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
socket_attr_set_value(socket_t *socket, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("socket = %p, ipc = %p, attr = %d, value = %p",
			socket, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_POWER:
	case PWR_ATTR_ENERGY:
	case PWR_ATTR_THROTTLED_TIME:
	case PWR_ATTR_TEMP:
		retval = PWR_RET_READ_ONLY;
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		retval = socket->ops->set_power_limit_max(socket, ipc, value);
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(socket), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
core_attr_get_value(core_t *core, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("core = %p, attr = %d, value = %p, ts = %p",
			core, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(core->obj.os_id, value, ts);
		break;
	case PWR_ATTR_TEMP:
		retval = core->ops->get_temp(core, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
core_attr_set_value(core_t *core, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("core = %p, ipc = %p, attr = %d, value = %p",
			core, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_TEMP:
		retval = PWR_RET_READ_ONLY;
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(core), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
pplane_attr_get_value(pplane_t *pplane, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, attr = %d, value = %p, ts = %p",
			pplane, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(pplane->obj.os_id, value, ts);
		break;
	case PWR_ATTR_POWER:
		retval = pplane->ops->get_power(pplane, value, ts);
		break;
	case PWR_ATTR_ENERGY:
		retval = pplane->ops->get_energy(pplane, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
pplane_attr_set_value(pplane_t *pplane, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("pplane = %p, ipc = %p, attr = %d, value = %p",
			pplane, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_POWER:
	case PWR_ATTR_ENERGY:
		retval = PWR_RET_READ_ONLY;
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(pplane), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
mem_attr_get_value(mem_t *mem, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, attr = %d, value = %p, ts = %p",
			mem, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(mem->obj.os_id, value, ts);
		break;
	case PWR_ATTR_POWER:
		retval = mem->ops->get_power(mem, value, ts);
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		retval = mem->ops->get_power_limit_max(mem, value, ts);
		break;
	case PWR_ATTR_ENERGY:
		retval = mem->ops->get_energy(mem, value, ts);
		break;
	case PWR_ATTR_THROTTLED_TIME:
		retval = mem->ops->get_throttled_time(mem, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
mem_attr_set_value(mem_t *mem, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("mem = %p, ipc = %p, attr = %d, value = %p",
			mem, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_POWER:
	case PWR_ATTR_ENERGY:
	case PWR_ATTR_THROTTLED_TIME:
		retval = PWR_RET_READ_ONLY;
		break;
	case PWR_ATTR_POWER_LIMIT_MAX:
		retval = mem->ops->set_power_limit_max(mem, ipc, value);
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(mem), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
ht_attr_get_value(ht_t *ht, PWR_AttrName attr, void *value,
		struct timespec *ts)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, attr = %d, value = %p, ts = %p",
			ht, attr, value, ts);

	switch (attr) {
	case PWR_ATTR_OS_ID:
		retval = get_os_id(ht->obj.os_id, value, ts);
		break;
	case PWR_ATTR_CSTATE_LIMIT:
		retval = ht->ops->get_cstate_limit(ht, value, ts);
		break;
	case PWR_ATTR_FREQ:
		retval = ht->ops->get_freq(ht, value, ts);
		break;
	case PWR_ATTR_FREQ_REQ:
		retval = ht->ops->get_freq_req(ht, value, ts);
		break;
	case PWR_ATTR_FREQ_LIMIT_MIN:
		retval = ht->ops->get_freq_limit_min(ht, value, ts);
		break;
	case PWR_ATTR_FREQ_LIMIT_MAX:
		retval = ht->ops->get_freq_limit_max(ht, value, ts);
		break;
	case PWR_ATTR_GOV:
		retval = ht->ops->get_governor(ht, value, ts);
		break;
	default:
		retval = PWR_RET_NOT_IMPLEMENTED;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

int
ht_attr_set_value(ht_t *ht, ipc_t *ipc, PWR_AttrName attr,
		const void *value)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("ht = %p, ipc = %p, attr = %d, value = %p",
			ht, ipc, attr, value);

	switch (attr) {
		/* attributes handled at this level go here */
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_FREQ:
		retval = PWR_RET_READ_ONLY;
		break;
	case PWR_ATTR_CSTATE_LIMIT:
		retval = ht->ops->set_cstate_limit(ht, ipc, value);
		break;
	case PWR_ATTR_FREQ_REQ:
		retval = ht->ops->set_freq_req(ht, ipc, value);
		break;
	case PWR_ATTR_FREQ_LIMIT_MIN:
		retval = ht->ops->set_freq_limit_min(ht, ipc, value);
		break;
	case PWR_ATTR_FREQ_LIMIT_MAX:
		retval = ht->ops->set_freq_limit_max(ht, ipc, value);
		break;
	case PWR_ATTR_GOV:
		retval = ht->ops->set_governor(ht, ipc, value);
		break;
	default:
		/* attributes not handled here get forwarded */
		retval = forward_attr_set_value(to_obj(ht), ipc,
				attr, value);
		break;
	}

	TRACE2_EXIT("retval = %d", retval);

	return retval;
}

static int
get_attr_list(const bitmask_t *bitmask, size_t len,
		const char **str_list, int *val_list)
{
	int retval = PWR_RET_SUCCESS;
	int bit = 0;
	int set = 0;

	TRACE3_ENTER("bitmask = %p, len = %zu, str_list = %p, val_list = %p",
			bitmask, len, str_list, val_list);

	for (bit = 0; bit < BITMASK_NUM_BITS_USED(bitmask) && set < len;
			bit++) {
		if (BITMASK_TEST(bitmask, bit)) {
			if (val_list)
				val_list[set] = bit;
			if (str_list)
				str_list[set] = attr_name[bit];
			++set;
		}
	}

	if (bit < BITMASK_NUM_BITS_USED(bitmask)) {
		retval = PWR_RET_WARN_TRUNC;
	}

	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

// Determine if the attribute is in range and supported by the object.
static int
validate_attr(obj_t *obj, PWR_AttrName attr)
{
	int status = PWR_RET_SUCCESS;

	TRACE3_ENTER("obj = %p, attr = %d", obj, attr);

	// Validate that attribute is in the range of valid values
	if (attr < 0 || attr >= PWR_NUM_ATTR_NAMES) {
		LOG_FAULT("Attribute value, %d, out of range", attr);
		status = PWR_RET_OUT_OF_RANGE;
		goto quick_return;
	}

	// Determine if the attribute is supported by the object type.
	switch (obj->type) {
	case PWR_OBJ_NODE:
		status = TEST_OBJ_ATTR(&node_attr_bitmask, attr);
		break;
	case PWR_OBJ_SOCKET:
		status = TEST_OBJ_ATTR(&socket_attr_bitmask, attr);
		break;
	case PWR_OBJ_CORE:
		status = TEST_OBJ_ATTR(&core_attr_bitmask, attr);
		break;
	case PWR_OBJ_POWER_PLANE:
		status = TEST_OBJ_ATTR(&pplane_attr_bitmask, attr);
		break;
	case PWR_OBJ_MEM:
		status = TEST_OBJ_ATTR(&mem_attr_bitmask, attr);
		break;
	case PWR_OBJ_HT:
		status = TEST_OBJ_ATTR(&ht_attr_bitmask, attr);
		break;
	default:
		status = PWR_RET_INVALID;
		break;
	}

	if (status) {
		LOG_WARN("Attribute %d not supported by object type %d",
				attr, obj->type);
		goto quick_return;
	}

quick_return:
	TRACE3_EXIT("status = %d", status);

	return status;
}


//----------------------------------------------------------------------//
//			API FUNCTIONS					//
//----------------------------------------------------------------------//


/*
 * PWR_ObjGetType - Returns the type of the object specified.
 *
 * Argument(s):
 *
 *	obj  - The object the user wishes to determine the type of.
 *	type - The type of the specified object.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS; type is set to the type of the
 *                        specified object.
 *	PWR_RET_FAILURE - Upon FAILURE; type is set to PWR_OBJ_INVALID.
 */
int
PWR_ObjGetType(PWR_Obj object, PWR_ObjType *type)
{
	int retval = PWR_RET_FAILURE;
	obj_t *obj = NULL;
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, type = %p", object, type);

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	obj = opaque_map_lookup_object(opaque_map, obj_key);

	// Invalid value found for the key: either no value,
	// mismatched type, of unsupported type.
	if (!obj) {
		*type = PWR_OBJ_INVALID;
		goto done;
	}

	*type = obj->type;
	retval = PWR_RET_SUCCESS;

done:
	TRACE1_EXIT("retval = %d, *type = %d", retval, *type);

	return retval;
}

/*
 * PWR_ObjGetName - Returns the name of the object specified.
 *
 * Argument(s):
 *
 *   obj  - The object the user wishes to determine the name of.
 *   dest - The address of the user provided buffer.
 *   len  - The length of the user provided buffer.
 *
 * Return Code(s):
 *
 *   PWR_RET_SUCCESS - Upon SUCCESS; dest will contain the name of the
 *                     object.  String will include terminating null byte.
 *   PWR_RET_WARN_TRUNC - Call succeeded, but the length of object name
 *                        was longer than the provided buffer and the name
 *                        was truncated.
 *   PWR_RET_FAILURE - Upon FAILURE
 */
int
PWR_ObjGetName(PWR_Obj object, char *dest, size_t len)
{
	int retval = PWR_RET_FAILURE;
	gsize src_len = 0;
	obj_t *obj = NULL;
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, dest = %p, len = %zu", object, dest, len);

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	obj = opaque_map_lookup_object(opaque_map, obj_key);

	// Invalid value found for the key: either no value,
	// mismatched type, or unsupported type.
	if (!obj)
		goto done;

	src_len = g_strlcpy(dest, obj->name, len);

	if (src_len >= len) {
		retval = PWR_RET_WARN_TRUNC;
		goto done;
	}

	retval = PWR_RET_SUCCESS;

done:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

/*
 * PWR_ObjGetParent - Returns the parent of the specified object.
 *
 * Argument(s):
 *
 *	object - The object the user wishes to determine the parent of.
 *	parent - The place to store the parent object.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS; parent will contain the parent object.
 *	PWR_RET_WARN_NO_PARENT - The specified object doesn't have a parent.
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
PWR_ObjGetParent(PWR_Obj object, PWR_Obj *parent)
{
	int retval = PWR_RET_FAILURE;
	obj_t *obj = NULL;
	obj_t *parent_obj = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_DATA_KEY(object);
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, parent = %p", object, parent);

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	obj = opaque_map_lookup_object(opaque_map, obj_key);
	if (!obj) {
		LOG_FAULT("Object not found for opaque reference %p", object);
		goto status_return;
	}

	// Check for error case and report it
	if (!obj->gnode) {
		LOG_FAULT("Object %s is missing gnode, not in hierarchy",
				obj->name);
		goto status_return;
	}

	// If the node has no parent, return warning that there is no
	// parent and set the opaque reference to all zeros.
	if (!obj->gnode->parent) {
		// Set the opaque reference to all zero.
		// The root object won't have a parent.
		LOG_DBG("Object %s is missing parent", obj->name);
		*parent = OPAQUE_GENERATE(0, 0);
		retval = PWR_RET_WARN_NO_PARENT;
		goto status_return;
	}

	// There is a parent gnode. Get the object linked to the gnode
	// and return the opaque reference.
	parent_obj = (obj_t *)obj->gnode->parent->data;
	*parent = OPAQUE_GENERATE(ctx_key, parent_obj->opaque.key);
	retval = PWR_RET_SUCCESS;

status_return:
	TRACE1_EXIT("retval = %d, *parent = %p", retval, *parent);

	return retval;
}

/*
 * PWR_ObjGetChildren - Returns the children of the specified object.
 *
 * Argument(s):
 *
 *	object - The object the user wishes to determine the children of.
 *	group - The place to store the group that contains the children.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS; group will contain the new group object.
 *	PWR_RET_WARN_NO_CHILDREN - The specified object doesn't have children.
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
PWR_ObjGetChildren(PWR_Obj object, PWR_Grp *group)
{
	int retval = PWR_RET_FAILURE;
	obj_t *obj = NULL;
	group_t *grp = NULL;
	context_t *ctx = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(object);
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, group = %p", object, group);

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx)
		goto done;

	// Find the object
	obj = opaque_map_lookup_object(opaque_map, obj_key);
	if (!obj)
		goto done;

	if (!g_node_first_child(obj->gnode)) {
		retval = PWR_RET_WARN_NO_CHILDREN;
		goto done;
	}

	grp = context_new_group(ctx);
	if (!grp)
		goto done;

	g_node_children_foreach(obj->gnode, G_TRAVERSE_ALL,
			group_insert_callback, grp);

	*group = OPAQUE_GENERATE(ctx_key, grp->opaque.key);
	retval = PWR_RET_SUCCESS;

done:
	TRACE1_EXIT("retval = %d, *group = %p", retval, *group);

	return retval;
}

/*
 * PWR_ObjAttrIsValid - Determines if a specified attribute is valid for a
 *			specified object.
 *
 * Argument(s):
 *
 *	object - The object that the user is acting on
 *	attr   - The attribute the user wishes to confirm is valid for the
 *	         specified object.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
PWR_ObjAttrIsValid(PWR_Obj object, PWR_AttrName attr)
{
	int status = PWR_RET_SUCCESS;
	obj_t *obj = NULL;
	opaque_key_t object_key = OPAQUE_GET_DATA_KEY(object);

	TRACE1_ENTER("object = %p, attr = %d", object, attr);

	// Find the object for the opaque key.
	obj = opaque_map_lookup_object(opaque_map, object_key);
	if (!obj) {
		LOG_FAULT("Object not found for opaque reference %p", object);
		status = PWR_RET_BAD_VALUE;
		goto status_return;
	}

	status = validate_attr(obj, attr);

status_return:
	TRACE1_EXIT("status = %d", status);

	return status;
}

/*
 * PWR_ObjAttrGetMeta - Returns the requested metadata item for the specified
 *			object or object and attribute name pair.
 *
 * Argument(s):
 *
 *	obj   - The target object
 *	attr  - The target attribute.  If object-only metadata is being
 *	        requested, should be set to PWR_ATTR_NOT_SPECIFIED
 *	meta  - The target metadata item to get
 *	value - Pointer to the caller allocated storage to hold the value
 *	        of the requested metadata item.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS   - Upon SUCCESS
 *	PWR_RET_FAILURE   - Upon FAILURE
 *	PWR_RET_NO_ATTRIB - The attribute specified is not implemented
 *	                  - or valud for the object
 *	PWR_RET_NO_META   - The metadata specified is not implemented
 *	                    or valid for the object/attribute
 *
 * Metadata Implementation Details
 * -------------------------------
 *
 * Requests for metadata are made using an object or an
 * object/attribute pair as the query parameter. Each metadata can be
 * viewed as belonging to one of five different classes of data. The
 * classes of data are based upon how common the metadata is for an
 * given query parameter:
 *
 *	MD-C	Common metadata value for all objects and all attributes
 *	MD-CO	Common metadata value for all objects of same type
 *	MD-CA	Common metadata value for all attributes of same name
 *	MD-UO	Unique metadata value for objects
 *	MD-COA	Common metadata value for objects of same type and attributes
 *		of the same name
 *	MD-UOA	Unique metadata value for object and attribute
 *
 * Some examples:
 *
 * MD-C:
 *	PWR_MD_MAX_LEN is the maximum string length for any metadata
 *	string. The same response is returned for any given object or
 *	object/attribute pair.
 *
 *	Can be handled without having to call into the plugin code.
 *
 * MD-CO:
 *	PWR_MD_NAME is the name string for an object or attribute, such as
 *	"PWR_OBJ_NODE". The same value would be returned for any given
 *	object of type PWR_OBJ_NODE when the attribute specified is
 *	PWR_ATTR_NOT_SPECIFIED.
 *
 *	Can be handled without having to call into the plugin code.
 *
 * MD-CA:
 *	PWR_MD_NAME is the name string for an object or attribute, such
 *	as "PWR_ATTR_ENERGY". The same value would be returned for every
 *	PWR_ATTR_ENERGY attribute in any object that support the
 *	attribute. If the object does not support the attribute the query
 *	would fail with PWR_MD_NO_ATTRIB as the status.
 *
 *	Can be handled without having to call into the plugin code.
 *
 * MD-UO:
 *	PWR_MD_VENDOR_INFO is any available vendor information about
 *	an object or attribute. If PWR_ATTR_NOT_SPECIFIED is the attribute
 *	then this will be the information about the object and it can be
 *	unique to the object, such as a node cname or socket part number.
 *
 *	Requires a call into the plugin code.
 *
 * MD-COA:
 *	PWR_MD_NUM is the number of values in the set of possible values
 *	for an attribute with a discrete set of values. The value 0 is
 *	returned if there is not a descrete set of values for the attribute.
 *	If the object type is a PWR_OBJ_SOCKET and the attribute name is
 *	PWR_ATTR_POWER the value is always 0.
 *
 *	Requires a call into the plugin code.
 *
 * MD-UOA:
 *	PWR_MD_UPDATE_RATE is the update rate of an attribute value for
 *	a given object. This metadata value may be modifiable, and the
 *	is considered unique for every object/attribute pair, if
 *	supported for the object/attribute pair. If the metadata is not
 *	supported the query would fail with PWR_RET_NO_META.
 *
 *	Requires a call into the plugin code.
 *
 */

int
PWR_ObjAttrGetMeta(PWR_Obj obj, PWR_AttrName attr, PWR_MetaName meta,
		void *value)
{
	int status = PWR_RET_SUCCESS;
	obj_t *object = NULL;
	context_t *context = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(obj);
	opaque_key_t object_key = OPAQUE_GET_DATA_KEY(obj);

	TRACE1_ENTER("obj = %p, attr = %d, meta = %d, value = %p",
			obj, attr, meta, value);

	// Find the context and the object for the obj opaque value
	context = opaque_map_lookup_context(opaque_map, context_key);
	if (!context) {
		LOG_FAULT("Failed to find context");
		status = PWR_RET_BAD_VALUE;
		goto quick_return;
	}

	object = opaque_map_lookup_object(opaque_map, object_key);
	if (!object) {
		LOG_FAULT("Failed to find object");
		status = PWR_RET_BAD_VALUE;
		goto quick_return;
	}

	// Validate that the metadata is in the valid range.
	if (meta < 0 || meta >= PWR_NUM_META_NAMES) {
		LOG_FAULT("Metadata value, %d, out of range", meta);
		status = PWR_RET_OUT_OF_RANGE;
		goto quick_return;
	}

	// If attribute is set to PWR_ATTR_NOT_SPECIFIED, it indicates a
	// query of metadata for the object. Otherwise,the attribute must be in
	// the range (0 <= attribute < PWR_NUM_ATTR_NAMES) to be considered
	// valid.
	//
	// If the attribute is valid, it must be supported by the object
	// type to allow a metadata query of the object/attribute pair.
	//
	if (attr != PWR_ATTR_NOT_SPECIFIED) {
		status = validate_attr(object, attr);
		if (status) {
			LOG_FAULT("Attribute %d not supported by object type %d",
					attr, object->type);
			goto quick_return;
		}
	}

	// Check for MD-C metadata (common to all objects & attributes)
	// The PWR_MD_MAX_LEN is currently the only such metadata.
	if (meta == PWR_MD_MAX_LEN) {
		*(uint64_t *)value = CRAY_PWR_MAX_STRING_SIZE;
		goto quick_return;
	}

	// If no attribute was specified, request is for object metadata,
	// else it is for object/attribute metadata
	if (attr == PWR_ATTR_NOT_SPECIFIED)
		status = obj_get_meta(object, meta, value);
	else
		status = obj_attr_get_meta(object, attr, meta, value);

quick_return:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_ObjAttrSetMeta(PWR_Obj obj, PWR_AttrName attr, PWR_MetaName meta,
		const void *value)
{
	int status = PWR_RET_SUCCESS;
	obj_t *object = NULL;
	context_t *context = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(obj);
	opaque_key_t object_key = OPAQUE_GET_DATA_KEY(obj);

	TRACE1_ENTER("obj = %p, attr = %d, meta = %d, value = %p",
			obj, attr, meta, value);

	// Find the context and the object for the obj opaque value
	context = opaque_map_lookup_context(opaque_map, context_key);
	if (!context) {
		LOG_FAULT("Failed to find context");
		status = PWR_RET_BAD_VALUE;
		goto status_return;
	}

	object = opaque_map_lookup_object(opaque_map, object_key);
	if (!object) {
		LOG_FAULT("Failed to find object");
		status = PWR_RET_BAD_VALUE;
		goto status_return;
	}

	// Check for metadata out of valid range of values
	if (meta < 0 || meta >= PWR_NUM_META_NAMES) {
		LOG_FAULT("Metadata value, %d, out of range", meta);
		status = PWR_RET_OUT_OF_RANGE;
		goto status_return;
	}

	// Is the attribute valid for the object type?
	status = validate_attr(object, attr);
	if (status) {
		LOG_FAULT("Attribute %d not supported by object type %d",
				attr, object->type);
		goto status_return;
	}

	// Check for read only metadata
	if (meta < PWR_MD_UPDATE_RATE || meta > PWR_MD_TIME_WINDOW) {
		LOG_FAULT("Metadata value, %d, is read-only", meta);
		status = PWR_RET_READ_ONLY;
		goto status_return;
	}

	// Might be settable, pass it off to the plugin to try
	status = obj_attr_set_meta_plugin(object, context->ipc, attr, meta,
			value);

status_return:
	TRACE1_EXIT("status = %d", status);

	return status;
}

/*
 * PWR_MetaValueAtIndex - Allows the available values for a given attribute to
 *			  be enumerated. It is assumed that the set of valid
 *			  values is static and has size equal to the value
 *			  returned by the PWR_MD_NUM metadata item. Once the
 *			  value of PWR_MD_NUM is known, PWR_MetaValueAtIndex()
 *			  can be called repeatedly with index from 0 to
 *			  PWR_MD_NUM - 1 to retrieve the list of valid values
 *			  for the target attribute. Each call will return the
 *			  value at the specified index as well as a human-
 *			  readable string representing the value in human
 *			  readable format.
 *
 * Argument(s):
 *
 *	obj       - The target object
 *	attr      - The target attribute.
 *	index     - The index of the metadata item value to lookup.
 *	value     - Pointer to the caller allocated storage to hold the value
 *		    of the requested metadata item.  Can be NULL if not needed.
 *	value_str - Pointer to the caller allocated storage to hold the human-
 *		    readable C-style NULL-terminated ASCII string representing
 *		    the metadata item value.  Can be NULL if not needed.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS   - Upon SUCCESS
 *	PWR_RET_FAILURE   - Upon FAILURE
 *	PWR_RET_NO_ATTRIB - The attribute specified is not implemented
 *	PWR_RET_NO_META   - The metadata specified is not implemented
 *	PWR_RET_BAD_INDEX - The index specified is not valid
 */
int
PWR_MetaValueAtIndex(PWR_Obj obj, PWR_AttrName attr, unsigned int index,
		void *value, char *value_str)
{
	int status = PWR_RET_SUCCESS;
	obj_t *object = NULL;
	context_t *context = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(obj);
	opaque_key_t object_key = OPAQUE_GET_DATA_KEY(obj);

	TRACE1_ENTER("obj = %p, attr = %d, index = %u, value = %p, value_str = %p",
			obj, attr, index, value, value_str);

	// Find the context and the object for the obj opaque value
	context = opaque_map_lookup_context(opaque_map, context_key);
	if (!context) {
		LOG_FAULT("Failed to find context");
		status = PWR_RET_BAD_VALUE;
		goto status_return;
	}

	object = opaque_map_lookup_object(opaque_map, object_key);
	if (!object) {
		LOG_FAULT("Failed to find object");
		status = PWR_RET_BAD_VALUE;
		goto status_return;
	}

	// Is the attribute valid for the object type?
	status = validate_attr(object, attr);
	if (status) {
		LOG_FAULT("Attribute %d not supported by object type %d",
				attr, object->type);
		goto status_return;
	}

	switch (object->type) {
	case PWR_OBJ_NODE:
		status = to_node(object)->ops->get_meta_at_index(
				to_node(object), attr, index, value,
				value_str);
		break;
	case PWR_OBJ_SOCKET:
		status = to_socket(object)->ops->get_meta_at_index(
				to_socket(object), attr, index, value,
				value_str);
		break;
	case PWR_OBJ_CORE:
		status = to_core(object)->ops->get_meta_at_index(
				to_core(object), attr, index, value,
				value_str);
		break;
	case PWR_OBJ_POWER_PLANE:
		status = to_pplane(object)->ops->get_meta_at_index(
				to_pplane(object), attr, index, value,
				value_str);
		break;
	case PWR_OBJ_MEM:
		status = to_mem(object)->ops->get_meta_at_index(to_mem(object),
				attr, index, value, value_str);
		break;
	case PWR_OBJ_HT:
		status = to_ht(object)->ops->get_meta_at_index(to_ht(object),
				attr, index, value, value_str);
		break;
	default:
		LOG_FAULT("Invalid PWR_Obj type %u", object->type);
		status = PWR_RET_FAILURE;
		break;
	}

status_return:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
CRAYPWR_AttrGetCount(PWR_ObjType obj, size_t *value)
{
	int retval = PWR_RET_SUCCESS;

	// Outside the range of supported values
	if (obj >= PWR_NUM_OBJ_TYPES) {
		retval = PWR_RET_OUT_OF_RANGE;
	}

	// An invalid or unspecified object type means
	// they want all supported attributes.
	else if (obj < 0) {
		*value = BITMASK_NUM_BITS_SET(&supported_attr_bitmask);
	}

	// Want count of attributes for a specific object type
	else {
		switch (obj) {
		case PWR_OBJ_NODE:
			*value = BITMASK_NUM_BITS_SET(&node_attr_bitmask);
			break;
		case PWR_OBJ_SOCKET:
			*value = BITMASK_NUM_BITS_SET(&socket_attr_bitmask);
			break;
		case PWR_OBJ_CORE:
			*value = BITMASK_NUM_BITS_SET(&core_attr_bitmask);
			break;
		case PWR_OBJ_POWER_PLANE:
			*value = BITMASK_NUM_BITS_SET(&pplane_attr_bitmask);
			break;
		case PWR_OBJ_MEM:
			*value = BITMASK_NUM_BITS_SET(&mem_attr_bitmask);
			break;
		case PWR_OBJ_HT:
			*value = BITMASK_NUM_BITS_SET(&ht_attr_bitmask);
			break;
		default:
			retval = PWR_RET_NO_ATTRIB;
			break;
		}
	}

	return retval;
}

int
CRAYPWR_AttrGetList(PWR_ObjType obj, size_t len, const char **str_list,
		int *val_list)
{
	int retval = PWR_RET_SUCCESS;

	if (obj >= PWR_NUM_OBJ_TYPES) {
		// Object type outside range of supported types
		retval = PWR_RET_OUT_OF_RANGE;
	} else if (obj < 0) {
		// Request to get list of all supported attributes.
		retval = get_attr_list(&supported_attr_bitmask, len,
				str_list, val_list);
	} else {
		// Request to get list of supported attributes for a
		// specified object type.
		switch (obj) {
		case PWR_OBJ_NODE:
			retval = get_attr_list(&node_attr_bitmask, len,
					str_list, val_list);
			break;
		case PWR_OBJ_SOCKET:
			retval = get_attr_list(&socket_attr_bitmask, len,
					str_list, val_list);
			break;
		case PWR_OBJ_CORE:
			retval = get_attr_list(&core_attr_bitmask, len,
					str_list, val_list);
			break;
		case PWR_OBJ_POWER_PLANE:
			retval = get_attr_list(&pplane_attr_bitmask, len,
					str_list, val_list);
			break;
		case PWR_OBJ_MEM:
			retval = get_attr_list(&mem_attr_bitmask, len,
					str_list, val_list);
			break;
		case PWR_OBJ_HT:
			retval = get_attr_list(&ht_attr_bitmask, len,
					str_list, val_list);
			break;
		default:
			retval = PWR_RET_NO_ATTRIB;
			break;
		}
	}

	return retval;
}

/**
 * Get the text string associated with an attribute number.
 *
 * @param attr - attribute desired
 * @param buf - buffer to contain string
 * @param max - size of buffer
 *
 * @return int - error code:
 *      PWR_RET_SUCCESS - string returned
 *      PWR_RET_FAILURE - buf is NULL, or max == 0, or attr out of range
 */
int
CRAYPWR_AttrGetName(PWR_AttrName attr, char *buf, size_t max)
{
	int retval = PWR_RET_FAILURE;

	TRACE3_ENTER("attr = %d, buf = %p, max = %ld", attr, buf, max);

	if (buf && max && attr >= 0 && attr < PWR_NUM_ATTR_NAMES) {
		snprintf(buf, max, "%s", attr_name[attr]);
		retval = PWR_RET_SUCCESS;
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Get the attribute value associated with an attribute name.
 *
 * This does a simple linear search through the list of attribute names, and
 * reports the index of the first match. It's a short list, and this is not a
 * commonly-used function.
 *
 * @param attrname - text string containing the attribute name
 *
 * @return PWR_AttrName - PWR_ATTR_* number, or PWR_ATTR_INVALID
 */
PWR_AttrName
CRAYPWR_AttrGetEnum(const char *attrname)
{
	PWR_AttrName retval = PWR_ATTR_INVALID;
	int index;

	TRACE3_ENTER("attname = %s", attrname);

	for (index = 0; index < PWR_NUM_ATTR_NAMES; index++) {
		if (!strcmp(attrname, attr_name[index])) {
			retval = index;
			break;
		}
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}
