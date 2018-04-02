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
 * This file contains the structures which describe the protocol used to
 * communicate with the powerapi daemon.
 */

#ifndef _POWERAPID_H
#define _POWERAPID_H

#include <limits.h>
#include <cray-powerapi/types.h>

/*
 * Requests to powerapi daemon
 */
typedef union {
    uint64_t ivalue;
    double fvalue;
} type_union_t;

/*
 * powerapi_reqtype_t - All possible request/response message types
 */
typedef enum {
    PwrAUTH = 0,    // authentication request
    PwrSET,         // obj/attr set value request
    PwrLOGLVL,      // set debug/trace level
    PwrDUMP         // dump state request
} powerapi_reqtype_t;

/*
 * Debug level request/response
 */
typedef struct {
    int  dbglvl;         // debug level (-1 to 2)
    int  trclvl;         // trace level (-1 to 3)
} powerapi_loglvlreq_t;

typedef struct {
    int  dbglvl;         // debug level (-1 to 2)
    int  trclvl;         // trace level (-1 to 3)
} powerapi_loglvlresp_t;

/*
 * Auth request/response
 */
typedef struct {
    PWR_Role role;
    char     context_name[PWR_MAX_STRING_LEN + 1];
} powerapi_authreq_t;

/*
 * Set request/response
 */
typedef struct {
    PWR_ObjType      object;            // object type
    PWR_AttrName     attribute;         // attribute type
    PWR_AttrDataType data_type;         // data type
    PWR_MetaName     metadata;          // metadata type
    type_union_t     value;             // new value
    char             path[PATH_MAX];    // control file pathname
} powerapi_setreq_t;

/*
 * powerapi_request_t - High level request structure
 */
typedef struct {
    powerapi_reqtype_t       ReqType;
    union {
        powerapi_authreq_t   auth;
        powerapi_setreq_t    set;
        powerapi_loglvlreq_t loglvl;
    };
} powerapi_request_t;

/*
 * powerapi_response_t - High level response structure
 */
typedef struct {
    int                retval;		// return value to client
    uint64_t           sequence;	// sequence number
    union {
        powerapi_loglvlresp_t loglvl;	// loglvl response message
    };
} powerapi_response_t;

// state directory for powerapi
#define POWERAPI_STATEDIR_PATH "/var/opt/cray/powerapi"

// log directory for powerapi
#define POWERAPI_LOGDIR_PATH POWERAPI_STATEDIR_PATH "/log"

// running directory for powerapi
#define POWERAPI_RUNDIR_PATH POWERAPI_STATEDIR_PATH "/run"

// default log file for daemon
#define POWERAPID_LOGFILE_PATH POWERAPI_LOGDIR_PATH "/powerapid.log"

// pid file for powerapid
#define POWERAPID_PIDFILE_PATH POWERAPI_RUNDIR_PATH "/powerapid.pid"

// named local socket for powerapid
#define POWERAPID_SOCKET_PATH POWERAPI_RUNDIR_PATH "/powerapid.sock"

// working directory for powerapid
#define POWERAPID_WORKDIR_PATH POWERAPI_RUNDIR_PATH "/powerapid"

// if this file exists, the daemon state is dirty
#define POWERAPID_STATE_DIRTY_PATH POWERAPID_WORKDIR_PATH "/dirty"

// if this file exists, a daemon restart is allowed
// it is in /tmp so that it is ephemeral and goes away each boot
#define POWERAPID_ALLOW_RESTART_PATH "/tmp/powerapid-allow-restart"

#endif // _POWERAPID_H
