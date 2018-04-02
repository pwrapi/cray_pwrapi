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
 *
 * Helper function to use the Node Health Checker network to execute
 * a command on the login node to set the current node admin down.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <nhm.h>

#include <log.h>

#include "powerapid.h"
#include "pwrapi_down.h"

static int
get_my_nid(void)
{
	FILE *fp;
	int my_nid = -1;

	TRACE1_ENTER("");

	fp = fopen("/proc/cray_xt/nid", "r");
	if (fp != NULL) {
		if (fscanf(fp, "%d", &my_nid) != 1) {
			my_nid = -1;
		}

		fclose(fp);
	}

	TRACE1_EXIT("my_nid = %d", my_nid);

	return my_nid;
}

void
set_node_admin_down(void)
{
	int my_nid;
	char nid_opt[32];
	const char *hostnames[1];
	const char *cmd[6];
	int fanin_option = NHM_FANIN_SERIAL;
	int use_pty = 0;
	int fanout_order = 1;
	int connect_timeout = 30;
	int cookie = 0;
	const char *cookie_string = "powerapid";
	unsigned long debug = 0;
	int fd;
	int max_message = nhm_get_max_message();
	int max_error = nhm_get_max_error();

	TRACE1_ENTER("");

	my_nid = get_my_nid();
	if (my_nid < 0)
		goto done;

	snprintf(nid_opt, sizeof(nid_opt), "--nid=%d", my_nid);

	hostnames[0] = "login";

	cmd[0] = "/opt/cray/sdb/default/bin/xtprocadmin";
	cmd[1] = "--quiet";
	cmd[2] = nid_opt;
	cmd[3] = "--key=s";
	cmd[4] = "up:admindown";
	cmd[5] = NULL;

	fd = nhm_remote_exec(hostnames, 1, (char **)cmd, fanin_option, use_pty,
			fanout_order, connect_timeout, cookie, (char *)cookie_string, debug);
	if (fd < 0)
		goto done;

	while (1) {
		int type;
		int status;
		int sender;
		int len;
		char payload[max_message];
		char error[max_error];
		int ret;

		alarm(120);

		ret = nhm_get_reply(fd, &type, &status, &sender, &len, payload, error);

		alarm(0);

		if (ret < 0) {
			LOG_WARN("nhm_get_reply: Returned error: %s", error);
			break;
		}

		if (type == MSGTYPE_COMMAND_COMPLETE) {
			if (status != 0) {
				LOG_WARN("nhm_get_reply: Remote command exited with %d", status);
			}
			break;
		} else if (type == MSGTYPE_STDOUT) {
			LOG_MSG("nhm_get_reply: Remote command stdout: %s", payload);
		} else if (type == MSGTYPE_STDERR) {
			LOG_MSG("nhm_get_reply: Remote command stderr: %s", payload);
		} else if (type == MSGTYPE_ERROR) {
			LOG_WARN("nhm_get_reply: Error status: %s", nhm_error_string(status));
		}
	}

done:
	TRACE1_EXIT("");
}
