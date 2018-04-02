/*
 * Copyright (c) 2017, Cray Inc.
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
 */

#ifndef _PWR_PWR_LIST_H
#define _PWR_PWR_LIST_H

#define PWR_LIST_INIT { 0 }

typedef struct pwr_list_uint64_s {
	size_t		alloc;
	size_t		num;
	uint64_t	*list;
	size_t		value_len;
	uint64_t	min;
	uint64_t	max;
} pwr_list_uint64_t;

typedef struct pwr_list_double_s {
	size_t		alloc;
	size_t		num;
	double		*list;
	size_t		value_len;
	double		min;
	double		max;
} pwr_list_double_t;

typedef struct pwr_list_string_s {
	size_t		alloc;
	size_t		num;
	char		**list;
	size_t		value_len;
} pwr_list_string_t;

void pwr_list_init_uint64(pwr_list_uint64_t *list);
void pwr_list_init_double(pwr_list_double_t *list);
void pwr_list_init_string(pwr_list_string_t *list);

void pwr_list_free_uint64(pwr_list_uint64_t *list);
void pwr_list_free_double(pwr_list_double_t *list);
void pwr_list_free_string(pwr_list_string_t *list);

int pwr_list_add_uint64(pwr_list_uint64_t *list, uint64_t val);
int pwr_list_add_double(pwr_list_double_t *list, double val);
int pwr_list_add_string(pwr_list_string_t *list, const char *str);

int pwr_list_add_str_uint64(pwr_list_uint64_t *list, const char *str);
int pwr_list_add_str_double(pwr_list_double_t *list, const char *str);

void pwr_list_sort_uint64(pwr_list_uint64_t *list);
void pwr_list_sort_double(pwr_list_double_t *list);

int pwr_list_value_at_index_uint64(pwr_list_uint64_t *list, unsigned int index,
		uint64_t *value, char *value_str);
int pwr_list_value_at_index_double(pwr_list_double_t *list, unsigned int index,
		double *value, char *value_str);
int pwr_list_value_at_index_string(pwr_list_string_t *list, unsigned int index,
		uint64_t *value, char *valur_str,
		int (*string_to_value)(const char *str));

#endif /* _PWR_PWR_LIST_H */
