/*
 * Copyright (c) 2017, Cray Inc. All rights reserved.
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

#include "pwrdemo.h"
#include "stats.h"
#include "workload.h"
#include <cray-powerapi/api.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Initialize a demo_opt_t
#define INIT_DEMO_OPT(X)                                                       \
	demo_opt_t X = {.retcode = 0,                                          \
			.workload = 0,                                         \
			.dgemm_N = 256,                                        \
			.knob = -1,                                            \
			.do_cycle = false,                                     \
			.asc = true,                                           \
			.obj_lvl = PWR_OBJ_NODE,                               \
			.do_profile = false,                                   \
			.do_stats = true}

// The number of values to cycle for power limits
#define PWRLIMIT_NUM_CYC_VALS 6

// Percent of max PWR_ATTR_POWER_LIMIT_MAX to step with in cycling power limits
#define PWRLIMIT_INCR_CYC_PERCENT 0.1

// Data structures for parsing command line arguments.
enum { cmdline_MIN = 1000,
       cmdline_help,
       cmdline_workload,
       cmdline_pstate,
       cmdline_pwrlimit,
       cmdline_cyc_pstates,
       cmdline_cyc_pwrlimits,
       cmdline_obj,
       cmdline_asc,
       cmdline_desc,
       cmdline_role,
       cmdline_profile,
       cmdline_nostats,
       cmdline_debug,
       cmdline_verbose,
       cmdline_MAX };

// Enum for power control 'knobs'
enum { knob_pstate = 0, knob_pwrlimit, knob_MAX };

static const char *Short_Options = "hw:p:l:o:N:Dv";
static struct option Long_Options[] = {
    {"help", no_argument, NULL, cmdline_help},
    {"workload", required_argument, NULL, cmdline_workload},
    {"p-state", required_argument, NULL, cmdline_pstate},
    {"power-limit", required_argument, NULL, cmdline_pwrlimit},
    {"cycle-p-states", no_argument, NULL, cmdline_cyc_pstates},
    {"cycle-power-limits", no_argument, NULL, cmdline_cyc_pwrlimits},
    {"obj", required_argument, NULL, cmdline_obj},
    {"asc", no_argument, NULL, cmdline_asc},
    {"desc", no_argument, NULL, cmdline_desc},
    {"profile", no_argument, NULL, cmdline_profile},
    {"no-stats", no_argument, NULL, cmdline_nostats},
    {"debug", no_argument, NULL, cmdline_debug},
    {"verbose", no_argument, NULL, cmdline_verbose},
    {0, 0, 0, 0}};

typedef struct {
	int w_flag; // workload flag
	int p_flag; // p-state flag
	int h_flag; // help flag
	int l_flag; // power limit flag
	int o_flag; // obj flag
	int v_flag; // verbose flag
	int D_flag; // debug flag
	int N_flag; // N for dgemm (dgemm only, hidden)
	int asc_flag;
	int desc_flag;
} cmd_flags_t;

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
static void __attribute__((noreturn)) usage(int exit_code)
{
	static const char *fmt =
	    "\n"
	    "usage: %s [-h] [-w WORKLOAD][-p FREQ] [-l LIMIT]\n"
	    "                    [--cycle-p-states] [--cycle-power-limits]\n"
	    "                    [-o OBJ-TYPE]\n"
	    "                    [--asc][--desc]\n"
	    "                    [--profile]\n"
	    "                    [--no-stats]\n"
	    "\n"
	    "Simple benchmark program for demonstration of the Power API.\n"
	    "\n"
	    "optional arguments:\n"
	    "  -h, --help            show this help message and exit.\n"
	    "  -w WORKLOAD, --workload WORKLOAD\n"
	    "                        specify the workload to run. WORKLOAD may "
	    "be either\n"
	    "                        DGEMM or STREAM. If not specified, "
	    "WORKLOAD \n"
	    "                        defaults to DGEMM.\n"
	    "  -p FREQ, --p-state FREQ\n"
	    "                        set a p-state in kHz under which to run "
	    "the \n"
	    "                        workload.\n"
	    "  -l LIMIT, --power-limit LIMIT\n"
	    "                        set a power limit in W under which to run "
	    "the \n"
	    "                        workload.\n"
	    "  --cycle-p-states\n"
	    "                        perform the workload under each of the "
	    "p-states\n"
	    "                        supported by the host, printing "
	    "statistics \n"
	    "                        following each run of the workload.\n"
	    "  --cycle-power-limits\n"
	    "                        perform the workload under a range of "
	    "power limits\n"
	    "                        supported by the host, up to the "
	    "PWR_MD_MAX of\n"
	    "                        PWR_ATTR_POWER_LIMIT_MAX for the object "
	    "type, \n"
	    "                        printing statistics following each run of "
	    "the \n"
	    "                        workload.\n"
	    "  -o OBJ-TYPE, --obj OBJ-TYPE\n"
	    "                        specify the object type upon which to "
	    "perform \n"
	    "                        p-state controls or power limiting. Valid "
	    "object\n"
	    "                        types for p-state controls are "
	    "PWR_OBJ_NODE, \n"
	    "                        PWR_OBJ_SOCKET, or PWR_OBJ_CORE. Valid "
	    "object \n"
	    "                        types for power limiting are\n"
	    "                        PWR_OBJ_NODE, PWR_OBJ_SOCKET, and "
	    "PWR_OBJ_MEM.\n"
	    "  --asc                 perform cycling of p-states or power "
	    "limits in an\n"
	    "                        ascending order. This option is only "
	    "useful when\n"
	    "                        combined with the --cycle-p-states or\n"
	    "                        --cycle-power-limits options.\n"
	    "  --desc                perform cycling of p-states or power "
	    "limits in an\n"
	    "                        descending order. This option is only "
	    "useful when\n"
	    "                        combined with the --cycle-p-states or\n"
	    "                        --cycle-power-limits options.\n"
	    "  --profile             enable profiling of Power API function "
	    "calls during\n"
	    "                        the workload runs. Profiling statistics "
	    "print upon\n"
	    "                        completion of all runs.\n"
	    "  --no-stats            disable the automatic collection and "
	    "reporting of\n"
	    "                        power, energy, and thermal statistics.\n"
	    "  -N SIZE               specify an integer matrix SIZE for "
	    "DGEMM.\n"
	    "  -D, --debug           enable debugging messages.\n"
	    "  -v, --verbose         enable verbose output.\n"
	    "\n";
	fprintf((exit_code) ? stderr : stdout, fmt, "pwrdemo");
	exit(exit_code);
}

//
// parse_cmd_line - Parse command line arguments.
//
// Argument(s):
//
//	tokc    - Number of arguments
//	tokv    - Argument list
//	cmd_opt - Pointer to struct to store demo options requested
//
// Return Code(s):
//
//	returns an int representing the number of errors in the cmd line
//      options. 0 means success; non-zero means error.
//
// Side Effect(s):
//
//	Global configuration variables will be updated.
//
static int parse_cmd_line(int tokc, char *tokv[], demo_opt_t *demo_opts,
			  cmd_flags_t *cmd_flags)
{
	int rv = 0;
	int opt = 0;

	while ((opt = getopt_long(tokc, tokv, Short_Options, Long_Options,
				  NULL)) != -1) {
		switch (opt) {
		case cmdline_workload:
		case 'w':
			if (++cmd_flags->w_flag > 1) {
				fprintf(stderr,
					"Too many -w|--workload defined\n");
				rv++;
			}
			if (!strcmp(optarg, "dgemm")) {
				// Set to workload_dgemm
				demo_opts->workload = workload_dgemm;
			} else if (!strcmp(optarg, "stream")) {
				demo_opts->workload = workload_stream;
			} else {
				fprintf(stderr,
					"Unrecognized or unsupported "
					"workload: %s\n",
					optarg);
				rv++;
			}
			break;
		case cmdline_pstate:
		case 'p':
			demo_opts->knob = knob_pstate;
			demo_opts->knob_val = atof(optarg);
			if (cmd_flags->o_flag <= 0) {
				demo_opts->obj_lvl = PWR_OBJ_NODE;
			}
			break;
		case cmdline_pwrlimit:
		case 'l':
			demo_opts->knob = knob_pwrlimit;
			demo_opts->knob_val = atof(optarg);
			if (cmd_flags->o_flag <= 0) {
				demo_opts->obj_lvl = PWR_OBJ_SOCKET;
			}
			break;
		case cmdline_cyc_pstates:
			demo_opts->knob = knob_pstate;
			demo_opts->do_cycle = true;
			if (cmd_flags->o_flag <= 0) {
				demo_opts->obj_lvl = PWR_OBJ_NODE;
			}
			break;
		case cmdline_cyc_pwrlimits:
			demo_opts->knob = knob_pwrlimit;
			if (cmd_flags->o_flag <= 0) {
				demo_opts->obj_lvl = PWR_OBJ_SOCKET;
			}
			demo_opts->do_cycle = true;
			break;
		case cmdline_obj:
		case 'o':
			if (++cmd_flags->o_flag > 1) {
				fprintf(stderr, "Too many -o|--obj defined\n");
				rv++;
			}
			if (!strcmp(optarg, "PWR_OBJ_NODE")) {
				demo_opts->obj_lvl = PWR_OBJ_NODE;
			} else if (!strcmp(optarg, "PWR_OBJ_SOCKET")) {
				demo_opts->obj_lvl = PWR_OBJ_SOCKET;
			} else if (!strcmp(optarg, "PWR_OBJ_CORE")) {
				demo_opts->obj_lvl = PWR_OBJ_CORE;
			} else if (!strcmp(optarg, "PWR_OBJ_MEM")) {
				demo_opts->obj_lvl = PWR_OBJ_MEM;
			} else {
				fprintf(stderr,
					"Unrecognized or unsupported"
					" object type: %s\n",
					optarg);
				rv++;
			}
			break;
		case cmdline_asc:
			if (cmd_flags->desc_flag > 1) {
				fprintf(stderr, "--asc and --desc are mutually"
						" exclusive.\n");
				rv++;
			}
			++cmd_flags->asc_flag;
			demo_opts->asc = true;
			break;
		case cmdline_desc:
			if (cmd_flags->asc_flag > 1) {
				fprintf(stderr, "--asc and --desc are mutually"
						" exclusive.\n");
				rv++;
			}
			++cmd_flags->desc_flag;
			demo_opts->asc = false;
			break;
		case cmdline_profile:
			demo_opts->do_profile = true;
			break;
		case cmdline_nostats:
			demo_opts->do_stats = false;
			break;
		case 'N': {
			++cmd_flags->N_flag;
			int temp_n;
			temp_n = atoi(optarg);
			if (temp_n > 1) {
				demo_opts->dgemm_N = temp_n;
			}
		} break;
		case cmdline_debug:
		case 'D':
			++cmd_flags->D_flag;
			break;
		case cmdline_verbose:
		case 'v':
			++cmd_flags->v_flag;
			break;
		case cmdline_help:
		case 'h':
			++cmd_flags->h_flag;
			break;
		default:
			fprintf(stderr, "Unknown option: %i\n", opt);
			rv++;
			break;
		}
	}

	return rv;
}

// api_init - Create an API context, find our entry in the hierarch, and
//	      perform any additional API initializations.
//
// Argument(s):
//
//	void
//
// Return Code(s):
//
//	void
//
static void api_init(PWR_Cntxt *ctx, PWR_Role role)
{
	int ret = PWR_RET_SUCCESS;
	uint64_t max_md_str_len = 0;
	PWR_Obj obj = NULL;
	PWR_ObjType obj_type = PWR_OBJ_INVALID;
	char obj_name[PWR_MAX_STRING_LEN];

	//
	// Get a context.
	//
	ret = PWR_CntxtInit(PWR_CNTXT_DEFAULT, role, "pwrdemo", ctx);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtInit() failed: %d\n", __func__,
			ret);
		exit(ret);
	}

	//
	// Get our location in the object hierarchy.
	//
	ret = PWR_CntxtGetEntryPoint(*ctx, &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtGetEntryPoint() failed: %d\n",
			__func__, ret);
		exit(ret);
	}

	//
	// Make sure we're where we expect to be in the power hierarchy.
	//
	if ((PWR_ObjGetType(obj, &obj_type) != PWR_RET_SUCCESS) ||
	    (obj_type != PWR_OBJ_NODE)) {
		char buf[PWR_MAX_STRING_LEN] = {0};

		(void)PWR_ObjGetName(obj, buf, sizeof(buf));
		fprintf(stderr,
			"Unexpected '%s' location in the power "
			"hierarchy\n",
			buf);
		exit(-1);
	}

	//
	// Get the official name of the node object.
	//
	if (PWR_ObjGetName(obj, obj_name, sizeof(obj_name)) !=
	    PWR_RET_SUCCESS) {
		fprintf(stderr, "Failed to get node name\n");
		exit(-1);
	}
}

//
// api_cleanup - Cleanup our PM API context.
//
// Argument(s):
//
//	ctx - the PWR_Cntxt to clean up
//
// Return Code(s):
//
//	void
//
static void api_cleanup(PWR_Cntxt *ctx)
{
	int ret = 0;

	//
	// Nothing there yet, just return.
	//
	if (*ctx == NULL)
		return;

	//
	// Remove context.
	//
	ret = PWR_CntxtDestroy(*ctx);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "PWR_CntxtDestroy() failed: %d\n", ret);
		return;
	}

	*ctx = NULL;
}

//
// get_freq_values - Get an array of valid PWR_ATTR_FREQ_REQ values
//
// Argument(s):
//
//	ctx - the PWR_Cntxt to use
//	cnt - the size of the returned double array
//
// Return Code(s):
//
//	static double* - a double array of valid PWR_ATTR_FREQ_REQ values.
//
static double *get_freq_values(PWR_Cntxt *ctx, int *cnt)
{
	PWR_Obj obj;
	uint64_t num_vals = 0;
	unsigned int i;
	double *ret_freqs = NULL;
	int ret;

	// Check that a hw thread object exists
	ret = PWR_CntxtGetObjByName(*ctx, "ht.0", &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtGetObjByName failed: %i\n",
			__func__, ret);
		*cnt = -1;
		return NULL;
	}

	// Get number of valid metadata values
	ret = PWR_ObjAttrGetMeta(obj, PWR_ATTR_FREQ, PWR_MD_NUM, &num_vals);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_ObjAttrGetMeta failed: %i\n", __func__,
			ret);
		*cnt = -1;
		return NULL;
	} else {
		*cnt = num_vals;
	}

	// If an attribute is not enumerable, then PWR_MD_NUM will return 0.
	// PWR_ATTR_FREQ should be enumerable.
	if (num_vals < 1) {
		*cnt = -1;
		return NULL;
	}

	// Copy out valid values to array.
	ret_freqs = (double *)malloc(num_vals * sizeof(double));
	for (i = 0; i < num_vals; i++) {
		double freq = 0.0;
		ret = PWR_MetaValueAtIndex(obj, PWR_ATTR_FREQ, i, &freq, NULL);

		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_MetaValueAtIndex failed: %i\n",
				__func__, ret);
			*cnt = -1;
			return NULL;
		}
		ret_freqs[i] = freq;
	}
	return ret_freqs;
}

//
// generate_pwr_limits - Get an array of valid PWR_ATTR_POWER_LIMIT_MAX values
//
// Argument(s):
//
//	ctx     - the PWR_Cntxt to use
//	cnt     - the size of the returned double array
//	obj_lvl - the object level of power limit values
//
// Return Code(s):
//
//	static double* - a double array of valid PWR_ATTR_POWER_LIMIT_MAX
//			values.
//
static double *generate_pwr_limits(PWR_Cntxt *ctx, int *cnt,
				   PWR_ObjType obj_lvl)
{
	double max_max = 0.0;
	int i;
	int incr;
	int ret;
	PWR_Obj obj;
	double *ret_lmts = NULL;

	// Acquire a mem or socket object to determine max limit
	if (obj_lvl == PWR_OBJ_MEM) {
		ret = PWR_CntxtGetObjByName(*ctx, "mem.0", &obj);
	} else {
		ret = PWR_CntxtGetObjByName(*ctx, "socket.0", &obj);
	}
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtGetObjByName failed: %i\n",
			__func__, ret);
		*cnt = -1;
		return NULL;
	}
	ret = PWR_ObjAttrGetMeta(obj, PWR_ATTR_POWER_LIMIT_MAX, PWR_MD_MAX,
				 &max_max);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_ObjAttrGetMeta failed: %i\n", __func__,
			ret);
		*cnt = -1;
		return NULL;
	}
	ret_lmts = (double *)malloc(PWRLIMIT_NUM_CYC_VALS * sizeof(double));
	ret_lmts[0] = 0.0;
	incr = floor(max_max * PWRLIMIT_INCR_CYC_PERCENT);
	for (i = 1; i < PWRLIMIT_NUM_CYC_VALS; i++) {
		ret_lmts[i] = max_max - (i - 1) * incr;
	}
	*cnt = PWRLIMIT_NUM_CYC_VALS;
	return ret_lmts;
}

//
// get_knob_values - Get an array of valid attribute values
//
// Argument(s):
//
//	ctx     - the PWR_Cntxt to use
//	cnt     - the size of the returned double array
//	obj_lvl - the object level of power limit values (needed for power
//		limits)
//
// Return Code(s):
//
//	static double* - a double array of valid PWR_ATTR_POWER_LIMIT_MAX
//			values.
//
static double *get_knob_values(PWR_Cntxt *ctx, int knob, int *cnt,
			       PWR_ObjType obj_lvl)
{
	switch (knob) {
	case knob_pstate:
		return get_freq_values(ctx, cnt);
		break;
	case knob_pwrlimit:
		return generate_pwr_limits(ctx, cnt, obj_lvl);
		break;
	default:
		fprintf(stderr, "%s: unknown knob/attribute\n", __func__);
		break;
	}
	return NULL;
}

//
// set_knob_value - Set a power control 'knob' to a value
//
// Argument(s):
//
//	ctx      - the PWR_Cntxt to use
//	knob     - the integer knob to set, from enum
//	knob_val - the value to which to set the control knob
//	obj_lvl  - the object level of power limit values
//	grps     - an array of PWR_Grp objects indexed by the object type
//	           enum (PWR_OBJ_NODE, PWR_OBJ_HT, etc.)
//
// Return Code(s):
//
//	zero for success, non-zero otherwise.
//
static int set_knob_value(PWR_Cntxt *ctx, int knob, double knob_val,
			  PWR_ObjType obj_lvl, PWR_Grp *grps)
{
	int ret;
	PWR_Obj my_obj;
	PWR_AttrName my_attr;
	switch (knob) {
	case knob_pstate:
		my_attr = PWR_ATTR_FREQ_REQ;
		break;
	case knob_pwrlimit:
		my_attr = PWR_ATTR_POWER_LIMIT_MAX;
		break;
	default:
		return -1;
		break;
	}

	ret = PWR_GrpAttrSetValue(grps[obj_lvl], my_attr, &knob_val, NULL);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: Failed to set attribute value: %i\n",
			__func__, ret);
		return -1;
	}
	return 0;
}

//
// run_report - Print run report.
//
// Argument(s):
//
//	s        - the run_stats_type object to use in reporting
//	wload    - the integer for the workload from enum
//	knob     - the power control knob acted upon from enum
//	knob_val - the value changed
//
// Return Code(s):
//
//	void
//
static void run_report(run_stats_type *s, int wload, int knob, double knob_val)
{
	int i = 0;
	double node_pwr_stat_val[stat_MAX];
	double socket_pwr_stat_val[stat_MAX];
	double socket_energy_stat_val[stat_MAX];
	double socket_temp_stat_val[stat_MAX];
	double mem_pwr_stat_val[stat_MAX];
	double mem_energy_stat_val[stat_MAX];
	double core_temp_stat_val[stat_MAX];
	PWR_TimePeriod node_pwr_stat_tp[stat_MAX];
	PWR_TimePeriod unused_tp;
	int ret;
	int grp_stat_idx;
	PWR_Time grp_stat_inst;
	char *wload_txt;
	char *knob_line;
	char hostname[20];

	for (i = 0; i < stat_MAX; i++) {
		ret = PWR_StatGetValue(s->node_pwr[i], &node_pwr_stat_val[i],
				       &node_pwr_stat_tp[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetValue() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->socket_cnt > 1) {
			ret = PWR_StatGetReduce(s->socket_pwr[i], map_stat[i],
				&grp_stat_idx, &socket_pwr_stat_val[i],
				&grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->socket_pwr[i],
				&socket_pwr_stat_val[i], &unused_tp);
		}

		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->socket_cnt > 1) {
			ret = PWR_StatGetReduce(
				s->socket_energy[i], map_stat[i], &grp_stat_idx,
				&socket_energy_stat_val[i], &grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->socket_energy[i],
				&socket_energy_stat_val[i], &unused_tp);
		}
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->socket_cnt > 1) {
			ret = PWR_StatGetReduce(
				s->socket_temp[i], map_stat[i], &grp_stat_idx,
				&socket_temp_stat_val[i], &grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->socket_temp[i],
				&socket_temp_stat_val[i], &unused_tp);
		}
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->mem_cnt > 1) {
			ret = PWR_StatGetReduce(
				s->mem_pwr[i], map_stat[i], &grp_stat_idx,
				&mem_pwr_stat_val[i], &grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->mem_pwr[i],
				&mem_pwr_stat_val[i], &unused_tp);
		}
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->mem_cnt > 1) {
			ret = PWR_StatGetReduce(s->mem_energy[i], map_stat[i],
				&grp_stat_idx, &mem_energy_stat_val[i],
				&grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->mem_energy[i],
				&mem_energy_stat_val[i], &unused_tp);
		}

		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	for (i = 0; i < stat_MAX; i++) {
		if (s->core_cnt > 1) {
			ret = PWR_StatGetReduce(s->core_temp[i], map_stat[i],
				&grp_stat_idx, &core_temp_stat_val[i],
				&grp_stat_inst);
		} else {
			ret = PWR_StatGetValue(s->core_temp[i],
				&core_temp_stat_val[i], &unused_tp);
		}

		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetReduce() failed: %i\n",
				__func__, ret);
			return;
		}
	}

	switch (wload) {
	case workload_dgemm:
		wload_txt = "DGEMM";
		break;
	case workload_stream:
		wload_txt = "STREAM";
		break;
	default:
		fprintf(stderr, "%s: Unknown workload %i\n", __func__, wload);
		exit(-1);
	}

	switch (knob) {
	case knob_pstate:
		knob_line = malloc(80 * sizeof(char));
		if (snprintf(knob_line, 80, "P-state/Frequency value: %.f kHz",
			     knob_val) < 0) {
			fprintf(stderr, "%s: snprintf failed\n", __func__);
			exit(-1);
		}
		break;
	case knob_pwrlimit:
		knob_line = malloc(80 * sizeof(char));
		if (snprintf(knob_line, 80, "Power limit value: %.f W",
			     knob_val) < 0) {
			fprintf(stderr, "%s: snprintf failed\n", __func__);
			exit(-1);
		}
		break;
	case -1: // Knob is unset, just ignore
		knob_line = "Power controls unchanged";
		break;
	default:
		fprintf(stderr, "%s: Unknown control knob %i\n", __func__,
			knob);
		exit(-1);
	}

	if (ret = gethostname(&hostname[0], 20)) {
		fprintf(stderr, "%s: gethostname failed: %d\n", __func__, ret);
		exit(-1);
	}

	static const char *fmt = "\n"
			"Run Summary\n"
			"===========\n"
			"Node: %s\n"
			"Workload: %s\n"
			"%s\n"
			"Run duration: %10.4f s\n"
			"Statistics for: %d socket%s, %d memory object%s, %d cores\n"
			"                     %-10s %-10s %-10s %-10s\n"
			"Node power (W)       %-10.f %-10.f %-10.4f %-10.4f\n"
			"Socket power (W)     %-10.f %-10.f %-10.4f %-10.4f\n"
			"Socket energy (J)    %-10.f %-10.f %-10.4f %-10.4f\n"
			"Socket temp (DegC)   %-10.f %-10.f %-10.4f %-10.4f\n"
			"Memory power (W)     %-10.f %-10.f %-10.4f %-10.4f\n"
			"Memory energy (J)    %-10.f %-10.f %-10.4f %-10.4f\n"
			"Core temp (DegC)     %-10.f %-10.f %-10.4f %-10.4f\n"
			"\n\n";
	printf(fmt, hostname, wload_txt, knob_line,
	       ((float)(node_pwr_stat_tp[stat_min].stop -
			node_pwr_stat_tp[stat_min].start) /
		1000000000.0),
	       s->socket_cnt, (s->socket_cnt > 1) ? "s" : "",
	       s->mem_cnt, (s->mem_cnt > 1) ? "s" : "", s->core_cnt,
	       "Min", "Max", "Avg", "Std",
	       node_pwr_stat_val[stat_min], node_pwr_stat_val[stat_max],
	       node_pwr_stat_val[stat_avg],node_pwr_stat_val[stat_stdev],
	       socket_pwr_stat_val[stat_min], socket_pwr_stat_val[stat_max],
	       socket_pwr_stat_val[stat_avg], socket_pwr_stat_val[stat_stdev],
	       socket_energy_stat_val[stat_min], socket_energy_stat_val[stat_max],
	       socket_energy_stat_val[stat_avg], socket_energy_stat_val[stat_stdev],
	       socket_temp_stat_val[stat_min], socket_temp_stat_val[stat_max],
	       socket_temp_stat_val[stat_avg], socket_temp_stat_val[stat_stdev],
	       mem_pwr_stat_val[stat_min], mem_pwr_stat_val[stat_max],
	       mem_pwr_stat_val[stat_avg], mem_pwr_stat_val[stat_stdev],
	       mem_energy_stat_val[stat_min], mem_energy_stat_val[stat_max],
	       mem_energy_stat_val[stat_avg], mem_energy_stat_val[stat_stdev],
	       core_temp_stat_val[stat_min], core_temp_stat_val[stat_max],
	       core_temp_stat_val[stat_avg], core_temp_stat_val[stat_stdev]);

	return;
}

//
// knob_compare_asc - Compare power control knob values for ascending sort.
//
// Argument(s):
//
//	a - pointer to a double for comparison
//	b - pointer to another double for comparison
//
// Return Code(s):
//
//	int
//
static int knob_compare_asc(const void *a, const void *b)
{
	// treat 0 as max
	if (*(double *)a == 0.0) {
		return 1;
	} else if (*(double *)b == 0.0) {
		return -1;
	}
	if (*(double *)a > *(double *)b) {
		return 1;
	} else if (*(double *)a < *(double *)b) {
		return -1;
	} else {
		return 0;
	}
}

//
// knob_compare_desc - Compare power control knob values for descending sort.
//
// Argument(s):
//
//	a - pointer to a double for comparison
//	b - pointer to another double for comparison
//
// Return Code(s):
//
//	int
//
static int knob_compare_desc(const void *a, const void *b)
{
	// treat 0 as max
	if (*(double *)a == 0.0) {
		return -1;
	} else if (*(double *)b == 0.0) {
		return 1;
	}
	if (*(double *)a > *(double *)b) {
		return -1;
	} else if (*(double *)a < *(double *)b) {
		return 1;
	} else {
		return 0;
	}
}

//
// workload_loop - Main workload loop, manages knob turning & running workloads
//
// Argument(s):
//
//	demo_opt  - demo options
//	cmd_flags - command line flags
//	ctx       - pointer to the PWR_Cntxt to use
//	grps      - a pointer to an array of groups of length PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	void
//
static void workload_loop(demo_opt_t *demo_opt, cmd_flags_t *cmd_flags,
			  PWR_Cntxt *ctx, PWR_Grp *grps)
{
	double *knob_vals = NULL;
	int knob_val_cnt = 1;
	int knob = demo_opt->knob;
	int i = 0;
	char *knob_name;

	// Get list of values over which to iterate
	if (knob > -1) {
		if (demo_opt->do_cycle) {
			printf("Querying for power control values...");
			knob_vals = get_knob_values(ctx, knob, &knob_val_cnt,
						    demo_opt->obj_lvl);
			if (knob_val_cnt < 1) {
				fprintf(stderr,
					"%s: Warning: query_knob_values failed"
					" to find any values\n",
					__func__);
			} else {
				if (demo_opt->asc) {
					qsort(knob_vals, knob_val_cnt,
					      sizeof(double), knob_compare_asc);
				} else {
					qsort(knob_vals, knob_val_cnt,
					      sizeof(double),
					      knob_compare_desc);
				}
				printf(" found %i values to iterate over:\n  ",
				       knob_val_cnt);
				for (i = 0; i < knob_val_cnt; i++) {
					printf("%.f ", knob_vals[i]);
				}
				printf("\n");
			}
		} else {
			knob_vals = (double *)malloc(sizeof(double));
			knob_vals[0] = (double)demo_opt->knob_val;
		}
	}

	// loop over values and run the workload
	for (i = 0; i < knob_val_cnt; i++) {
		run_stats_type s;

		if (knob > -1) {
			set_knob_value(ctx, knob, knob_vals[i],
				       demo_opt->obj_lvl, grps);
		}

		if (demo_opt->do_stats && init_stats(ctx, grps, &s)) {
			fprintf(stderr, "%s: Failed to create stats\n",
				__func__);
			exit(-1);
		}

		if (demo_opt->do_stats && start_stats(&s)) {
			fprintf(stderr, "%s: Failed to start stats\n",
				__func__);
			return;
		}

		run_workload(demo_opt);

		if (demo_opt->do_stats) {
			stop_stats(&s);
			run_report(&s, demo_opt->workload, knob,
				   knob > -1 ? knob_vals[i] : 0.0);
			cleanup_stats(&s);
		}
		fflush(stdout);
	}

	if (knob > -1) {
		free(knob_vals);
	}
}

//
// discover_objs - Traverse hierarchy and build groups of objects by PWR_ObjType
//
// Argument(s):
//
//	ctx       - pointer tp the PWR_Cntxt to use
//	grps      - a pointer to an allocated array of groups of
//	            length PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
static int discover_objs(PWR_Cntxt *ctx, PWR_Grp *groups)
{
	int ret;
	PWR_Obj obj;
	int i;
	PWR_Grp obj_q; // queue of objects for traversal

	// Create empty groups for each object type
	for (i = 0; i < PWR_NUM_OBJ_TYPES; i++) {
		ret = PWR_GrpCreate(*ctx, &groups[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_GrpCreate() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
	}

	ret = PWR_CntxtGetEntryPoint(*ctx, &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "PWR_CntxtGetEntryPoint() failed: %d\n", ret);
		exit(ret);
	}

	// Create object queue group and add entry point to it
	ret = PWR_GrpCreate(*ctx, &obj_q);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_GrpCreate() failed: %d\n", __func__,
			ret);
		exit(ret);
	}
	ret = PWR_GrpAddObj(obj_q, obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_GrpAddObj() failed: %d\n", __func__,
			ret);
		exit(ret);
	}

	// Iterate over the queue until there are no objects unvisited (BFS)
	while (PWR_GrpGetNumObjs(obj_q) > 0) {
		// Initialize groups for group/set math
		PWR_Grp children; // Hold obj's children
		PWR_Grp temp;
		ret = PWR_GrpCreate(*ctx, &children);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_GrpCreate() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		ret = PWR_GrpCreate(*ctx, &temp);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_GrpCreate() failed: %d\n",
				__func__, ret);
			exit(ret);
		}

		// Get children and if any, add to queue
		ret = PWR_ObjGetChildren(obj, &children);
		if (ret != PWR_RET_WARN_NO_CHILDREN) {
			ret = PWR_GrpUnion(obj_q, children, &temp);
			if (ret != PWR_RET_SUCCESS) {
				fprintf(stderr,
					"%s: PWR_GrpUnion() failed: %d\n",
					__func__, ret);
				exit(ret);
			}
			obj_q = temp;
		} else if (ret < PWR_RET_SUCCESS) {
			// No children but call failed.
			fprintf(stderr, "%s: PWR_ObjGetChildren() failed: %d\n",
				__func__, ret);
			exit(ret);
		}

		// Delete children
		ret = PWR_GrpDestroy(children);
		if (ret != PWR_RET_SUCCESS) {
			// non-fatal error/warning
			fprintf(stderr, "%s: PWR_GrpDestroy() failed: %d\n",
				__func__, ret);
		}

		// Add obj to appropriate group
		PWR_ObjType obj_type;
		ret = PWR_ObjGetType(obj, &obj_type);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_ObjGetType() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		ret = PWR_GrpAddObj(groups[obj_type], obj);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_GrpAddObj() failed: %d\n",
				__func__, ret);
			exit(ret);
		}

		// Remove obj from obj_q

		ret = PWR_GrpRemoveObj(obj_q, obj);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_GrpRemoveObj() failed: %d\n",
				__func__, ret);
			exit(ret);
		}

		// Set obj to the next obj in obj_q
		ret = PWR_GrpGetObjByIndx(obj_q, 0, &obj);
		if (ret != PWR_RET_SUCCESS) {
			if (ret == PWR_RET_NO_OBJ_AT_INDEX) {
				return;
			}
			fprintf(stderr,
				"%s: PWR_GrpGetObjByIndx() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
	}
}

//
// set_userspace_gov - Sets performance governor to 'userspace' to allow
//	p-state/frequency changes.
//
// Argument(s):
//
//	ctx  - pointer to an initialized Power API context.
//
// Return Code(s):
//
//	int - return value from PWR_ObjAttrSetValue (e.g., PWR_RET_*)
//
int set_userspace_gov(PWR_Cntxt *ctx)
{
	PWR_AttrGov gov;
	PWR_Obj obj;
	int ret;

	// Check that a socket object exists
	ret = PWR_CntxtGetObjByName(*ctx, "node.0", &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtGetObjByName failed: %i\n",
			__func__, ret);
		exit(ret);
	}
	gov = PWR_GOV_LINUX_USERSPACE;
	ret = PWR_ObjAttrSetValue(obj, PWR_ATTR_GOV, &gov);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr,
			"%s: Failed to set PWR_ATTR_GOV "
			"to PWR_GOV_LINUX_USERSPACE: %d\n",
			__func__, ret);
		exit(ret);
	}

	return ret;
}

//
// main - Main entry point.
//
// Argument(s):
//
//  argc - Number of arguments
//  argv - Arguments
//
// Return Code(s):
//
//  int - Return code
//
int main(int argc, char *argv[])
{
	PWR_Cntxt ctx; // our context
	cmd_flags_t cmd_flags;
	PWR_Grp *groups = malloc(PWR_NUM_OBJ_TYPES * sizeof(PWR_Grp));
	int err = 0;

	// Initialize options and parse command line args
	INIT_DEMO_OPT(demo_opt);
	memset(&cmd_flags, 0, sizeof(cmd_flags_t));
	if (err = parse_cmd_line(argc, argv, &demo_opt, &cmd_flags)) {
		fprintf(stderr, "Error%s in command line options\n",
			(err > 1) ? "s" : "");
		exit(EINVAL);
	}

	printf("Cray Power API pwrdemo\nApplication-Level Power Control &"
	       " Monitoring Demo Application\n");
	printf("Compiled against library supporting spec version: %d.%d\n\n",
	       PWR_GetMajorVersion(), PWR_GetMinorVersion());

	if (cmd_flags.h_flag > 0) {
		usage(0);
	}

	// Do Power API context set up
	api_init(&ctx, PWR_ROLE_APP);

	// Walk the object hierarchy and populate groups of like-objects
	discover_objs(&ctx, groups);

	// If setting p-states/freqs, governor needs to be userspace
	set_userspace_gov(&ctx);

	fflush(stdout);

	// Run workload
	workload_loop(&demo_opt, &cmd_flags, &ctx, groups);

	// Run profiling battery and report if requested
	if (demo_opt.do_profile) {
		profile_and_report(&ctx, groups);
	}

	// Cleanup
	api_cleanup(&ctx);
	printf("All complete.\n");
	exit(0);
}
