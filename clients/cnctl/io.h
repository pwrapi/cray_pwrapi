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

#ifndef __CNCTL_IO_H
#define __CNCTL_IO_H

#include <glib.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "api.h"

/*
 * PRINT - Standard print function to print to stdout.
 */
#define PRINT(fmt, args...)						\
	_print(__func__, __LINE__, 0, fmt, ## args);

/*
 * PRINT_ERR - Standard print function to print to stderr.
 */
#define PRINT_ERR(fmt, args...)						\
	_print(__func__, __LINE__, 1, fmt, ## args);

extern void _print(const char *func, int line, int is_err,
		   const char *format, ...)
	__attribute__ (( __format__ (__printf__, 4, 5) ));
extern void print_attr_cap(void);
extern void print_attr_val_cap(const rattr_t *rattr, int nvals, const char **vals);
extern void print_attr_val(const rattr_t *rattr, const char *val, PWR_Time ts);
extern void print_attr_set_result(const rattr_t *rattr);
extern void print_perms_list(const char *filepath);
extern void flush_output(void);

extern void parse_json_input(int *a_flag, int *c_flag, cmd_type_t *cmd,
			     int *v_flag, GArray *rattrs);
extern void enable_json_output(void);
extern void disable_json_output(void);
extern void json_attr_cap_init(void);

extern void set_json_ret_code(int64_t ret_code);

#endif /* ifndef __CNCTL_IO_H */

