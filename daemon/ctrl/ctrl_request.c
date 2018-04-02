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

#include "ctrl.h"
#include "ctrl_request.h"
#include "ctrl_socket.h"

//
// do_loglvl_request - Process a log level request
//
// Argument(s):
//
//      Dlevel - debug level
//      Tlevel - trace level
//      Sflag - show current values
//
// Return Code(s):
//
//      void
//
void
do_loglvl_request(int Dlevel, int Tlevel, int Sflag)
{
    powerapi_request_t req;
    powerapi_response_t resp;

    TRACE1_ENTER("Dlevel = %d, Tlevel = %d, Sflag = %d", Dlevel, Tlevel, Sflag);

    //
    // Send the request
    //
    req.ReqType = PwrLOGLVL;
    req.loglvl.dbglvl = Dlevel;
    req.loglvl.trclvl = Tlevel;

    send_req(&req);

    //
    // Get the response
    //
    get_resp(&resp);

    if (resp.retval == PWR_RET_OP_NO_PERM) {
        LOG_CRIT("No permission");
        exit(1);
    } else if (resp.retval != PWR_RET_SUCCESS) {
        LOG_CRIT("Error code from server = %d", resp.retval);
        exit(1);
    }

    if (Sflag) {
        printf("Powerapid debug = %d, trace = %d\n", resp.loglvl.dbglvl, resp.loglvl.trclvl);
    }

    TRACE1_EXIT("dbglvl = %d, trclvl = %d", resp.loglvl.dbglvl, resp.loglvl.trclvl);
}

//
// do_dump_request - Process a debug dump request
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
do_dump_request(void)
{
    powerapi_request_t req;
    powerapi_response_t resp;

    TRACE1_ENTER("");

    //
    // Send the request
    //
    req.ReqType = PwrDUMP;

    send_req(&req);

    //
    // Get the response
    //
    get_resp(&resp);

    if (resp.retval == PWR_RET_OP_NO_PERM) {
        LOG_CRIT("No permission");
        exit(1);
    } else if (resp.retval != PWR_RET_SUCCESS) {
        LOG_CRIT("Error code from server = %d", resp.retval);
        exit(1);
    }

    TRACE1_EXIT("");
}
