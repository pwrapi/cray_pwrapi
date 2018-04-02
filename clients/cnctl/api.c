/*
 * Copyright (c) 2015-2016, Cray Inc.
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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cnctl.h"
#include "io.h"
#include "api.h"

typedef union {
	double type_double;
	uint64_t type_uint64;
} type_union_t;

static int API_major_version = -1;      // major version of HPC Power API lib
static int API_minor_version = -1;      // minor version of HPC Power API lib

static PWR_Cntxt ctx;                   // our context
static PWR_Obj entry;                   // our location
static PWR_Obj target;                  // target object

static char *md_str = NULL;             // buffer for all metadata operations

size_t   cattrs_count = 0;
cattrs_t cattrs[PWR_NUM_ATTR_NAMES] = { { NULL, 0 } };

/* Subset of attributes supported by cnctl */
const char *attrs_supported[] = {
	"PWR_ATTR_CSTATE_LIMIT",
	"PWR_ATTR_FREQ",
	"PWR_ATTR_FREQ_LIMIT_MAX",
	"PWR_ATTR_FREQ_LIMIT_MIN",
	NULL    /* NULL pointer terminates the list */
};


static char *
get_object_name(PWR_Obj object, char *buf, size_t len)
{
	TRACE2_ENTER("object = %p, buf = %p, len = %zu", object, buf, len);

	/*
	 * Get the official name of the object.
	 */
	if (PWR_ObjGetName(object, buf, len) != PWR_RET_SUCCESS) {
		snprintf(buf, len, "%s", "<error>");
	}

	TRACE2_EXIT("buf = '%s'", buf);

	return buf;
}

static int
find_object_of_type(PWR_Obj parent, PWR_ObjType find_type, PWR_Obj *result)
{
	int retval = PWR_RET_SUCCESS;
	PWR_ObjType type = PWR_OBJ_INVALID;
	PWR_Grp group = NULL;
	PWR_Obj child = NULL;
	int num_obj = 0;
	int idx;

	TRACE3_ENTER("parent = %p, find_type = %d, result = %p",
			parent, find_type, result);

	// Get the parent's type
	retval = PWR_ObjGetType(parent, &type);
	if (retval != PWR_RET_SUCCESS)
		goto done;

	// See if the parent's type is what we are looking for.
	if (type == find_type) {
		// It is indeed. Store the result and return.
		*result = parent;
		goto done;
	}

	// "group" will contain all of the children of the parent.
	retval = PWR_ObjGetChildren(parent, &group);
	if (retval == PWR_RET_WARN_NO_CHILDREN) {
		retval = PWR_RET_SUCCESS;
		goto done;
	}
	if (retval != PWR_RET_SUCCESS)
		goto done;

	// Nasty API twist -- retval is overloaded with two meanings
	retval = PWR_GrpGetNumObjs(group);
	if (retval < 0)
		goto done;
	num_obj = retval;

	// Iterate through the group checking each of its children.
	for (idx = 0; idx < num_obj; idx++) {
		retval = PWR_GrpGetObjByIndx(group, idx, &child);
		if (retval != PWR_RET_SUCCESS) {
			PWR_GrpDestroy(group);
			goto done;
		}

		// See if this object matches, and check its children too.
		retval = find_object_of_type(child, find_type, result);
		if (retval != PWR_RET_SUCCESS) {
			PWR_GrpDestroy(group);
			goto done;
		}

		// Stop if we found what we were looking for.
		if (*result != NULL) {
			break;
		}
	}

	retval = PWR_GrpDestroy(group);
	if (retval != PWR_RET_SUCCESS)
		goto done;

done:
	TRACE3_EXIT("retval = %d, *result = %p", retval, *result);
	return retval;
}

int
use_ht_object(void)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("");

	target = NULL;

	retval = find_object_of_type(entry, PWR_OBJ_HT, &target);
	if (retval != PWR_RET_SUCCESS)
		goto done;

	if (target == NULL) {
		retval = PWR_RET_FAILURE;
		goto done;
	}

done:
	TRACE2_EXIT("retval = %d, target = %p", retval, target);

	return retval;
}

/*
 * cache_attr_strings - Cache the string representation of all PWR_AttrName
 *			attributes and all their possible attribute values
 *			so that we don't have to continually query the
 *			library for them.
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	void
 */
static void
cache_attr_strings(void)
{
	int retval = PWR_RET_SUCCESS;
	size_t i = 0;
	size_t attr_count = 0;
	const char **attr_str_list = NULL;
	int *attr_val_list = NULL;

	TRACE1_ENTER("");

	/* Query library for number of supported attributes */
	retval = CRAYPWR_AttrGetCount(PWR_OBJ_INVALID, &attr_count);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERR("CRAYPWR_AttrGetCount() failed");
		set_json_ret_code(retval);
		cnctl_exit(retval);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Allocate space to hold the list of name strings for
	 * supported attributes.
	 */
	attr_str_list = calloc(attr_count, sizeof(char *));
	if (attr_str_list == NULL) {
		PRINT_ERR("Unable to allocate memory");
		set_json_ret_code(1);
		cnctl_exit(1);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Allocate space to hold the list of name values for
	 * supported attributes.
	 */
	attr_val_list = calloc(attr_count, sizeof(int));
	if (attr_val_list == NULL) {
		PRINT_ERR("Unable to allocate memory");
		set_json_ret_code(1);
		cnctl_exit(1);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/* Query library for list of all supported attributes */
	retval = CRAYPWR_AttrGetList(PWR_OBJ_INVALID, attr_count,
			attr_str_list, attr_val_list);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERR("CRAYPWR_AttrGetList() failed");
		set_json_ret_code(retval);
		cnctl_exit(retval);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/* Copy only supported attributes to the cache.  */
	for (i = 0, cattrs_count = 0; i < attr_count; i++) {
		size_t j = 0;

		/* Check attribute against list of supported attributes */
		for (j = 0; attrs_supported[j] != NULL; j++) {
			if (strcmp(attrs_supported[j], attr_str_list[i]) == 0)
				break;
		}

		/* If attribute was supported, record it in the cache */
		if (attrs_supported[j]) {
			cattrs[cattrs_count].name_str = attrs_supported[j];
			cattrs[cattrs_count].name_val = attr_val_list[i];

			LOG_DBG("cattrs[%lu] %d = '%s'", cattrs_count,
					cattrs[cattrs_count].name_val,
					cattrs[cattrs_count].name_str);

			++cattrs_count;
		}
	}

	/* Cleanup the lists allocated for the attribute query */
	free(attr_str_list);
	free(attr_val_list);

	TRACE1_EXIT("");
}

/*
 * validate_rattr_str - Validate the user provided target attribute string.
 *
 * Argument(s):
 *
 *	rattr - The user's requested attribute argument
 *
 * Return Code(s):
 *
 *	void
 */
static void
validate_rattr_str(rattr_t *rattr)
{
	const char *attr_str = rattr->attr_str;
#if 0
	// TODO: See below for where retval would be used.
	int retval = PWR_RET_SUCCESS;
#endif
	PWR_AttrName attr = PWR_ATTR_PSTATE;

	TRACE1_ENTER("attr_str = '%s'", attr_str);

	/*
	 * Ensure we first were given an attribute.
	 */
	if (attr_str == NULL) {
		PRINT_ERR("Must specify an attribute to operate on");
		rattr->retcode = PWR_RET_NO_ATTRIB;
		cnctl_exit(rattr->retcode);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Iterate through all of the attributes looking for a match.
	 */
	for (attr = 0; attr < cattrs_count; attr++) {
		/*
		 * Does it match the user's argument?
		 */
		if (strcmp(attr_str, cattrs[attr].name_str) == 0) {
			break;
		}
	}

	/*
	 * Did we find it?
	 */
	if (attr == cattrs_count) {
		PRINT_ERR("Unknown attribute: %s", attr_str);
		rattr->retcode = PWR_RET_NO_ATTRIB;
		cnctl_exit(rattr->retcode);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

#if 0
	// TODO:
	// This check has been disabled because it doesn't work properly.
	// Since it is possible to set some attributes using the parent
	// of a child for whom the attribute is valid, it is not correct
	// to check just for the attribute being valid for the parent,
	// and that is what PWR_ObjAttrIsValid() currently does. If/when
	// PWR_ObjAttrIsValid() is changed to check its children to see
	// if an attribute is valid for one of them, this check can be
	// reenabled.
	/*
	 * Verify the attribute is valid for our target object (target).
	 */
	retval = PWR_ObjAttrIsValid(target, cattrs[attr].name_val);
	if (retval != PWR_RET_SUCCESS) {
		char name[PWR_MAX_STRING_LEN + 1];

		PRINT_ERR("Requested attribute '%s' not valid for object '%s'",
			  attr_str, get_object_name(target, name, sizeof(name)));
		rattr->retcode = retval;
		cnctl_exit(rattr->retcode);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
#endif

	/*
	 * All good!
	 */
	rattr->attr = cattrs[attr].name_val;

	TRACE1_EXIT("attr_str = '%s', attr = %d", attr_str, rattr->attr);
}

/*
 * validate_rattrs_strs - Validate the requested attribute strings.
 *
 * Argument(s):
 *
 *	rattrs    - Requested attributes to operate on
 *
 * Return Code(s):
 *
 *	void
 */
void
validate_rattrs_strs(GArray *rattrs)
{
	int i = 0;

	TRACE1_ENTER("rattrs = %p", rattrs);

	/*
	 * Parse and validate all the attribute/value tuples.
	 */
	for (i = 0; i < rattrs->len; i++) {
		rattr_t *rattr = g_array_index(rattrs, rattr_t*, i);

		/*
		 * Validate the attribute argument.
		 */
		validate_rattr_str(rattr);
	}

	TRACE1_EXIT("");
}

/*
 * get_attr_val_cap - Get the current value for the requested attribute.
 *
 * Argument(s):
 *
 *	rattr - The target attribute
 *
 * Return Code(s):
 *
 *	void
 */
void
get_attr_val_cap(rattr_t *rattr)
{
	int nvals = 0, i = 0;
	const char **vals = NULL;

	TRACE1_ENTER("attr_str = '%s'", rattr->attr_str);

	/*
	 * How many values are there for this attribute?
	 */
	rattr->retcode = PWR_ObjAttrGetMeta(target, rattr->attr, PWR_MD_NUM,
			&nvals);
	if (rattr->retcode != PWR_RET_SUCCESS) {
		PRINT_ERR("PWR_ObjAttrGetMeta(%d, PWR_MD_NUM) failed: %d",
				rattr->attr, rattr->retcode);
		nvals = 0;
		goto get_attr_val_cap_print;
	}

	/*
	 * Allocate a buffer for reading their string representations.
	 * Use calloc so it's initialized to zero.
	 */
	vals = calloc(nvals, sizeof(char *));
	if (vals == NULL) {
		PRINT_ERR("Unable to allocate memory");
		rattr->retcode = PWR_RET_FAILURE;
		nvals = 0;
		goto get_attr_val_cap_print;
	}

	/*
	 * Iterate through all possible values for this attribute and
	 * cache their string representations.
	 */
	for (i = 0; i < nvals; i++) {
		/*
		 * Get the value string.
		 */
		rattr->retcode = PWR_MetaValueAtIndex(target, rattr->attr, i,
				NULL, md_str);
		if (rattr->retcode != PWR_RET_SUCCESS) {
			PRINT_ERR("PWR_MetaValueAtIndex(%d, %d) failed: %d",
					rattr->attr, i, rattr->retcode);
			goto get_attr_val_cap_print;
		}

		/*
		 * Cache the value string.
		 */
		vals[i] = strdup(md_str);
		if (vals[i] == NULL) {
			PRINT_ERR("Unable to allocate memory");
			rattr->retcode = PWR_RET_FAILURE;
			goto get_attr_val_cap_print;
		}

		LOG_DBG("Cached attr=%s value[%d]=%s",
				rattr->attr_str, i, vals[i]);
	}

get_attr_val_cap_print:

	print_attr_val_cap(rattr, nvals, vals);

	/*
	 * Clean up buffers used to cache the attribute values.
	 */
	for (i = 0; i < nvals; i++) {
		free((void *)vals[i]);
	}
	free(vals);

	TRACE1_EXIT("");
}

/*
 * get_attr_val - Get the current value for the requested attribute.
 *
 * Argument(s):
 *
 *	rattr - The target attribute
 *
 * Return Code(s):
 *
 *	void
 */
void
get_attr_val(rattr_t *rattr)
{
	PWR_Time ts = PWR_TIME_UNKNOWN;
	type_union_t val;
	char get_val_str[64];

	TRACE1_ENTER("attr_str = '%s'", rattr->attr_str);

	/*
	 * Read the attribute.
	 */
	memset(&val, 0, sizeof(val));
	rattr->retcode = PWR_ObjAttrGetValue(target, rattr->attr, &val, &ts);
	if (rattr->retcode != PWR_RET_SUCCESS) {
		PRINT_ERR("PWR_ObjAttrGetValue(%s) returned %d\n",
				rattr->attr_str, rattr->retcode);
		goto get_attr_val_print;
	}

	/*
	 * Make things easy because of all the different value types.
	 * Just convert the value into a string.
	 */
	switch (rattr->attr) {
	case PWR_ATTR_CSTATE_LIMIT:
		snprintf(get_val_str, sizeof(get_val_str),
				"%ld", val.type_uint64);
		break;
	case PWR_ATTR_FREQ:
	case PWR_ATTR_FREQ_LIMIT_MAX:
	case PWR_ATTR_FREQ_LIMIT_MIN:
		snprintf(get_val_str, sizeof(get_val_str),
				"%.0lf", val.type_double);
		break;
	default:
		// We already checked this case.
		get_val_str[0] = '\0';
		break;
	}

get_attr_val_print:

	/*
	 * Print the value.
	 */
	print_attr_val(rattr, get_val_str, ts);

	TRACE1_EXIT("retcode = %d, attr_str = '%s', value = '%s', ts = %lu",
			rattr->retcode, rattr->attr_str, get_val_str, ts);
}

/*
 * set_attr_val - Set the requested attribute value.
 *
 * Argument(s):
 *
 *	rattr - The target attribute
 *
 * Return Code(s):
 *
 *	void
 */
void
set_attr_val(rattr_t *rattr)
{
	const char *attr_str = rattr->attr_str;
	const char *val_str = rattr->val_str;
	type_union_t val;
	int bad_val = 0;
	char *endptr = NULL;

	TRACE1_ENTER("attr_str = '%s', val_str = '%s'", attr_str, val_str);

	/*
	 * Make sure we got a value.
	 */
	if (val_str == NULL || val_str[0] == '\0') {
		PRINT_ERR("Value parameter required for set command (%s)",
				attr_str);
		rattr->retcode = PWR_RET_BAD_VALUE;
		goto set_attr_val_print;
	}

	/*
	 * Convert requested value from a string representation to the actual
	 * numeric value required by the PowerAPI.
	 */
	switch (rattr->attr) {
	case PWR_ATTR_CSTATE_LIMIT:
		val.type_uint64 = strtoul(val_str, &endptr, 10);

		if (val.type_uint64 == LLONG_MIN
				|| val.type_uint64 == LLONG_MAX) {
			PRINT_ERR("Requested value '%s' for %s causes overflow",
					val_str, attr_str);
			bad_val = 1;
		} else if (*endptr != '\0') {
			PRINT_ERR("Requested value '%s' for %s contains "
					"invalid characters",
					val_str, attr_str);
			bad_val = 1;
		}
		break;
	case PWR_ATTR_FREQ:
	case PWR_ATTR_FREQ_LIMIT_MAX:
	case PWR_ATTR_FREQ_LIMIT_MIN:
		val.type_double = strtod(val_str, &endptr);

		if (val.type_double == HUGE_VAL
				|| val.type_double == -HUGE_VAL) {
			/*
			 * Continue on error.
			 */
			PRINT_ERR("Requested value '%s' for %s causes overflow",
					val_str, attr_str);
			bad_val = 1;
		} else if (*endptr != '\0') {
			PRINT_ERR("Requested value '%s' for %s contains "
					"invalid characters",
					val_str, attr_str);
			bad_val = 1;
		}
		break;
	default:
		// We already checked this case.
		PRINT_ERR("Internal Error: Don't know format of value for %s",
				attr_str);
		bad_val = 1;
		break;
	}

	/*
	 * Look for bad value input.
	 */
	if (bad_val) {
		rattr->retcode = PWR_RET_BAD_VALUE;
		goto set_attr_val_print;
	}

	/*
	 * Set the attribute value.
	 */
	rattr->retcode = PWR_ObjAttrSetValue(target, rattr->attr, &val);
	if (rattr->retcode != PWR_RET_SUCCESS) {
		if (rattr->retcode == PWR_RET_READ_ONLY) {
			PRINT_ERR("Attribute %s is read-only", attr_str);
		} else {
			PRINT_ERR("PWR_ObjAttrSetValue() returned %d",
					rattr->retcode);
		}
		goto set_attr_val_print;
	}

set_attr_val_print:

	/*
	 * Print the results.
	 */
	print_attr_set_result(rattr);

	TRACE1_EXIT("retcode = %d", rattr->retcode);
}

/*
 * get_api_version - Attempt to read the version of the API implementation.
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	void
 */
void
get_api_version(void)
{
	TRACE1_ENTER("");

	/*
	 * If already set, just return.
	 */
	if (API_major_version != -1 && API_minor_version != -1) {
		TRACE1_EXIT("already set");
		return;
	}

	/*
	 * Get major version.
	 */
	API_major_version = PWR_GetMajorVersion();
	if (API_major_version == PWR_RET_FAILURE) {
		PRINT_ERR("PWR_GetMajorVersion() failed: %d",
				API_major_version);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Get minor version.
	 */
	API_minor_version = PWR_GetMinorVersion();
	if (API_minor_version == PWR_RET_FAILURE) {
		PRINT_ERR("PWR_GetMinorVersion() failed: %d",
				API_minor_version);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * We currently don't do anything with what we get back...  This is
	 * where the future handling code would go when needed.
	 */

	TRACE1_EXIT("major = %d, minor = %d",
			API_major_version, API_minor_version);
}

/*
 * api_init - Create an API context, find our entry in the hierarchy, and
 *	      perform any additional API initializations.
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	void
 */
void
api_init(void)
{
	int retval = PWR_RET_SUCCESS;
	uint64_t max_md_str_len = 0;
	PWR_ObjType obj_type = PWR_OBJ_INVALID;

	TRACE1_ENTER("");

	/*
	 * Get a context.
	 */
	retval = PWR_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_RM, "cnctl", &ctx);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERR("PWR_CntxtInit() failed");
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Get our location in the object hierarchy.
	 */
	retval = PWR_CntxtGetEntryPoint(ctx, &entry);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERR("PWR_CntxtGetEntryPoint() failed");
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	target = entry;

	/*
	 * Make sure we're where we expect to be in the power hierarchy.
	 */
	if ((PWR_ObjGetType(entry, &obj_type) != PWR_RET_SUCCESS) ||
			(obj_type != PWR_OBJ_NODE)) {
		char name[PWR_MAX_STRING_LEN + 1];

		PRINT_ERR("Unexpected '%s' location in the power hierarchy",
				get_object_name(entry, name, sizeof(name)));
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Find the max possible metadata string length.  This will be used
	 * for allocating buffers for general metadata operations.
	 */
	retval = PWR_ObjAttrGetMeta(entry, PWR_ATTR_NOT_SPECIFIED,
			PWR_MD_MAX_LEN, &max_md_str_len);
	if (retval != PWR_RET_SUCCESS) {
		PRINT_ERR("PWR_ObjAttrGetMeta(PWR_MD_MAX_LEN) failed: %d",
				retval);
		cnctl_exit(retval);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Allocate a temporary buffer for metadata string operations.
	 * This is sized larger than the actual strings we'll get back
	 * but the later strdup() fixes that issue.
	 */
	md_str = (char *)malloc(max_md_str_len);
	if (md_str == NULL) {
		PRINT_ERR("Unable to allocate memory");
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Which version is this library implementation?
	 */
	get_api_version();

	/*
	 * Do some initial string caching.
	 */
	cache_attr_strings();

	TRACE1_EXIT("");
}

/*
 * api_cleanup - Cleanup our PM API context.
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	void
 */
void
api_cleanup(void)
{
	int retval = 0;

	TRACE1_ENTER("");

	/*
	 * Nothing there yet, just return.
	 */
	if (ctx == NULL)
		goto done;

	/*
	 * Remove context.
	 */
	retval = PWR_CntxtDestroy(ctx);
	if (retval != PWR_RET_SUCCESS) {
		/*
		 * Even though it wasn't destroyed, set ctx to NULL anyways
		 * to avoid recursive loop when exiting.
		 */
		ctx = NULL;
		PRINT_ERR("PWR_CntxtDestroy() returned %d", retval);
		cnctl_exit(PWR_RET_FAILURE);
	}

	ctx = NULL;

	/*
	 * Free up any memory.
	 */
	if (md_str) {
		free(md_str);
		md_str = NULL;
	}

done:
	TRACE1_EXIT("");
}

