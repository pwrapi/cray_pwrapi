/*
 * Copyright (c) 2016, Cray Inc.
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

#ifndef __PWRCMD_IO_H
#define __PWRCMD_IO_H

#include <stdbool.h>
#include "../cson/cson_amalgamation_core.h"

#define JSON_RET_CODE_STR		"PWR_ReturnCode"
#define JSON_MSGS_STR			"PWR_Messages"
#define JSON_ERR_MSGS_STR		"PWR_ErrorMessages"

/*
 * These macros should be used to display informational messages or errors.
 *
 * When json_is_enabled() == true, these will append to either the PWR_Messages
 * or the PWR_ErrorMessages array. If the errcode value is 0, these are
 * informational messages. If the errcode value is not 0, these are error
 * messages. The call with errcode != 0 will automatically register the error
 * using json_set_ret_code().
 */
#define PRINT(fmt, args...) \
	json_print(__func__, __LINE__, 0, fmt, ## args);
#define	PRINT_ERR(fmt, args...) \
	json_print(__func__, __LINE__, PWR_RET_FAILURE, fmt, ## args);
#define	PRINT_ERRCODE(errcode, fmt, args...) \
	json_print(__func__, __LINE__, errcode, fmt, ## args);

void json_print(const char *func, int line, int errcode,
		       const char *format, ...)
	__attribute__ (( __format__ (__printf__, 4, 5) ));

/*
 * Control functions.
 */
void json_enable_output(void);
void json_disable_output(void);
int json_is_enabled(void);
void json_set_ret_code(int64_t ret_code);

/*
 * Flushing the output renders the JSON and then clears the JSON structures.
 */
void json_flush_output(bool force);

/*
 * These will render as simple text if json_is_enabled() == false.
 */
bool json_add_null(void *base, const char *name);
bool json_add_integer(void *base, const char *name, cson_int_t value);
bool json_add_double(void *base, const char *name, cson_double_t value);
bool json_add_string(void *base, const char *name, const char *string);

/*
 * These will fail silently and return NULL if json_is_enabled() == false.
 */
cson_object *json_add_object(void *base, const char *name);
cson_array *json_add_array(void *base, const char *name);

#endif /* ifndef __PWRCMD_IO_H */

