/*
 * Copyright (c) 2015-2016, 2018, Cray Inc.
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
 * This file contains functions for group membership manipulation.
 */

#include <stdio.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "group.h"
#include "context.h"
#include "utility.h"

// This is a function used by glib, so it uses glib basic types.
static gint
group_compare_obj(gconstpointer obj1, gconstpointer obj2, gpointer data)
{
	gint result = 0;        // remains 0 if obj1 == obj2

	if (obj1 < obj2)
		result = -1;
	else if (obj1 > obj2)
		result = 1;

	return result;
}

// This is a function used by glib, so it uses glib basic types.
static gint
group_compare_obj_iter(gconstpointer data1, gconstpointer data2, gpointer data)
{
	gpointer *obj1 = NULL, obj2 = NULL;

	if (data1)
		obj1 = g_sequence_get((GSequenceIter *)data1);
	if (data2)
		obj2 = g_sequence_get((GSequenceIter *)data2);

	return group_compare_obj(obj1, obj2, data);
}

group_t *
new_group(void)
{
	guint error_state = 0;
	group_t *group = NULL;

	TRACE2_ENTER("");

	// A new group container would be nice
	group = g_new0(group_t, 1);
	if (!group) {
		++error_state;
		goto error_handling;
	}

	// And a list to hold the objects in the group
	group->list = g_sequence_new(NULL);
	if (!group->list) {
		++error_state;
		goto error_handling;
	}

	// Since groups get returned to library users, it needs to
	// go into the opaque_map so it has an opaque key.
	if (!opaque_map_insert(opaque_map, OPAQUE_GROUP, &group->opaque)) {
		++error_state;
		goto error_handling;
	}

error_handling:
	// If any errors were encountered, cleanup
	if (error_state) {
		del_group(group);
		group = NULL;
	}

	TRACE2_EXIT("group = %p", group);

	return group;
}

static
void group_invalidate_statistics(group_t *group)
{
	if (group->stat_list) {
		// Invalidate the list of statistics associated with this group
		g_list_free_full(group->stat_list, stat_invalidate_callback);
	}
}

void
del_group(group_t *group)
{
	TRACE3_ENTER("group = %p", group);

	if (!group) {
		TRACE2_EXIT("");
		return;
	}

	// We need to invalidate the statistics before the group's opaque key,
	// because the statistics' monitoring threads use the group's opaque
	// key.
	group_invalidate_statistics(group);

	// If the group has an opaque key, remove it from the opaque_map.
	if (group->opaque.key != 0)
		opaque_map_remove(opaque_map, group->opaque.key);

	// Delete the list of objects in the group. This does not
	// delete any of the objects as the group does not own the
	// objects.
	if (group->list)
		g_sequence_free(group->list);

	// Delete the group container
	g_free(group);

	TRACE3_EXIT("");
}

void
group_destroy_callback(gpointer data)
{
	group_t *group = (group_t *)data;

	TRACE3_ENTER("data = %p", data);

	del_group(group);

	TRACE3_EXIT("");
}

static int
group_copy(group_t *from, group_t *to)
{
	int retval = -1;
	obj_t *obj = NULL;
	GSequenceIter *iter = NULL;

	TRACE3_ENTER("from = %p, to = %p", from, to);

	if (!from || !to)
		goto done;

	iter = g_sequence_get_begin_iter(from->list);
	while (!g_sequence_iter_is_end(iter)) {
		obj = g_sequence_get(iter);
		g_sequence_append(to->list, obj);
		iter = g_sequence_iter_next(iter);
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

bool
group_insert_obj(group_t *group, obj_t *obj)
{
	bool retval = false;
	GSequenceIter *iter = NULL;

	TRACE3_ENTER("group = %p, obj = %p", group, obj);

	if (!group || !obj)
		goto done;

	// Try to find object in group list. If the object is already
	// present, that is not an error.
	iter = g_sequence_lookup(group->list, obj, group_compare_obj, NULL);
	if (iter != NULL) {
		retval = true;
		goto done;
	}

	// We need to invalidate the statistics associated with this group
	// because the size of the group is increasing.
	group_invalidate_statistics(group);

	iter = g_sequence_insert_sorted(group->list, obj, group_compare_obj,
			NULL);
	if (iter == NULL)
		goto done;

	retval = true;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

void
group_insert_callback(GNode *gnode, gpointer data)
{
	group_t  *group = (group_t *)data;
	obj_t    *obj = (obj_t *)gnode->data;

	TRACE3_ENTER("gnode = %p, data = %p", gnode, data);

	if (!group_insert_obj(group, obj)) {
		LOG_FAULT("Failed to insert object %s into group %p",
				obj->name, group);
	}

	TRACE3_EXIT("");
}

bool
group_remove_obj(group_t *group, obj_t *obj)
{
	bool retval = false;
	GSequenceIter *iter = NULL;

	TRACE3_ENTER("group = %p, obj = %p", group, obj);

	if (!group || !obj)
		goto done;

	// Try to find object in group list. If the object is not
	// present, that is not an error.
	iter = g_sequence_lookup(group->list, obj, group_compare_obj, NULL);
	if (iter == NULL) {
		retval = true;
		goto done;
	}

	// We need to invalidate the statistics associated with this group
	// because the size of the group is decreasing.
	group_invalidate_statistics(group);

	g_sequence_remove(iter);

	retval = true;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

static int
group_get_length(group_t *group)
{
	int length = -1;

	TRACE3_ENTER("group = %p", group);

	if (!group)
		goto done;

	length = g_sequence_get_length(group->list);

done:
	TRACE3_EXIT("length = %d", length);

	return length;
}

stat_t *
group_new_statistic(group_t *group)
{
	context_t *ctx = NULL;
	stat_t *stat = NULL;

	TRACE2_ENTER("group = %p", group);

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, group->context_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	stat = context_new_statistic(ctx);
	if (!stat) {
		goto error_handling;
	}

	// Link statistic to the group
	group->stat_list = g_list_prepend(group->stat_list, stat);
	stat->link = group->stat_list;

error_handling:
	TRACE2_EXIT("stat = %p", stat);
	return stat;
}

void
group_del_statistic(group_t *group, stat_t *stat)
{
	context_t *ctx = NULL;

	TRACE2_ENTER("group = %p, stat = %p", group, stat);

	if (stat->link)
		group->stat_list =
				g_list_delete_link(group->stat_list, stat->link);

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, group->context_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		return;
	}

	context_del_statistic(ctx, stat);

	TRACE2_EXIT("");
}

static int
group_union(group_t *group1, group_t *group2, group_t *union_grp)
{
	int retval = -1;
	GSequenceIter *iter1 = NULL, *iter2 = NULL;
	obj_t *obj1 = NULL, *obj2 = NULL;

	TRACE3_ENTER("group1 = %p, group2 = %p, union_grp = %p",
			group1, group2, union_grp);

	if (!group1 || !group2 || !union_grp)
		goto done;

	// Begin at the beginning of each ordered sequence.
	iter1 = g_sequence_get_begin_iter(group1->list);
	iter2 = g_sequence_get_begin_iter(group2->list);

	// Walk the sequences in parallel.  Add the object with the lowest
	// value (address) and advance the iterator that referenced it.
	// If both objects are the same address, just add one and advance
	// both iterators.
	while (!g_sequence_iter_is_end(iter1) &&
			!g_sequence_iter_is_end(iter2)) {
		obj1 = g_sequence_get(iter1);
		obj2 = g_sequence_get(iter2);

		switch (group_compare_obj(obj1, obj2, NULL)) {
		case -1:
			g_sequence_append(union_grp->list, obj1);
			iter1 = g_sequence_iter_next(iter1);
			break;
		case 0:
			g_sequence_append(union_grp->list, obj1);
			iter1 = g_sequence_iter_next(iter1);
			iter2 = g_sequence_iter_next(iter2);
			break;
		case 1:
			g_sequence_append(union_grp->list, obj2);
			iter2 = g_sequence_iter_next(iter2);
			break;
		}
	}

	// If the first iterator wasn't at the end of its sequence, walk
	// the rest of the sequence appending the objects to the union.
	while (!g_sequence_iter_is_end(iter1)) {
		obj1 = g_sequence_get(iter1);
		g_sequence_append(union_grp->list, obj1);
		iter1 = g_sequence_iter_next(iter1);
	}

	// If the second iterator wasn't at the end of its sequence, walk
	// the rest of the sequence appending the objects to the union.
	while (!g_sequence_iter_is_end(iter2)) {
		obj2 = g_sequence_get(iter2);
		g_sequence_append(union_grp->list, obj2);
		iter2 = g_sequence_iter_next(iter2);
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

static int
group_intersection(group_t *group1, group_t *group2, group_t *inter_grp)
{
	int retval = -1;
	GSequenceIter *iter1 = NULL, *iter2 = NULL;

	TRACE3_ENTER("group1 = %p, group2 = %p, inter_grp = %p",
			group1, group2, inter_grp);

	if (!group1 || !group2 || !inter_grp)
		goto done;

	// Begin at the beginning of each ordered sequence.
	iter1 = g_sequence_get_begin_iter(group1->list);
	iter2 = g_sequence_get_begin_iter(group2->list);

	// Loop through objects in both groups, adding them to the intersection
	// group if they match.  When the objects in either group are exhausted,
	// no more matches can occur so exit.
	while (!g_sequence_iter_is_end(iter1) &&
			!g_sequence_iter_is_end(iter2)) {
		obj_t *obj = NULL;

		switch (group_compare_obj_iter(iter1, iter2, NULL)) {
		case -1:
			iter1 = g_sequence_iter_next(iter1);
			break;
		case 0:
			obj = g_sequence_get(iter1);
			g_sequence_append(inter_grp->list, obj);
			iter1 = g_sequence_iter_next(iter1);
			iter2 = g_sequence_iter_next(iter2);
			break;
		case 1:
			iter2 = g_sequence_iter_next(iter2);
			break;
		}
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

static int
group_difference(group_t *group1, group_t *group2, group_t *diff_grp)
{
	int retval = -1;
	GSequenceIter *iter1 = NULL, *iter2 = NULL;
	obj_t *obj1 = NULL, *obj2 = NULL;

	TRACE3_ENTER("group1 = %p, group2 = %p, diff_grp = %p",
			group1, group2, diff_grp);

	if (!group1 || !group2 || !diff_grp)
		goto done;

	// Begin at the beginning of each ordered sequence.
	iter1 = g_sequence_get_begin_iter(group1->list);
	iter2 = g_sequence_get_begin_iter(group2->list);

	// Begin at the beginning of each ordered sequence.
	iter1 = g_sequence_get_begin_iter(group1->list);
	iter2 = g_sequence_get_begin_iter(group2->list);

	// The code below does difference.
	// Loop through objects in both groups, adding the objects from
	// group1 that don't match group2.  When the objects in group 2 are
	// exhausted, add the remaining objects from group 1.
	while (!g_sequence_iter_is_end(iter1) &&
			!g_sequence_iter_is_end(iter2)) {
		obj1 = g_sequence_get(iter1);
		obj2 = g_sequence_get(iter2);

		switch (group_compare_obj(obj1, obj2, NULL)) {
		case -1:
			g_sequence_append(diff_grp->list, obj1);
			iter1 = g_sequence_iter_next(iter1);
			break;
		case 0:
			iter1 = g_sequence_iter_next(iter1);
			iter2 = g_sequence_iter_next(iter2);
			break;
		case 1:
			iter2 = g_sequence_iter_next(iter2);
			break;
		}
	}

	// If the first iterator wasn't at the end of its sequence, walk
	// the rest of the sequence appending the objects to the difference.
	while (!g_sequence_iter_is_end(iter1)) {
		obj1 = g_sequence_get(iter1);
		g_sequence_append(diff_grp->list, obj1);
		iter1 = g_sequence_iter_next(iter1);
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

static int
group_sym_difference(group_t *group1, group_t *group2, group_t *diff_grp)
{
	int retval = -1;
	GSequenceIter *iter1 = NULL, *iter2 = NULL;
	obj_t *obj1 = NULL, *obj2 = NULL;

	TRACE3_ENTER("group1 = %p, group2 = %p, diff_grp = %p",
			group1, group2, diff_grp);

	if (!group1 || !group2 || !diff_grp)
		goto done;

	// Begin at the beginning of each ordered sequence.
	iter1 = g_sequence_get_begin_iter(group1->list);
	iter2 = g_sequence_get_begin_iter(group2->list);

	// The code below does symmetric difference.
	// Loop through objects in both groups, adding them to the
	// difference group in order.  Skip objects that match.  When
	// the objects in one group are exhausted, add the remaining
	// objects from the other group.
	while (!g_sequence_iter_is_end(iter1) &&
			!g_sequence_iter_is_end(iter2)) {
		obj1 = g_sequence_get(iter1);
		obj2 = g_sequence_get(iter2);

		switch (group_compare_obj(obj1, obj2, NULL)) {
		case -1:
			g_sequence_append(diff_grp->list, obj1);
			iter1 = g_sequence_iter_next(iter1);
			break;
		case 0:
			iter1 = g_sequence_iter_next(iter1);
			iter2 = g_sequence_iter_next(iter2);
			break;
		case 1:
			g_sequence_append(diff_grp->list, obj2);
			iter2 = g_sequence_iter_next(iter2);
			break;
		}
	}

	// If the first iterator wasn't at the end of its sequence, walk
	// the rest of the sequence appending the objects to the difference.
	while (!g_sequence_iter_is_end(iter1)) {
		obj1 = g_sequence_get(iter1);
		g_sequence_append(diff_grp->list, obj1);
		iter1 = g_sequence_iter_next(iter1);
	}

	// If the second iterator wasn't at the end of its sequence, walk
	// the rest of the sequence appending the objects to the difference.
	while (!g_sequence_iter_is_end(iter2)) {
		obj2 = g_sequence_get(iter2);
		g_sequence_append(diff_grp->list, obj2);
		iter2 = g_sequence_iter_next(iter2);
	}

	retval = 0;

done:
	TRACE3_EXIT("retval = %d", retval);

	return retval;
}

static group_t *
find_group_by_opaque(PWR_Grp group)
{
	group_t *grp = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(group);
	opaque_key_t grp_key = OPAQUE_GET_DATA_KEY(group);

	TRACE3_ENTER("group = %p", group);

	// Find the group
	grp = opaque_map_lookup_group(opaque_map, grp_key);
	if (!grp) {
		LOG_FAULT("Group not found in map!");
		goto error_handling;
	}

	// Validate the group's context key against the one provided
	// via the input opaque group.
	if (ctx_key != grp->context_key) {
		LOG_FAULT("Group context invalid!");
		grp = NULL;
		goto error_handling;
	}

error_handling:
	TRACE3_EXIT("grp = %p", grp);

	return grp;
}

static context_t *
find_context_by_opaque(opaque_key_t opaque)
{
	context_t *ctx = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(opaque);

	TRACE2_ENTER("opaque = %p", opaque);

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("Context not found in map!");
		goto error_handling;
	}

	// Validation of the context associated with the opaque
	// type vs. context associated with the creation of the group.
	if (ctx_key != OPAQUE_GET_DATA_KEY(ctx->opaque.key)) {
		LOG_FAULT("Context key doesn't match requested key!");
		LOG_FAULT("    req key = %p, ctx key = %p",
				ctx_key, OPAQUE_GET_DATA_KEY(ctx->opaque.key));
		ctx = NULL;
		goto error_handling;
	}

error_handling:
	TRACE2_EXIT("ctx = %p", ctx);

	return ctx;
}

//----------------------------------------------------------------------//
//		External Group Interfaces				//
//----------------------------------------------------------------------//

int
PWR_GrpCreate(PWR_Cntxt context, PWR_Grp *group)
{
	int status = PWR_RET_FAILURE;
	context_t *ctx = NULL;
	group_t *grp = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(context);

	TRACE1_ENTER("context = %p, group = %p", context, group);

	if (ctx_key != OPAQUE_GET_DATA_KEY(context)) {
		LOG_FAULT("context keys don't match!");
		goto error_handling;
	}

	// Find the context
	ctx = opaque_map_lookup_context(opaque_map, ctx_key);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp = context_new_group(ctx);
	if (!grp) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group = OPAQUE_GENERATE(ctx->opaque.key, grp->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d, *group = %p", status, *group);

	return status;
}

int
PWR_GrpDestroy(PWR_Grp group)
{
	int status = PWR_RET_FAILURE;
	context_t *ctx = NULL;
	group_t *grp = NULL;

	TRACE1_ENTER("group = %p", group);

	// Find the group
	grp = find_group_by_opaque(group);
	if (!grp) {
		LOG_FAULT("group not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	context_del_group(ctx, grp);

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_GrpDuplicate(PWR_Grp group1, PWR_Grp *group2)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx1_key = OPAQUE_GET_CONTEXT_KEY(group1);
	context_t *ctx = NULL;
	group_t *grp1 = NULL, *grp2 = NULL;

	TRACE1_ENTER("group1 = %p, group2 = %p", group1, group2);

	// Find the group
	grp1 = find_group_by_opaque(group1);
	if (!grp1) {
		LOG_FAULT("group not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group1);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp2 = context_new_group(ctx);
	if (!grp2) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	if (group_copy(grp1, grp2) != 0) {
		LOG_FAULT("unable to copy group!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group2 = OPAQUE_GENERATE(ctx1_key, grp2->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && (grp2 != NULL))
		context_del_group(ctx, grp2);

	TRACE1_EXIT("status = %d, *group2 = %p", status, *group2);

	return status;
}

int
PWR_GrpAddObj(PWR_Grp group, PWR_Obj object)
{
	int status = PWR_RET_FAILURE;
	group_t *grp = NULL;
	obj_t *obj = NULL;
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);
	opaque_key_t octx_key = OPAQUE_GET_CONTEXT_KEY(object);
	opaque_key_t gctx_key = OPAQUE_GET_CONTEXT_KEY(group);

	TRACE1_ENTER("group = %p, object = %p", group, object);

	// The object and group must be in the same context.
	if (octx_key != gctx_key) {
		LOG_FAULT("object and group are not in the same context!");
		goto error_handling;
	}

	// Find the group
	grp = find_group_by_opaque(group);
	if (!grp) {
		LOG_FAULT("Group not found!");
		goto error_handling;
	}

	// Find the object
	obj = opaque_map_lookup_object(opaque_map, obj_key);
	if (!obj) {
		LOG_FAULT("object key not found!");
		goto error_handling;
	}

	if (!group_insert_obj(grp, obj)) {
		LOG_FAULT("insert failed!");
		goto error_handling;
	}

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_GrpRemoveObj(PWR_Grp group, PWR_Obj object)
{
	int status = PWR_RET_FAILURE;
	group_t *grp = NULL;
	obj_t *obj = NULL;
	opaque_key_t obj_key = OPAQUE_GET_DATA_KEY(object);
	opaque_key_t octx_key = OPAQUE_GET_CONTEXT_KEY(object);
	opaque_key_t gctx_key = OPAQUE_GET_CONTEXT_KEY(group);

	TRACE1_ENTER("group = %p, object = %p", group, object);

	// The object and group must be in the same context.
	if (octx_key != gctx_key) {
		LOG_FAULT("object and group are not in the same context!");
		goto error_handling;
	}

	// Find the group
	grp = find_group_by_opaque(group);
	if (!grp) {
		LOG_FAULT("Group not found!");
		goto error_handling;
	}

	// Find the object
	obj = opaque_map_lookup_object(opaque_map, obj_key);
	if (!obj) {
		LOG_FAULT("object key not found!");
		goto error_handling;
	}

	if (!group_remove_obj(grp, obj)) {
		LOG_FAULT("remove failed!");
		goto error_handling;
	}

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_GrpGetNumObjs(PWR_Grp group)
{
	// Start with status as failure and only set it to
	// a value >= 0 if we get a valid number of objects
	int status = PWR_RET_FAILURE;
	group_t *grp = NULL;

	TRACE1_ENTER("group = %p", group);

	// Find the group
	grp = find_group_by_opaque(group);
	if (!grp) {
		LOG_FAULT("Group not found!");
		goto error_handling;
	}

	status = group_get_length(grp);

error_handling:
	TRACE1_EXIT("status = %d", status);

	return status;
}

int
PWR_GrpGetObjByIndx(PWR_Grp group, int index, PWR_Obj *object)
{
	int status = PWR_RET_FAILURE;
	obj_t *obj = NULL;
	group_t *grp = NULL;
	GSequenceIter *iter = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(group);

	TRACE1_ENTER("group = %p, index = %d, object = %p",
			group, index, object);

	if (index < 0) {
		LOG_FAULT("negative index (%d)!", index);
		status = PWR_RET_BAD_INDEX;
		goto error_handling;
	}

	// Find the group
	grp = find_group_by_opaque(group);
	if (!grp) {
		LOG_FAULT("Group not found!");
		goto error_handling;
	}

	// Try to get the i-th object in the group
	iter = g_sequence_get_iter_at_pos(grp->list, index);
	if (g_sequence_iter_is_end(iter)) {
		LOG_FAULT("no group at index (%d)!", index);
		status = PWR_RET_NO_OBJ_AT_INDEX;
		goto error_handling;
	}

	obj = g_sequence_get(iter);
	*object = OPAQUE_GENERATE(ctx_key, obj->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	TRACE1_EXIT("status = %d, *object = %p", status, *object);

	return status;
}

int
PWR_GrpUnion(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx1_key = OPAQUE_GET_CONTEXT_KEY(group1);
	opaque_key_t ctx2_key = OPAQUE_GET_CONTEXT_KEY(group2);
	context_t *ctx = NULL;
	group_t *grp1 = NULL, *grp2 = NULL, *grp3 = NULL;

	// The two groups must be in the same context.
	if (ctx1_key != ctx2_key) {
		LOG_FAULT("groups are not in the same context!");
		goto error_handling;
	}

	// Find group1
	grp1 = find_group_by_opaque(group1);
	if (!grp1) {
		LOG_FAULT("Group 1 not found!");
		goto error_handling;
	}

	// Find group2
	grp2 = find_group_by_opaque(group2);
	if (!grp2) {
		LOG_FAULT("Group 2 not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group1);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp3 = context_new_group(ctx);
	if (!grp3) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	if (group_union(grp1, grp2, grp3) != 0) {
		LOG_FAULT("unable to unify groups!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group3 = OPAQUE_GENERATE(ctx1_key, grp3->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && (grp3 != NULL))
		context_del_group(ctx, grp3);

	TRACE1_EXIT("status = %d, *group3 = %p", status, *group3);

	return status;
}

int
PWR_GrpIntersection(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx1_key = OPAQUE_GET_CONTEXT_KEY(group1);
	opaque_key_t ctx2_key = OPAQUE_GET_CONTEXT_KEY(group2);
	context_t *ctx = NULL;
	group_t *grp1 = NULL, *grp2 = NULL, *grp3 = NULL;

	TRACE1_ENTER("group1 = %p, group2 = %p, group3 = %p",
			group1, group2, group3);

	// The two groups must be in the same context.
	if (ctx1_key != ctx2_key) {
		LOG_FAULT("groups are not in the same context!");
		goto error_handling;
	}

	// Find group1
	grp1 = find_group_by_opaque(group1);
	if (!grp1) {
		LOG_FAULT("Group 1 not found!");
		goto error_handling;
	}

	// Find group2
	grp2 = find_group_by_opaque(group2);
	if (!grp2) {
		LOG_FAULT("Group 2 not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group1);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp3 = context_new_group(ctx);
	if (!grp3) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	if (group_intersection(grp1, grp2, grp3) != 0) {
		LOG_FAULT("unable to intersect groups!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group3 = OPAQUE_GENERATE(ctx1_key, grp3->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && (grp3 != NULL))
		context_del_group(ctx, grp3);

	TRACE1_EXIT("status = %d, *group3 = %p", status, *group3);

	return status;
}

int
PWR_GrpDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx1_key = OPAQUE_GET_CONTEXT_KEY(group1);
	opaque_key_t ctx2_key = OPAQUE_GET_CONTEXT_KEY(group2);
	context_t *ctx = NULL;
	group_t *grp1 = NULL, *grp2 = NULL, *grp3 = NULL;

	TRACE1_ENTER("group1 = %p, group2 = %p, group3 = %p",
			group1, group2, group3);

	// The two groups must be in the same context.
	if (ctx1_key != ctx2_key) {
		LOG_FAULT("groups are not in the same context!");
		goto error_handling;
	}

	// Find group1
	grp1 = find_group_by_opaque(group1);
	if (!grp1) {
		LOG_FAULT("Group 1 not found!");
		goto error_handling;
	}

	// Find group2
	grp2 = find_group_by_opaque(group2);
	if (!grp2) {
		LOG_FAULT("Group 2 not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group1);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp3 = context_new_group(ctx);
	if (!grp3) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	if (group_difference(grp1, grp2, grp3) != 0) {
		LOG_FAULT("unable to compare groups!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group3 = OPAQUE_GENERATE(ctx1_key, grp3->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && (grp3 != NULL))
		context_del_group(ctx, grp3);

	TRACE1_EXIT("status = %d, *group3 = %p", status, *group3);

	return status;
}

int
PWR_GrpSymDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3)
{
	int status = PWR_RET_FAILURE;
	opaque_key_t ctx1_key = OPAQUE_GET_CONTEXT_KEY(group1);
	opaque_key_t ctx2_key = OPAQUE_GET_CONTEXT_KEY(group2);
	context_t *ctx = NULL;
	group_t *grp1 = NULL, *grp2 = NULL, *grp3 = NULL;

	TRACE1_ENTER("group1 = %p, group2 = %p, group3 = %p",
			group1, group2, group3);

	// The two groups must be in the same context.
	if (ctx1_key != ctx2_key) {
		LOG_FAULT("groups are not in the same context!");
		goto error_handling;
	}

	// Find group1
	grp1 = find_group_by_opaque(group1);
	if (!grp1) {
		LOG_FAULT("Group 1 not found!");
		goto error_handling;
	}

	// Find group2
	grp2 = find_group_by_opaque(group2);
	if (!grp2) {
		LOG_FAULT("Group 2 not found!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(group1);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp3 = context_new_group(ctx);
	if (!grp3) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	if (group_sym_difference(grp1, grp2, grp3) != 0) {
		LOG_FAULT("unable to compare groups!");
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group3 = OPAQUE_GENERATE(ctx1_key, grp3->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && (grp3 != NULL))
		context_del_group(ctx, grp3);

	TRACE1_EXIT("status = %d, *group3 = %p", status, *group3);

	return status;
}

typedef struct {
	group_t *grp;
	PWR_ObjType type;
	int error;
} populate_arg_t;

static gboolean
populate_named_group(GNode *node, gpointer data)
{
	obj_t		*obj = (obj_t *)node->data;
	populate_arg_t	*args = (populate_arg_t *)data;

	TRACE3_ENTER("node = %p, data = %p", node, data);

	if (obj->type == args->type) {
		if (!group_insert_obj(args->grp, obj)) {
			LOG_FAULT("insert failed!");
			++args->error;
		}
	}

	TRACE3_EXIT("");

	return FALSE; // continue traversal
}

typedef struct {
	const char *name;
	PWR_ObjType type;
} named_group_info_t;

named_group_info_t named_groups[] = { { CRAY_NAMED_GRP_SOCKETS, PWR_OBJ_SOCKET },
	{ CRAY_NAMED_GRP_CORES,   PWR_OBJ_CORE },
	{ CRAY_NAMED_GRP_MEMS,    PWR_OBJ_MEM },
	{ CRAY_NAMED_GRP_HTS,     PWR_OBJ_HT },
	{ .name = NULL } };

int
PWR_CntxtGetGrpByName(PWR_Cntxt context, const char *name, PWR_Grp *group)
{
	int status = PWR_RET_FAILURE;
	context_t *ctx = NULL;
	group_t *grp = NULL;
	opaque_key_t ctx_key = OPAQUE_GET_CONTEXT_KEY(context);
	named_group_info_t *grpinfo = named_groups;

	TRACE1_ENTER("context = %p, name = '%s', group = %p",
			context, name, group);

	if (name == NULL) {
		LOG_FAULT("invalid name specified!");
		goto error_handling;
	}

	if (group == NULL) {
		LOG_FAULT("NULL group pointer!");
		goto error_handling;
	}

	// find out what this named group contains
	while (grpinfo) {
		if (grpinfo->name == NULL) { // end of list
			LOG_FAULT("unknown named group: %s", name);
			status = PWR_RET_WARN_NO_GRP_BY_NAME;
			goto error_handling;
		} else {
			if (g_strcmp0(name, grpinfo->name) == 0)
				break;
		}
		++grpinfo;
	}

	if (ctx_key != OPAQUE_GET_DATA_KEY(context)) {
		LOG_FAULT("context keys don't match!");
		goto error_handling;
	}

	// Find the context
	ctx = find_context_by_opaque(context);
	if (!ctx) {
		LOG_FAULT("context not found!");
		goto error_handling;
	}

	// Have context create the group
	grp = context_new_group(ctx);
	if (!grp) {
		LOG_FAULT("unable to create new group!");
		goto error_handling;
	}

	// traverse the context's hierarchy and populate the named group with
	// objects of the requested type
	populate_arg_t args;
	args.grp = grp;
	args.type = grpinfo->type;
	args.error = 0;
	g_node_traverse(ctx->entry_point,
			G_IN_ORDER, G_TRAVERSE_ALL, -1,
			populate_named_group, &args);
	if (args.error) {
		goto error_handling;
	}

	// Provide opaque_key to the caller
	*group = OPAQUE_GENERATE(ctx->opaque.key, grp->opaque.key);

	status = PWR_RET_SUCCESS;

error_handling:
	if ((status != PWR_RET_SUCCESS) && grp) {
		context_del_group(ctx, grp);
	}

	TRACE1_EXIT("status = %d", status);

	return status;
}

