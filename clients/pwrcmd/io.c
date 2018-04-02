/*
 * Copyright (c) 2016, 2018, Cray Inc.
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
#include <glib.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "api.h"
#include "io.h"
#include "pwrcmd.h"

#define MAX_MSG_STR	1024

static cson_value *cout = NULL;			// main CSON output data struct
static cson_object *cout_obj = NULL;		// main CSON output object

static cson_value *cmsgs = NULL;		// messages, if any
static cson_value *cerrmsgs = NULL;		// error messages, if any
static int num_cmsgs = 0;			// number of messages
static int num_cerrmsgs = 0;			// number of error messages
static int num_objects = 0;			// number of objects in structure

static int cson_ret_code = PWR_RET_SUCCESS;	// return code

/*
 * json_enable_output - Enable JSON output.
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
json_enable_output(void)
{
	TRACE1_ENTER("");

	/*
	 * Destroy any prior structure.
	 */
	json_disable_output();
	/*
	 * Basic CSON initialization.
	 */
	cout = cson_value_new_object();
	cout_obj = cson_value_get_object(cout);
	num_cmsgs = 0;
	num_cerrmsgs = 0;
	num_objects = 0;
	cson_ret_code = PWR_RET_SUCCESS;

	TRACE1_EXIT("");
}

/*
 * json_disable_output - Disable JSON output.
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
json_disable_output(void)
{
	TRACE1_ENTER("");

	if (cout) {
		// Freeing the value frees everything
		cson_value_free(cout);
		cout = NULL;
		cout_obj = NULL;
	}

	TRACE1_EXIT("");
}

/*
 * json_is_enabled - Are we printing output in JSON?
 *
 * Argument(s):
 *
 *	void
 *
 * Return Code(s):
 *
 *	int - 0 if output is JSON, 1 otherwise
 */
int
json_is_enabled(void)
{
	return cout_obj != NULL;
}

/*
 * json_set_ret_code - Sets the return code to be included in any JSON output.
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
json_set_ret_code(int64_t ret_code)
{
	TRACE1_ENTER("ret_code = %ld", ret_code);

	if (json_is_enabled()) {
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
 * json_flush_output - Flush any output. This is really only dumping the JSON
 *		  data if it exists.
 *
 * Argument(s):
 *
 *	force - flush a JSON structure with error code even if JSON is empty
 *
 * Return Code(s):
 *
 *	void
 */
void
json_flush_output(bool force)
{
	TRACE1_ENTER("");

	if (json_is_enabled() && (force || num_objects > 0)) {
		/*
		 * Options for displaying the JSON.
		 */
		cson_output_opt opts = {
			.indentation = 4,
			.maxDepth = 25,
			.addNewline = true,
			.addSpaceAfterColon = true,
			.indentSingleMemberValues = true,
			.escapeForwardSlashes = false
		};

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
		cson_output_FILE(cout, stdout, &opts);
		/*
		 * Reset the output structure
		 */
		json_enable_output();
	}

	TRACE1_EXIT("");
}

/*
 * json_print - Internal
 *
 * Argument(s):
 *
 *      func     - name of the calling function
 *      line     - line number of the calling function
 *	errcode  - Non-zero if error, zero if regular message
 *	format   - Format string for error message
 *	...      - VarArgs argument for format string
 *
 * Return Code(s):
 *
 *	void
 */
void
json_print(const char *func, int line, int errcode, const char *format, ...)
{
	va_list ap;

	TRACE2_ENTER("func = %s, line = %d, errcode = %d, format = %s",
		    func, line, errcode, format);

	va_start(ap, format);

	if (json_is_enabled()) {
		cson_value **msgsV = &cmsgs;
		cson_array *msgs = NULL;
		int *nmsgs = &num_cmsgs;
		char str[MAX_MSG_STR];
		char *p = &str[0];
		char *e = &str[MAX_MSG_STR];

		/*
		 * If processing an error, use a different list
		 */
		if (errcode) {
			msgsV = &cerrmsgs;
			nmsgs = &num_cerrmsgs;
			// first error code seen will stick
			json_set_ret_code(errcode);
		}

		/*
		 * Initialize the message array if not yet done, and attach it to the top
		 * level structure.
		 */
		if (*msgsV == NULL) {
			*msgsV = cson_value_new_array();
			if (errcode) {
				cson_object_set(cout_obj, JSON_ERR_MSGS_STR,
						*msgsV);
			} else {
				cson_object_set(cout_obj, JSON_MSGS_STR,
						*msgsV);
			}
		}

		/*
		 * Generate the message string.
		 */
		str[0] = '\0';
		if (errcode) {
			p += snprintf(p, e-p, "ERROR(%s:%d) %d: ",
				      func, line, errcode);
		}
		p += vsnprintf(p, e-p, format, ap);

		/*
		 * Add the message string to the array.
		 */
		msgs = cson_value_get_array(*msgsV);
		cson_array_set(msgs, *nmsgs,
			       cson_value_new_string(str, strlen(str)));
		*nmsgs += 1;
		num_objects += 1;
	} else {
		/*
		 * Just print the message to stdout or stderr.
		 */
		FILE *stream = stdout;
		if (errcode) {
			stream = stderr;
			fprintf(stream, "ERROR(%s:%d) %d: ",
				func, line, errcode);
		}
		vfprintf(stream, format, ap);
		fprintf(stream, "\n");
	}

	va_end(ap);

	TRACE1_EXIT("");
}

/**
 * Do a generic addition to either an object, or an array.
 *
 * 'base' can be an array, or an object, or NULL (which is interpreted as the
 * root JSON object). cson exports these as type-differentiated, but otherwise
 * opaque, and has no disambiguating features.
 *
 * We use the 'name' pointer to disambiguate. If 'base' is an object (or NULL),
 * then the 'name' value MUST be non-NULL (objects contain named elements). If
 * 'base' is an array, then the 'name' value MUST be NULL (arrays don't have
 * named elements).
 *
 * We can detect the invalid case of 'base' and 'name' both being NULL, (meaning
 * we are trying to add an unnamed value to an object), but we cannot detect the
 * invalid case of 'base' representing an array and 'name' being non-NULL
 * (meaning we are trying to add a named element to an array) -- results are
 * undefined if you try to do this, but it will probably be bad.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 * @param v - value to add
 *
 * @return bool - true if successful, false otherwise
 */
bool
json_generic_add(void *base, const char *name, cson_value *v)
{
	bool retval = false;
	int rval;

	TRACE3_ENTER("base = %p, name = %s, cson_value = %p",
		    base, name, v);

	if (!base && !name)
		goto error_handling;

	if (!base || name) {
		cson_object *objp = (cson_object *)base;
		rval = cson_object_set((objp) ? objp : cout_obj, name, v);
	} else {
		cson_array *list = (cson_array *)base;
		rval = cson_array_append(list, v);
	}
	if (rval != 0) {
		cson_value_free(v);
		goto error_handling;
	}
	retval = true;

error_handling:
	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Add a 'null' entry.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 *
 * @return bool - true if successful
 */
bool
json_add_null(void *base, const char *name)
{
	int retval = true;
	TRACE3_ENTER("base = %p, name = %s", base, name);

	if (json_is_enabled()) {
		cson_value *v = cson_value_null();
		if ((retval = json_generic_add(base, name, v)))
			num_objects += 1;
	} else if (name) {
		printf("%s: (nil)\n", name);
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Add an integer.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 * @param value - integer value to set
 *
 * @return bool - true if successful
 */
bool
json_add_integer(void *base, const char *name, cson_int_t value)
{
	int retval = true;
	TRACE3_ENTER("base = %p, name = %s, value = %ld", base, name, value);

	if (json_is_enabled()) {
		cson_value *v = cson_value_new_integer(value);
		if ((retval = json_generic_add(base, name, v)))
			num_objects += 1;
	} else if (name) {
		printf("%s: %ld\n", name, value);
	} else {
		printf("  %ld\n", value);
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Add a double.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 * @param value - double value to set
 *
 * @return bool - true if successful
 */
bool
json_add_double(void *base, const char *name, cson_double_t value)
{
	int retval = true;
	TRACE3_ENTER("base = %p, name = %s, value = %.0lf", base, name, value);

	if (json_is_enabled()) {
		cson_value *v = cson_value_new_double(value);
		if ((retval = json_generic_add(base, name, v)))
			num_objects += 1;
	} else if (name) {
		printf("%s: %.0lf\n", name, value);
	} else {
		printf("  %.0lf\n", value);
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Add a string.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 * @param string - string value to set
 *
 * @return bool - true if successful
 */
bool
json_add_string(void *base, const char *name, const char *string)
{
	int retval = true;
	TRACE3_ENTER("base = %p, name = %s, string = %s", base, name, string);

	if (json_is_enabled()) {
		cson_value *v = cson_value_new_string(string, strlen(string));
		if ((retval = json_generic_add(base, name, v)))
			num_objects += 1;
	} else if (name) {
		printf("%s: %s\n", name, string);
	} else {
		printf("  %s\n", string);
	}

	TRACE3_EXIT("retval = %d", retval);
	return retval;
}

/**
 * Add an object to an array or object, and return a pointer to the new object.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 *
 * @return cson_object* - new object created, or NULL on error
 */
cson_object *
json_add_object(void *base, const char *name)
{
	cson_object *obj = NULL;
	TRACE3_ENTER("base = %p, name = %s", base, name);

	if (json_is_enabled()) {
		cson_value *objV = cson_value_new_object();

		if (json_generic_add(base, name, objV)) {
			obj = cson_value_get_object(objV);
			num_objects += 1;
		}
	}

	TRACE3_EXIT("obj = %p", obj);
	return obj;
}

/**
 * Add an array to an array or object, and return a pointer to the new array.
 *
 * @param base - array, object, or NULL (== root object)
 * @param name - tag name, or NULL for adding to an array
 *
 * @return cson_array* - new array created, or NULL on error
 */
cson_array *
json_add_array(void *base, const char *name)
{
	cson_array *list = NULL;
	TRACE3_ENTER("base = %p, name = %s", base, name);

	if (json_is_enabled()) {
		cson_value *listV = cson_value_new_array();

		if (json_generic_add(base, name, listV)) {
			list = cson_value_get_array(listV);
			num_objects += 1;
		}
	}

	TRACE3_EXIT("list = %p", list);
	return list;
}

