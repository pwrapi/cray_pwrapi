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
 * Main functions for the powerapi daemon.  powerapid is a daemon with root
 * privileges which will enable user-level processes to make calls to the ACES
 * powerapi library in order to set power parameters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>
#include <glib.h>
#include <log.h>

#include "ctrl_socket.h"
#include "ctrl_request.h"

// Data structures for parsing command line arguments.
enum {
    cmdline_MIN = 1000,
    cmdline_help,
    cmdline_clear,
    cmdline_debug,
    cmdline_trace,
    cmdline_status,
    cmdline_dump,
    cmdline_MAX
};

static const char *Short_Options = "hd::t::suDT";
static struct option Long_Options[] = {
    { "help",       no_argument,        NULL, cmdline_help },
    { "clear",      no_argument,        NULL, cmdline_clear },
    { "debug",      optional_argument,  NULL, cmdline_debug },
    { "trace",      optional_argument,  NULL, cmdline_trace },
    { "status",     no_argument,        NULL, cmdline_status },
    { "dump",       no_argument,        NULL, cmdline_dump },
    { NULL }
};

static int h_flag = 0;          // help flag
static int d_flag = -1;         // debug flag
static int t_flag = -1;         // trace flag
static int s_flag = 0;          // status flag
static int u_flag = 0;          // dump flag
static int D_flag = 0;          // internal debug flag
static int T_flag = 0;          // internal trace flag

//
// usage - Print usage statement.
//
// Argument(s):
//
//      exitcode - Exit code to pass to exit() after printing the usage
//                 statement.
//
// Return Code(s):
//
//      DOES NOT RETURN
//
void __attribute__((noreturn))
usage(int exit_code)
{
    static const char *fmt =
        "\n"
        "Usage: %s [-h] [-d|--debug[=level]] [-t|--trace[=level]]\n"
        "                [-c|--clear] [-s|--status] [-u|--dump] [-DT]\n"
        "\n"
        "Options:\n"
        "\n"
        "   -h/--help           Print this usage message\n"
        "   -c/--clear          Clear debug/trace level to zero\n"
        "   -d/--debug[=level]  Increase or set daemon debug level\n"
        "   -t/--trace[=level]  Increase or set daemon trace level\n"
        "   -s/--status         Report current daemon debug/trace levels\n"
        "   -u/--dump           Dump daemon internal state\n"
        "   -D                  Increase debug level for control app (not daemon)\n"
        "   -T                  Increase trace level for control app (not daemon)\n"
	"\n"
	"   -d   == --debug=1, in daemon, display DBG1\n"
	"   -dd  == --debug=2, in daemon, display DBG1 and DBG2\n"
	"   -t   == --trace=1, in daemon, display TRC1\n"
	"   -tt  == --trace=2, in daemon, display TRC1 and TRC2\n"
	"   -ttt == --trace=3, in daemon, display TRC1, TRC2, and TRC3\n"
	"\n"
	"   -D   -> in control app, display DBG1\n"
	"   -DD  -> in control app, display DBG1 and DBG2\n"
	"   -T   -> in control app, display TRC1\n"
	"   -TT  -> in control app, display TRC1 and TRC2\n"
	"   -TTT -> in control app, display TRC1, TRC2, and TRC3\n"
        "\n"
        ;
    TRACE1_ENTER("exit_code = %d", exit_code);

    fprintf((exit_code) ? stderr : stdout, fmt, g_get_prgname());

    TRACE1_EXIT("exit_code = %d", exit_code);
    exit(exit_code);
}

//
// parse_cmd_line - Parse command line arguments.
//
// Argument(s):
//
//      argc   - Number of arguments
//      argv   - Argument list
//
// Return Code(s):
//
//      void
//
static void
parse_cmd_line(int argc, char **argv)
{
    int c = 0;

    TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

    // Loop through all specified options
    while ((c = getopt_long(argc, argv, Short_Options, Long_Options,
                NULL)) != -1) {
        switch (c) {
        case cmdline_help:
        case 'h':
            h_flag++;
            usage(0);
            // NOT REACHED
            LOG_DBG("NOT REACHED");
            break;
        case cmdline_clear:
        case 'c':
            d_flag = 0;
            t_flag = 0;
            break;
        case cmdline_debug:
        case 'd':
            if (d_flag < 0) {
                d_flag = 0;
            }
            if (optarg == NULL) {
                d_flag++;
            } else {
                d_flag = strtol(optarg, NULL, 0);
            }
            break;
        case cmdline_trace:
        case 't':
            if (t_flag < 0) {
                t_flag = 0;
            }
            if (optarg == NULL) {
                t_flag++;
            } else {
                t_flag = strtol(optarg, NULL, 0);
            }
            break;
        case cmdline_status:
        case 's':
            s_flag++;
            break;
        case cmdline_dump:
        case 'u':
            u_flag++;
            break;
        case 'D':
            D_flag++;
            break;
        case 'T':
            T_flag++;
            break;
        default:
            usage(1);
            // NOT REACHED
            LOG_DBG("NOT REACHED");
            break;
        }
    }
}

// main - Main routine
//
int
main(int argc, char *argv[])
{
    gchar *prgname = NULL;

    // Set program/application name
    prgname = g_path_get_basename(argv[0]);
    g_set_prgname(prgname);
    g_free(prgname);
    g_set_application_name("PowerAPI Control Daemon Control");

    /*
     * NOTE: this initializes the logging system with defaults.
     */
    LOG_DBG("%s started", g_get_prgname());
    TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

    // Parse command line args
    parse_cmd_line(argc, argv);

    /*
     * Set the DEBUG and TRACE mask to stderr.
     */
    pmlog_stderr_set_level(D_flag, T_flag);

    // Connect to daemon
    daemon_connect();

    if (d_flag || t_flag || s_flag) {
        // Process the debug/trace request
        do_loglvl_request(d_flag, t_flag, s_flag);
    }

    if (u_flag) {
        // Process the dump request
        do_dump_request();
    }

    // Disconnect from daemon
    daemon_disconnect();

    TRACE1_EXIT("");

    return 0;
}
