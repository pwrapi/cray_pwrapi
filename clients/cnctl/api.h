/*
 * Copyright (c) 2015-2016, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.

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
 */

#ifndef __CNCTL_API_H
#define __CNCTL_API_H

#include <glib.h>

#include <cray-powerapi/api.h>

#include "cnctl.h"

#define JSON_VERS_MAJ_STR		"PWR_MajorVersion"
#define JSON_VERS_MIN_STR		"PWR_MinorVersion"

#define JSON_RET_CODE_STR		"PWR_ReturnCode"
#define JSON_MSGS_STR			"PWR_Messages"
#define JSON_ERR_MSGS_STR		"PWR_ErrorMessages"

#define JSON_CMD_STR			"PWR_Function"
#define JSON_CMD_GET_STR		"PWR_ObjAttrGetValues"
#define JSON_CMD_SET_STR		"PWR_ObjAttrSetValues"
#define JSON_CMD_CAP_ATTR_STR		"PWR_ObjAttrCapabilities"
#define JSON_CMD_CAP_ATTR_VAL_STR	"PWR_ObjAttrValueCapabilities"
#define JSON_CMD_UID_ADD		"PWR_UIDAdd"
#define JSON_CMD_UID_REMOVE		"PWR_UIDRemove"
#define JSON_CMD_UID_CLEAR		"PWR_UIDClear"
#define JSON_CMD_UID_RESTORE		"PWR_UIDRestore"
#define JSON_CMD_UID_LIST		"PWR_UIDList"

#define JSON_ATTRS_STR			"PWR_Attrs"
#define JSON_ATTR_NAME_STR		"PWR_AttrName"
#define JSON_ATTR_CAP_STR		"PWR_AttrCapabilities"
#define JSON_ATTR_VAL_STR		"PWR_AttrValue"
#define JSON_ATTR_VAL_CAP_STR		"PWR_AttrValueCapabilities"

#define JSON_UID_STR			"PWR_UID"
#define JSON_UIDS_STR			"PWR_UIDS"

#define JSON_TIME_STR			"PWR_Time"
#define JSON_TIME_SEC_STR		"PWR_TimeSeconds"
#define JSON_TIME_NSEC_STR		"PWR_TimeNanoseconds"

/*
 * rattr_t - Data structure representing a requested attribute.
 */
typedef struct rattr_s {
	PWR_AttrName attr;	// requested attribute
	char *attr_str;		// string version of requested attribute
	char *val_str;		// string version of requested value
	int retcode;		// return code for this attribute
} rattr_t;

/*
 * cattr_t - Data structure representing a cached attribute.
 */
typedef struct cattrs_s {
	const char *name_str;	// attribute name string
	int  name_val;		// PWR_AttrName value
} cattrs_t;

extern size_t   cattrs_count;
extern cattrs_t cattrs[PWR_NUM_ATTR_NAMES];

void api_init(void);
void api_cleanup(void);
void validate_rattrs_strs(GArray *rattrs);
void get_attr_val_cap(rattr_t *rattr);
void get_attr_val(rattr_t *rattr);
void set_attr_val(rattr_t *rattr);
int use_ht_object(void);

#endif /* ifndef __CNCTL_API_H */

