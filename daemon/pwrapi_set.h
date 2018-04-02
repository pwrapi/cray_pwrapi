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
 * Declare helper functions for manipulating set data (attribute value changes)
 * in the powerapi daemon.
 */

#ifndef _POWERAPI_SET_H
#define _POWERAPI_SET_H

#include <glib.h>

#include <cray-powerapi/powerapid.h>
#include "pwrapi_socket.h"

typedef struct {
    powerapi_setreq_t  setreq;      // set request itself
    socket_info_t     *skinfo;      // requesting socket
    uint64_t           timestamp;   // time that set was requested
} set_info_t;

void set_insert(set_info_t *setp, GHashTable *hash);
void set_remove(set_info_t *setp, GHashTable *hash);
void set_destroy(set_info_t *setp);
gboolean set_rollback(gpointer key, gpointer value, gpointer user_data);
void set_print(gpointer key, gpointer value, gpointer user_data);
set_info_t *set_create_item(powerapi_setreq_t *setreq, socket_info_t *skinfo);
int attr_value_comp(gconstpointer set1, gconstpointer set2);

#endif // _POWERAPI_SET_H
