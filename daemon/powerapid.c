/*
 * Copyright (c) 2016-2017, Cray Inc.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#include <glib.h>

#include <cray-powerapi/powerapid.h>
#include <log.h>

#include <permissions.h>
#include "powerapid.h"
#include "pwrapi_socket.h"
#include "pwrapi_set.h"
#include "pwrapi_worker.h"
#include "pwrapi_signal.h"
#include "pwrapi_down.h"

#define MAX_CLIENT_SOCKETS 300

// open_sockets is a hash table of currently open sockets. It is keyed by
// socket number (file descriptor).  Entries in the hash table are
// socket_info_t structs.
//
// def_values is a hash table of the default values for the attributes that
// have been set.  The table is keyed by the set path.  Entries in the hash
// table are set_info_t structs.
//
// all_changes is a hash table of all the change requests that have been
// received.  The table is keyed by the set path.  Entries in the hash table
// are singly linked lists of set_info_t structs, sorted by the appropriate
// priority for the attribute, highest priority first.
//
// work_queue is a thread safe queue of set requests (set_info_t).  The main
// thread will push incoming requests onto the queue and a second "worker"
// thread will process the requests in order.
GHashTable   *open_sockets = NULL;
GHashTable   *def_values   = NULL;
GHashTable   *all_changes  = NULL;
GAsyncQueue  *work_queue   = NULL;

int daemonize = 1;
int daemon_run = 1;
static const char *pidfile = POWERAPID_PIDFILE_PATH;
static int restart = 0;

static int D_flag;
static int T_flag;

// Data structures for parsing command line arguments.
enum {
	cmdline_MIN = 1000,
	cmdline_help,
	cmdline_pidfile,
	cmdline_restart,
	cmdline_nodaemon,
	cmdline_debug,
	cmdline_trace,
	cmdline_MAX
};

static const char *Short_Options = "hp:rnDT";
static struct option Long_Options[] = {
	{ "help",     no_argument,       NULL, cmdline_help },
	{ "pidfile",  required_argument, NULL, cmdline_pidfile },
	{ "restart",  no_argument,       NULL, cmdline_restart },
	{ "nodaemon", no_argument,       NULL, cmdline_nodaemon },
	{ "debug",    no_argument,       NULL, cmdline_debug },
	{ "trace",    no_argument,       NULL, cmdline_trace },
	{ NULL }
};

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
static void __attribute__((noreturn))
usage(int exit_code)
{
	static const char *fmt =
			"\n"
			"Usage: %s [-hrnDT] [-p pidfile]\n"
			"\n"
			"Options:\n"
			"\n"
			"   -h/--help       print this usage message\n"
			"   -p/--pidfile    Pathname to pidfile to use\n"
			"   -r/--restart    Allow daemon restart\n"
			"   -n/--nodaemon   Don't run as a daemon (for debugging)\n"
			"   -D/--debug      Increase debug level to stderr\n"
			"   -T/--trace      Increase trace level to stderr\n"
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
	int p_flag = 0;

	TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

	while ((c = getopt_long(argc, argv, Short_Options, Long_Options, NULL)) != -1) {
		switch (c) {
		case cmdline_help:
		case 'h':
			usage(0);
	    // NOT REACHED
			LOG_DBG("NOT REACHED");
			break;
		case cmdline_pidfile:
		case 'p':
	    // Verify option only specified once.
			if (++p_flag > 1) {
				fprintf(stderr, "The -p/--pidfile option may only be specified once.");
				usage(1);
		// NOT REACHED
				LOG_DBG("NOT REACHED");
			}

	    // Save pathname
			pidfile = optarg;
			LOG_DBG("-p/--pidfile command line option specified: %s", pidfile);
			break;
		case cmdline_restart:
		case 'r':
			restart = 1;
			LOG_DBG("-r/--restart command line option specified");
			break;
		case cmdline_nodaemon:
		case 'n':
			daemonize = 0;
			LOG_DBG("-n/--nodaemon command line option specified");
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
			usage(1);
	    // NOT REACHED
			LOG_DBG("NOT REACHED");
			break;
		}
	}

	TRACE1_EXIT("");
}

// create_pidfile - Create pidfile and write the daemon's PID into it
//
// Argument(s):
//
//      none
//
// Return Code(s):
//
//      void
//
static void
create_pidfile(void)
{
	GError *error = NULL;
	char *pidfile_contents = g_strdup_printf("%d", getpid());

	TRACE1_ENTER("pidfile = '%s', pidfile_contents = '%s'",
			pidfile, pidfile_contents);

	if (!g_file_set_contents(pidfile, pidfile_contents,
					strlen(pidfile_contents), &error)) {
		LOG_CRIT("Could not write pidfile '%s': %s", pidfile, error->message);
		exit(1);
	}

	g_free(pidfile_contents);

	TRACE1_EXIT("");
}

// print out a list of attribute set requests from all_changes
static void
list_print(gpointer key, gpointer value, gpointer user_data)
{
	GSList     *list = NULL, *iter;
	set_info_t *first = NULL;
	const char *path = NULL;

	if (value) {
		list = (GSList *)value;
		first = (set_info_t *)(list->data);
		if (first)
			path = first->setreq.path;
		else
			path = "empty key";
	}

	LOG_DBG("key: %s", path);
	for (iter = list; iter; iter = iter->next) {
		set_print(NULL, iter->data, NULL);
	}
}

static void
debug_dump(void)
{
	TRACE2_ENTER("");

	LOG_DBG("Dump open_sockets");
	g_hash_table_foreach(open_sockets, socket_print, NULL);
	LOG_DBG("Dump def_values");
	g_hash_table_foreach(def_values, set_print, NULL);
	LOG_DBG("Dump all_changes");
	g_hash_table_foreach(all_changes, list_print, NULL);
	LOG_DBG("Dump done");

	TRACE2_EXIT("");
}

static void
send_response(socket_info_t *skinfo, powerapi_response_t *resp)
{
	ssize_t bytes_written;

	TRACE1_ENTER("skinfo = %p, resp = %p", skinfo, resp);

	LOG_DBG("resp->retval = %d", resp->retval);

	resp->sequence = skinfo->seqnum++;

	bytes_written = write(skinfo->sockid, resp, sizeof(*resp));
	if (bytes_written != sizeof(*resp)) {
		if (bytes_written < 0) {
			LOG_FAULT("Response write error: fd = %d: %m", skinfo->sockid);
		} else {
			LOG_FAULT("Response write error: fd = %d, "
					"bytes_written = %zd, attempted = %zu",
					skinfo->sockid, bytes_written, sizeof(*resp));
		}
	}

	TRACE1_EXIT("");
}

void
send_ret_code_response(socket_info_t *skinfo, int ret_code)
{
	powerapi_response_t resp = { .retval = ret_code };

	TRACE1_ENTER("skinfo = %p, ret_code = %d", skinfo, ret_code);

	send_response(skinfo, &resp);

	TRACE1_EXIT("");
}

static int
process_client_req(int client_socket)
{
	int                  retval = 1;
	ssize_t              bytes_read;
	powerapi_request_t   req;
	powerapi_response_t  resp = { .retval = PWR_RET_SUCCESS };
	socket_info_t        *skinfo;
	int                  send_response_now = TRUE;

	TRACE1_ENTER("client_socket = %d", client_socket);

	bytes_read = read(client_socket, &req, sizeof(req));
	if (bytes_read != sizeof(req)) {
		if (bytes_read < 0) {
			LOG_FAULT("Request read error: fd = %d: %m", client_socket);
		} else if (bytes_read == 0) {
			LOG_DBG("Client socket %d closed.", client_socket);
		} else {
			LOG_FAULT("Request read error: fd = %d, "
					"bytes_read = %zd, attempted = %zu",
					client_socket, bytes_read, sizeof(req));
		}
		goto done;
	}

	skinfo = socket_lookup(client_socket);
	if (skinfo == NULL) {
		LOG_FAULT("Socket info not found for client %d!", client_socket);
		goto done;
	}

	switch (req.ReqType) {
	case PwrAUTH:
		LOG_DBG("Processing PwrAUTH request");

		if (skinfo->role != PWR_ROLE_NOT_SPECIFIED) {
			LOG_FAULT("Redundant authorization request from client %d!",
					client_socket);
			resp.retval = PWR_RET_INVALID;
			break;
		}

		skinfo->role         = req.auth.role;
		skinfo->context_name = g_strdup(req.auth.context_name);
		if (!skinfo->context_name) {
			LOG_CRIT(MEM_ERROR_EXIT);
			exit(1);
		}

		LOG_DBG("Auth request received: client = %d, role = %d, "
				"name = %s, uid = %d, gid = %d, pid = %d",
				client_socket, skinfo->role, skinfo->context_name,
				skinfo->cred.uid, skinfo->cred.gid, skinfo->cred.pid);
		break;
	case PwrSET:
		LOG_DBG("Processing PwrSET request");

		if (skinfo->role == PWR_ROLE_NOT_SPECIFIED) {
			LOG_FAULT("Set request from unauthorized client %d!",
					client_socket);
			resp.retval = PWR_RET_INVALID;
			break;
		}

		set_info_t *setp = set_create_item(&req.set, skinfo);
		g_async_queue_push(work_queue, setp);
	// The response will be sent when the set request
	// is processed by the worker thread.
		send_response_now = FALSE;
		break;
	case PwrLOGLVL:
		LOG_DBG("Processing PwrLOGLVL request, dbglvl = %d, trclvl = %d",
				req.loglvl.dbglvl, req.loglvl.trclvl);
		if (pmlog_stderr_set_level(req.loglvl.dbglvl, req.loglvl.trclvl) == -1) {
			LOG_DBG("pmlog_stderr_set_level failed");
			resp.retval = PWR_RET_INVALID;
			break;
		}
		if (pmlog_stderr_get_level(&resp.loglvl.dbglvl, &resp.loglvl.trclvl) == -1) {
			LOG_DBG("pmlog_stderr_get_level failed");
			resp.retval = PWR_RET_INVALID;
			break;
		}
		LOG_DBG("PwrLOGLVL success, dbglvl = %d, trclvl = %d",
				resp.loglvl.dbglvl, resp.loglvl.trclvl);
		break;
	case PwrDUMP:
		LOG_DBG("Processing PwrDUMP request");
		if (skinfo->cred.uid != 0) {
			resp.retval = PWR_RET_OP_NO_PERM;
			break;
		}
		debug_dump();
		break;
	default:
		LOG_FAULT("Invalid request type (%d) received from client %d",
				req.ReqType, client_socket);
		resp.retval = PWR_RET_INVALID;
		break;
	}

	if (send_response_now) {
		send_response(skinfo, &resp);
	}

	retval = 0;

done:
	TRACE1_EXIT("retval = %d", retval);

	return retval;
}

static void
abort_connect_req(int client_socket, int ret_code)
{
	socket_info_t skinfo = { .sockid = client_socket };
	powerapi_response_t resp = { .retval = ret_code };

	TRACE1_ENTER("client_socket = %d, ret_code = %d", client_socket, ret_code);

	send_response(&skinfo, &resp);

	close(client_socket);

	TRACE1_EXIT("");
}

static int
process_connect_req(int server_socket, int num_client_sockets,
		struct ucred *cred)
{
	static int     err_throttle = 0;
	int            retval = -1;
	int            client_socket;
	socklen_t      len;

	TRACE1_ENTER("server_socket = %d, num_client_sockets = %d, cred = %p",
			server_socket, num_client_sockets, cred);

	client_socket = accept(server_socket, NULL, NULL);
	if (client_socket < 0) {
		LOG_FAULT("accept() failed: %m");
		goto done;
	}

	if (num_client_sockets >= MAX_CLIENT_SOCKETS) {
		if (err_throttle++ == 0) {
			LOG_FAULT("error: open socket limit reached!");
		}
		abort_connect_req(client_socket, PWR_RET_FAILURE);
		LOG_DBG("open socket limit reached");
		goto done;
	}

	len = sizeof(*cred);
	if (getsockopt(client_socket, SOL_SOCKET, SO_PEERCRED, cred, &len) == -1) {
		if (err_throttle++ == 0) {
			LOG_FAULT("error: unable to get client "
					"credentials: %m");
		}
		abort_connect_req(client_socket, PWR_RET_INVALID);

		LOG_DBG("unable to get client credentials");
		goto done;
	}

	err_throttle = 0;

	if (check_permissions_file(cred->uid) != 0) {
		LOG_FAULT("authentication error: uid %u not permitted to connect",
				cred->uid);
		abort_connect_req(client_socket, PWR_RET_OP_NO_PERM);
		goto done;
	}

	retval = client_socket;

done:
	TRACE1_EXIT("client_socket = %d, uid = %d, gid = %d, pid = %d",
			client_socket, cred->uid, cred->gid, cred->pid);

	return retval;
}

static void
set_state_dirty(void)
{
	int fd;

	TRACE1_ENTER("");

	fd = open(POWERAPID_STATE_DIRTY_PATH, O_CREAT | O_RDWR, 0666);
	if (fd < 0) {
		LOG_CRIT("Unable to create %s!", POWERAPID_STATE_DIRTY_PATH);
		exit(1);
	}

	close(fd);

	TRACE1_EXIT("");
}

static void
set_state_clean(void)
{
	TRACE1_ENTER("");

	unlink(POWERAPID_STATE_DIRTY_PATH);

	TRACE1_EXIT("");
}

static void
check_state_dirty(void)
{
	struct stat statbuf = { 0 };

	TRACE1_ENTER("");

	if (stat(POWERAPID_STATE_DIRTY_PATH, &statbuf) != 0) {
		TRACE1_EXIT("state not dirty");
		return;
	}

	LOG_CRIT("File %s exists! Daemon state is dirty.",
			POWERAPID_STATE_DIRTY_PATH);
	LOG_CRIT("Daemon appears to have exited abnormally.");

	if (stat(POWERAPID_ALLOW_RESTART_PATH, &statbuf) == 0) {
		LOG_CRIT("File %s exists. Allowing restart...",
				POWERAPID_ALLOW_RESTART_PATH);
		restart = 1;
	}

	if (!restart) {
		LOG_CRIT("Restart disallowed. Setting node admin-down.");
		set_node_admin_down();
		LOG_CRIT("Waiting for signal to exit.");
		pause();
		exit(1);
	}

	LOG_WARN("Restart allowed. Continuing...");

	set_state_clean();

	TRACE1_EXIT("");
}

static GThread *
worker_start(void)
{
	GThread *worker;

	TRACE1_ENTER("");

	worker = g_thread_new("worker", worker_process_items, NULL);
	if (worker == NULL) {
		LOG_CRIT("Unable to create worker thread!");
		exit(1);
	}

	TRACE1_EXIT("worker = %p", worker);

	return worker;
}

static void
worker_stop(GThread *worker)
{
	set_info_t item;

	TRACE1_ENTER("worker = %p", worker);

	memset(&item, 0, sizeof(item));

    // Push an empty request onto the the work queue to
    // wake up the worker thread and have it exit.
	g_async_queue_push(work_queue, &item);

    // Wait for worker thread to exit.
	g_thread_join(worker);

	TRACE1_EXIT("");
}

static int
named_socket_construct(void)
{
	int                new_socket;
	struct sockaddr_un saddr = { 0 };
	int                result;

	TRACE1_ENTER("");

	new_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (new_socket < 0) {
		LOG_CRIT("socket() failed: %m");
		exit(1);
	}

	saddr.sun_family = AF_UNIX;
	if (g_strlcpy(saddr.sun_path, POWERAPID_SOCKET_PATH,
					sizeof(saddr.sun_path)) >= sizeof(saddr.sun_path)) {
		LOG_CRIT("Named socket path '%s' too long for buffer!",
				POWERAPID_SOCKET_PATH);
		exit(1);
	}

	unlink(POWERAPID_SOCKET_PATH);

	result = bind(new_socket, (struct sockaddr *)&saddr, sizeof(saddr));
	if (result == -1) {
		LOG_CRIT("bind() failed: %m");
		exit(1);
	}

	chmod(POWERAPID_SOCKET_PATH, 0666);

	result = listen(new_socket, 10);
	if (result == -1) {
		LOG_CRIT("listen() failed: %m");
		exit(1);
	}

	TRACE1_EXIT("new_socket = %d", new_socket);

	return new_socket;
}

static void
named_socket_destruct(int named_socket)
{
	TRACE1_ENTER("named_socket = %d", named_socket);

	close(named_socket);

	unlink(POWERAPID_SOCKET_PATH);

	TRACE1_EXIT("");
}

int
main(int argc, char *argv[])
{
	int      named_socket;
	int      max_socket = 0;
	fd_set   incoming_sockets, select_sockets;
	int      num_client_sockets = 0;
	GThread *worker;
	char    *prgname = NULL;
	int      fd;
	struct rlimit rlim;

    // Set program/application name
	prgname = g_path_get_basename(argv[0]);
	g_set_prgname(prgname);
	g_free(prgname);
	g_set_application_name("PowerAPI Control Daemon");

    /*
     * NOTE: this initializes the logging system with defaults.
     */
	pmlog_init(POWERAPID_LOGFILE_PATH, 0, 0, 0, 0);
	LOG_DBG("%s started", g_get_prgname());

	TRACE1_ENTER("argc = %d, argv = %p", argc, argv);

    // Parse command line args before daemonizing
	parse_cmd_line(argc, argv);

    //
    // Set the DEBUG and TRACE mask to stderr.
    //
	pmlog_stderr_set_level(D_flag, T_flag);

    // Daemonize
	if (daemonize) {
		LOG_DBG("%s daemonizing", g_get_prgname());
		pmlog_term();
		if (daemon(0, 0) != 0) {
			LOG_CRIT("unable to launch daemon: %m");
			exit(1);
		}
		pmlog_init(POWERAPID_LOGFILE_PATH, 0, 0, 0, 0);
		pmlog_stderr_set_level(D_flag, T_flag);
		LOG_DBG("%s daemonized", g_get_prgname());
	}

	if (chdir(POWERAPID_WORKDIR_PATH) != 0) {
	/* Log error, but don't exit. */
		LOG_FAULT("Can't change working directory to %s: %m",
				POWERAPID_WORKDIR_PATH);
	}

	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;

	if (setrlimit(RLIMIT_CORE, &rlim) != 0) {
	/* Log error, but don't exit. */
		LOG_FAULT("Can't set RLIMIT_CORE to RLIM_INFINITY: %m");
	}

    // Create pidfile
	create_pidfile();

    // create daemon data structures
	open_sockets = g_hash_table_new(g_int_hash, g_int_equal);
	def_values   = g_hash_table_new(g_str_hash, g_str_equal);
	all_changes  = g_hash_table_new(g_str_hash, g_str_equal);
	work_queue   = g_async_queue_new();
	if (!open_sockets || !def_values || !all_changes || !work_queue) {
		LOG_CRIT(MEM_ERROR_EXIT);
		exit(1);
	}

	pwrapi_handle_signals();

	check_state_dirty();

	if (restore_permissions_file() != 0) {
		LOG_CRIT("Unable to initialize powerapi permissions file!");
		exit(1);
	}

	worker = worker_start();

	named_socket = named_socket_construct();
	max_socket = named_socket + 1;

    // process incoming socket requests
	FD_ZERO(&incoming_sockets);
	FD_SET(named_socket, &incoming_sockets);

	while (daemon_run) {
		int result;

		select_sockets = incoming_sockets;

		result = select(max_socket, &select_sockets, NULL, NULL, NULL);
		if (result < 0) {
			if (errno != EINTR) {
				LOG_FAULT("select() failed: %m");
			}
			continue;
		} else if (result == 0) {
			LOG_FAULT("select() timeout??");
			continue;
		}
	// else select succeeded

		for (fd = 0; fd < max_socket; ++fd) {
			if (FD_ISSET(fd, &select_sockets)) {
				if (fd == named_socket) {
					struct ucred cred = { .uid = -1 };
					int client_socket;

					client_socket = process_connect_req(fd,
							num_client_sockets, &cred);
					if (client_socket >= 0) {
						set_state_dirty();
						num_client_sockets++;
						FD_SET(client_socket, &incoming_sockets);
						if (client_socket >= max_socket) {
							max_socket = client_socket + 1;
						}
						socket_construct(client_socket, &cred);
					}
				} else {        // client socket
					if (process_client_req(fd) != 0) {
						socket_destruct(fd);
						close(fd);
						FD_CLR(fd, &incoming_sockets);
						num_client_sockets--;
						if (num_client_sockets == 0) {
							set_state_clean();
						}
					}
				}
			}
		}
	}

    // Stop the worker before we start resetting values.
	worker_stop(worker);

    // Don't try to clean up the named socket.
	FD_CLR(named_socket, &incoming_sockets);

    // Clean up and close all client sockets and thereby
    // reset all attributes to their persistent values.
	for (fd = 0; fd < max_socket; fd++) {
		if (FD_ISSET(fd, &incoming_sockets)) {
			socket_destruct(fd);
			close(fd);
		}
	}

	named_socket_destruct(named_socket);

	TRACE1_EXIT("main() is exiting!!");

	return 0;
}
