/*
 * Copyright (c) 2015-2017, Cray Inc.
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
 * This file contains functions for initializing and cleaning up contexts.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "hints/hint.h"
#include "utility.h"
#include "hierarchy.h"
#include "context.h"

// Maps the name of a context to the address of the context structure.
static GHashTable *context_name_map = NULL;

// Create a new group under management of the context.
group_t *
context_new_group(context_t *context)
{
	group_t *group = NULL;

	TRACE2_ENTER("context = %p", context);

	// Create the group
	group = new_group();
	if (!group) {
		goto error_handling;
	}

	context->group_list = g_list_prepend(context->group_list,
							group);
	group->link = context->group_list;
	group->context_key = context->opaque.key;

error_handling:
	TRACE2_EXIT("group = %p", group);

	return group;
}

// Delete a group under management of the context.
void
context_del_group(context_t *context, group_t *group)
{
	TRACE2_ENTER("context = %p, group = %p", context, group);

	if (group->link)
		context->group_list =
			g_list_delete_link(context->group_list,
					   group->link);
	del_group(group);

	TRACE2_EXIT("");
}

status_t *
context_new_status(context_t *context)
{
	status_t *stat = NULL;

	TRACE2_ENTER("context = %p", context);

	stat = new_status();
	if (!stat) {
		goto error_handling;
	}

	// Link status to the context
	context->status_list = g_list_prepend(context->status_list, stat);
	stat->link = context->status_list;
	stat->context_key = context->opaque.key;

error_handling:
	TRACE2_EXIT("stat = %p", stat);
	return stat;
}

void
context_del_status(context_t *context, status_t *stat)
{
	TRACE2_ENTER("context = %p, stat = %p", context, stat);

	if (stat->link)
		context->status_list =
			g_list_delete_link(context->status_list,
					   stat->link);
	del_status(stat);

	TRACE2_EXIT("");
}

stat_t *
context_new_statistic(context_t *context)
{
	stat_t *stat = NULL;

	TRACE2_ENTER("context = %p", context);

	stat = new_stat();
	if (!stat) {
		goto error_handling;
	}

	// Link statistic to the context
	context->stat_list = g_list_prepend(context->stat_list, stat);
	stat->ctx_link = context->stat_list;
	stat->context_key = context->opaque.key;

error_handling:
	TRACE2_EXIT("stat = %p", stat);
	return stat;
}

void
context_del_statistic(context_t *context, stat_t *stat)
{
	TRACE2_ENTER("context = %p, stat = %p", context, stat);

	if (stat->ctx_link)
		context->stat_list =
			g_list_delete_link(context->stat_list,
					   stat->ctx_link);
	del_stat(stat);

	TRACE2_EXIT("");
}

static void
del_context(context_t *context)
{
	TRACE2_ENTER("context = %p", context);

	if (context) {
		// Nesting termination of the AppHint logging application
		app_hint_term();

		if (context->opaque.key != 0)
			opaque_map_remove(opaque_map, context->opaque.key);
		if (context->name)
			g_hash_table_remove(context_name_map, context->name);
		del_ipc(context->ipc);
		/*
		 * NOTE: deleting the hierarchy will delete all of the objects,
		 * which will delete all of the hints. context->hintnames should
		 * exist afterwards, but it will be empty.
		 */
		del_hierarchy(context->hierarchy);
		if (context->group_list)
			g_list_free_full(context->group_list,
					group_destroy_callback);
		if (context->status_list)
			g_list_free_full(context->status_list,
					 status_destroy_callback);
		if (context->stat_list) {
			g_list_free_full(context->stat_list,
					 stat_destroy_callback);
		}
		if (context->hintnames) {
			g_sequence_free(context->hintnames);
		}
		g_free(context->name);
		g_free(context);
	}

	TRACE2_EXIT("");
}


static context_t *
new_context(PWR_CntxtType type, PWR_Role role, const char *name)
{
	int error = 0;
	context_t *context = NULL;

	TRACE2_ENTER("type = %d, role = %d, name = '%s'", type, role, name);

	// Allocate memory, with all data set to zero
	context = g_new0(context_t, 1);
	if (!context) {
		error = 1;
		goto error_handling;
	}

	context->type = type;
	context->role = role;
	context->name = g_strdup(name);
	if (!context->name) {
		error = 1;
		goto error_handling;
	}

	// Initialize storage lists
	context->group_list  = NULL;
	context->status_list = NULL;
	context->stat_list   = NULL;
	context->hintnames   = g_sequence_new(NULL);
	if (!context->hintnames) {
		error = 1;
		goto error_handling;
	}
	context->hintunique  = 0;

	// Create the hierarchy of power objects
	context->hierarchy = new_hierarchy();
	if (!context->hierarchy) {
		error = 1;
		goto error_handling;
	}
	context->entry_point = context->hierarchy->tree;

	// Setup the mechanism for IPC to powerapid
	context->ipc = new_ipc(IPC_SOCKET, context->name, context->role);
	if (!context->ipc) {
		error = 1;
		goto error_handling;
	}

	// Add context to the context name map
	g_hash_table_insert(context_name_map, context->name, context);

	// Add context to the opaque map
	if (opaque_map_insert(opaque_map, OPAQUE_CONTEXT,
				&context->opaque) == 0) {
		error = 1;
		goto error_handling;
	}

	// Nesting initialization of the AppHint logging process
	app_hint_init();

error_handling:
	if (error) {
		del_context(context);
		context = NULL;
	}

	TRACE2_EXIT("context = %p", context);

	return context;
}


/*
 * PWR_CntxtInit - The PWR_CntxtInit function is used to initialize a new
 *		   context prior to using any function listed in the Power API
 *		   specification.
 *
 * Argument(s):
 *
 *	type	- The requested context type.
 *	role	- The role of the user.
 *	name	- User specified string name to be associated with the context.
 *	context	- The user's context.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS	- Upon SUCCESS, context is set to a valid user context.
 *	PWR_RET_FAILURE	- Upon FAILURE.
 */
int
PWR_CntxtInit(PWR_CntxtType type, PWR_Role role, const char *name,
	      PWR_Cntxt *context)
{
	int status = PWR_RET_SUCCESS;
	context_t *ctx_ptr = NULL;

	TRACE1_ENTER("type = %d, role = %d, name = '%s', context = %p",
			type, role, name, context);

	// If global data structures haven't been initialized, do it now.
	if (!global_init()) {
		LOG_FAULT("Failed to create required data");
		status = PWR_RET_FAILURE;
		goto failure_return;
	}

	// If the file scope context name map hasn't been initialized,
	// do it now.
	if (!context_name_map) {
		context_name_map = g_hash_table_new(g_str_hash, g_str_equal);
		if (!context_name_map) {
			LOG_FAULT("Failed to create context name map");
			status = PWR_RET_FAILURE;
			goto failure_return;
		}
	}

	// Only PWR_CNTXT_DEFAULT implemented at this time
	if (type != PWR_CNTXT_DEFAULT) {
		LOG_FAULT("Unsupported type %d", type);
		status = PWR_RET_NOT_IMPLEMENTED;
		goto failure_return;
	}

	// Currently, only contexts for the PWR_ROLE_RM and PWR_ROLE_APP roles
	// are created.
	if (role != PWR_ROLE_RM && role != PWR_ROLE_APP) {
		LOG_FAULT("Unsupported role %d", role);
		status = PWR_RET_NOT_IMPLEMENTED;
		goto failure_return;
	}

	ctx_ptr = new_context(type, role, name);
	if (!ctx_ptr) {
		LOG_FAULT("Unable to allocate context '%s'", name);
		status = PWR_RET_FAILURE;
		goto failure_return;
	}

failure_return:
	// If returning with failure, cleanup allocated memory, except
	// for the opaque_map and context_name_map globals.
	if (status != PWR_RET_SUCCESS) {
		del_context(ctx_ptr);
	} else {
		*context = OPAQUE_GENERATE(ctx_ptr->opaque.key,
					   ctx_ptr->opaque.key);
	}

	TRACE1_EXIT("status = %d, *context = %p", status, *context);

	return status;
}

/*
 * PWR_CntxtDestroy - Destroys (cleans up) the context obtained with
 *		      PWR_CntxtInit().
 *
 * Argument(s):
 *
 *	context - The context obtained using PWR_CntxtInit() that the user
 *		  wishes to destroy.
 *
 * Return Code(s):
 *
 *	PWR_RET_SUCCESS - Upon SUCCESS
 *	PWR_RET_FAILURE - Upon FAILURE
 */
int
PWR_CntxtDestroy(PWR_Cntxt context)
{
	int status = PWR_RET_SUCCESS;
	context_t *ctx_ptr = NULL;
	gpointer context_key = OPAQUE_GET_CONTEXT_KEY(context);
	gpointer data_key  = OPAQUE_GET_DATA_KEY(context);

	TRACE1_ENTER("context = %p", context);

	// Verify this is a context by ensuring the context key
	// matches the data key. This is only true for opaque
	// references for contexts.
	if (context_key != data_key) {
		status = PWR_RET_FAILURE;
		goto failure_return;
	}

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	ctx_ptr = opaque_map_lookup_context(opaque_map, data_key);

	// Invalid value found for the key: either no value,
	// mismatched type, or unsupported type.
	if (!ctx_ptr) {
		status = PWR_RET_FAILURE;
		goto failure_return;
	}

	del_context(ctx_ptr);

failure_return:
	TRACE1_EXIT("status = %d", status);

	return status;
}

/*
 * PWR_CntxtGetEntryPoint - Returns the user's initial entry location in
 *                          the system description.
 *
 * Argument(s):
 *
 *      context     - The user's context
 *      entry_point - The user's entry point into the system description
 *                    (the same for the life of the context).
 *
 * Return Code(s):
 *
 *      PWR_RET_SUCCESS - Upon SUCCESS, entry set to system description
 *                        entry point (object).
 *      PWR_RET_FAILURE - Upon FAILURE.
 */
int
PWR_CntxtGetEntryPoint(PWR_Cntxt context, PWR_Obj *entry_point)
{
	int status = PWR_RET_SUCCESS;
	context_t    *ctx_ptr = NULL;
	obj_t        *obj = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(context);
	opaque_key_t data_key  = OPAQUE_GET_DATA_KEY(context);

	TRACE1_ENTER("context = %p, entry_point = %p", context, entry_point);

	// Verify this is a context by ensuring the context key
	// matches the data key. This is only true for opaque
	// references for contexts.
	if (context_key != data_key) {
		LOG_FAULT("Opaque reference is not valid for a context");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	ctx_ptr = opaque_map_lookup_context(opaque_map, data_key);

	// Invalid value found for the key: either no value,
	// mismatched type, or unsupported type.
	if (!ctx_ptr) {
		LOG_FAULT("Failed to find context key = %p", data_key);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Get the entry point object into the hierarchy for the
	// context, and provide the opaque key reference for the
	// object.

	// Check entry point for a value
	if (!ctx_ptr->entry_point) {
		LOG_FAULT("Context '%s' entry point not set", ctx_ptr->name);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Get the opaque key for the entry point.
	obj = to_obj(ctx_ptr->entry_point->data);
	if (!obj) {
		LOG_FAULT("Entry point has invalid address");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	*entry_point = OPAQUE_GENERATE(ctx_ptr->opaque.key, obj->opaque.key);

status_return:
	TRACE1_EXIT("status = %d, *entry_point = %p", status, *entry_point);

	return status;
}

int
PWR_CntxtGetObjByName(PWR_Cntxt context, const char *name, PWR_Obj *object)
{
	int status = PWR_RET_SUCCESS;
	context_t    *ctx_ptr = NULL;
	obj_t        *obj = NULL;
	opaque_key_t context_key = OPAQUE_GET_CONTEXT_KEY(context);
	opaque_key_t data_key  = OPAQUE_GET_DATA_KEY(context);

	TRACE1_ENTER("context = %p, name = '%s', object = %p",
			context, name, object);

	// Verify this is a context by ensuring the context key
	// matches the data key. This is only true for opaque
	// references for contexts.
	if (context_key != data_key) {
		LOG_FAULT("Opaque reference is not valid for a context");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Lookup opaque key, validate any value found for the
	// type, and convert to the correct type.
	ctx_ptr = opaque_map_lookup_context(opaque_map, data_key);

	// Invalid value found for the key: either no value,
	// mismatched type, or unsupported type.
	if (!ctx_ptr) {
		LOG_FAULT("Failed to find context key = %p", data_key);
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Search the context name map for the object name
	obj = g_hash_table_lookup(ctx_ptr->hierarchy->map, name);
	if (!obj) {
		LOG_WARN("Failed to find object %s", name);
		status = PWR_RET_WARN_NO_OBJ_BY_NAME;
		goto status_return;
	}

	*object = OPAQUE_GENERATE(ctx_ptr->opaque.key, obj->opaque.key);

status_return:
	TRACE1_EXIT("status = %d, *object = %p", status, *object);

	return status;
}

