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
 * Helper functions for manipulating set data (attribute value changes)
 * in the powerapi daemon.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <cray-powerapi/powerapid.h>
#include <log.h>

#include "powerapid.h"
#include "pwrapi_set.h"
#include "pwrapi_worker.h"

void
set_insert(set_info_t *setp, GHashTable *hash)
{
    char *path = setp->setreq.path;
    GSList *slist;

    TRACE2_ENTER("setp = %p (path = '%s'), hash = %p", setp, path, hash);

    if (hash) {
        g_hash_table_insert(hash, path, setp);
    }

    slist = g_hash_table_lookup(all_changes, path);
    slist = g_slist_insert_sorted(slist, setp, attr_value_comp);
    g_hash_table_insert(all_changes, path, slist);

    TRACE2_EXIT("");
}

void
set_remove(set_info_t *setp, GHashTable *hash)
{
    char *path = setp->setreq.path;
    GSList *slist;

    TRACE2_ENTER("setp = %p (path = '%s'), hash = %p", setp, path, hash);

    if (hash) {
        g_hash_table_remove(hash, path);
    }

    slist = g_hash_table_lookup(all_changes, path);
    slist = g_slist_remove(slist, setp);
    g_hash_table_insert(all_changes, path, slist);

    TRACE2_EXIT("");
}

void
set_destroy(set_info_t *setp)
{
    TRACE2_ENTER("setp = %p", setp);

    g_free(setp);

    TRACE2_EXIT("");
}

// process set requests in the my_changes list of a socket being closed
// - remove the set request from all_changes and if the set was the
//   highest priority, then roll the change back to the next highest
//   priority (write new value to file system)
gboolean
set_rollback(gpointer key, gpointer value, gpointer user_data)
{
    set_info_t *setp = (set_info_t *)value;
    const socket_info_t *skinfo = setp->skinfo;
    char *path;

    TRACE1_ENTER("key = %p, value = %p, user_data = %p",
            key, value, user_data);

    set_print(NULL, setp, NULL);

    path = setp->setreq.path;

    // remove the set from all_changes
    set_remove(setp, NULL);

    // get the top priority value; if it's lower than the value that
    // was just removed, then write it to the filesystem;
    // attr_value_comp() will return a negative number if setp is
    // higher priority than top (this means that setp was at the head
    // of the list and that top was further down); the list is ordered
    // such that the highest priority item is first, at the head (top)
    GSList *slist = g_hash_table_lookup(all_changes, path);
    set_info_t *top = slist->data;
    if (attr_value_comp(setp, top) < 0) {
        LOG_MSG("Rolling back client socket %d value of %s",
                skinfo ? skinfo->sockid : -1, path);
        write_attr_value(top);
    }

    set_destroy(setp);

    TRACE1_EXIT("");

    // by returning TRUE, we are telling g_hash_table_foreach_remove
    // (the calling function) to remove this entry from my_changes
    return TRUE;
}

void
set_print(gpointer key, gpointer value, gpointer user_data)
{
    const set_info_t *setp = (set_info_t *)value;
    const powerapi_setreq_t *setreq;
    const socket_info_t *skinfo;

    TRACE3_ENTER("key = %p, value = %p, user_data = %p",
            key, value, user_data);

    setreq = &setp->setreq;
    skinfo = setp->skinfo;

    switch (setreq->data_type) {
    case PWR_ATTR_DATA_UINT64:
        // value is a long integer
        LOG_MSG("Set obj = %d, attr = %d, data_type = %d, "
                "value = %ld, path = '%s', sockid = %d",
                setreq->object, setreq->attribute, setreq->data_type,
                setreq->value.ivalue, setreq->path,
                skinfo ? skinfo->sockid : -1);
        break;
    case PWR_ATTR_DATA_DOUBLE:
        // value is a double float
        LOG_MSG("Set obj = %d, attr = %d, data_type = %d, "
                "value = %lf, path = '%s', sockid = %d",
                setreq->object, setreq->attribute, setreq->data_type,
                setreq->value.fvalue, setreq->path,
                skinfo ? skinfo->sockid : -1);
        break;
    default:
        // should never get here...
        LOG_MSG("Set obj = %d, attr = %d, data_type = %d (?), "
                "value = @(%p) (?), path = '%s', sockid = %d",
                setreq->object, setreq->attribute, setreq->data_type,
                &setreq->value, setreq->path,
                skinfo ? skinfo->sockid : -1);
        break;
    }

    TRACE3_EXIT("");
}

set_info_t *
set_create_item(powerapi_setreq_t *setreq, socket_info_t *skinfo)
{
    set_info_t *setp = NULL;

    TRACE2_ENTER("setreq = %p, skinfo = %p", setreq, skinfo);

    setp = g_new0(set_info_t, 1);
    if (!setp) {
        LOG_CRIT(MEM_ERROR_EXIT);
        exit(1);
    }
    setp->setreq = *setreq;
    setp->skinfo = skinfo;

    TRACE2_EXIT("setp = %p", setp);

    return setp;
}

// compare data values
// a negative return value means that v1 is higher priority than v2
static int
value_compare(PWR_AttrDataType data_type,
        const type_union_t *v1, const type_union_t *v2)
{
    int ret = 0;
    uint64_t i1, i2;
    double f1, f2;

    TRACE3_ENTER("data_type = %d, v1 = %p, v2 = %p", data_type, v1, v2);

    switch (data_type) {
    case PWR_ATTR_DATA_UINT64:
        i1 = v1->ivalue;
        i2 = v2->ivalue;
        if (i1 == i2) {
            ret = 0;
        } else {
            ret = (i1 < i2) ? -1 : 1;
        }
        break;
    case PWR_ATTR_DATA_DOUBLE:
        f1 = v1->fvalue;
        f2 = v2->fvalue;
        if (f1 == f2) {
            ret = 0;
        } else {
            ret = (f1 < f2) ? -1 : 1;
        }
        break;
    default:
        // should never get here...
        break;
    }

    TRACE3_EXIT("ret = %d", ret);

    return ret;
}

// compare governor values
// a negative return value means that s1 is higher priority than s2
static int
gov_compare(const set_info_t *s1, const set_info_t *s2)
{
    int ret = 0;
    const powerapi_setreq_t *sr1 = &s1->setreq;
    const powerapi_setreq_t *sr2 = &s2->setreq;
    uint64_t g1 = sr1->value.ivalue;
    uint64_t g2 = sr2->value.ivalue;

    TRACE3_ENTER("s1 = %p, s2 = %p", s1, s2);

    // userspace governor has highest priority, others
    // are based upon when they were requested
    if (g1 == g2) {
        ret = 0;
    } else if (g1 == PWR_GOV_LINUX_USERSPACE) {
        ret = -1;
    } else if (g2 == PWR_GOV_LINUX_USERSPACE) {
        ret = 1;
    } else {
        // newer (larger) timestamp is higher priority
        ret = (s1->timestamp > s2->timestamp) ? -1 : 1;
    }

    TRACE3_EXIT("ret = %d", ret);

    return ret;
}

// use attribute type to sort values
int
attr_value_comp(gconstpointer set1, gconstpointer set2)
{
    int ret = 0;
    const set_info_t *s1 = set1;
    const set_info_t *s2 = set2;
    const powerapi_setreq_t *sr1 = &s1->setreq;
    const powerapi_setreq_t *sr2 = &s2->setreq;
    const type_union_t *v1 = &sr1->value;
    const type_union_t *v2 = &sr2->value;

    TRACE2_ENTER("set1 = %p, set2 = %p", set1, set2);

    // a negative return means that set1 is higher priority than set2
    // the list is ordered such that higher priority items come before
    // (are earlier in the list) than lower priority ones so that the
    // head (top) of the list is always the highest priority item

    switch (sr1->attribute) {
    case PWR_ATTR_CSTATE_LIMIT:
    case PWR_ATTR_FREQ_REQ:
    case PWR_ATTR_FREQ_LIMIT_MAX:
    case PWR_ATTR_POWER_LIMIT_MAX:
        // lower value is higher priority
        ret = value_compare(sr1->data_type, v1, v2);
        break;
    case PWR_ATTR_FREQ_LIMIT_MIN:
    case PWR_ATTR_POWER_LIMIT_MIN:
        // higher value is higher priority
        // reverse the order of the arguments to achieve this
        ret = value_compare(sr1->data_type, v2, v1);
        break;
    case PWR_ATTR_GOV:
        // special priority
        ret = gov_compare(s1, s2);
        break;
    default:
        // should never get here...
        break;
    }

    TRACE2_EXIT("ret = %d", ret);

    return ret;
}
