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

static int named_socket;    // socket to communicate with daemon

//
// send_req - Send a request packet to the daemon
//
// Argument(s):
//
//      req - The request packet
//
// Return Code(s):
//
//      void
//
void
send_req(powerapi_request_t *req)
{
    ssize_t num_written = 0;

    TRACE1_ENTER("req = %p", req);

    // Send request.  This will generate two return packets
    num_written = write(named_socket, req, sizeof(*req));
    if (num_written != sizeof(*req)) {
        LOG_CRIT("Wrote %zd bytes, attempted to write %zu bytes!",
                num_written, sizeof(*req));
        exit(1);
    }

    TRACE1_EXIT("");
}

//
// get_resp - Get a response packet for our request from the daemon
//
// Argument(s):
//
//      resp - Pointer to response packet structure to be filled in
//
// Return Code(s):
//
//      void
//
void
get_resp(powerapi_response_t *resp)
{
    ssize_t num_read = 0;

    TRACE1_ENTER("resp = %p", resp);

    num_read = read(named_socket, resp, sizeof(*resp));
    if (num_read != sizeof(*resp)) {
        LOG_CRIT("Read %zd bytes, attempted to read %zu bytes!",
                num_read, sizeof(*resp));
        exit(1);
    }

    TRACE1_EXIT("");
}

//
// daemon_connect - Connect to daemon
//
// Argument(s):
//
//      void
//
// Return Code(s):
//
//      void
//
void
daemon_connect(void)
{
    struct sockaddr_un saddr = { 0 };
    int result;

    TRACE1_ENTER("");

    LOG_DBG("Creating socket...");
    named_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    LOG_DBG("Socket num = %d", named_socket);

    saddr.sun_family = AF_UNIX;
    if (g_strlcpy(saddr.sun_path, POWERAPID_SOCKET_PATH,
            sizeof(saddr.sun_path)) >= sizeof(saddr.sun_path)) {
        LOG_CRIT("Named socket path '%s' too long for buffer!",
                POWERAPID_SOCKET_PATH);
        exit(1);
    }

    LOG_DBG("Connecting to daemon...");
    result = connect(named_socket, (struct sockaddr *)&saddr, sizeof(saddr));
    if (result != 0) {
        LOG_CRIT("Connect to daemon failed");
        exit(1);
    }

    TRACE1_EXIT("named_socket = %d", named_socket);
}

//
// daemon_disconnect - Disconnect from daemon
//
// Argument(s):
//
//      void
//
// Return Code(s):
//
//      void
//
void
daemon_disconnect(void)
{
    TRACE1_ENTER("named_socket = %d", named_socket);

    close(named_socket);

    TRACE1_EXIT("");
}

