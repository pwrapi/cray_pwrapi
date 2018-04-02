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
 * This file contains the functions necessary for hierarchy navigation.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#include <cray-powerapi/types.h>
#include <cray-powerapi/powerapid.h>
#include <log.h>

#include "ipc_socket.h"


static int
ipc_socket_req(ipc_t *ipc, powerapi_request_t *req, powerapi_response_t *resp)
{
	ipc_socket_t *ipc_sock = ipc->plugin_data;
	int status = PWR_RET_FAILURE;
	ssize_t bytes = 0;

	TRACE2_ENTER("ipc = %p, req = %p, resp = %p", ipc, req, resp);

	//
	// Send request
	//
	bytes = write(ipc_sock->fd, req, sizeof(*req));
	if (bytes != sizeof(*req)) {
		LOG_FAULT("Failed write to socket: %m");
		goto failure_return;
	}

	//
	// Receive response
	//
	bytes = read(ipc_sock->fd, resp, sizeof(*resp));
	if (bytes != sizeof(*resp)) {
		LOG_FAULT("Failed read from socket: %m");
		goto failure_return;
	}

	status = resp->retval;

failure_return:
	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
ipc_socket_auth(ipc_t *ipc)
{
	powerapi_request_t req = { 0 };
	powerapi_response_t resp = { 0 };
	int status = PWR_RET_FAILURE;

	TRACE2_ENTER("ipc = %p", ipc);

	//
	// Setup authorization request
	//
	req.ReqType = PwrAUTH;
	req.auth.role = ipc->context_role;
	if (g_strlcpy(req.auth.context_name, ipc->context_name,
			sizeof(req.auth.context_name)) >=
			sizeof(req.auth.context_name)) {
		LOG_FAULT("Context name '%s' too long for buffer!",
				ipc->context_name);
		goto failure_return;
	}

	//
	// Send the request to powerapid
	//
	status = ipc_socket_req(ipc, &req, &resp);

failure_return:
	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
ipc_socket_connect(ipc_t *ipc)
{
	ipc_socket_t *ipc_sock = ipc->plugin_data;
	int status = PWR_RET_FAILURE;
	int fd = -1;
	struct sockaddr_un saddr = { 0 };

	TRACE2_ENTER("ipc = %p", ipc);

	// If file descriptor for the socket has been set to a
	// valid value, assume it has already been connected.
	if (ipc_sock->fd >= 0) {
		status = PWR_RET_SUCCESS;
		goto failure_return;
	}

	//
	// Setup socket and connect
	//
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		LOG_FAULT("Failed socket create: %m");
		goto failure_return;
	}

	saddr.sun_family = AF_UNIX;
	if (g_strlcpy(saddr.sun_path, POWERAPID_SOCKET_PATH,
			sizeof(saddr.sun_path)) >= sizeof(saddr.sun_path)) {
		LOG_FAULT("Named socket path '%s' too long for buffer!",
				POWERAPID_SOCKET_PATH);
		goto failure_return;
	}

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		LOG_FAULT("Failed socket connect: %m");
		goto failure_return;
	}

	ipc_sock->fd = fd;

	//
	// Send authentication request
	//
	status = ipc_socket_auth(ipc);

failure_return:
	if (status != PWR_RET_SUCCESS) {
		if (fd >= 0) {
			close(fd);
			fd = -1;
		}
	}

	TRACE2_EXIT("status = %d, fd = %d", status, fd);

	return status;
}

static int
ipc_socket_set(ipc_t *ipc, PWR_ObjType obj_type, PWR_AttrName attr_name,
		PWR_MetaName meta_name, PWR_AttrDataType attr_type,
		const void *value, const char *path)
{
	int status = PWR_RET_FAILURE;
	powerapi_request_t req = { 0 };
	powerapi_response_t resp = { 0 };

	TRACE2_ENTER("ipc = %p, obj_type = %d, attr_name = %d, attr_type = %d, "
			"value = %p, path = '%s'",
			ipc, obj_type, attr_name, attr_type, value, path);

	status = ipc_socket_connect(ipc);
	if (status != PWR_RET_SUCCESS) {
		goto failure_return;
	}

	//
	// Setup set request
	//
	req.ReqType = PwrSET;
	req.set.object = obj_type;
	req.set.attribute = attr_name;
	req.set.metadata = meta_name;
	req.set.data_type = attr_type;

	//
	// Save the value in the correct type
	//
	switch (attr_type) {
	case PWR_ATTR_DATA_DOUBLE:
		req.set.value.fvalue = *((double *)value);
		break;
	case PWR_ATTR_DATA_UINT64:
		req.set.value.ivalue = *((uint64_t *)value);
		break;
	default:
		status = PWR_RET_INVALID;
		goto failure_return;
	}

	//
	// Copy over path in sysfs for setting the value
	//
	if (g_strlcpy(req.set.path, path,
			sizeof(req.set.path)) >= sizeof(req.set.path)) {
		LOG_FAULT("Path '%s' too long for buffer!", path);
		status = PWR_RET_FAILURE;
		goto failure_return;
	}

	//
	// Send the request to powerapid
	//
	status = ipc_socket_req(ipc, &req, &resp);

failure_return:
	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
ipc_socket_set_uint64(ipc_t *ipc, PWR_ObjType obj_type, PWR_AttrName attr_name,
		PWR_MetaName meta_name, const uint64_t *value, const char *path)
{
	int status = PWR_RET_FAILURE;

	TRACE2_ENTER("ipc = %p, obj_type = %d, attr_name = %d, "
			"value = %p, path = '%s'",
			ipc, obj_type, attr_name, value, path);

	status = ipc_socket_set(ipc, obj_type, attr_name, meta_name,
			PWR_ATTR_DATA_UINT64, value, path);

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
ipc_socket_set_double(ipc_t *ipc, PWR_ObjType obj_type, PWR_AttrName attr_name,
		PWR_MetaName meta_name, const double *value, const char *path)
{
	int status = PWR_RET_FAILURE;

	TRACE2_ENTER("ipc = %p, obj_type = %d, attr_name = %d, "
			"value = %p, path = '%s'",
			ipc, obj_type, attr_name, value, path);

	status = ipc_socket_set(ipc, obj_type, attr_name, meta_name,
			PWR_ATTR_DATA_DOUBLE, value, path);

	TRACE2_EXIT("status = %d", status);

	return status;
}

static int
ipc_socket_destruct(ipc_t *ipc)
{
	int status = PWR_RET_FAILURE;
	ipc_socket_t *ipc_sock = NULL;

	TRACE2_ENTER("ipc = %p", ipc);

	if (!ipc || !ipc->plugin_data) {
		goto failure_return;
	}

	ipc_sock = (ipc_socket_t *)ipc->plugin_data;
	ipc->plugin_data = NULL;

	ipc->ops = NULL;

	// If the socket was connected, disconnect it and
	// deallocate the memory.
	if (ipc_sock->fd >= 0)
		close(ipc_sock->fd);

	g_free(ipc_sock);

	status = PWR_RET_SUCCESS;

failure_return:
	TRACE2_EXIT("status = %d", status);

	return status;
}


const struct ipc_ops ipc_socket_ops = {
	.destruct = ipc_socket_destruct,
	.set_uint64 = ipc_socket_set_uint64,
	.set_double = ipc_socket_set_double
};


int
ipc_socket_construct(ipc_t *ipc)
{
	int status = PWR_RET_FAILURE;
	ipc_socket_t *ipc_sock = NULL;

	TRACE2_ENTER("ipc = %p", ipc);

	if (!ipc) {
		goto failure_return;
	}

	//
	// Allocate the plugin data
	//
	ipc_sock = g_new0(ipc_socket_t, 1);
	if (!ipc_sock) {
		LOG_FAULT("Failed to allocate ipc_socket_t");
		goto failure_return;
	}

	ipc->plugin_data = (void *)ipc_sock;

	ipc->ops = &ipc_socket_ops;

	// Initialize file desc. to illegal value, to indicate the
	// lazy socket connection must be done.
	ipc_sock->fd = -1;

	status = PWR_RET_SUCCESS;

failure_return:
	if (status != PWR_RET_SUCCESS) {
		g_free(ipc_sock);
		ipc_sock = NULL;
	}

	TRACE2_EXIT("status = %d, ipc_sock = %p", status, ipc_sock);

	return status;
}
