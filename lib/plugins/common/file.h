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

#ifndef _PWR_PLUGINS_COMMON_FILE_H
#define _PWR_PLUGINS_COMMON_FILE_H

#include <time.h>

#include "common.h"

int read_val_from_file(const char *path, void *val, val_type_t type,
		struct timespec *tspec);

int read_line_from_file(const char *path, unsigned int num, char **line,
		struct timespec *tspec);

static inline int
read_uint64_from_file(const char *path, uint64_t *val, struct timespec *tspec)
{
	return read_val_from_file(path, val, TYPE_UINT64, tspec);
}

static inline int
read_double_from_file(const char *path, double *val, struct timespec *tspec)
{
	return read_val_from_file(path, val, TYPE_DOUBLE, tspec);
}

static inline int
read_string_from_file(const char *path, char **val, struct timespec *tspec)
{
	return read_val_from_file(path, val, TYPE_STRING, tspec);
}

#endif /* _PWR_PLUGINS_COMMON_FILE_H */
