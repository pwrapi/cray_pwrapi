//
// Copyright (c) 2015-2017, 2018, Cray Inc.
//  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include <common.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "api.h"
#include "io.h"
#include "pwrcmd.h"

typedef union {
	double type_double;
	uint64_t type_uint64;
} type_union_t;

static int API_major_version = -1;      // major version of HPC Power API lib
static int API_minor_version = -1;      // minor version of HPC Power API lib

static PWR_Cntxt ctx;                   // our context
static PWR_Obj obj;                     // our location
static char obj_name[PWR_MAX_STRING_LEN]; // string representation of object name

char *md_str = NULL;                    // buffer for all metadata operations

/**
 * The following enum_map_t structures are used to parse arguments given on the
 * command-line.
 */
enum_map_t role_enum[PWR_NUM_ROLES] = {
	[PWR_ROLE_APP] =		{ "PWR_ROLE_APP",		1 },
	[PWR_ROLE_MC] =			{ "PWR_ROLE_MC",		0 },
	[PWR_ROLE_OS] =			{ "PWR_ROLE_OS",		0 },
	[PWR_ROLE_USER] =		{ "PWR_ROLE_USER",		0 },
	[PWR_ROLE_RM] =			{ "PWR_ROLE_RM",		1 },
	[PWR_ROLE_ADMIN] =		{ "PWR_ROLE_ADMIN",		0 },
	[PWR_ROLE_MGR] =		{ "PWR_ROLE_MGR",		0 },
	[PWR_ROLE_ACC] =		{ "PWR_ROLE_ACC",		0 }
};

enum_map_t meta_enum[PWR_NUM_META_NAMES] = {
	[PWR_MD_NUM] =			{ "PWR_MD_NUM",			1 },
	[PWR_MD_MIN] =			{ "PWR_MD_MIN",			1 },
	[PWR_MD_MAX] =			{ "PWR_MD_MAX",			1 },
	[PWR_MD_PRECISION] =		{ "PWR_MD_PRECISION",		1 },
	[PWR_MD_ACCURACY] =		{ "PWR_MD_ACCURACY",		1 },
	[PWR_MD_UPDATE_RATE] =		{ "PWR_MD_UPDATE_RATE",		1 },
	[PWR_MD_SAMPLE_RATE] =		{ "PWR_MD_SAMPLE_RATE",		1 },
	[PWR_MD_TIME_WINDOW] =		{ "PWR_MD_TIME_WINDOW",		1 },
	[PWR_MD_TS_LATENCY] =		{ "PWR_MD_TS_LATENCY",		1 },
	[PWR_MD_TS_ACCURACY] =		{ "PWR_MD_TS_ACCURACY",		1 },
	[PWR_MD_MAX_LEN] =		{ "PWR_MD_MAX_LEN",		1 },
	[PWR_MD_NAME_LEN] =		{ "PWR_MD_NAME_LEN",		1 },
	[PWR_MD_NAME] =			{ "PWR_MD_NAME",		1 },
	[PWR_MD_DESC_LEN] =		{ "PWR_MD_DESC_LEN",		1 },
	[PWR_MD_DESC] =			{ "PWR_MD_DESC",		1 },
	[PWR_MD_VALUE_LEN] =		{ "PWR_MD_VALUE_LEN",		1 },
	[PWR_MD_VENDOR_INFO_LEN] =	{ "PWR_MD_VENDOR_INFO_LEN",	1 },
	[PWR_MD_VENDOR_INFO] =		{ "PWR_MD_VENDOR_INFO",		1 },
	[PWR_MD_MEASURE_METHOD] =	{ "PWR_MD_MEASURE_METHOD",	1 }
};

/**
 * Reset all of the command options, for use in interactive mode.
 *
 * @param cmd_opt - structure to reset
 */
void
reset_cmd_opt(cmd_opt_t *cmd_opt)
{
	cmd_opt_t initval = INIT_CMD_OPT();

	TRACE3_ENTER("cmd_opt = %p", cmd_opt);

	free(cmd_opt->name_str);
	free(cmd_opt->attr_str);
	free(cmd_opt->val_str);
	g_slist_free_full(cmd_opt->names, free);
	g_slist_free_full(cmd_opt->attrs, free);
	g_slist_free_full(cmd_opt->values, free);
	*cmd_opt = initval;

	TRACE3_EXIT("");
}

/**
 * Render a timestamp value using the JSON tools.
 *
 * @param base - JSON object or array pointer, or NULL to use the base object
 * @param name - name to use, or NULL if base is an array pointer
 * @param ts - timestamp to represent
 *
 * @return bool - true if successful, false otherwise
 */
static bool
json_render_timestamp(void *base, const char *name, PWR_Time ts)
{
	time_t s = ts / NSEC_PER_SEC;
	int ns   = ts % NSEC_PER_SEC;
	struct tm tmr;
	char buf[CRAY_PWR_MAX_STRING_SIZE];
	bool retval = true;

	TRACE2_ENTER("base = %p, name = %s, ts = %ld", base, name, ts);

	if (ts != 0) {
		if (!localtime_r(&s, &tmr)) {
			retval = false;
			goto error_handler;
		}
		snprintf(buf, sizeof(buf),
				"%04d-%02d-%02d %02d:%02d:%02d.%09d",
				1900 + tmr.tm_year,
				1 + tmr.tm_mon,
				tmr.tm_mday,
				tmr.tm_hour,
				tmr.tm_min,
				tmr.tm_sec,
				ns);
	} else {
		snprintf(buf, sizeof(buf), "PWR_TIME_UNKNOWN");
	}
	retval = json_add_string(base, name, buf);

error_handler:
	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Render a metadta indexed value using the JSON tools.
 *
 * Each query can return a different type of data, but always returns
 * that value in string format. The string representation is what
 * will be displayed.
 *
 * @param base - JSON object or array pointer, or NULL to use the base object
 * @param name - name to use, or NULL if base is an array pointer
 * @param attr - attribute enum
 * @param val - value to render
 *
 * @return bool - true if successful, false otherwise
 */
static bool
json_render_meta_index_value(void *base, const char *name, PWR_AttrName attr,
		char *val_str)
{
	int retval = true;

	TRACE2_ENTER("base = %p, name = %s, attr = %d, val_str = %s",
			base, name, attr, val_str);

	retval = json_add_string(base, name, val_str);

	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Render an attribute value using the JSON tools.
 *
 * Each attribute can return a different type of data, so we have to use the
 * operation type to disambiguate the correct format of the value.
 *
 * @param base - JSON object or array pointer, or NULL to use the base object
 * @param name - name to use, or NULL if base is an array pointer
 * @param attr - attribute enum
 * @param val - value to render
 *
 * @return bool - true if successful, false otherwise
 */
static bool
json_render_attr_value(void *base, const char *name, PWR_AttrName attr, cmd_val_t *val)
{
	int retval = true;

	TRACE2_ENTER("base = %p, name = %s, attr = %d, val = %p", base, name, attr, val);

	switch (attr) {
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_CSTATE_LIMIT:
	case PWR_ATTR_THROTTLED_TIME:
		retval = json_add_integer(base, name, val->whole);
		break;
	case PWR_ATTR_FREQ:
	case PWR_ATTR_FREQ_REQ:
	case PWR_ATTR_FREQ_LIMIT_MIN:
	case PWR_ATTR_FREQ_LIMIT_MAX:
	case PWR_ATTR_POWER:
	case PWR_ATTR_POWER_LIMIT_MAX:
	case PWR_ATTR_ENERGY:
	case PWR_ATTR_TEMP:
		retval = json_add_double(base, name, val->real);
		break;
	case PWR_ATTR_GOV:
		retval = json_add_string(base, name,
				pwr_gov_to_string(val->whole));
		break;
	default:
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unhandled attribute type: %d",
				attr);
		retval = false;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Render metadata using the JSON tools.
 *
 * Each metadata type can return a different type of data, so we have to use the
 * operation type to disambiguate the correct format of the value.
 *
 * @param base - JSON object or array pointer, or NULL to use the base object
 * @param name - name to use, or NULL if base is an array pointer
 * @param meta - metadata enum
 * @param attr - attribute enum
 * @param val - value to render
 *
 * @return bool - true if successful, false otherwise
 */
static bool
json_render_meta_value(void *base, const char *name, PWR_MetaName meta, PWR_AttrName attr, cmd_val_t *val)
{
	int retval = true;

	TRACE2_ENTER("base = %p, name = %s, meta = %d, attr = %d, val = %p",
			base, name, meta, attr, val);

	//
	// Make things easy because of all the different value types.  Just
	// convert the value into a string.
	//
	switch (meta) {

	// Returning uint64_t values
	case PWR_MD_NUM:
	case PWR_MD_PRECISION:
	case PWR_MD_MAX_LEN:
	case PWR_MD_NAME_LEN:
	case PWR_MD_DESC_LEN:
	case PWR_MD_VALUE_LEN:
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_MEASURE_METHOD:
		retval = json_add_integer(base, name, val->whole);
		break;

	// Returning double values
	case PWR_MD_ACCURACY:
	case PWR_MD_UPDATE_RATE:
	case PWR_MD_SAMPLE_RATE:
		retval = json_add_double(base, name, val->real);
		break;

	// Returning char * values
	case PWR_MD_NAME:
	case PWR_MD_DESC:
	case PWR_MD_VENDOR_INFO:
		retval = json_add_string(base, name, val->str);
		break;

	// Returning PWR_Time values
	case PWR_MD_TIME_WINDOW:
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
		retval = json_add_integer(base, name, val->time);
		break;

	// Returning attribute dependent values
	case PWR_MD_MIN:
	case PWR_MD_MAX:
		retval = json_render_attr_value(base, name, attr, val);
		break;

	default:
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unhandled metadata type: %d",
				meta);
		retval = false;
		break;
	}

	TRACE2_EXIT("retval = %d", retval);
	return retval;
}

//
// get_meta_enum - Convert a metadata string to an metadata enum value
//
// Argument(s):
//
//	meta_str - The metadata string to convert
//
// Return Code(s):
//
//	int - the metadata name enum value
//
int
get_meta_enum(char *meta_str)
{
	PWR_MetaName meta = 0;

	TRACE2_ENTER("meta_str = %s", meta_str);

	if (meta_str == NULL) {
		PRINT_ERRCODE(PWR_RET_NO_ATTRIB,
				"Must specify a metadata to operate on");
		meta = PWR_RET_NO_ATTRIB;
		goto error_handler;
	}

	//
	// Iterate through all of the metadata looking for a match.
	//
	for (meta = 0; meta < PWR_NUM_META_NAMES; meta++) {
		//
		// Does it match the users argument?
		//
		if (!strcmp(meta_str, meta_enum[meta].name)) {
			break;
		}
	}

	//
	// Did we find it?
	//
	if (meta == PWR_NUM_META_NAMES) {
		PRINT_ERRCODE(PWR_RET_NO_ATTRIB,
				"Unknown metadata: %s", meta_str);
		meta = PWR_RET_NO_ATTRIB;
		goto error_handler;
	}

	//
	// Is the metadata supported?
	//
	if (!meta_enum[meta].supported) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unsupported metadata: %s", meta_str);
		meta = PWR_RET_FAILURE;
		goto error_handler;
	}

error_handler:
	TRACE2_EXIT("meta = %d", meta);
	return meta;
}

//
// get_role_enum - Convert an role string to an role enum value
//
// Argument(s):
//
//	role_str - The role string to convert
//
// Return Code(s):
//
//	PWR_Role - the role enum value
//
PWR_Role
get_role_enum(char *role_str)
{
	PWR_Role role = 0;

	TRACE2_ENTER("role_str = '%s'", role_str);

	if (role_str == NULL) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Must specify a role to operate on");
		role = PWR_RET_FAILURE;
		goto error_handler;
	}

	//
	// Iterate through all of the attributes looking for a match.
	//
	for (role = 0; role < PWR_NUM_ROLES; role++) {
		//
		// Does it match the users argument?
		//
		if (!strcmp(role_str, role_enum[role].name)) {
			break;
		}
	}

	//
	// Did we find it?
	//
	if (role == PWR_NUM_ROLES) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unknown role: %s", role_str);
		role = PWR_RET_FAILURE;
		goto error_handler;
	}

	//
	// Is the role supported?
	//
	if (!role_enum[role].supported) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unsupported role: %s", role_str);
		role = PWR_RET_FAILURE;
		goto error_handler;
	}

error_handler:
	TRACE2_EXIT("role = %d", role);

	return role;
}

/**
 * Parse the command-line attribute value and convert to the correct type. This
 * returns the type that the string was converted to.
 *
 * The conversion is based on the attribute specified.
 *
 * @param cmd_opt - command options and results
 *
 * @return val_type_t - type of the value, or val_is_invalid
 */
static val_type_t
cmd_parse_attr_val(PWR_AttrName attr, const char *val_str, cmd_val_t *val)
{
	val_type_t val_type = val_is_invalid;
	char *endptr = NULL;

	TRACE2_ENTER("attr = %d, val_str = %s, val = %p",
			attr, val_str, val);

	// An empty string is always invalid
	if (!val_str) {
		PRINT_ERRCODE(PWR_RET_BAD_VALUE,
				"Value parameter required for set command");
		goto error_handler;
	}

	// Conversion depends on the attribute selected
	switch (attr) {

	// Convert string to uint64_t
	case PWR_ATTR_PSTATE:
	case PWR_ATTR_CSTATE:
	case PWR_ATTR_CSTATE_LIMIT:
	case PWR_ATTR_SSTATE:
	case PWR_ATTR_OS_ID:
	case PWR_ATTR_THROTTLED_TIME:
	case PWR_ATTR_THROTTLED_COUNT:
		{
			uint64_t whole = strtoul(val_str, &endptr, 10);

			if (whole == LLONG_MIN || whole == LLONG_MAX) {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d causes "
						"overflow",
						val_str, attr);
			} else if (*endptr != '\0') {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d contains "
						"invalid characters",
						val_str, attr);
			} else {
				val->whole = whole;
				val_type = val_is_whole;
			}
			break;
		}

	// Conver string to double
	case PWR_ATTR_CURRENT:
	case PWR_ATTR_VOLTAGE:
	case PWR_ATTR_POWER:
	case PWR_ATTR_POWER_LIMIT_MIN:
	case PWR_ATTR_POWER_LIMIT_MAX:
	case PWR_ATTR_FREQ:
	case PWR_ATTR_FREQ_REQ:
	case PWR_ATTR_FREQ_LIMIT_MIN:
	case PWR_ATTR_FREQ_LIMIT_MAX:
	case PWR_ATTR_ENERGY:
	case PWR_ATTR_TEMP:
		{
			double real = strtod(val_str, &endptr);

			if (real == HUGE_VAL || real == -HUGE_VAL) {
			//
			// Continue on error.
			//
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d causes "
						"overflow",
						val_str, attr);
			} else if (*endptr != '\0') {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d contains "
						"invalid characters",
						val_str, attr);
			} else {
				val->real = real;
				val_type = val_is_real;
			}
			break;
		}

	// Convert string to matching enum
	case PWR_ATTR_GOV:
		{
			PWR_AttrGov gov = pwr_string_to_gov(val_str);

			if (gov == PWR_GOV_INVALID) {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d is invalid",
						val_str, attr);
			} else {
				val->whole = gov;
				val_type = val_is_whole;
			}
			break;
		}

	// Unhandled attribute type
	default:
		PRINT_ERRCODE(PWR_RET_BAD_VALUE,
				"Failed to parse value '%s', attribute %d not handled",
				val_str, attr);
		break;
	}

error_handler:
	TRACE2_EXIT("val_type = %d", val_type);
	return val_type;
}

/**
 * Parse the command-line metadata value and convert to the correct type. This
 * returns the type that the string was converted to.
 *
 * The conversion is based on the attribute specified.
 *
 * @param cmd_opt - command options and results
 *
 * @return val_type_t - type of the value, or val_is_invalid
 */
static val_type_t
cmd_parse_meta_val(PWR_MetaName meta, PWR_AttrName attr, const char *val_str,
		cmd_val_t *val)
{
	val_type_t val_type = val_is_invalid;
	char *endptr = NULL;

	TRACE2_ENTER("attr = %d, val_str = %s, val = %p",
			attr, val_str, val);

	// An empty string is always invalid
	if (!val_str) {
		PRINT_ERRCODE(PWR_RET_BAD_VALUE,
				"Value parameter required for set command");
		goto error_handler;
	}

	switch (meta) {

	// Conversion of string based on attribute type
	case PWR_MD_MIN:
	case PWR_MD_MAX:
		val_type = cmd_parse_attr_val(attr, val_str, val);
		break;

	// Keep as string
	case PWR_MD_NAME:
	case PWR_MD_DESC:
	case PWR_MD_VENDOR_INFO:
		val_type = val_is_string;
		strncpy(val->str, val_str, sizeof(val->str));
		break;

	// Conversion string number to uint64_t
	case PWR_MD_NUM:
	case PWR_MD_PRECISION:
	case PWR_MD_MAX_LEN:
	case PWR_MD_NAME_LEN:
	case PWR_MD_DESC_LEN:
	case PWR_MD_VALUE_LEN:
	case PWR_MD_VENDOR_INFO_LEN:
	case PWR_MD_MEASURE_METHOD:
		{
			uint64_t whole = strtoul(val_str, &endptr, 10);

			if (whole == LLONG_MIN || whole == LLONG_MAX) {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d causes "
						"overflow",
						val_str, attr);
			} else if (*endptr != '\0') {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d contains "
						"invalid characters",
						val_str, attr);
			} else {
				val->whole = whole;
				val_type = val_is_whole;
			}
			break;
		}

	// Conversion string number to double
	case PWR_MD_ACCURACY:
	case PWR_MD_UPDATE_RATE:
	case PWR_MD_SAMPLE_RATE:
		{
			double real = strtod(val_str, &endptr);

			if (real == HUGE_VAL || real == -HUGE_VAL) {
			//
			// Continue on error.
			//
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d causes "
						"overflow",
						val_str, attr);
			} else if (*endptr != '\0') {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d contains "
						"invalid characters",
						val_str, attr);
			} else {
				val->real = real;
				val_type = val_is_real;
			}
			break;
		}

	// Conversion string number to time (ns)
	case PWR_MD_TIME_WINDOW:
	case PWR_MD_TS_LATENCY:
	case PWR_MD_TS_ACCURACY:
		{
			PWR_Time time = strtoul(val_str, &endptr, 10);

			if (time == LLONG_MIN || time == LLONG_MAX) {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d causes "
						"overflow",
						val_str, attr);
			} else if (*endptr != '\0') {
				PRINT_ERRCODE(PWR_RET_BAD_VALUE,
						"Requested value '%s' for %d contains "
						"invalid characters",
						val_str, attr);
			} else {
				val->time = time;
				val_type = val_is_time;
			}
			break;
		}

	// Unhandled metadata type
	default:
		PRINT_ERRCODE(PWR_RET_BAD_VALUE,
				"Failed to parse value '%s', metadata %d not handled",
				val_str, meta);
		break;
	}

error_handler:
	TRACE2_EXIT("val_type = %d", val_type);
	return val_type;
}

/**
 * Create a group containing the objects named in the cmd_opt.
 *
 * This will skip over objects that cannot be found, though it will generate
 * error messages for each.
 *
 * @param cmd_opt - command option structure
 * @param grp - pointer to the return value for the group
 *
 * @return int - count of objects in the group
 */
static int
create_group(cmd_opt_t *cmd_opt, PWR_Grp *grp)
{
	GSList *item;
	int count = 0;

	TRACE2_ENTER("cmd_opt = %p, grp = %p", cmd_opt, grp);

	// Create a new group
	cmd_opt->retcode = PWR_GrpCreate(ctx, grp);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpCreate() failed");
		count = -1;
		goto error_handler;
	}

	// Add the objects to the group, and count the ones found
	for (item = cmd_opt->names; item; item = g_slist_next(item)) {
		char *name = (char *)item->data;
		PWR_Obj obj;
		int errcode;

		// Allow for empty names - ignore them
		if (!name || !*name) {
			continue;
		}

		// Get the object by name
		errcode = PWR_CntxtGetObjByName(ctx, name, &obj);
		if (errcode != PWR_RET_SUCCESS) {
			// Subtlety: do NOT set the JSON return value here
			PRINT_ERR("PWR_CntxtGetObjByName(%s) failed", name);
			// Ignore this failure, keep going - can skip bad objects
			continue;
		}

		// Add the object to the group
		cmd_opt->retcode = PWR_GrpAddObj(*grp, obj);
		if (cmd_opt->retcode != PWR_RET_SUCCESS) {
			// The group itself is corrupted - fail
			PRINT_ERRCODE(cmd_opt->retcode,
					"PWR_GrpAddObj() failed");
			count = -1;
			goto error_handler;
		}

		// This is now in our group
		count++;
	}

error_handler:
	TRACE2_EXIT("count = %d", count);

	return count;
}

/**
 * Create and fill an attributes array.
 *
 * This will skip over invalid attributes, though it will generate error
 * messages for each.
 *
 * @param cmd_opt - command option structure
 * @param attrp - return pointer for attribute array
 *
 * @return int - total count of items in the attribute array
 */
static int
create_attrs(cmd_opt_t *cmd_opt, PWR_AttrName **attrp)
{
	GSList *aitem = NULL;
	int count = 0;

	// We need a value here
	if (cmd_opt->attrs_cnt <= 0) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "no attrs supplied");
		goto error_handler;
	}
	// Allocate space for the attributes
	(*attrp) = calloc(cmd_opt->attrs_cnt, sizeof(PWR_AttrName));
	if (!(*attrp)) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "calloc() failed");
		goto error_handler;
	}
	// Fill the attribute array
	for (aitem = cmd_opt->attrs; aitem; aitem = g_slist_next(aitem)) {
		char *attr_str = (char *)aitem->data;
		PWR_AttrName *attr = &(*attrp)[count];

		// Should never happen
		if (count >= cmd_opt->attrs_cnt) {
			// Subtlety: do NOT set the JSON return value here
			PRINT_ERR("attrs list truncated");
			break;
		}
		// Allow for NULL entries
		if (!attr_str || !*attr_str) {
			continue;
		}
		// Acquire the attribute enum
		*attr = CRAYPWR_AttrGetEnum(attr_str);
		if (*attr == PWR_ATTR_INVALID) {
			// Subtlety: do NOT set the JSON return value here
			PRINT_ERR("CRAYPWR_AttrGetEnum(%s) failed", attr_str);
			// Ignore this failure, keep going - can skip bad objects
			continue;
		}
		// We can keep this one
		count++;
	}
error_handler:
	TRACE2_EXIT("count = %d", count);

	return count;
}

/**
 * Create and fill attributes and values in separate arrays.
 *
 * The callers guarantee that cmd_opt->attrs_cnt == cmd_opt->values_cnt, and
 * we've selected attrs_cnt as our counter limit.
 *
 * We require that each attr be valid, and that the parsed value is also valid.
 * If either is invalid, we skip that pair.
 *
 * @param cmd_opt - command option structure
 * @param attrp - return pointer for attribute array
 * @param valuep = return pointer for values array
 *
 * @return int - total count of items in the attribute array
 */
static int
create_values(cmd_opt_t *cmd_opt, PWR_AttrName **attrp, void **valuep)
{
	GSList *aitem = NULL;
	GSList *vitem = NULL;
	int count = 0;

	// Allocate space for the attributes
	(*attrp) = calloc(cmd_opt->attrs_cnt, sizeof(PWR_AttrName));
	(*valuep) = calloc(cmd_opt->attrs_cnt, 8);
	if (!(*attrp) || !(*valuep)) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "calloc() failed");
		goto error_handler;
	}
	// Fill the attrs and values arrays
	for (aitem = cmd_opt->attrs, vitem = cmd_opt->values;
			aitem && vitem;
			aitem = g_slist_next(aitem), vitem = g_slist_next(vitem)) {
		char *attr_str = (char *)aitem->data;
		char *value_str = (char *)vitem->data;
		PWR_AttrName *attr = &(*attrp)[count];
		cmd_val_t *val = (*valuep) + count * 8;

		// Should never happen
		if (count >= cmd_opt->attrs_cnt) {
			PRINT_ERR("values list truncated");
			break;
		}
		// Allow for NULL entries
		if (!attr_str || !*attr_str || !value_str || !*value_str) {
			continue;
		}
		// Acquire the attribute enum
		*attr = CRAYPWR_AttrGetEnum(attr_str);
		if (*attr == PWR_ATTR_INVALID) {
			// Subtlety: do NOT set the JSON return value here
			PRINT_ERR("CRAYPWR_AttrGetEnum(%s) failed", attr_str);
			// Ignore this failure, keep going - can skip bad objects
			continue;
		}
		switch (cmd_parse_attr_val(*attr, value_str, val)) {
		case val_is_invalid:
		case val_is_string:
			PRINT_ERR("Invalid attribute type");
			continue;
		default:
			break;
		}
		// We can keep this pair
		count++;
	}
error_handler:
	TRACE2_EXIT("count = %d", count);

	return count;
}

/**
 * Render a PWR_Status structure in JSON.
 *
 * This consumes the status by popping off all the errors and rendering them in
 * JSON. It returns the number of errors popped.
 *
 * It is safe to pass a NULL pointer, or an empty status object.
 *
 * @param status - PWR_Status opaque handle, or NULL
 *
 * @return int - number of status entries popped
 */
static int
json_render_status(PWR_Status status)
{
	cson_array *list = NULL;
	int count = 0;

	TRACE2_ENTER("status = %p", status);

	while (1) {
		PWR_AttrAccessError error;
		cson_object *obj;
		char buf[PWR_MAX_STRING_LEN];

		// Terminate as soon as we pop them all
		if (PWR_StatusPopError(status, &error))
			break;
		// Lazy array creation, we need it now
		if (!list)
			list = json_add_array(NULL, "status");
		// Add a JSON object to contain the error
		obj = json_add_object(list, NULL);
		// Render the object name
		if (PWR_ObjGetName(error.obj, buf, sizeof(buf)) != PWR_RET_SUCCESS)
			snprintf(buf, sizeof(buf), "PWR_OBJ_INVALID");
		json_add_string(obj, "object", buf);
		// Render the attribute name
		if (CRAYPWR_AttrGetName(error.name, buf, sizeof(buf)) != PWR_RET_SUCCESS)
			snprintf(buf, sizeof(buf), "PWR_ATTR_INVALID");
		json_add_string(obj, "attr", buf);
		// Render the index value
		json_add_integer(obj, "index", error.index);
		// Render the error code
		json_add_integer(obj, "error", error.error);
		// Add to the count
		count++;
	}
	// If there was no status, create an empty placeholder
	if (!count) {
		json_add_null(NULL, "status");
	}

	TRACE2_EXIT("count = %d", count);
	return count;
}

//
// cmd_get_attr - Get the current value for the requested attribute.
//
// Argument(s):
//
//	cmd_opt - The command options and results
//
// Return Code(s):
//
//	void
//
void
cmd_get_attr(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Time ts = PWR_TIME_UNKNOWN;
	cson_array *list = NULL;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->names_cnt != 1 ||
			cmd_opt->attrs_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"name count(%d) or attr count(%d) != 1",
				cmd_opt->names_cnt, cmd_opt->attrs_cnt);
		goto error_handler;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto error_handler;
	}

	// Read the attribute.
	memset(&cmd_opt->val, 0, sizeof(cmd_opt->val));
	cmd_opt->retcode = PWR_ObjAttrGetValue(
			obj, cmd_opt->attr, (void *)&cmd_opt->val, &ts);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjAttrGetValue(%d) failed",
				cmd_opt->attr);
		goto error_handler;
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_ObjAttrGetValue");
	list = json_add_array(NULL, "attr_vals");
	json_render_attr_value(list, NULL, cmd_opt->attr, &cmd_opt->val);
	list = json_add_array(NULL, "timestamps");
	json_render_timestamp(list, NULL, ts);
	json_render_status(NULL);

error_handler:
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_ObjAttrGetValues()
 *
 * @param cmd_opt - command option structure
 */
void
cmd_get_attrs(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Status status = NULL;
	PWR_AttrName *attrp = NULL;
	void *valuep = NULL;
	PWR_Time *tsp = NULL;
	cson_array *list = NULL;
	int attrcnt = 0;
	int i;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->names_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"name count(%d) != 1",
				cmd_opt->names_cnt);
		goto error_handler;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto error_handler;
	}

	// Build the attribute list
	attrcnt = create_attrs(cmd_opt, &attrp);
	if (attrcnt <= 0) {
		goto error_handler;
	}

	// Allocate space for the call arrays
	valuep = calloc(cmd_opt->attrs_cnt, 8);
	tsp = calloc(cmd_opt->attrs_cnt, sizeof(PWR_Time));
	if (!valuep || !tsp) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "Out of memory");
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Read the attributes of the object
	cmd_opt->retcode = PWR_ObjAttrGetValues(obj, attrcnt, attrp, valuep, tsp, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjAttrGetValues() failed");
		// continue -- there may be some successes
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_ObjAttrGetValues");
	list = json_add_array(NULL, "attr_vals");
	for (i = 0; i < attrcnt; i++) {
		cmd_val_t *val;
		val = (cmd_val_t *)(valuep + i * 8);
		json_render_attr_value(list, NULL, attrp[i], val);
	}
	list = json_add_array(NULL, "timestamps");
	for (i = 0; i < attrcnt; i++) {
		json_render_timestamp(list, NULL, tsp[i]);
	}

	// If we have success, but also render Status, that's an error
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	free(tsp);
	free(valuep);
	free(attrp);
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_GrpAttrGetValue().
 *
 * The pwrcmd interface exercises this by providing a list of object names,
 * which must first be gathered into a group to call PWR_GrpAttrGetValue().
 *
 * @param cmd_opt - command option structure
 */
void
cmd_get_grp_attr(cmd_opt_t *cmd_opt)
{
	PWR_Grp grp = NULL;
	PWR_Status status = NULL;
	void *valuep = NULL;
	PWR_Time *tsp = NULL;
	cson_array *list = NULL;
	int grpcnt = 0;
	int i;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->attrs_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count(%d) != 1",
				cmd_opt->attrs_cnt);
		goto error_handler;
	}

	// Create a new group
	grpcnt = create_group(cmd_opt, &grp);
	if (grpcnt <= 0) {
		goto error_handler;
	}

	// Allocate space for the values and timestamps
	valuep = calloc(grpcnt, 8);
	tsp = calloc(grpcnt, sizeof(PWR_Time));
	if (!valuep || !tsp) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "Out of memory");
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Read the attribute across the entire group
	memset(&cmd_opt->val, 0, sizeof(cmd_opt->val));
	cmd_opt->retcode = PWR_GrpAttrGetValue(grp, cmd_opt->attr,
			valuep, tsp, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpAttrGetValue() failed");
		// continue -- there may be some successes
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_GrpAttrGetValue");
	list = json_add_array(NULL, "attr_vals");
	for (i = 0; i < grpcnt; i++) {
		memcpy(&cmd_opt->val, valuep + 8 * i, 8);
		json_render_attr_value(list, NULL, cmd_opt->attr, &cmd_opt->val);
	}
	list = json_add_array(NULL, "timestamps");
	for (i = 0; i < grpcnt; i++) {
		json_render_timestamp(list, NULL, tsp[i]);
	}

	// If we have success, but also render Status, that's an error
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	PWR_GrpDestroy(grp);
	free(tsp);
	free(valuep);
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_GrpAttrGetValues().
 *
 * The pwrcmd interface exercises this by providing a list of object names,
 * which must first be gathered into a group to call PWR_GrpAttrGetValue().
 *
 * @param cmd_opt - command option structure
 */
void
cmd_get_grp_attrs(cmd_opt_t *cmd_opt)
{
	PWR_Grp grp = NULL;
	PWR_Status status = NULL;
	PWR_AttrName *attrp = NULL;
	void *valuep = NULL;
	PWR_Time *tsp = NULL;
	cson_array *list = NULL;
	int grpcnt = 0;
	int attrcnt = 0;
	int i, j, offset;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Create a new group
	grpcnt = create_group(cmd_opt, &grp);
	if (grpcnt <= 0) {
		goto error_handler;
	}

	// Build the attribute list
	attrcnt = create_attrs(cmd_opt, &attrp);
	if (attrcnt <= 0) {
		goto error_handler;
	}

	// Allocate space for the call arrays
	valuep = calloc(grpcnt * cmd_opt->attrs_cnt, 8);
	tsp = calloc(grpcnt * cmd_opt->attrs_cnt, sizeof(PWR_Time));
	if (!valuep || !tsp) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "Out of memory");
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Read the attributes across the entire group
	cmd_opt->retcode = PWR_GrpAttrGetValues(grp, attrcnt, attrp, valuep, tsp, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpAttrGetValues() failed");
		// continue -- there may be some successes
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_GrpAttrGetValues");
	list = json_add_array(NULL, "attr_vals");
	offset = 0;
	for (i = 0; i < grpcnt; i++) {
		for (j = 0; j < attrcnt; j++, offset++) {
			PWR_AttrName attr = attrp[j];
			cmd_val_t *val;
			val = (cmd_val_t *)(valuep + offset * 8);
			json_render_attr_value(list, NULL, attr, val);
		}
	}
	list = json_add_array(NULL, "timestamps");
	offset = 0;
	for (i = 0; i < grpcnt; i++) {
		for (j = 0; j < attrcnt; j++, offset++) {
			json_render_timestamp(list, NULL, tsp[offset]);
		}
	}

	// If we have success, but also rendered Status, that's an error
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	PWR_GrpDestroy(grp);
	free(tsp);
	free(valuep);
	free(attrp);
	TRACE2_EXIT("");
}

//
// cmd_set_attr - Set the requested attribute value.
//
// Argument(s):
//
//	cmd_opt - The command options and results
//
// Return Code(s):
//
//	void
//
void
cmd_set_attr(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	val_type_t val_type;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->names_cnt != 1 ||
			cmd_opt->attrs_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"name count(%d) or attr count(%d) != 1",
				cmd_opt->names_cnt, cmd_opt->attrs_cnt);
		goto error_handler;
	}
	// User error, too many or too few values
	if (cmd_opt->attrs_cnt != cmd_opt->values_cnt) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count (%d) != values count(%d)",
				cmd_opt->attrs_cnt, cmd_opt->values_cnt);
		goto error_handler;
	}

	//
	// Convert requested value from a string representation to the actual
	// numeric value required by the PowerAPI.
	//
	val_type = cmd_parse_attr_val(cmd_opt->attr, cmd_opt->val_str, &cmd_opt->val);
	if (val_type == val_is_invalid) {
		goto error_handler;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		TRACE2_EXIT("");
		return;
	}

	//
	// Set the attribute value.
	//
	cmd_opt->retcode = PWR_ObjAttrSetValue(obj, cmd_opt->attr,
			(void *)&cmd_opt->val);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		if (cmd_opt->retcode == PWR_RET_READ_ONLY) {
			PRINT_ERRCODE(cmd_opt->retcode,
					"Attribute %d is read-only",
					cmd_opt->attr);
		} else {
			PRINT_ERRCODE(cmd_opt->retcode,
					"PWR_ObjAttrSetValue() failed");
		}
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_ObjAttrSetValue");
	json_render_status(NULL);

error_handler:
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_ObjAttrSetValues()
 *
 * @param cmd_opt - command option structure
 */
void
cmd_set_attrs(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Status status = NULL;
	PWR_AttrName *attrp = NULL;
	void *valuep = NULL;
	int attrcnt = 0;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->names_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"name count(%d) != 1",
				cmd_opt->names_cnt);
		goto error_handler;
	}
	// User error, too many or too few values
	if (cmd_opt->attrs_cnt != cmd_opt->values_cnt) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count (%d) != values count(%d)",
				cmd_opt->attrs_cnt, cmd_opt->values_cnt);
		goto error_handler;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto error_handler;
	}

	// Build the attribute list
	attrcnt = create_values(cmd_opt, &attrp, &valuep);
	if (attrcnt <= 0) {
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Set the attributes of the object
	cmd_opt->retcode = PWR_ObjAttrSetValues(obj, attrcnt, attrp, valuep, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjAttrSetValues() failed");
		// continue -- there may be some successes
	}

	// If we have success, but also render Status, that's an error
	json_add_string(NULL, "method", "PWR_ObjAttrSetValues");
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	free(valuep);
	free(attrp);
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_GrpAttrSetValue().
 *
 * The pwrcmd interface exercises this by providing a list of object names,
 * which must first be gathered into a group to call PWR_GrpAttrSetValue().
 *
 * @param cmd_opt - command option structure
 */
void
cmd_set_grp_attr(cmd_opt_t *cmd_opt)
{
	PWR_Grp grp = NULL;
	PWR_Status status = NULL;
	val_type_t val_type;
	int grpcnt;

	TRACE2_ENTER("cmd_opt = %p, cmd_opt->val.str = %p, cmd_opt->val_str = %p",
			cmd_opt, cmd_opt->val.str, cmd_opt->val_str);

	// Sanity check, impossible unless pwrcmd calls are wrong
	if (cmd_opt->attrs_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count(%d) != 1",
				cmd_opt->attrs_cnt);
		goto error_handler;
	}
	// User error, too many or too few values
	if (cmd_opt->attrs_cnt != cmd_opt->values_cnt) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count (%d) != values count(%d)",
				cmd_opt->attrs_cnt, cmd_opt->values_cnt);
		goto error_handler;
	}

	//
	// Convert requested value from a string representation to the actual
	// numeric value required by the PowerAPI.
	//
	val_type = cmd_parse_attr_val(cmd_opt->attr, cmd_opt->val_str, &cmd_opt->val);
	if (val_type == val_is_invalid) {
		goto error_handler;
	}

	// Create a new group
	grpcnt = create_group(cmd_opt, &grp);
	if (grpcnt <= 0) {
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Set the attribute across the entire group
	cmd_opt->retcode = PWR_GrpAttrSetValue(grp, cmd_opt->attr,
			&cmd_opt->val, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpAttrSetValue() failed");
		// continue -- there may be some successes
	}

	// If we have success, but also render Status, that's an error
	json_add_string(NULL, "method", "PWR_GrpAttrSetValue");
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	PWR_GrpDestroy(grp);
	TRACE2_EXIT("");
}

/**
 * Exercise PWR_GrpAttrSetValues().
 *
 * The pwrcmd interface exercises this by providing a list of object names,
 * which must first be gathered into a group to call PWR_GrpAttrSetValues().
 *
 * @param cmd_opt - command option structure
 */
void
cmd_set_grp_attrs(cmd_opt_t *cmd_opt)
{
	PWR_Grp grp = NULL;
	PWR_Status status = NULL;
	PWR_AttrName *attrp = NULL;
	void *valuep = NULL;
	int grpcnt = 0;
	int attrcnt = 0;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// User error, too many or too few values
	if (cmd_opt->attrs_cnt != cmd_opt->values_cnt) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"attr count (%d) != values count(%d)",
				cmd_opt->attrs_cnt, cmd_opt->values_cnt);
		goto error_handler;
	}

	// Create a new group
	grpcnt = create_group(cmd_opt, &grp);
	if (grpcnt <= 0) {
		goto error_handler;
	}

	// Build the attribute and value lists
	attrcnt = create_values(cmd_opt, &attrp, &valuep);
	if (attrcnt <= 0) {
		goto error_handler;
	}

	// Create a PWR_Status object
	cmd_opt->retcode = PWR_StatusCreate(ctx, &status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_StatusCreate() failed");
		goto error_handler;
	}

	// Set the attributes across the entire group
	cmd_opt->retcode = PWR_GrpAttrSetValues(grp, attrcnt, attrp, valuep, status);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		// Subtlety: DO set the JSON return value here
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpAttrSetValues() failed");
		// continue -- there may be some successes
	}

	// If we have success, but also render Status, that's an error
	json_add_string(NULL, "method", "PWR_GrpAttrSetValues");
	if (json_render_status(status) > 0 &&
			cmd_opt->retcode == PWR_RET_SUCCESS) {
		cmd_opt->retcode = PWR_RET_FAILURE;
		PRINT_ERRCODE(cmd_opt->retcode,
				"retcode == 0, but there were status entries");
	}

error_handler:
	PWR_StatusDestroy(status);
	PWR_GrpDestroy(grp);
	free(valuep);
	free(attrp);
	TRACE2_EXIT("");
}

//
// cmd_get_meta - Get the current value for the requested metadata
//
// Argument(s):
//
//	cmd_opt - The command options and results
//
// Return Code(s):
//
//	void
//
void
cmd_get_meta(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Time ts = PWR_TIME_UNKNOWN;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// A get on metadata can only be done on a single object
	if (cmd_opt->names_cnt != 1 ) {
		PRINT_ERRCODE(PWR_RET_FAILURE, "name count(%d)!= 1", 
			cmd_opt->names_cnt);
		goto done;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto done;
	}

	// Read the metadata.
	memset(&cmd_opt->val, 0, sizeof(cmd_opt->val));
	cmd_opt->retcode = PWR_ObjAttrGetMeta(obj, cmd_opt->attr, cmd_opt->meta,
			(void *)&cmd_opt->val);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjAttrGetMeta(attr=%d, meta=%d) failed",
				cmd_opt->attr, cmd_opt->meta);
		goto done;
	}

	//
	// Print the value.
	//
	json_render_meta_value(NULL, "value", cmd_opt->meta, cmd_opt->attr, &cmd_opt->val);

	//
	// Display the timestamp
	//
	if (ts > 0) {
		json_render_timestamp(NULL, "timestamp", ts);
	}

done:
	TRACE2_EXIT("");
}

//
// cmd_get_meta_at_index - Get the current value for the requested enumerated
// metadata
//
// Argument(s):
//
//	cmd_opt - The command options and results
//
// Return Code(s):
//
//	void
//
void
cmd_get_meta_at_index(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	char buffer[1024] = { 0 };

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto done;
	}

	// Read the metadata.
	memset(&cmd_opt->val, 0, sizeof(cmd_opt->val));
	cmd_opt->retcode = PWR_MetaValueAtIndex(obj, cmd_opt->attr,
			cmd_opt->index, (void *)&cmd_opt->val,
			buffer);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_MetaValueAtIndex(attr=%d, index=%d) failed",
				cmd_opt->attr, cmd_opt->index);
		goto done;
	}

	//
	// Print the value.
	//
	json_render_meta_index_value(NULL, "value", cmd_opt->attr,
			buffer);

done:
	TRACE2_EXIT("");
}

//
// cmd_set_meta - Set the requested attribute value.
//
// Argument(s):
//
//	cmd_opt - The command options and results
//
// Return Code(s):
//
//	void
//
void
cmd_set_meta(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	val_type_t val_type;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// A set on metadata requires specification of the object name, the
	// attribute of the object, the metadata of the attribute, and a value.
	// Since the metadata value in cmd_opt was set to something other
	// that PWR_MD_NOT_SPECIFIED the metadata requirement was met. Now
	// check that one each of the other requirements are present.
	if (cmd_opt->names_cnt != 1 ||
			cmd_opt->attrs_cnt != 1 ||
			cmd_opt->values_cnt != 1) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"name count(%d), attr count(%d), or value count(%d) != 1",
				cmd_opt->names_cnt, cmd_opt->attrs_cnt,
				cmd_opt->values_cnt);
		goto error_handler;
	}

	//
	// Convert value from a string representation to the actual
	// numeric value required by the PowerAPI.
	//
	val_type = cmd_parse_meta_val(cmd_opt->meta, cmd_opt->attr,
			cmd_opt->val_str, &cmd_opt->val);
	if (val_type == val_is_invalid) {
		goto error_handler;
	}

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		TRACE2_EXIT("");
		return;
	}

	//
	// Set the metadata value.
	//
	cmd_opt->retcode = PWR_ObjAttrSetMeta(obj, cmd_opt->attr,
			cmd_opt->meta, (void *)&cmd_opt->val);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		if (cmd_opt->retcode == PWR_RET_READ_ONLY) {
			PRINT_ERRCODE(cmd_opt->retcode,
					"Attribute %d, metadata %d is read-only",
					cmd_opt->attr, cmd_opt->meta);
		} else {
			PRINT_ERRCODE(cmd_opt->retcode,
					"PWR_ObjAttrSetValue() failed");
		}
	}

	// Render the result for JSON consumption
	json_add_string(NULL, "method", "PWR_ObjAttrSetMeta");
	json_render_status(NULL);

error_handler:
	TRACE2_EXIT("");
}

//
// cmd_get_parent - Get the name of the parent of the named object
//
// Argument(s):
//
//	cmd_opt - The command options
//
// Return Code(s):
//
//	void
//
void
cmd_get_parent(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Obj parent = NULL;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto done;
	}

	cmd_opt->retcode = PWR_ObjGetParent(obj, &parent);
	if (cmd_opt->retcode == PWR_RET_WARN_NO_PARENT) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"No parent found");
		goto done;
	} else if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjGetParent(%p) failed", obj);
		goto done;
	}

	cmd_opt->retcode = PWR_ObjGetName(parent, cmd_opt->val.str,
			sizeof(cmd_opt->val.str));
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjGetName(%p) failed", parent);
		goto done;
	}

	//
	// Print the value.
	//
	json_add_string(NULL, "parent", cmd_opt->val.str);

done:
	TRACE2_EXIT("");
}

//
// cmd_get_children - Get the names of the children of the named object
//
// Argument(s):
//
//	cmd_opt - The command options
//
// Return Code(s):
//
//	void
//
void
cmd_get_children(cmd_opt_t *cmd_opt)
{
	PWR_Obj obj = NULL;
	PWR_Obj child = NULL;
	PWR_Grp children = NULL;
	cson_array *list = NULL;
	int i = 0, count = 0;

	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	// Get the object by name
	cmd_opt->retcode = PWR_CntxtGetObjByName(ctx, cmd_opt->name_str, &obj);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_CntxtGetObjByName(%s) failed",
				cmd_opt->name_str);
		goto done;
	}

	// Get the group of children
	cmd_opt->retcode = PWR_ObjGetChildren(obj, &children);
	if (cmd_opt->retcode != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_ObjGetChildren(%p) failed", obj);
		goto done;
	}

	cmd_opt->retcode = PWR_GrpGetNumObjs(children);
	if (cmd_opt->retcode < PWR_RET_SUCCESS) {
		PRINT_ERRCODE(cmd_opt->retcode,
				"PWR_GrpGetNumObjs(%p) failed", children);
		goto done;
	}
	count = cmd_opt->retcode;
	cmd_opt->retcode = PWR_RET_SUCCESS;
	if (count == 0) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"No children found");
		goto done;
	}

	list = json_add_array(NULL, "children");

	// Walk the group of children, getting the name for each child
	for (i = 0; i < count; ++i) {
		cmd_opt->retcode = PWR_GrpGetObjByIndx(children, i, &child);
		if (cmd_opt->retcode != PWR_RET_SUCCESS) {
			PRINT_ERRCODE(cmd_opt->retcode,
					"PWR_GrpGetObjByIndx(%p,%d) failed",
					children, i);
			goto done;
		}

		cmd_opt->retcode = PWR_ObjGetName(child, cmd_opt->val.str,
				sizeof(cmd_opt->val.str));
		if (cmd_opt->retcode != PWR_RET_SUCCESS) {
			PRINT_ERRCODE(cmd_opt->retcode,
					"PWR_ObjGetName(%p) failed", child);
			goto done;
		}

		json_add_string(list, NULL, cmd_opt->val.str);
	}

done:
	TRACE2_EXIT("");
}

/*
 * HIERARCHY LIST
 */

/**
 * Simple linked-list stack for preserving json nesting.
 */
typedef struct json_stack_s json_stack_t;
struct json_stack_s {
	json_stack_t *next;     // pointer to next stack container
	void *data;             // data attached to container
};
static json_stack_t *json_hier_head = NULL;

/**
 * Push data onto the HEAD of the stack.
 *
 * @param data - persistent data to be pushed
 */
static void
json_push(void *data)
{
	json_stack_t *container;

	TRACE3_ENTER("data = %p", data);
	container = malloc(sizeof(json_stack_t));
	container->data = data;
	container->next = json_hier_head;
	json_hier_head = container;
	TRACE3_EXIT("container = %p", container);
}

/**
 * Pops data off the HEAD of the stack. Returns NULL if there is nothing on the
 * stack.
 *
 * @return void* - original data pushed onto the stack, or NULL
 */
static void *
json_pop(void)
{
	json_stack_t *container = json_hier_head;
	void *data = NULL;

	TRACE3_ENTER("container = %p", container);
	if (container) {
		json_hier_head = container->next;
		data = container->data;
		free(container);
	}
	TRACE3_EXIT("data = %p", data);
	return data;
}

/**
 * Generate nested JSON objects on entry to a new object level.
 *
 * @param object - current object being traversed
 * @param child_count - child count of current object
 * @param data - pointer to the current JSON object pointer
 */
static void
json_hier_enter(PWR_Obj object, int child_count, void *data)
{
	cson_object **cobjP = (cson_object **)data;
	char name[PWR_MAX_STRING_LEN] = { 0 };
	char type[PWR_MAX_STRING_LEN] = { 0 };

	TRACE3_ENTER("object = %p, child_count = %d, data = %p, cson_object = %p",
			object, child_count, data, (*cobjP));

	// Acquire the object name and type-name
	PWR_ObjGetName(object, name, sizeof(name));
	PWR_ObjAttrGetMeta(object, PWR_ATTR_NOT_SPECIFIED, PWR_MD_NAME, (void *)type);

	// Push the current JSON object and create a new object
	// This new JSON object corresponds to 'object'
	json_push((*cobjP));
	(*cobjP) = json_add_object((*cobjP), name);

	// Add a type: tag with the object type-name
	json_add_string((*cobjP), "type", type);

	// Push object so pop counts are deterministic
	json_push((*cobjP));
	if (child_count > 0) {
		/*
		 * There are children, so we need a container for them. This
		 * will become the "current" container for the iteration over
		 * the children.
		 */
		(*cobjP) = json_add_object((*cobjP), "children");
	} else {
		/*
		 * There are no children, so add a 'null' value. We don't need a
		 * new object, because there are no children to iterate over,
		 * and we're just going pop the above push.
		 */
		json_add_null((*cobjP), "children");
	}

	TRACE3_EXIT("");
}

/**
 * Leave the hierarchy level.
 *
 * @param object - current object (ignored)
 * @param child_count - child count of current object (ignored)
 * @param data - pointer to the current JSON object pointer
 */
static void
json_hier_leave(PWR_Obj object, int child_count, void *data)
{
	cson_object **cobjP = (cson_object **)data;

	TRACE3_ENTER("object = %p, child_count = %d, data = %p, cson_object = %p",
			object, child_count, data, (*cobjP));

	// Pop the two pushes from entry
	(*cobjP) = json_pop();
	(*cobjP) = json_pop();

	TRACE3_EXIT("");
}

/**
 * Generate text-based pictures on entry to a new object level.
 *
 * @param object - current object being traversed
 * @param child_count - child count of current object (ignored)
 * @param data - pointer to the current recursion depth
 */
static void
text_hier_enter(PWR_Obj object, int child_count, void *data)
{
	int *level = (int *)data;
	char name[PWR_MAX_STRING_LEN] = { 0 };
	char type[PWR_MAX_STRING_LEN] = { 0 };
	int i;

	TRACE3_ENTER("object = %p, child_count = %d, data = %p, level = %d",
			object, child_count, data, *level);

	*level += 1;
	PWR_ObjGetName(object, name, sizeof(name));
	PWR_ObjAttrGetMeta(object, PWR_ATTR_NOT_SPECIFIED, PWR_MD_NAME, (void *)type);
	if (*level > 1) {
		for (i = 1; i < *level; ++i)
			printf("    ");
		printf("|\n");
	}
	for (i = 1; i < *level; ++i)
		printf("    ");
	printf("+-> %s (%s)\n", name, type);

	TRACE3_EXIT("");
}

/**
 * Leave the hierarchy level.
 *
 * @param object - current object being traversed (ignored)
 * @param child_count - child count of current object (ignored)
 * @param data - pointer to the current recursion depth
 */
static void
text_hier_leave(PWR_Obj object, int child_count, void *data)
{
	int *level = (int *)data;

	TRACE3_ENTER("object = %p, child_count = %d, data = %p, level = %d",
			object, child_count, data, *level);

	*level -= 1;

	TRACE3_EXIT("");
}

/**
 * Traverse the hierarchy in a pre-visit order, for rendering.
 *
 * @param root
 * @param data
 * @param visit_function
 * @param leave_function
 */
void
traverse_pre_order(PWR_Obj root, void *data,
		void (*visit_function)(PWR_Obj, int, void *),
		void (*leave_function)(PWR_Obj, int, void *))
{
	PWR_Grp children = 0;
	int child_count = 0;
	int status = PWR_RET_SUCCESS;

	TRACE3_ENTER("root = %p, data = %p, "
			"visit_function = %p, "
			"leave_function = %p",
			root, data, visit_function, leave_function);

	// Need to know if this object has children
	status = PWR_ObjGetChildren(root, &children);
	switch (status) {
	case PWR_RET_SUCCESS:
		{
		// Need to count the children -- can be zero
			child_count = PWR_GrpGetNumObjs(children);
			break;
		}
	case PWR_RET_WARN_NO_CHILDREN:
		// There are no children -- normal
		break;
	default:
		// Should not happen
		PRINT_ERRCODE(status,
				"PWR_ObjGetChildren() failed");
		break;
	}

	// This is a pre-order traversal, so call the visit_function
	// for the current object before traversing any children.
	if (visit_function)
		visit_function(root, child_count, data);

	// Traverse each of the children
	if (child_count > 0) {
		PWR_Obj child = 0;
		int i;

		for (i = 0; i < child_count; ++i) {
			PWR_GrpGetObjByIndx(children, i, &child);
			traverse_pre_order(child, data, visit_function,
					leave_function);
		}
	}

	// Call the leave_function before leaving the current object's
	// subtree traversal.
	if (leave_function)
		leave_function(root, child_count, data);

	TRACE3_EXIT("");
}

/**
 * Print the hierarchy, in either JSON or text format.
 */
void
print_hierarchy_list(void)
{
	PWR_Obj object = 0;
	int level = 0;

	TRACE3_ENTER("");

	// Get the root object (NODE) of the hierarchy
	PWR_CntxtGetEntryPoint(ctx, &object);

	if (json_is_enabled()) {
		// Create the 'hier_tree' object and go wild
		cson_object *cobj = json_add_object(NULL, "hier_tree");
		traverse_pre_order(object, &cobj, json_hier_enter, json_hier_leave);

	} else {
		// Create a header and go wild
		printf("\n"
				"Object Hierarchy\n"
				"----------------\n");
		traverse_pre_order(object, &level, text_hier_enter, text_hier_leave);
	}

	TRACE3_EXIT("");
}

/*
 * NAME LIST
 */

/**
 * Comparator for sorting.
 *
 * @param val1 - pointer to string1
 * @param val2 - pointer to string2
 * @param data - context data (ignored)
 *
 * @return gint - sort comparator value (-1, 0, 1)
 */
static gint
name_compare(gconstpointer val1, gconstpointer val2, gpointer data)
{
	return g_strcmp0((char *)val1, (char *)val2);
}

/**
 * Pre-order entry into hierarchy traversal. This makes a copy of the object
 * name, and sorts it into the sorter list.
 *
 * There is no corresponding leave function.
 *
 * @param object - current object being traversed
 * @param child_count - child count of current object (ignored)
 * @param data - pointer to the sorter GTree object
 */
static void
name_save(PWR_Obj object, int child_count, void *data)
{
	GTree *sorter = (GTree *)data;
	char name[PWR_MAX_STRING_LEN] = { 0 };
	char *copy = NULL;

	TRACE3_ENTER("object = %p, child_count = %d, data = %p",
			object, child_count, data);

	PWR_ObjGetName(object, name, sizeof(name));
	copy = g_strdup(name);

	g_tree_insert(sorter, copy, copy);

	TRACE3_EXIT("");
}

/**
 * Callback to render each name as we traverse the sorter tree.
 *
 * @param key - object key (== value) (ignored)
 * @param value - object name
 * @param data - JSON array object into which we are writing
 *
 * @return gboolean
 */
static gboolean
name_print(gpointer key, gpointer value, gpointer data)
{
	char *name = (char *)value;
	cson_array *list = (cson_array *)data;

	TRACE3_ENTER("key = %p, name = \"%s\", data = %p",
			key, name, data);

	json_add_string(list, NULL, (name) ? name : "NULL");

	TRACE3_EXIT("");
	return FALSE; // Continue traversal
}

/**
 * Render the list of object names.
 */
void
print_name_list(void)
{
	// Traverse the hierarchy, gathering the names into sorted order.
	cson_array *list = NULL;
	PWR_Obj object = 0;
	GTree *sorter = NULL;

	TRACE3_ENTER("");

	// Create the sorter b-tree
	sorter = g_tree_new_full(name_compare, NULL, NULL, g_free);

	// Get the root object (NODE) of the hierarchy
	PWR_CntxtGetEntryPoint(ctx, &object);

	// Traverse the tree, adding object names to the sorter tree
	traverse_pre_order(object, sorter, name_save, NULL);

	if (json_is_enabled()) {
		// Insert items into this array
		list = json_add_array(NULL, "name_list");
	} else {
		// Print a header
		printf("\n"
				"Object Names\n"
				"------------\n");
	}

	// Walk the sorter tree in sorted order
	g_tree_foreach(sorter, name_print, list);

	// All done
	g_tree_destroy(sorter);

	TRACE3_EXIT("");
}

/*
 * ATTRIBUTE LIST
 */

/**
 * Render a list of all the supported attributes.
 */
void
print_attribute_list(void)
{
	cson_array *list = NULL;
	int retval = 0;
	size_t attr_count = 0;
	const char **attr_str_list = NULL;
	size_t i;

	TRACE3_ENTER("");

	// Get the count of attributes
	retval = CRAYPWR_AttrGetCount(PWR_OBJ_INVALID, &attr_count);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(retval,
				"CRAYPWR_AttrGetCount(PWR_OBJ_INVALID) failed");
		goto done;
	}

	// Allocate string memory
	attr_str_list = calloc(attr_count, sizeof(char *));
	if (attr_str_list == NULL) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"calloc(%ld,%ld) failed", attr_count, sizeof(char *));
		goto done;
	}

	// Fill the string memory
	retval = CRAYPWR_AttrGetList(PWR_OBJ_INVALID, attr_count, attr_str_list, NULL);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(retval,
				"CRAYPWR_AttrGetList(PWR_OBJ_INVALID,%ld) failed",
				attr_count);
		goto done;
	}

	if (json_is_enabled()) {
		// Insert items into this array
		list = json_add_array(NULL, "attr_list");
	} else {
		// Print a header
		printf("\n"
				"Attribute List\n"
				"--------------\n");
	}

	// Walk the list
	for (i = 0; i < attr_count; i++) {
		json_add_string(list, NULL, attr_str_list[i]);
	}

done:
	// All done
	free(attr_str_list);

	TRACE3_EXIT("");
}

/**
 * Called by the application to render one or more lists.
 *
 * @param cmd_opt - command option pointer
 */
void
cmd_get_list(cmd_opt_t *cmd_opt)
{
	TRACE2_ENTER("cmd_opt = %p", cmd_opt);

	switch (cmd_opt->list) {
	case list_all:
		print_hierarchy_list();
		print_name_list();
		print_attribute_list();
		break;
	case list_hier:
		print_hierarchy_list();
		break;
	case list_name:
		print_name_list();
		break;
	case list_attr:
		print_attribute_list();
		break;
	default:
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unrecognized list type: %u", cmd_opt->list);
		break;
	}

	TRACE2_EXIT("");
}

//
// get_api_version - Attempt to read the version of the API implementation.
//
// Argument(s):
//
//	void
//
// Return Code(s):
//
//	void
//
void
get_api_version(void)
{
	TRACE2_ENTER("");

	//
	// If already set, just return.
	//
	if (API_major_version != -1 && API_minor_version != -1) {
		TRACE2_EXIT("API_major_version = %d, API_minor_version = %d",
				API_major_version, API_minor_version);
		return;
	}

	//
	// Get major version.
	//
	API_major_version = PWR_GetMajorVersion();
	if (API_major_version == PWR_RET_FAILURE) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"PWR_GetMajorVersion() failed: %d",
				API_major_version);
		force_exit(PWR_RET_FAILURE);    // get_api_version() no major
	}

	//
	// Get minor version.
	//
	API_minor_version = PWR_GetMinorVersion();
	if (API_minor_version == PWR_RET_FAILURE) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"PWR_GetMinorVersion() failed: %d",
				API_minor_version);
		force_exit(PWR_RET_FAILURE);    // get_api_version() no minor
	}

	//
	// We currently don't do anything with what we get back...  This is
	// where the future handling code would go when needed.
	//

	TRACE2_EXIT("API_major_version = %d, API_minor_version = %d",
			API_major_version, API_minor_version);
}

//
// api_init - Create an API context, find our entry in the hierarch, and
//	      perform any additional API initializations.
//
// Argument(s):
//
//	void
//
// Return Code(s):
//
//	void
//
void
api_init(PWR_Role role)
{
	int retval = PWR_RET_SUCCESS;
	uint64_t max_md_str_len = 0;
	PWR_ObjType obj_type = PWR_OBJ_INVALID;

	TRACE2_ENTER("role = %d", role);

	//
	// Get a context.
	//
	retval = PWR_CntxtInit(PWR_CNTXT_DEFAULT, role, "pwrcmd",
			&ctx);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"PWR_CntxtInit() failed");
		force_exit(PWR_RET_FAILURE);    // api_init() context init failure
	}

	//
	// Get our location in the object hierarchy.
	//
	retval = PWR_CntxtGetEntryPoint(ctx, &obj);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"PWR_CntxtGetEntryPoint() failed");
		force_exit(PWR_RET_FAILURE);    // api_init() no context
	}

	//
	// Make sure we're where we expect to be in the power hierarchy.
	//
	if ((PWR_ObjGetType(obj, &obj_type) != PWR_RET_SUCCESS) ||
			(obj_type != PWR_OBJ_NODE)) {
		char buf[PWR_MAX_STRING_LEN] = { 0 };

		(void)PWR_ObjGetName(obj, buf, sizeof(buf));
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unexpected '%s' location in the power "
				"hierarchy",
				buf);
		force_exit(PWR_RET_FAILURE);    // api_init() hierarchy is bad
	}

	//
	// Get the official name of the node object.
	//
	if (PWR_ObjGetName(obj, obj_name, sizeof(obj_name)) != PWR_RET_SUCCESS) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Failed to get node name");
		force_exit(PWR_RET_FAILURE);    // api_init() node has no name
	}

	//
	// Allocate a temporary buffer for metadata string operations.
	// This is sized larger than the actual strings we'll get back
	// but the later strdup() fixes that issue.
	//
	md_str = (char *)malloc(max_md_str_len);
	if (md_str == NULL) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Unable to allocate memory");
		force_exit(PWR_RET_FAILURE);    // api_init() OOM
	}

	//
	// Which version is this library implementation?
	//
	get_api_version();

	TRACE2_EXIT("");
}

//
// api_cleanup - Cleanup our PM API context.
//
// Argument(s):
//
//	void
//
// Return Code(s):
//
//	void
//
void
api_cleanup(void)
{
	int retval = 0;

	TRACE2_ENTER("");

	//
	// Nothing there yet, just return.
	//
	if (ctx == NULL)
		goto done;

	//
	// Remove context.
	//
	retval = PWR_CntxtDestroy(ctx);
	if (retval != PWR_RET_SUCCESS) {
		//
		// Even though it wasn't destroyed, set ctx to NULL anyways
		// to avoid recursive loop when exiting.
		//
		ctx = NULL;
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"PWR_CntxtDestroy() returned %d", retval);
		force_exit(PWR_RET_FAILURE);    // api_cleanup() failure
	}

	ctx = NULL;

	//
	// Free up any memory.
	//
	if (md_str) {
		free(md_str);
		md_str = NULL;
	}

done:
	TRACE2_EXIT("");
}
