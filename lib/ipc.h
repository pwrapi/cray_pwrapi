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
 * This file contains the structure definitions and prototypes for internal
 * use of power objects and object hierarchies.
 */

#ifndef _PWR_IPC_H
#define _PWR_IPC_H

#include <stdint.h>

#include <cray-powerapi/types.h>

#include "typedefs.h"

//----------------------------------------------------------------------//
// IPC: types and prototypes						//
//----------------------------------------------------------------------//

struct ipc_ops {
	int (*destruct) (ipc_t *ipc);
	int (*set_uint64) (ipc_t *ipc, PWR_ObjType obj_type,
			PWR_AttrName attr_name, PWR_MetaName meta_name,
			const uint64_t *value, const char *path);
	int (*set_double) (ipc_t *ipc, PWR_ObjType obj_type,
			PWR_AttrName attr_name, PWR_MetaName meta_name,
			const double *value, const char *path);
};


typedef enum {
	IPC_INVALID = -1,
	IPC_SOCKET = 0,
	IPC_SHMEM,		// Not implemented
	IPC_MAX
} ipc_type_t;


// See typedefs.h for:
// typedef struct ipc_s ipc_t;
struct ipc_s {
	ipc_type_t	 type;
	char		 *context_name;
	PWR_Role	 context_role;

	void		 *plugin_data;

	const struct ipc_ops *ops;
};


ipc_t *new_ipc(ipc_type_t type, const char *context_name, PWR_Role context_role);
void del_ipc(ipc_t *ipc);

#endif // _PWR_IPC_H
