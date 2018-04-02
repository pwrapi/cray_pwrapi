//
// Copyright (c) 2016, 2018, Cray Inc.
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
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#include <glib.h>
#include <errno.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "deps/linenoise/linenoise.h"
#include "api.h"
#include "io.h"
#include "pwrcmd.h"

// Data structures for parsing command line arguments.
enum {
	cmdline_MIN = 1000,
	cmdline_attribute,
	cmdline_command,
	cmdline_help,
	cmdline_interactive,
	cmdline_json,
	cmdline_list,
	cmdline_metadata,
	cmdline_name,
	cmdline_role,
	cmdline_script,
	cmdline_trav,
	cmdline_value,
	cmdline_index,
	cmdline_debug,
	cmdline_trace,
	cmdline_MAX
};

typedef enum {
	i_none = -1,
	i_help = 0,
	i_do,
	i_quit
} i_cmd_t;

static const char *Short_Options = "a:c:hijl:m:n:r:st:v:x:DT";
static struct option Long_Options[] = {
	{ "attribute",      required_argument,	NULL, cmdline_attribute },
	{ "command",        required_argument,	NULL, cmdline_command },
	{ "help",           no_argument,	NULL, cmdline_help },
	{ "interactive",    no_argument,	NULL, cmdline_interactive },
	{ "json",           no_argument,	NULL, cmdline_json },
	{ "list",           required_argument,  NULL, cmdline_list },
	{ "metadata",       required_argument,  NULL, cmdline_metadata },
	{ "name",           required_argument,	NULL, cmdline_name },
	{ "role",           required_argument,	NULL, cmdline_role },
	{ "script",         no_argument,	NULL, cmdline_script },
	{ "trav",           required_argument,	NULL, cmdline_trav },
	{ "value",          required_argument,	NULL, cmdline_value },
	{ "index",          required_argument,  NULL, cmdline_index },
	{ "debug",          no_argument,	NULL, cmdline_debug },
	{ "trace",          no_argument,	NULL, cmdline_trace },
	{ 0, 0, 0, 0 }
};

typedef struct {
	int a_flag;             // attribute flag
	int c_flag;             // command flag
	int h_flag;             // help flag
	int i_flag;             // interactive flag
	int l_flag;             // list flag
	int m_flag;             // metadata flag
	int n_flag;             // name flag
	int r_flag;             // role flag
	int s_flag;             // script flag
	int t_flag;             // traverse flag
	int v_flag;             // value flag
	int x_flag;             // index flag
	int D_flag;             // debug flag
	int T_flag;             // trace flag
} cmd_flags_t;

static bool interactive = false; // Flag to run in interactive mode
static i_cmd_t interactive_cmd = i_none;

static void
interactive_help(void)
{
	static const char *fmt =
			"Commands:\n"
			"    do         Execute a command.\n"
			"    help       This help screen.\n"
			"    quit       Quit interactive mode.\n"
			"\n"
			"Command help option:\n"
			"    -h         Print help for specified command\n"
	;
	TRACE1_ENTER("");

	printf(fmt);

	TRACE1_EXIT("");
}

static void
interactive_help_quit(void)
{
	static const char *fmt =
			"Quit interactive mode.\n"
			"\n"
			"   quit [ -h ]\n"
			"\n"
			"Options:\n"
			"\n"
			"   -h/--help       Print this help message, all other options ignored\n"
			"\n"
	;
	TRACE1_ENTER("");

	printf(fmt);

	TRACE1_EXIT("");
}

static void
interactive_help_do(void)
{
	static const char *fmt =
			"\n"
			"   do -h |\n"
			"      -c command [ -n name[,name...]\n"
			"                     [ -a attribute[,attribute...] ]\n"
			"                     [ -v value[,value...] ]\n"
			"                     [ -m metadata ]"
			"                 ]\n"
			"                 [ -l list ] [ -t traverse -n name ]\n"
			"                 [ -r role ] [ -jsDT ]\n"
			"\n"
			"Options:\n"
			"\n"
			"   -a/--attribute     The attribute to target\n"
			"   -c/--command       The command to perform:\n"
			"\n"
			"                          get   Get the attribute or metadata value.\n"
			"                                If -m option is specified the operation\n"
			"                                is on metadata, else it is an operation\n"
			"                                on an attribute.\n"
			"\n"
			"                                Metadata:\n"
			"                                    -n and -m required, -a optional\n"
			"\n"
			"                                Attribute:\n"
			"                                    -n and -a required\n"
			"\n"
			"                          set   Set the attribute or metadata value.\n"
			"                                If -m option is specified the operation\n"
			"                                is on metadata, else it is an operation\n"
			"                                on an attribute.\n"
			"\n"
			"                                Metadata:\n"
			"                                    -n, -m, -v required, -a optional\n"
			"\n"
			"                                Attribute:\n"
			"                                    -n, -a, -v required\n"
			"\n"
			"                          trav  Traverse and display object names\n"
			"\n"
			"                          list  List attributes, names, hierarchy or all\n"
			"                                Required: -l option specified\n"
			"\n"
			"   -h/--help          Print this help message, all other options ignored\n"
			"   -j/--json          Use JSON output\n"
			"   -l/--list          List to display\n"
			"\n"
			"                          all   All of the following lists\n"
			"                          attr  List of supported attributes\n"
			"                          name  List of available power object names\n"
			"                          hier  Hierarchal view of available power objects\n"
			"\n"
			"   -m/--metadata      Name of the metadata to target\n"
			"   -n/--name          Name of power object to target\n"
			"   -s/--script        Suppress JSON output for scripting\n"
			"   -t/--trav          Direction to travel in the hierarchy\n"
			"\n"
			"                          up    Display parent name\n"
			"                          down  Display children names\n"
			"\n"
			"   -v/--value         Input data value to act upon\n"
			"   -x/--index         Index of value to target\n"
			"   -D/--debug         Increase debug level to stderr\n"
			"   -T/--trace         Increase trace level to stderr\n"
			"\n"
			"   -D   -> display DBG1\n"
			"   -DD  -> display DBG1 and DBG2\n"
			"   -T   -> display TRC1\n"
			"   -TT  -> display TRC1 and TRC2\n"
			"   -TTT -> display TRC1, TRC2, and TRC3\n"
			"\n"
	;
	TRACE1_ENTER("");

	printf(fmt);

	TRACE1_EXIT("");
}


//
// usage - Print usage statement.
//
// Argument(s):
//
//	exit_code - exit code for application exit
//
// Return Code(s):
//
//	DOES NOT RETURN
//
static void __attribute__((noreturn))
usage(int exit_code)
{
	static const char *fmt =
			"\n"
			"Usage: pwrcmd  -h | \n"
			"               -i [ -r role ] [ -js ] | \n"
			"               -c command [ -n name[,name...]\n"
			"                              [ -a attribute[,attribute...] ]\n"
			"                              [ -v value[,value...] ]\n"
			"                              [ -m metadata ]"
			"                          ]\n"
			"                          [ -l list ] [ -t traverse -n name ]\n"
			"                          [ -r role ] [ -jsDT ]\n"
			"\n"
			"Options:\n"
			"\n"
			"   -a/--attribute     The attribute to target\n"
			"   -c/--command       The command to perform:\n"
			"\n"
			"                          get   Get the attribute or metadata value.\n"
			"                                If -m option is specified the operation\n"
			"                                is on metadata, else it is an operation\n"
			"                                on an attribute.\n"
			"\n"
			"                                Metadata:\n"
			"                                    -n and -m required, -a optional\n"
			"\n"
			"                                Attribute:\n"
			"                                    -n and -a required\n"
			"\n"
			"                          set   Set the attribute or metadata value.\n"
			"                                If -m option is specified the operation\n"
			"                                is on metadata, else it is an operation\n"
			"                                on an attribute.\n"
			"\n"
			"                                Metadata:\n"
			"                                    -n, -m, -v required, -a optional\n"
			"\n"
			"                                Attribute:\n"
			"                                    -n, -a, -v required\n"
			"\n"
			"                          trav  Traverse and display object names\n"
			"\n"
			"                          list  List attributes, names, hierarchy or all\n"
			"                                Required: -l option specified\n"
			"\n"
			"   -h/--help          Print this help message, all other options ignored\n"
			"   -i/--interactive   Enter interactive mode\n"
			"   -j/--json          Use JSON output\n"
			"   -l/--list          List to display\n"
			"\n"
			"                          all   All of the following lists\n"
			"                          attr  List of supported attributes\n"
			"                          name  List of available power object names\n"
			"                          hier  Hierarchal view of available power objects\n"
			"\n"
			"   -m/--metadata      Name of the metadata to target\n"
			"   -n/--name          Name of power object to target\n"
			"   -r/--role          Role to use when creating context\n"
			"\n"
			"                          PWR_ROLE_APP    Application\n"
			"                          PWR_ROLE_MC     Monitor and Control\n"
			"                          PWR_ROLE_OS     Operating System\n"
			"                          PWR_ROLE_USER   User\n"
			"                          PWR_ROLE_RM     Resource Manager\n"
			"                          PWR_ROLE_ADMIN  Administrator\n"
			"                          PWR_ROLE_MGR    HPCS Manager\n"
			"                          PWR_ROLE_ACC    Accounting\n"
			"\n"
			"                      If running in interactive mode, this option will be\n"
			"                      ignored for commands. Only the role specified on the\n"
			"                      command line (vs. prompt line) is used."
			"\n"
			"   -s/--script        Suppress JSON output for scripting\n"
			"   -t/--trav          Direction to travel in the hierarchy\n"
			"\n"
			"                          up    Display parent name\n"
			"                          down  Display children names\n"
			"\n"
			"   -v/--value         Input data value to act upon\n"
			"   -x/--index         Index of value to target\n"
			"   -D/--debug         Increase debug level to stderr\n"
			"   -T/--trace         Increase trace level to stderr\n"
			"\n"
			"   -D   -> display DBG1\n"
			"   -DD  -> display DBG1 and DBG2\n"
			"   -T   -> display TRC1\n"
			"   -TT  -> display TRC1 and TRC2\n"
			"   -TTT -> display TRC1, TRC2, and TRC3\n"
			"\n"
	;
	TRACE1_ENTER("exit_code = %d", exit_code);

	fprintf((exit_code) ? stderr : stdout, fmt, g_get_prgname());

	TRACE1_EXIT("exit_code = %d", exit_code);
	exit(exit_code);
}

//
// force_exit - close down api and force exit
//
// Argument(s):
//
//	exit_code - Exit code
//
// Return Code(s):
//
//	Never returns.
//
//
//
void
force_exit(int exit_code)
{
	TRACE1_ENTER("exit_code = %d", exit_code);

	//
	// Only flush if there is something to flush
	//
	json_flush_output(false);

	//
	// Be nice and cleanup before exiting if possible.
	//
	api_cleanup();

	//
	// Exit with requested code.
	//
	if (exit_code < 0) {
		LOG_CRIT("Exit with error code %d", exit_code);
		exit(1);
	}
	exit(0);
}

static int
help_try_exit(int status)
{
	TRACE1_ENTER("status = %d", status);

	if (interactive) {
		// Only flush if there is something to flush
		json_flush_output(false);
		switch (interactive_cmd) {
		case i_none:
			interactive_help();
			break;
		case i_do:
			interactive_help_do();
			break;
		case i_help:
			interactive_help();
			break;
		case i_quit:
			interactive_help_quit();
			break;
		default:
			interactive_help();
			break;
		}
		TRACE1_EXIT("interactive");
		return 1;
	}

	// If not interactive, just display usage and quit
	usage(status);
}

/**
 * Parse comma-separated arguments.
 *
 * This returns the count of arguments found (number of commas plus 1) in *cnt.
 *
 * If only one argument is found, it is placed in *v1, which is a pointer to a
 * (char *) that will receive the argument pointer.
 *
 * If multiple arguments are found, they are linked to the *vN list.
 *
 * @param cnt - returned count of arguments
 * @param v1 - pointer to return argument if *cnt == 1
 * @param vN - pointer to return GSList if *cnt > 1
 * @param arg - argument to parse
 *
 * @return bool - true if successful, false otherwise
 */
static bool
parse_cmdopt_arg(int *cnt, char **v1, GSList **vN, char *arg)
{
	char **tokv = NULL;
	int  tokc;
	bool retval = false;

	if (!(tokv = g_strsplit_set(arg, ",", -1)))
		goto done;
	for (tokc = 0; tokv[tokc] != NULL; tokc++)
	;
	*cnt = tokc;
	if (tokc == 0)
		goto done;
	if (tokc == 1) {
		if ((*v1 = strdup(arg)))
			retval = true;
		goto done;

	}
	for (tokc = 0; tokv[tokc] != NULL; tokc++) {
		if (!(*vN = g_slist_append(*vN, strdup(tokv[tokc]))))
			goto done;
	}
	retval = true;

done:
	g_strfreev(tokv);
	return retval;
}

//
// parse_cmd_line - Parse command line arguments.
//
// Argument(s):
//
//	tokc    - Number of arguments
//	tokv    - Argument list
//	cmd_opt - Pointer to struct to store command options requested
//
// Return Code(s):
//
//	-1 	Error
//	 0	Command options parsed, run command
//	 1	Prompt for next command
//
// Side Effect(s):
//
//	Global configuration variables will be updated.
//
static int
parse_cmd_line(int tokc, char *tokv[], cmd_opt_t *cmd_opt, cmd_flags_t *cmd_flags)
{
	int opt = 0;
	int retval = 0;

	TRACE1_ENTER("tokc = %d, tokv = %p, cmd_opt = %p",
			tokc, tokv, cmd_opt);

	optind = 1; // Ensure this is reset to 1 for interactive processing

	// This is a calling error, can't proceed
	if (cmd_opt == NULL || cmd_flags == NULL) {
		PRINT_ERRCODE(PWR_RET_FAILURE,
				"Missing command options buffer");
		force_exit(PWR_RET_FAILURE);    // calling error
	}

	// Command parsing errors should be delivered to stderr, not JSON
	json_disable_output();

	// Reset the command options to default values
	reset_cmd_opt(cmd_opt);

	while ((opt = getopt_long(tokc, tokv, Short_Options, Long_Options,
							NULL)) != -1) {
		switch (opt) {
		case cmdline_attribute:
		case 'a':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->a_flag > 1) {
				PRINT_ERR("The -a/--attribute parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (!parse_cmdopt_arg(&cmd_opt->attrs_cnt,
							&cmd_opt->attr_str,
							&cmd_opt->attrs,
							optarg)) {
				PRINT_ERR("Unable to allocate memory");
				force_exit(PWR_RET_FAILURE);    // OOM
			}
			if (cmd_opt->attr_str) {
				cmd_opt->attr = CRAYPWR_AttrGetEnum(cmd_opt->attr_str);
				if (cmd_opt->attr < 0) {
					PRINT_ERR("Unrecognized attribute");
					retval = help_try_exit(PWR_RET_FAILURE);
					goto error_handler;
				}
			}
			break;
		case cmdline_command:
		case 'c':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->c_flag > 1) {
				PRINT_ERR("The -c/--command parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			//
			// Parse the desired command to perform.
			//
			if (!strcmp(optarg, "get")) {
				cmd_opt->type = cmd_get;
			} else if (!strcmp(optarg, "set")) {
				cmd_opt->type = cmd_set;
			} else if (!strcmp(optarg, "list")) {
				cmd_opt->type = cmd_list;
			} else if (!strcmp(optarg, "trav")) {
				cmd_opt->type = cmd_trav;
			} else {
				PRINT_ERR("Unsupported command: %s", optarg);
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmdline_help:
		case 'h':
			cmd_flags->h_flag++;
			break;
		case cmdline_interactive:
		case 'i':
			cmd_flags->i_flag++;
			break;
		case cmdline_json:
		case 'j':
			cmd_flags->s_flag = 0;
			break;
		case cmdline_list:
		case 'l':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->l_flag > 1) {
				PRINT_ERR("The -l/--list parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			//
			// Parse the desired command to perform.
			//
			if (!strcmp(optarg, "all")) {
				cmd_opt->list = list_all;
			} else if (!strcmp(optarg, "attr")) {
				cmd_opt->list = list_attr;
			} else if (!strcmp(optarg, "hier")) {
				cmd_opt->list = list_hier;
			} else if (!strcmp(optarg, "name")) {
				cmd_opt->list = list_name;
			} else {
				PRINT_ERR("Unsupported list: %s", optarg);
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmdline_metadata:
		case 'm':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->m_flag > 1) {
				PRINT_ERR("The -m/--metadata parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			cmd_opt->meta = get_meta_enum(optarg);
			if (cmd_opt->meta < 0) {
				PRINT_ERR("Unrecognized metadata");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmdline_name:
		case 'n':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->n_flag > 1) {
				PRINT_ERR("The -n/--name parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (!parse_cmdopt_arg(&cmd_opt->names_cnt,
							&cmd_opt->name_str,
							&cmd_opt->names,
							optarg)) {
				PRINT_ERR("Unable to allocate memory");
				force_exit(PWR_RET_FAILURE);    // OOM
			}
			break;
		case cmdline_role:
		case 'r':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->r_flag > 1) {
				PRINT_ERR("The -r/--role parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			cmd_opt->role = get_role_enum(optarg);
			if (cmd_opt->role < 0) {
				PRINT_ERR("Unrecognized role");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmdline_script:
		case 's':
			cmd_flags->s_flag++;
			break;
		case cmdline_trav:
		case 't':
			if (++cmd_flags->t_flag > 1) {
				PRINT_ERR("The -t/--trav parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			//
			// Parse the desired command to perform.
			//
			if (!strcmp(optarg, "up")) {
				cmd_opt->trav = trav_up;
			} else if (!strcmp(optarg, "down")) {
				cmd_opt->trav = trav_down;
			} else {
				PRINT_ERR("Unsupported traversal: %s", optarg);
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmdline_value:
		case 'v':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->v_flag > 1) {
				PRINT_ERR("The -v/--value parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			// Cannot convert value string to the appropriate
			// type until everything is known about the
			// command type and sub-types.
			if (!parse_cmdopt_arg(&cmd_opt->values_cnt,
							&cmd_opt->val_str,
							&cmd_opt->values,
							optarg)) {
				PRINT_ERR("Unable to allocate memory");
				force_exit(PWR_RET_FAILURE);    // OOM
			}
			break;
		case cmdline_index:
		case 'x':
			//
			// Verify option only specified once.
			//
			if (++cmd_flags->x_flag > 1) {
				PRINT_ERR("The -x/--index parameter "
						"may only be specified once");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			cmd_opt->index = atoi(optarg);
			break;
		case cmdline_debug:
		case 'D':
			cmd_flags->D_flag++;
			break;
		case cmdline_trace:
		case 'T':
			cmd_flags->T_flag++;
			break;
		default:
			PRINT_ERR("Unrecognized option: %s", tokv[optind]);
			retval = help_try_exit(PWR_RET_FAILURE);
			goto error_handler;
		}
	}

	//
	// Display help if requested.
	//
	if (cmd_flags->h_flag) {
		retval = help_try_exit(PWR_RET_FAILURE);
		goto error_handler;
	}

	// Validate --interactive options
	if (cmd_flags->i_flag) {
		// Check if already in interactive mode
		if (interactive) {
			PRINT_ERR("Already in interactive mode!\n");
			retval = 1;
			goto error_handler;
		}

		// Cannot set any --command options along with the
		// --interactive option.
		if (cmd_flags->a_flag || cmd_flags->c_flag || cmd_flags->l_flag
				|| cmd_flags->m_flag || cmd_flags->n_flag
				|| cmd_flags->t_flag || cmd_flags->v_flag) {
			retval = help_try_exit(PWR_RET_FAILURE);
			goto error_handler;
		}

		// Set flag to run in interactive mode
		interactive = true;
		retval = PWR_RET_SUCCESS;
		goto error_handler;
	}

	// Validate --command options
	//
	// Get command:
	// 	Metadata: require -m and -n options
	// 	Attribute: require -a and -n options
	// 	Disallow -v or -l options
	// Set command:
	// 	Metadata: require -m, -n, -v options
	// 	Attribute: require -a, -n, -v options
	// 	Disallow -l option
	// List command:
	// 	Require -l option
	// 	Disallow -a, -n or -v options
	//
	if (cmd_flags->c_flag) {
		switch (cmd_opt->type) {
		case cmd_get:
			// Check if get on metadata
			if (!cmd_flags->n_flag) {
				PRINT_ERR("Get command requires -n option\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}

			if (!cmd_flags->m_flag) {
				if (!cmd_flags->a_flag) {
					PRINT_ERR("Get command requires -a or -m options\n");
					retval = help_try_exit(PWR_RET_FAILURE);
					goto error_handler;
				}
			}

			if (cmd_flags->v_flag || cmd_flags->l_flag) {
				PRINT_ERR("Get command disallows -l and -v options\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmd_set:
			if (!(cmd_flags->a_flag && cmd_flags->n_flag
							&& cmd_flags->v_flag)) {
				PRINT_ERR("Set command requires -a, -n and -v options\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (cmd_flags->l_flag) {
				PRINT_ERR("Set command disallows -l option\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmd_list:
			if (!cmd_flags->l_flag) {
				PRINT_ERR("List command requires -l option\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (cmd_flags->a_flag || cmd_flags->n_flag
					|| cmd_flags->v_flag) {
				PRINT_ERR("List command disallows -a, -n  and -v options\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		case cmd_trav:
			if (!(cmd_flags->t_flag && cmd_flags->n_flag)) {
				PRINT_ERR("Trav command requires -t and -n options\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (cmd_flags->a_flag || cmd_flags->v_flag) {
				PRINT_ERR("Trav command disallows -a and -v options\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			if (cmd_opt->names_cnt != 1) {
				PRINT_ERR("Trav command allows only one object name\n");
				retval = help_try_exit(PWR_RET_FAILURE);
				goto error_handler;
			}
			break;
		default:
			PRINT_ERR("Unknown command\n");
			retval = help_try_exit(PWR_RET_FAILURE);
			goto error_handler;
		}
	} else {
		PRINT_ERR("The -c option is required if -h not specified\n");
		retval = help_try_exit(PWR_RET_FAILURE);
		goto error_handler;
	}

	/*
	 * All of the above was performed without the benefit of JSON
	 * formatting, and (being errors) was delivered to stderr. Now, enable
	 * JSON by default, unless -s was specified.
	 */
	if (!cmd_flags->s_flag) {
		json_enable_output();
	}

error_handler:
	TRACE1_EXIT("retval = %d", retval);
	return retval;
}

static void
process_command(cmd_opt_t *cmd_opt)
{
	TRACE1_ENTER("cmd_opt = %p, cmd_opt->type = %d", cmd_opt, cmd_opt->type);

	//
	// Perform requested command.
	//
	switch (cmd_opt->type) {
	case cmd_list:
		cmd_get_list(cmd_opt);
		break;
	case cmd_get:
		if (cmd_opt->index != -1) {
			cmd_get_meta_at_index(cmd_opt);
		} else if (cmd_opt->meta > PWR_MD_INVALID) {
			cmd_get_meta(cmd_opt);
		} else if (cmd_opt->names_cnt == 1 &&
				cmd_opt->attrs_cnt == 1) {
			cmd_get_attr(cmd_opt);
		} else if (cmd_opt->names_cnt == 1) {
			cmd_get_attrs(cmd_opt);
		} else if (cmd_opt->attrs_cnt == 1) {
			cmd_get_grp_attr(cmd_opt);
		} else {
			cmd_get_grp_attrs(cmd_opt);
		}
		break;
	case cmd_set:
		if (cmd_opt->meta > PWR_MD_INVALID) {
			cmd_set_meta(cmd_opt);
		} else if (cmd_opt->names_cnt == 1 &&
				cmd_opt->attrs_cnt == 1) {
			cmd_set_attr(cmd_opt);
		} else if (cmd_opt->names_cnt == 1) {
			cmd_set_attrs(cmd_opt);
		} else if (cmd_opt->attrs_cnt == 1) {
			cmd_set_grp_attr(cmd_opt);
		} else {
			cmd_set_grp_attrs(cmd_opt);
		}
		break;
	case cmd_trav:
		if (cmd_opt->trav == trav_up)
			cmd_get_parent(cmd_opt);
		else if (cmd_opt->trav == trav_down)
			cmd_get_children(cmd_opt);
		else
			PRINT_ERR("Nothing to do!");
		break;
	default:
		PRINT_ERR("Unrecognized command");
		break;
	}

	//
	// Always flush, to show error codes if nothing else
	//
	json_flush_output(true);

	TRACE1_EXIT("");
}


static void
interactive_prompt(cmd_flags_t *cmdflags_dflt)
{
	cmd_opt_t cmd_opt = INIT_CMD_OPT();
	char *line = NULL;
	ssize_t status = 1;

	TRACE1_ENTER("");

	// Begin with history length of 10 lines. The user
	// can increase with '/history NUMBER' command.
	linenoiseHistorySetMaxLen(10);

	// Setup to allow multiline editing.
	// Without multiline a long line will scroll off the edge of
	// screen. That can be harder to work with. Multiline keeps
	// it in the screen borders by wrapping the line.
	linenoiseSetMultiLine(1);

	while (status > 0) {
		cmd_flags_t cmd_flags = *cmdflags_dflt;
		char **tokv = NULL;
		int  tokc = 0;

		line = linenoise("pwrcmd> ");

		// If empty line, prompt for next command
		if (line[0] == '\0')
			continue;

		// If linenoise command do it or ignore
		if (line[0] == '/') {
			if (!strncmp(line, "/history", 8)) {
				int len = atoi(line + 8);
				linenoiseHistorySetMaxLen(len);
			} else {
				PRINT_ERR("Unrecognized command: %s\n",
						line);
			}
			continue;
		}

		linenoiseHistoryAdd(line);


		// Split the string into tokens. From this point memory for
		// tokv must be de-allocated.
		tokv = g_strsplit_set(line, " \t", -1);
		if (tokv == NULL) {
			// Failed the split, stop with error
			PRINT_ERR("Failed to split tokens.\n");
			status = -1;
			continue;
		}

		// Count the number of tokens
		for (tokc = 0; tokv[tokc] != NULL; tokc++)
		;

		if (strcmp("help", tokv[0]) == 0) {
			interactive_cmd = i_help;
			interactive_help();
			status = 1; // do another command;
			interactive_cmd = i_none;
		} else if (strcmp("do", tokv[0]) == 0) {
			interactive_cmd = i_do;
			//
			// Parse command line for attributes and object names.
			//
			status = parse_cmd_line(tokc, tokv, &cmd_opt, &cmd_flags);
			if (status == 0) {
				//
				// Set the DEBUG and TRACE mask to stderr.
				//
				pmlog_stderr_set_level(cmd_flags.D_flag, cmd_flags.T_flag);

				// Successful parse
				process_command(&cmd_opt);
				status = 1; // do another command
			}
			interactive_cmd = i_none;
		} else if (strcmp("quit", tokv[0]) == 0) {
			interactive_cmd = i_quit;
			if (tokc >= 2) {
				interactive_help_quit();
				status = 1; // do another command
			} else {
				status = 0; // quit interactive mode, normally
			}
			interactive_cmd = i_none;
		} else {
			interactive_cmd = i_none;
			PRINT_ERR("Bad interactive command: %s\n", tokv[0]);
			help_try_exit(PWR_RET_FAILURE);
		}
		g_strfreev(tokv);       // free the token vector
	}

	free(line);

	TRACE1_EXIT("");
}


//
// main - Main entry point.
//
// Argument(s):
//
//	argc - Number of arguments
//	argv - Arguments
//
// Return Code(s):
//
//	int - Return code
//
int
main(int argc, char *argv[])
{
	cmd_opt_t cmd_opt =  INIT_CMD_OPT();
	cmd_flags_t cmdflags_dflt;
	char *prgname = NULL;

	// Set program/application name
	prgname = g_path_get_basename(argv[0]);
	g_set_prgname(prgname);
	g_free(prgname);
	g_set_application_name("PowerAPI Pwrcmd App");

	/*
	 * NOTE: this initializes the logging system with defaults.
	 */
	LOG_DBG("%s started", g_get_prgname());
	TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

	memset(&cmdflags_dflt, 0, sizeof(cmd_flags_t));

	//
	// Parse command line for attributes and object names.
	//
	parse_cmd_line(argc, argv, &cmd_opt, &cmdflags_dflt);

	//
	// Set the DEBUG and TRACE mask to stderr.
	//
	pmlog_stderr_set_level(cmdflags_dflt.D_flag, cmdflags_dflt.T_flag);

	//
	// Initialize the PM API.
	//
	api_init(cmd_opt.role);

	if (interactive) {
		cmdflags_dflt.i_flag = 0;
		interactive_prompt(&cmdflags_dflt);
	} else {
		process_command(&cmd_opt);
	}

	TRACE1_EXIT("");

	//
	// If we got here all is good.  Exit success.
	//
	force_exit(PWR_RET_SUCCESS);    // app exit

	return 666;
}
