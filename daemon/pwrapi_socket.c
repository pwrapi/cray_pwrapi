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
 * Helper functions for maintaining information about open socket
 * connections to the powerapi daemon.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <cray-powerapi/powerapid.h>
#include <log.h>

#include "powerapid.h"
#include "pwrapi_set.h"
#include "pwrapi_socket.h"

gboolean
is_persistent(const socket_info_t *skinfo)
{
    gboolean ret = FALSE;

    TRACE1_ENTER("skinfo = %p", skinfo);

    ret = ((skinfo == NULL) ||
            ((skinfo->cred.uid == 0) && (skinfo->role == PWR_ROLE_RM)));

    TRACE1_EXIT("ret = %d", ret);

    return ret;
}

void
socket_construct(int client_socket, const struct ucred *cred)
{
    socket_info_t *skinfo;

    TRACE1_ENTER("client_socket = %d, cred = %p", client_socket, cred);

    skinfo = g_new0(socket_info_t, 1);
    if (!skinfo) {
        LOG_CRIT(MEM_ERROR_EXIT);
        exit(1);
    }
    skinfo->sockid     = client_socket;
    skinfo->cred       = *cred;
    skinfo->role       = PWR_ROLE_NOT_SPECIFIED;
    skinfo->my_changes = g_hash_table_new_full(g_str_hash,
            g_str_equal, NULL, NULL);
    if (!skinfo->my_changes) {
        LOG_CRIT(MEM_ERROR_EXIT);
        exit(1);
    }
    skinfo->timestamp = time(NULL);

    g_hash_table_insert(open_sockets, &skinfo->sockid, skinfo);

    TRACE1_EXIT("skinfo = %p", skinfo);
}

void
socket_destruct(int client_socket)
{
    socket_info_t *skinfo;

    TRACE1_ENTER("client_socket = %d", client_socket);

    LOG_DBG("Cleaning up client socket %d", client_socket);

    skinfo = g_hash_table_lookup(open_sockets, &client_socket);
    if (skinfo) {
        g_hash_table_remove(open_sockets, &client_socket);

        // roll back non-persistent changes
        g_hash_table_foreach_remove(skinfo->my_changes, set_rollback, NULL);
        g_hash_table_destroy(skinfo->my_changes);
        g_free(skinfo->context_name);
        g_free(skinfo);
    }

    TRACE1_EXIT("skinfo = %p", skinfo);
}

socket_info_t *
socket_lookup(int client_socket)
{
    socket_info_t *skinfo;

    TRACE1_ENTER("client_socket = %d", client_socket);

    skinfo = g_hash_table_lookup(open_sockets, &client_socket);

    TRACE1_EXIT("skinfo = %p", skinfo);

    return skinfo;
}

void
socket_print(gpointer key, gpointer value, gpointer user_data)
{
    socket_info_t *skinfo = (socket_info_t *)value;
    struct tm *tm;
    char tsbuf[32];

    TRACE2_ENTER("key = %p, value = %p, user_data = %p",
            key, value, user_data);

    tm = localtime(&skinfo->timestamp);
    if (!tm || strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dt%H:%M:%S", tm) == 0) {
        if (snprintf(tsbuf, sizeof(tsbuf), "%lu", skinfo->timestamp) < 0) {
            strcpy(tsbuf, "<error>");
        }
    }

    LOG_MSG("Socket %d/%d, uid/gid/pid = %d/%d/%d, role = %d, name = %s, "
            "timestamp = %s", *((int *)key), skinfo->sockid,
            skinfo->cred.uid, skinfo->cred.gid, skinfo->cred.pid,
            skinfo->role, skinfo->context_name, tsbuf);

    TRACE2_EXIT("");
}
