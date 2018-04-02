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
 * Helper functions to read and set values in sysfs control files for
 * the powerapi daemon.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <log.h>

#include "powerapid.h"
#include "pwrapi_signal.h"

static void
stop_daemon(int signal_num)
{
    TRACE1_ENTER("signal_num = %d", signal_num);

    daemon_run = 0;

    TRACE1_EXIT("");
}

static void
handle_alarm(int signal_num)
{
    TRACE1_ENTER("signal_num = %d", signal_num);

    // Nothing to do really. We just want to interrupt any in-progress system call.

    TRACE1_EXIT("");
}

void
pwrapi_handle_signals(void)
{
    struct sigaction sa;

    TRACE1_ENTER("");

    memset(&sa, 0, sizeof(sa));

    sigemptyset(&sa.sa_mask);

    sa.sa_handler = stop_daemon;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_CRIT("Unable to set signal handler for SIGINT: %m");
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_CRIT("Unable to set signal handler for SIGTERM: %m");
        exit(1);
    }

    sa.sa_handler = handle_alarm;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        LOG_CRIT("Unable to set signal handler for SIGALRM: %m");
        exit(1);
    }

    // Set SIGPIPE to ignore; writes will return EPIPE when writing
    // to a closed socket.
    sa.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        LOG_CRIT("Unable to set signal handler for SIGPIPE: %m");
        exit(1);
    }

    TRACE1_EXIT("");
}
