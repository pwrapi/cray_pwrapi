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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#include <glib.h>

#include <cray-powerapi/api.h>

#include "api.h"
#include "cnctl.h"
#include "io.h"
#include "perms.h"

/*
 * Data structures for parsing command line arguments.
 */
enum {
	cmdline_MIN = 1000,
	cmdline_attribute,
	cmdline_command,
	cmdline_help,
	cmdline_json,
	cmdline_value,
	cmdline_debug,
	cmdline_trace,
	cmdline_MAX
};

static const char *Short_Options = "a:c:hjv:DT";
static struct option Long_Options[] = {
	{ "attribute",	    required_argument,	NULL, cmdline_attribute },
	{ "command",	    required_argument,	NULL, cmdline_command },
	{ "help",	    no_argument,	NULL, cmdline_help },
	{ "json",	    no_argument,	NULL, cmdline_json },
	{ "value",	    required_argument,	NULL, cmdline_value },
	{ "debug",	    no_argument,	NULL, cmdline_debug },
	{ "trace",	    no_argument,	NULL, cmdline_trace },
	{ 0, 0, 0, 0 }
};

static int a_flag = 0;          // attribute flag
static int c_flag = 0;          // command flag
static int h_flag = 0;          // help flag
static int j_flag = 0;          // JSON flag
static int v_flag = 0;          // value flag
static int D_flag = 0;          // debug level
static int T_flag = 0;          // trace level

/*
 * usage - Print usage statement.
 *
 * Argument(s):
 *
 *	exitcode - Exit code to pass to exit() after printing the usage
 *		   statement.
 *
 * Return Code(s):
 *
 *	DOES NOT RETURN
 */
static void __attribute__((noreturn))
usage(int exit_code)
{
	static const char *fmt =
			"\n"
			"Usage: cnctl -c command [-a attribute] [-v value] [-hj]\n"
			"\n"
			"Options:\n"
			"\n"
			"   -a/--attribute     The attribute to target\n"
			"   -c/--command       The command to perform:\n"
			"\n"
			"                          get  Get the value of the specified attribute\n"
			"                          set  Set the value of the specified attribute\n"
			"                          cap  List capabilities for specified attribute or\n"
			"                               all valid attributes if no -a/--attribute\n"
			"                               option was specified\n"
			"\n"
			"   -h/--help          Print this help message\n"
			"   -j/--json          Format all output in JSON\n"
			"   -v/--value         Input data value to act upon\n"
			"   -D/--debug         Increase debug level to stderr\n"
			"   -T/--trace         Increase trace level to stderr\n"
			"\n"
			"   -D   -> display DBG1\n"
			"   -DD  -> display DBG1 and DBG2\n"
			"   -T   -> display TRC1\n"
			"   -TT  -> display TRC1 and TRC2\n"
			"   -TTT -> display TRC1, TRC2, and TRC3\n"
	;

	TRACE1_ENTER("exit_code = %d", exit_code);

	fprintf((exit_code) ? stderr : stdout, fmt, g_get_prgname());

	TRACE1_EXIT("exit_code = %d", exit_code);
	exit(exit_code);
}

/*
 * parse_cmd_line - Parse command line arguments.
 *
 * Argument(s):
 *
 *	argc   - Number of arguments
 *	argv   - Argument list
 *	cmd    - The requested command
 *	rattrs - Pointer to array to store requested attributes
 *
 * Return Code(s):
 *
 *	void
 *
 * Side Effect(s):
 *
 *	Global configuration variables will be updated.
 */
static void
parse_cmd_line(int argc, char **argv, cmd_type_t *cmd, GArray *rattrs)
{
	int c = 0;
	int i;
	rattr_t *rattr = NULL;

	TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

	while ((c = getopt_long(argc, argv, Short_Options, Long_Options,
							NULL)) != -1) {
		switch (c) {
		case cmdline_attribute:
		case 'a':
			/*
			 * Verify option only specified once.
			 */
			if (++a_flag > 1) {
				PRINT_ERR("The -a/--attribute parameter "
						"may only be specified once");
				usage(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Save the attribute argument.  Will process later.
			 */
			if ((rattr == NULL)
					&& ((rattr = (rattr_t *)calloc(1, sizeof(rattr_t)))
							== NULL)) {
				PRINT_ERR("Unable to allocate memory");
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			rattr->attr_str = strdup(optarg);
			if (rattr->attr_str == NULL) {
				PRINT_ERR("Unable to allocate memory");
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			break;
		case cmdline_command:
		case 'c':
			/*
			 * Verify option only specified once.
			 */
			if (++c_flag > 1) {
				PRINT_ERR("The -c/--command parameter "
						"may only be specified once");
				usage(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Parse the desired command to perform.
			 */
			if (!strcmp(optarg, "get")) {
				*cmd = cmd_get;
			} else if (!strcmp(optarg, "set")) {
				*cmd = cmd_set;
			} else if (!strcmp(optarg, "cap")) {
				*cmd = cmd_attr_cap;
			} else {
				PRINT_ERR("Unsupported command");
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			break;
		case cmdline_help:
		case 'h':
			h_flag++;
			break;
		case cmdline_json:
		case 'j':
			enable_json_output();
			j_flag++;
			break;
		case cmdline_value:
		case 'v':
			/*
			 * Verify option only specified once.
			 */
			if (++v_flag > 1) {
				PRINT_ERR("The -v/--value parameter "
						"may only be specified once");
				usage(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

			/*
			 * Save the value argument.  Will process later.
			 */
			if ((rattr == NULL)
					&& ((rattr = (rattr_t *)calloc(1, sizeof(rattr_t)))
							== NULL)) {
				PRINT_ERR("Unable to allocate memory");
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			rattr->val_str = strdup(optarg);
			if (rattr->val_str == NULL) {
				PRINT_ERR("Unable to allocate memory");
				cnctl_exit(PWR_RET_FAILURE);
				// NOT REACHED
				LOG_DBG("NOT REACHED");
			}
			break;
		case cmdline_debug:
		case 'D':
			D_flag++;
			break;
		case cmdline_trace:
		case 'T':
			T_flag++;
			break;
		default:
			usage(PWR_RET_FAILURE);
			// NOT REACHED
			LOG_DBG("NOT REACHED");
			break;
		}
	}

	/*
	 * If -j/--json was the only command line parameter specified it
	 * indicates that a JSON formatted command string is sitting on
	 * stdin.  Parse it.
	 */
	if (j_flag && !(a_flag | c_flag | h_flag | v_flag)) {
		parse_json_input(&a_flag, &c_flag, cmd, &v_flag, rattrs);
	} else if (a_flag || v_flag) {
		/*
		 * We have a single requested attribute coming from the
		 * command line.  Add it to the request array here.
		 */
		rattr->retcode = PWR_RET_OP_NOT_ATTEMPTED;
		g_array_append_val(rattrs, rattr);
	}

	/*
	 * Display usage if requested.
	 */
	if (h_flag) {
		usage(0);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Default capability request is cmd_attr_cap unless -a/--attribute
	 * was also specified.
	 */
	if (*cmd == cmd_attr_cap && a_flag) {
		*cmd = cmd_attr_val_cap;
	}

	/*
	 * Return if it's a permissions command.
	 */
	if (*cmd == cmd_uid_add   || *cmd == cmd_uid_remove ||
			*cmd == cmd_uid_clear || *cmd == cmd_uid_restore ||
			*cmd == cmd_uid_list) {
		goto parse_cmd_line_exit;
	}

	/*
	 * Once we're sure we have the right command, perform last little
	 * bit of JSON inititialization if appropriate.
	 */
	if (*cmd != cmd_attr_cap) {
		json_attr_cap_init();
	}

	/*
	 * Return if just displaying capabilities.
	 */
	if (*cmd == cmd_attr_cap || *cmd == cmd_attr_val_cap) {
		goto parse_cmd_line_exit;
	}

	/*
	 * Both a command and attribute must have been specified.
	 */
	if (!a_flag || !c_flag) {
		PRINT_ERR("Must specifiy a command and an attribute");
		usage(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Verify correct usage of the value parameter.
	 */
	if (v_flag && (*cmd != cmd_set)) {
		PRINT_ERR("Value parameter only valid for set command");
		usage(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	} else if (!v_flag && (*cmd == cmd_set)) {
		PRINT_ERR("Value parameter required for set command");
		usage(PWR_RET_FAILURE);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

parse_cmd_line_exit:

	LOG_DBG("a_flag=%d c_flag=%d h_flag=%d j_flag=%d "
			"v_flag=%d cmd=%d", a_flag, c_flag,
			h_flag, j_flag, v_flag, *cmd);

	for (i = 0; i < rattrs->len; i++) {
		rattr_t *rattr = g_array_index(rattrs, rattr_t *, i);
		LOG_DBG("rattrs[%d]: attr_str = '%s', val_str = '%s'",
				i, rattr->attr_str, rattr->val_str);
	}

	TRACE1_EXIT("");
}

/*
 * cnctl_exit - Print message (if appropriate) and exit program.
 *
 * Argument(s):
 *
 *	exit_code - Exit code
 *
 * Return Code(s):
 *
 *	DOES NOT RETURN
 */
void __attribute__((noreturn))
cnctl_exit(int exit_code)
{
	TRACE1_ENTER("exit_code = %d", exit_code);

	/*
	 * Set JSON exit code.
	 */
	set_json_ret_code(exit_code);

	/*
	 * Be nice and cleanup before exiting if possible.
	 */
	api_cleanup();
	flush_output();

	/*
	 * Exit with requested code.
	 */
	if (exit_code < 0) {
		LOG_CRIT("Exit with error code %d", exit_code);
		exit(1);
	}
	exit(0);
}

/*
 * main - Main entry point.
 *
 * Argument(s):
 *
 *	argc - Number of arguments
 *	argv - Arguments
 *
 * Return Code(s):
 *
 *	int - Return code
 */
int
main(int argc, char **argv)
{
	cmd_type_t cmd = cmd_attr_cap;          // command to perform
	GArray *rattrs = NULL;  // requested attributes (rattr_t)
	char *prgname = NULL;

	// Set program/application name
	prgname = g_path_get_basename(argv[0]);
	g_set_prgname(prgname);
	g_free(prgname);
	g_set_application_name("PowerAPI Cnctl App");

	/*
	 * NOTE: this initializes the logging system with defaults.
	 */
	LOG_DBG("%s started", g_get_prgname());
	TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

	/*
	 * Parse command line (or JSON input on stdin) for requested attributes.
	 */
	rattrs = g_array_new(TRUE, TRUE, sizeof(rattr_t *));
	parse_cmd_line(argc, argv, &cmd, rattrs);

	/*
	 * Set the DEBUG and TRACE mask to stderr.
	 */
	pmlog_stderr_set_level(D_flag, T_flag);

	/*
	 * Must be root to issue a set or modify permissions.  The kernel also
	 * prevents this so this is just a quick short-cut for the common
	 * non-nefarious case.
	 */
	if (((cmd == cmd_set) ||
					(cmd == cmd_uid_add) ||
					(cmd == cmd_uid_remove) ||
					(cmd == cmd_uid_clear) ||
					(cmd == cmd_uid_restore)) &&
			(geteuid() != 0)) {
		PRINT_ERR("Only root can perform that operation");
		cnctl_exit(PWR_RET_READ_ONLY);
		// NOT REACHED
		LOG_DBG("NOT REACHED");
	}

	/*
	 * Initialize the PM API.
	 */
	api_init();

	/*
	 * Some commands require an HT object instead of the default
	 * NODE one. If we're doing one of those commands, use an HT
	 * object. This assumes that all HT objects are set to the
	 * same value. Not necessarily true.
	 */
	if (cmd == cmd_attr_val_cap || cmd == cmd_get) {
		int retval = use_ht_object();
		if (retval != PWR_RET_SUCCESS) {
			PRINT_ERR("Unable to use HT object");
			cnctl_exit(retval);
			// NOT REACHED
			LOG_DBG("NOT REACHED");
		}
	}

	/*
	 * Validate requested attribute strings.
	 */
	validate_rattrs_strs(rattrs);

	/*
	 * Perform requested command.
	 */
	switch (cmd) {
	case cmd_attr_cap:
		print_attr_cap();
		break;
	case cmd_uid_add:
		perms_add();
		break;
	case cmd_uid_remove:
		perms_remove();
		break;
	case cmd_uid_clear:
		perms_clear();
		break;
	case cmd_uid_restore:
		perms_restore();
		break;
	case cmd_uid_list:
		perms_list();
		break;
	default:
		{
		/*
		 * All other commands operate on one or more specified attrs.
		 */
			int i = 0;

			for (i = 0; i < rattrs->len; i++) {
				rattr_t *rattr = g_array_index(rattrs, rattr_t *, i);

				if (cmd == cmd_attr_val_cap) {
					get_attr_val_cap(rattr);
				} else if (cmd == cmd_get) {
					get_attr_val(rattr);
				} else if (cmd == cmd_set) {
					set_attr_val(rattr);
				}
			}
		}
	}

	/*
	 * If we got here all is good.  Exit success.
	 */
	cnctl_exit(PWR_RET_SUCCESS);
	// NOT REACHED
	TRACE1_EXIT("NOT REACHED");

	return 666;
}
