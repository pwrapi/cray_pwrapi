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
 * This file contains the functions necessary for interprocess communication.
 */

#include <glib.h>

#include <log.h>

#include "ipc.h"
#include "plugins/ipc_socket/ipc_socket.h"

//----------------------------------------------------------------------//
// 			IPC INTERFACES					//
//----------------------------------------------------------------------//

ipc_t *
new_ipc(ipc_type_t type, const char *context_name, PWR_Role context_role)
{
	int status = PWR_RET_SUCCESS;
	ipc_t *ipc = NULL;

	TRACE2_ENTER("type = %d, context_name = '%s', context_role = %d",
			type, context_name, context_role);

	ipc = g_new0(ipc_t, 1);
	if (!ipc) {
		status = PWR_RET_FAILURE;
		goto failure_return;
	}
	ipc->type = type;
	ipc->context_name = g_strdup(context_name);
	ipc->context_role = context_role;

	switch (type) {
	case IPC_SOCKET:
		status = ipc_socket_construct(ipc);
		break;
	default:
		status = PWR_RET_FAILURE;
		break;
	}

failure_return:
	if (status != PWR_RET_SUCCESS) {
		del_ipc(ipc);
		ipc = NULL;
	}

	TRACE2_EXIT("ipc = %p", ipc);

	return ipc;
}

void
del_ipc(ipc_t *ipc)
{
	TRACE2_ENTER("ipc = %p", ipc);

	if (ipc) {
		if (ipc->ops && ipc->ops->destruct) {
			ipc->ops->destruct(ipc);
		}

		g_free(ipc->context_name);
		g_free(ipc);
	}

	TRACE2_EXIT("");
}
