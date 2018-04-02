/*
 * Copyright (c) 2016-2017, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.

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

#ifndef __PWRCMD_API_H
#define __PWRCMD_API_H

//
// cmd_type_t - An enum representing the requested command type.
//
typedef enum {
	cmd_invalid = -1,	// Invalid command enum
	cmd_get,		// 'get' command
	cmd_set,		// 'set' command
	cmd_list,		// 'list' command
	cmd_trav,		// 'trav' command
} cmd_type_t;

//
// list_type_t - enum representing the type of list requested
//
typedef enum {
	list_invalid = -1,	// Invalid list enum
	list_all,
	list_attr,
	list_name,
	list_hier,
} list_type_t;

//
// trav_type_t - enum representing the type of traversal requested
//
typedef enum {
	trav_invalid = -1,	// Invalid traversal enum
	trav_up,
	trav_down,
} trav_type_t;

//
// val_type_t - enum representing the type of an attribute or
//              metadata value.
//
typedef enum {
	val_is_invalid = -1,
	val_is_whole,
	val_is_real,
	val_is_time,
	val_is_string,
} val_type_t;

//
// cmd_val_t - union with space enough for any attr type value
//
typedef union cmd_val_s {
	uint64_t whole;
	double   real;
	PWR_Time time;
	char     str[CRAY_PWR_MAX_STRING_SIZE];
} cmd_val_t;

//
// cmd_opt_t - the options for a command to be executed
//
typedef struct cmd_opt_s {
	cmd_type_t type;
	PWR_Role   role;
	int  retcode;

	// Command subtype value
	list_type_t list;
	trav_type_t trav;
	PWR_AttrName attr;
	PWR_MetaName meta;
	int index;

	char *name_str;
	char *attr_str;
	char *val_str;
	cmd_val_t val;
	GSList *names;
	GSList *attrs;
	GSList *values;
	int names_cnt;
	int attrs_cnt;
	int values_cnt;
} cmd_opt_t;

// Initialze cmd_opt_t.  Default role of PWR_ROLE_APP, all other values are
// initialized to invalid values.
#define INIT_CMD_OPT()				\
	{					\
		.type = cmd_invalid,		\
		.role = PWR_ROLE_APP,		\
		.list = list_invalid,		\
		.trav = trav_invalid,		\
		.attr = PWR_ATTR_NOT_SPECIFIED,	\
		.meta = PWR_MD_NOT_SPECIFIED,	\
		.index = -1,			\
		.name_str = NULL,		\
		.attr_str = NULL,		\
		.val_str = NULL,		\
		.val.whole = 0,			\
		.retcode = 0,			\
		.names = NULL,			\
		.attrs = NULL,			\
		.values = NULL,			\
		.names_cnt = 0,			\
		.attrs_cnt = 0,			\
		.values_cnt = 0			\
	}

//
// enum_map_t - used to map a string to the corresponding enum value
//
typedef struct enum_map_t {
	const char *name;	// enum value as string
	int supported;		// set if enum value is supported
} enum_map_t;

extern enum_map_t role_enum[PWR_NUM_ROLES];

void reset_cmd_opt(cmd_opt_t *cmd_opt);

void api_init(PWR_Role role);
void api_cleanup(void);
int  get_role_enum(char *role_str);
int  get_meta_enum(char *meta_str);
void cmd_get_list(cmd_opt_t *cmd_opt);
void cmd_get_attr(cmd_opt_t *cmd_opt);
void cmd_get_attrs(cmd_opt_t *cmd_opt);
void cmd_get_grp_attr(cmd_opt_t *cmd_opt);
void cmd_get_grp_attrs(cmd_opt_t *cmd_opt);
void cmd_set_attr(cmd_opt_t *cmd_opt);
void cmd_set_attrs(cmd_opt_t *cmd_opt);
void cmd_set_grp_attr(cmd_opt_t *cmd_opt);
void cmd_set_grp_attrs(cmd_opt_t *cmd_opt);
void cmd_get_meta(cmd_opt_t *cmd_opt);
void cmd_get_meta_at_index(cmd_opt_t *cmd_opt);
void cmd_set_meta(cmd_opt_t *cmd_opt);
void cmd_get_parent(cmd_opt_t *cmd_opt);
void cmd_get_children(cmd_opt_t *cmd_opt);

#endif /* ifndef __PWRCMD_API_H */

