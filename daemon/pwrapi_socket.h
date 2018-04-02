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
 * Declare functions for manipulating connected socket data in the powerapi
 * daemon.
 */

#ifndef _POWERAPI_SOCKET_H
#define _POWERAPI_SOCKET_H

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>

#include <glib.h>

#include <cray-powerapi/powerapid.h>

typedef struct {
    int         sockid;            // file descriptor # for this socket
    struct ucred cred;             // credentials of requesting user
    PWR_Role    role;              // role of remote context
    char       *context_name;      // name of remote context
    GHashTable *my_changes;        // all of this socket's changes
    time_t      timestamp;         // time of original connection
    uint64_t    seqnum;            // reply sequence number
} socket_info_t;

void socket_construct(int client_socket, const struct ucred *cred);
void socket_destruct(int client_socket);
socket_info_t *socket_lookup(int client_socket);
void socket_print(gpointer key, gpointer value, gpointer user_data);
gboolean is_persistent(const socket_info_t *skinfo);

#endif // _POWERAPI_SOCKET_H
