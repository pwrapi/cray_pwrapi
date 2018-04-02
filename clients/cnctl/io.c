/*
 * Copyright (c) 2015-2016, 2018, Cray Inc.
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
#include <errno.h>
#include <string.h>
#include <time.h>

#include "cnctl.h"
#include "io.h"
#include "api.h"
#include "perms.h"
#include "../cson/cson_amalgamation_core.h"

#define MAX_MSG_STR	1024

static cson_value *cout = NULL;			// main CSON output data struct
static cson_object *cout_obj = NULL;		// main CSON output object
static cson_value *attr_tuple_arrV = NULL;	// attr tuple array value
static cson_array *attr_tuple_arr = NULL;	// attr tuple array
static int narr_tuples = 0;			// number of tuples in attr arr

static cson_value *cmsgs = NULL;		// messages, if any
static cson_value *cerrmsgs = NULL;		// error messages, if any
static int num_cmsgs = 0;			// number of messages
static int num_cerrmsgs = 0;			// number of error messages

static int cson_ret_code = PWR_RET_SUCCESS;	// return code

static int64_t client_version_major = -1;	// major version of incoming req
static int64_t client_version_minor = -1;	// minor version of incoming req

/*
 * output_is_json - Are we printing output in JSON?
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	int - 0 if output is JSON, 1 otherwise
 */
static inline int
output_is_json(void)
{
	return cout_obj != NULL;
}

/*
 * process_missing_key - Final handler when there's missing key/value input.
 *
 * Argument(s):
 *
 *	key - Missing key string
 *
 * Return Code(s):
 *
 *	void
 */
__attribute__ ((noreturn)) void
process_missing_key(const char *key)
{
	TRACE1_ENTER("key = '%s'", key);

	PRINT_ERR("'%s' missing from incoming request", key);

	cnctl_exit(PWR_RET_FAILURE);
	// NOT REACHED
	TRACE1_EXIT("NOT REACHED");
}

/*
 * parse_json_attr_list - Parse the attribute list found in the JSON packet.
 *
 * Argument(s):
 *
 *	a_flag    - Pointer to buffer that contains the number of times the
 *		    equivalent -a/--attribute parameter is specified
 *	v_flag    - Pointer to buffer that contains the number of times the
 *		    equivalent -v/--value parameter is specified
 *	rattrs    - Pointer to array of attribute requests
 *	cin       - The CSON input value that contains the attribute list
 *
 * Return Code(s):
 *
 *	void
 */
void
parse_json_attr_list(int *a_flag, int *v_flag, GArray *rattrs, cson_value *cin)
{
	int retval = 0;
	cson_object *cin_obj = NULL;
	cson_object_iterator iter;
	cson_kvp *kvp;	// key/value pair
	rattr_t *rattr = NULL;

	TRACE1_ENTER("");

	/*
	 * Allocate a request attribute structure to be later inserted
	 * into the attribute request array.
	 */
	if ((rattr = (rattr_t *) calloc(1, sizeof(rattr_t))) == NULL) {
		PRINT_ERR("Unable to allocate memory");
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Ensure a valid CSON object is present and read it.
	 */
	if (!cson_value_is_object(cin)) {
		PRINT_ERR("%s list in JSON packet is empty", JSON_ATTRS_STR);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
	cin_obj = cson_value_get_object(cin);

	/*
	 * Initialize the iterator.
	 */
	retval = cson_object_iter_init(cin_obj, &iter);
	if (retval != 0) {
		PRINT_ERR("cson_object_iter_init() failed: %d", retval);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	// TODO: loop does not create new rattr pointers, memory leak
	/*
	 * Iterate over all attr/value tuples.
	 */
	while ((kvp = cson_object_iter_next(&iter))) {
		cson_string const *ckey = cson_kvp_key(kvp);
		const char *key = cson_string_cstr(ckey);
		cson_value *v = cson_kvp_value(kvp);
		int64_t v_int = (int64_t)cson_value_get_integer(v);
		const char *v_str = (const char *)cson_value_get_cstr(v);

		LOG_DBG("key=%s v_str=%s v_int=%ld", key, v_str, v_int);

		if (!key) {
			continue;
		}

		/*
		 * Process either an attribute name or value.
		 */
		if (!strcmp(key, JSON_ATTR_NAME_STR)) {
			/*
			 * Must have been a string.
			 */
			if (!v_str) {
				PRINT_ERR("Invalid Attr (NULL/%ld) found in "
					  "input", v_int);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			rattr->attr_str = strdup(v_str);
			*a_flag += 1;
		} else if (!strcmp(key, JSON_ATTR_VAL_STR)) {
			/*
			 * Values can be integers or strings but we don't
			 * care yet which it is so convert to string if it
			 * isn't already.
			 */
			if (v_str) {
				rattr->val_str = strdup(v_str);
			} else {
				char buf[256];
				snprintf(buf, 256, "%ld", v_int);
				rattr->val_str = strdup(buf);
			}
			*v_flag += 1;
		} else {
			PRINT_ERR("Unknown attribute '%s' found in input", key);
			cnctl_exit(PWR_RET_FAILURE);
			// NOT REACHED
			LOG_DBG("NOT REACHED");
		}
	}

	/*
	 * Insert into the attribute request array.
	 */
	rattr->retcode = PWR_RET_OP_NOT_ATTEMPTED;
	g_array_append_val(rattrs, rattr);

	TRACE1_EXIT("rattrs = %p", rattrs);
}

/*
 * parse_json_input - Parse the command to execute in JSON on stdin.
 *
 * Argument(s):
 *
 *	a_flag   - Pointer to buffer that contains the number of times the
 *		   equivalent -a/--attribute parameter is specified
 *	c_flag   - Pointer to buffer that contains the number of times the
 *		   equivalent -c/--command parameter is specified
 *	cmd      - Pointer to buffer to contain command argument
 *	v_flag   - Pointer to buffer that contains the number of times the
 *		   equivalent -v/--value parameter is specified
 *	rattrs   - Pointer to array of attribute requests
 *
 * Return Code(s):
 *
 *	void
 */
void
parse_json_input(int *a_flag, int *c_flag, cmd_type_t *cmd,
		 int *v_flag, GArray *rattrs)
{
	int retval = 0, attr_list_present = 0;
	cson_value *cin = NULL;
	cson_object *cin_obj = NULL;
	cson_object_iterator iter;
	cson_kvp *kvp;	// key/value pair

	TRACE1_ENTER("rattrs = %p", rattrs);

	/*
	 * Initialize input.
	 */
	*cmd = cmd_attr_cap;

	/*
	 * Read JSON data stream sitting in stdin.
	 */
	retval = cson_parse_FILE(&cin, stdin, NULL, NULL);
	if (retval != 0) {
		PRINT_ERR("cson_parse_FILE() failed: %d", retval);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Ensure a valid CSON object is present and read it.
	 */
	if (!cson_value_is_object(cin)) {
		PRINT_ERR("No CSON value found on input stream");
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
	cin_obj = cson_value_get_object(cin);

	/*
	 * Initialize the iterator.
	 */
	retval = cson_object_iter_init(cin_obj, &iter);
	if (retval != 0) {
		PRINT_ERR("cson_object_iter_init() failed: %d", retval);
		cnctl_exit(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Iterate over all input.
	 */
	while ((kvp = cson_object_iter_next(&iter))) {
		cson_string const *ckey = cson_kvp_key(kvp);
		const char *key = cson_string_cstr(ckey);
		cson_value *v = cson_kvp_value(kvp);
		int64_t v_int = (int64_t)cson_value_get_integer(v);
		const char *v_str = (const char *)cson_value_get_cstr(v);

		LOG_DBG("key=%s v_str=%s v_int=%ld", key, v_str, v_int);

		if (!key) {
			continue;
		}

		if (!strcmp(key, JSON_CMD_STR)) {
			/*
			 * Must have been a string.
			 */
			if (!v_str) {
				PRINT_ERR("Invalid command (NULL/%ld) found in "
					  "input", v_int);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Parse command value.
			 */
			if (!strcmp(v_str, JSON_CMD_GET_STR)) {
				*cmd = cmd_get;
			} else if (!strcmp(v_str, JSON_CMD_SET_STR)) {
				*cmd = cmd_set;
			} else if (!strcmp(v_str, JSON_CMD_CAP_ATTR_STR)) {
				*cmd = cmd_attr_cap;
			} else if (!strcmp(v_str, JSON_CMD_CAP_ATTR_VAL_STR)) {
				*cmd = cmd_attr_val_cap;
			} else if (!strcmp(v_str, JSON_CMD_UID_ADD)) {
				*cmd = cmd_uid_add;
			} else if (!strcmp(v_str, JSON_CMD_UID_REMOVE)) {
				*cmd = cmd_uid_remove;
			} else if (!strcmp(v_str, JSON_CMD_UID_CLEAR)) {
				*cmd = cmd_uid_clear;
			} else if (!strcmp(v_str, JSON_CMD_UID_RESTORE)) {
				*cmd = cmd_uid_restore;
			} else if (!strcmp(v_str, JSON_CMD_UID_LIST)) {
				*cmd = cmd_uid_list;
			} else {
				PRINT_ERR("Unknown command '%s' found input",
					  v_str);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			*c_flag += 1;
		} else if (!strcmp(key, JSON_ATTRS_STR)) {
			/*
			 * PWR_Attrs is an array of objects.
			 */
			int i = 0, vlen = 0;
			cson_array *varr = NULL;

			/*
			 * Ensure the value is an array.
			 */
			if (!cson_value_is_array(v)) {
				PRINT_ERR("No %s array found in JSON packet",
					  JSON_ATTRS_STR);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Get the array.
			 */
			varr = cson_value_get_array(v);
			if (varr == NULL) {
				PRINT_ERR("No %s array found in JSON packet",
					  JSON_ATTRS_STR);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Get the array length and error check.
			 */
			vlen = cson_array_length_get(varr);
			if (vlen == 0) {
				PRINT_ERR("Zero length %s array found in JSON "
					  "packet", JSON_ATTRS_STR);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			} else if (vlen > PWR_NUM_ATTR_NAMES) {
				PRINT_ERR("%s array in JSON packet is too large"
					  "(%d elements but should be <= %d) - "
					  "this means there are duplicate or "
					  "invalid attributes in the array",
					  JSON_ATTRS_STR, vlen,
					  PWR_NUM_ATTR_NAMES);
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Parse through the array.
			 */
			for (i = 0; i < vlen; i++) {
				parse_json_attr_list(a_flag, v_flag, rattrs,
						     cson_array_get(varr, i));
			}
			attr_list_present++;
		} else if (!strcmp(key, JSON_UID_STR)) {
			specified_uid = v_int;
		} else if (!strcmp(key, JSON_VERS_MAJ_STR)) {
			client_version_major = v_int;
		} else if (!strcmp(key, JSON_VERS_MIN_STR)) {
			client_version_minor = v_int;
		} else {
			PRINT_ERR("Unexpected key '%s' found in input", key);
			cnctl_exit(PWR_RET_FAILURE);
			// NOT REACHED
			LOG_DBG("NOT REACHED");
		}
	}

	/*
	 * Verify all required key/values are present.
	 */
	if (client_version_major == -1) {
		process_missing_key(JSON_VERS_MAJ_STR);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
	if (client_version_minor == -1) {
		process_missing_key(JSON_VERS_MIN_STR);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
	if (!*c_flag) {
		process_missing_key(JSON_CMD_STR);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}
	if (*cmd == cmd_get || *cmd == cmd_set || *cmd == cmd_attr_val_cap) {
		if (!attr_list_present) {
			process_missing_key(JSON_ATTRS_STR);
		} else if (rattrs->len == 0) {
			process_missing_key(JSON_ATTR_NAME_STR);
		}
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	if ((*cmd == cmd_uid_add || *cmd == cmd_uid_remove) &&
	    (specified_uid == -1)) {
		process_missing_key(JSON_UID_STR);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Clean up and return.
	 */
	cson_value_free(cin);

	TRACE1_EXIT("");
}

/*
 * flush_output - Flush any output.  This is really only dumping the JSON
 *		  data if it exists.
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
flush_output(void)
{
	TRACE1_ENTER("");

	if (output_is_json()) {
		/*
		 * Always set the global return code when flushing.
		 */
		cson_object_set(cout_obj, JSON_RET_CODE_STR,
				cson_value_new_integer(cson_ret_code));

		/*
		 * If no prior messages printed set them to 'null'.
		 */
		if (!cson_object_get(cout_obj, JSON_MSGS_STR)) {
			cson_object_set(cout_obj, JSON_MSGS_STR,
					cson_value_null());
		}
		if (!cson_object_get(cout_obj, JSON_ERR_MSGS_STR)) {
			cson_object_set(cout_obj, JSON_ERR_MSGS_STR,
					cson_value_null());
		}

		/*
		 * Flush...
		 */
		cson_output_FILE(cout, stdout, NULL);
	}

	TRACE1_EXIT("");
}

/*
 * check_attr_retcode - Common code to check if a processed attribute
 *		        encountered any errors.  If outputting JSON we
 *			capture bad return code and continue on error.
 *			If interactive on command line, terminate execution.
 *
 * Argument(s):
 *
 *	rattr   - The attribute being checked
 *
 * Return Code(s):
 *
 *	void
 */
static void
check_attr_retcode(const rattr_t *rattr)
{
	TRACE1_ENTER("attr = '%s', retcode = %d",
			rattr->attr_str, rattr->retcode);

	/*
	 * See if errors were encountered while processing this attribute.
	 */
	if (rattr->retcode != PWR_RET_SUCCESS) {
		if (output_is_json()) {
			/*
			 * Record global return code and continue on error.
			 */
			set_json_ret_code(rattr->retcode);
		} else {
			/*
			 * Terminate execution immediately.
			 */
			cnctl_exit(rattr->retcode);
			// NOT REACHED
			LOG_DBG("NOT REACHED");
		}
	}

	TRACE1_EXIT("");
}

/*
 * print_attr_cap - Print a list of attributes supported by cnctl.
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
print_attr_cap(void)
{
	int i = 0;
	cson_value *c_arrV = NULL;
	cson_array *c_arr = NULL;

	TRACE1_ENTER("");

	/*
	 * Print header (or allocate CSON objs).
	 */
	if (output_is_json()) {
		c_arrV = cson_value_new_array();

		cson_object_set(cout_obj, JSON_ATTR_CAP_STR, c_arrV);

		c_arr = cson_value_get_array(c_arrV);
	} else {
		printf("%s:\n\n", JSON_ATTR_CAP_STR);
	}

	/*
	 * Print/generate JSON.
	 */
	for (i = 0; i < cattrs_count; i++) {
		if (output_is_json()) {
			cson_array_set(c_arr, i, cson_value_new_string(
					cattrs[i].name_str,
					strlen(cattrs[i].name_str)));
		} else {
			printf("\t%s\n", cattrs[i].name_str);
		}
	}

	if (!output_is_json()) {
		printf("\n");
	}

	TRACE1_EXIT("");
}

/*
 * print_attr_val_cap - The user requested a listing of the current
 *		        capabilities.  This includes all valid attributes
 *			and all valid attribute values.
 *
 * Argument(s):
 *
 *	rattr   - The attribute being printed
 *	nvals   - The number of values for this attribute
 *	vals    - The actual values, in string form
 *
 * Return Code(s):
 *
 *	DOES NOT RETURN
 */
void
print_attr_val_cap(const rattr_t *rattr, int nvals, const char **vals)
{
	PWR_AttrName i = PWR_ATTR_PSTATE, j = 0;
	cson_value *attr_tuple_objV = NULL;
	cson_object *attr_tuple_obj = NULL;
	cson_value *attr_val_cap_arrV = NULL;
	cson_array *attr_val_cap_arr = NULL;

	TRACE1_ENTER("attr_str = '%s'", rattr->attr_str);

	/*
	 * See if errors were encountered while processing this attribute.
	 */
	check_attr_retcode(rattr);

	/*
	 * Print header / allocate CSON objects.
	 */
	if (output_is_json()) {
		/*
		 * Attribute tuple (a CSON object).
		 */
		attr_tuple_objV = cson_value_new_object();
		attr_tuple_obj = cson_value_get_object(attr_tuple_objV);
		cson_array_set(attr_tuple_arr, narr_tuples++, attr_tuple_objV);

		/*
		 * Attribute name (a CSON string)
		 */
		cson_object_set(attr_tuple_obj, JSON_ATTR_NAME_STR,
				cson_value_new_string(rattr->attr_str,
					      strlen(rattr->attr_str)));
		/*
		 * Attribute value capabilities (a CSON array)
		 */
		attr_val_cap_arrV = cson_value_new_array();
		cson_object_set(attr_tuple_obj, JSON_ATTR_VAL_CAP_STR,
				attr_val_cap_arrV);
		attr_val_cap_arr = cson_value_get_array(attr_val_cap_arrV);

		/*
		 * Return code for this particular attribute (a CSON array)
		 */
		cson_object_set(attr_tuple_obj, JSON_RET_CODE_STR,
				cson_value_new_integer(rattr->retcode));
	} else {
		printf("%s for %s:\n\n", JSON_ATTR_VAL_CAP_STR,
			rattr->attr_str);
	}

	/*
	 * Output actual value capabilities.
	 */
	for (i = 0; i < nvals; i++) {
		if (output_is_json()) {
			/*
			 * Attribute value (a CSON integer).
			 */
			cson_array_set(attr_val_cap_arr, j,
				       cson_value_new_integer(
				       atoi(vals[i])));
		} else {
			printf("\t%s\n", vals[i]);
		}

		j++;
	}

	if (!output_is_json()) {
		printf("\n");
	}

	TRACE1_EXIT("");
}

/*
 * print_attr_val - Print target attribute value.
 *
 * Argument(s):
 *
 *	rattr     - The attribute being printed
 *	val       - The attribute value being printed
 *	ts        - The timestamp the value was taken at
 *
 * Return Code(s):
 *
 *	void
 */
void
print_attr_val(const rattr_t *rattr, const char *val, PWR_Time ts)
{
	cson_value *attr_tuple_objV = NULL;
	cson_object *attr_tuple_obj = NULL;
	time_t s = ts / NSEC_PER_SEC;
	int ns   = ts % NSEC_PER_SEC;

	TRACE1_ENTER("val = '%s', ts = %lu", val, ts);

	/*
	 * See if errors were encountered while processing this attribute.
	 */
	check_attr_retcode(rattr);

	/*
	 * Print in appropriate format.
	 */
	if (output_is_json()) {
		/*
		 * Attribute tuple (a CSON object).
		 */
		attr_tuple_objV = cson_value_new_object();
		attr_tuple_obj = cson_value_get_object(attr_tuple_objV);
		cson_array_set(attr_tuple_arr, narr_tuples++, attr_tuple_objV);

		/*
		 * Attribute name (a CSON string).
		 */
		cson_object_set(attr_tuple_obj, JSON_ATTR_NAME_STR,
				cson_value_new_string(rattr->attr_str,
					      strlen(rattr->attr_str)));
		/*
		 * Attribute value (a CSON integer).
		 */
		cson_object_set(attr_tuple_obj, JSON_ATTR_VAL_STR,
				cson_value_new_integer(atoi(val)));

		/*
		 * Even though the HPC Power API gives the time in "nanoseconds
		 * since the Epoch" we pack the JSON packet with "seconds since
		 * the Epoch" and a fractional nanoseconds.  This will help if
		 * the JSON packet ever hits a 32-bit interface.
		 */
		cson_object_set(attr_tuple_obj, JSON_TIME_SEC_STR,
				cson_value_new_integer(s));
		cson_object_set(attr_tuple_obj, JSON_TIME_NSEC_STR,
				cson_value_new_integer(ns));

		/*
		 * Return code for this particular attribute (a CSON integer).
		 */
		cson_object_set(attr_tuple_obj, JSON_RET_CODE_STR,
                                cson_value_new_integer(rattr->retcode));
	} else {
		/*
		 * Format timestamp to be something more readable.
		 */
		struct tm tmr;

		if (!localtime_r(&s, &tmr)) {
			PRINT_ERR("!localtime_r() failed");
			cnctl_exit(PWR_RET_FAILURE);
		}

		printf("%s:   %s\n%s:  %s\n%s:       ",
		       JSON_ATTR_NAME_STR, rattr->attr_str,
		       JSON_ATTR_VAL_STR, val, JSON_TIME_STR);

		printf("%04d-%02d-%02d %02d:%02d:%02d.%09d\n",
		       1900 + tmr.tm_year, 1 + tmr.tm_mon, tmr.tm_mday,
		       tmr.tm_hour, tmr.tm_min, tmr.tm_sec, ns);
	}

	TRACE1_EXIT("");
}

/*
 * print_attr_set_result - Print result for set command.
 *
 * Argument(s):
 *
 *	rattr   - The attribute being printed
 *
 * Return Code(s):
 *
 *	void
 */
void
print_attr_set_result(const rattr_t *rattr)
{
	cson_value *attr_tuple_objV = NULL;
	cson_object *attr_tuple_obj = NULL;

	TRACE1_ENTER("rattr = %p", rattr);

	/*
	 * See if errors were encountered while processing this attribute.
	 */
	check_attr_retcode(rattr);

	/*
	 * Only have something to do here if we're outputting in JSON.
	 */
	if (output_is_json()) {
		/*
		 * Attribute tuple (a CSON object).
		 */
		attr_tuple_objV = cson_value_new_object();
		attr_tuple_obj = cson_value_get_object(attr_tuple_objV);
		cson_array_set(attr_tuple_arr, narr_tuples++, attr_tuple_objV);

		/*
		 * Attribute name (a CSON string)
		 */
		cson_object_set(attr_tuple_obj, JSON_ATTR_NAME_STR,
				cson_value_new_string(rattr->attr_str,
					      strlen(rattr->attr_str)));
		/*
		 * Return code for this particular attribute (a CSON array).
		 */
		cson_object_set(attr_tuple_obj, JSON_RET_CODE_STR,
                                cson_value_new_integer(rattr->retcode));
	}

	TRACE1_EXIT("");
}

/*
 * print_perms_list - Print the list of UIDs in the specified file.
 *
 * Argument(s):
 *
 *	filepath    - The file of UIDs
 *
 * Return Code(s):
 *
 *	void
 */
void
print_perms_list(const char *filepath)
{
	int i = 0;
	int retval = 0;
	FILE *fp = NULL;
	unsigned int current = 0;
	cson_value *c_arrV = NULL;
	cson_array *c_arr = NULL;
	struct timespec ts = {0};

	TRACE2_ENTER("");

	fp = fopen(filepath, "r");
	if (fp == NULL) {
		PRINT_ERR("unable to open %s: %m", filepath);
		retval = 1;
		goto error_handling;
	}

	if (output_is_json()) {
		c_arrV = cson_value_new_array();

		cson_object_set(cout_obj, JSON_UIDS_STR, c_arrV);

		c_arr = cson_value_get_array(c_arrV);
	} else {
		printf("%s:\n\n", JSON_UIDS_STR);
	}

	while (fscanf(fp, "%u", &current) == 1) {
		// add uid to PWR_UIDS array
		if (output_is_json()) {
			cson_array_set(c_arr, i++,
				       cson_value_new_integer(current));
		} else {
			printf("\t%u\n", current);
		}
	}

	if (!output_is_json()) {
		printf("\n");
	}

	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		cson_object_set(cout_obj, JSON_TIME_SEC_STR,
				cson_value_new_integer(ts.tv_sec));
		cson_object_set(cout_obj, JSON_TIME_NSEC_STR,
				cson_value_new_integer(ts.tv_nsec));
	}

error_handling:
	if (fp)
		fclose(fp);

	if (retval)
		set_json_ret_code(retval);

	TRACE2_EXIT("retval = %d", retval);
}

/*
 * set_json_ret_code - Sets the return code to be included in any JSON output.
 *
 * Argument(s):
 *
 *	ret_code - Return code - overwrites any previous setting
 *
 * Return Code(s):
 *
 *	void
 */
void
set_json_ret_code(int64_t ret_code)
{
	TRACE1_ENTER("ret_code = %ld", ret_code);

	if (output_is_json()) {
		/*
		 * Always retain the first error code that was hit as the
		 * global error code.
		 */
		if (cson_ret_code == PWR_RET_SUCCESS) {
			cson_ret_code = ret_code;
		} else {
			LOG_DBG("ret_code=%ld not recorded as global",
				  ret_code);
		}
	}

	TRACE1_EXIT("");
}

/*
 * _print - Internal
 *
 * Argument(s):
 *
 *	is_err   - Non-zero if error, zero if regular message
 *	format   - Format string for error message
 *	...      - VarArgs argument for format string
 *
 * Return Code(s):
 *
 *	void
 */
void
_print(const char *func, int line, int is_err, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	if (output_is_json()) {
		cson_value **msgsV = &cmsgs;
		cson_array *msgs = NULL;
		int *nmsgs = &num_cmsgs;
		char str[MAX_MSG_STR];

		/*
		 * Initialize the error array if not yet done.
		 */
		if (is_err) {
			msgsV = &cerrmsgs;
			nmsgs = &num_cerrmsgs;
		}

		if (*msgsV == NULL) {
			*msgsV = cson_value_new_array();
			if (is_err) {
				cson_object_set(cout_obj, JSON_ERR_MSGS_STR,
						*msgsV);
			} else {
				cson_object_set(cout_obj, JSON_MSGS_STR, *msgsV);
			}
		}

		/*
		 * Append a new error string.
		 */
		msgs = cson_value_get_array(*msgsV);

		str[0] = '\0';
		if (is_err) {
			snprintf(str, MAX_MSG_STR, "ERROR(%s:%d): ",
				 func, line);
		}

		// TODO: math is wrong, this clips first char
		vsnprintf(str + strlen(str) - 1, MAX_MSG_STR - strlen(str),
			  format, ap);

		cson_array_set(msgs, *nmsgs, cson_value_new_string(
						str, strlen(str)));
		*nmsgs += 1;
	} else {
		FILE *stream = stdout;

		if (is_err) {
			stream = stderr;
			fprintf(stream, "ERROR(%s:%d): ", func, line);
		}
		vfprintf(stream, format, ap);
		fprintf(stream, "\n");
	}

	va_end(ap);
}

/*
 * json_attr_cap_init - Attr capabilities initialization.
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
json_attr_cap_init(void)
{
	TRACE1_ENTER("");

	/*
	 * Set up the attribute tuple array.
	 */
	attr_tuple_arrV = cson_value_new_array();
	cson_object_set(cout_obj, JSON_ATTRS_STR, attr_tuple_arrV);
	attr_tuple_arr = cson_value_get_array(attr_tuple_arrV);

	TRACE1_EXIT("");
}

/*
 * enable_json_output - Enable JSON output.
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
enable_json_output(void)
{
	TRACE1_ENTER("");

	/*
	 * Basic CSON initialization.
	 */
	cout = cson_value_new_object();
	cout_obj = cson_value_get_object(cout);

	/*
	 * Set version information in the return packet.
	 */
	cson_object_set(cout_obj, JSON_VERS_MAJ_STR,
			cson_value_new_integer(CNCTL_MAJOR_VERSION));
	cson_object_set(cout_obj, JSON_VERS_MIN_STR,
			cson_value_new_integer(CNCTL_MINOR_VERSION));

	TRACE1_EXIT("");
}

/*
 * disable_json_output - Disable JSON output.
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
disable_json_output(void)
{
	TRACE1_ENTER("");

	if (cout) {
		cson_value_free(cout);
		cout = NULL;
	}

	TRACE1_EXIT("");
}

