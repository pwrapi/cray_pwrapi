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
 * This file contains the functions necessary for hierarchy navigation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _GNU_SOURCE // Enable GNU extensions

#include <glib.h>

#include <cray-powerapi/api.h>
#include <log.h>

#include "plugin.h"
#include "hierarchy.h"
#include "plugins/x86/x86_plugin.h"


//
// hierarchy->tree  parent  child  Handling
// ---------------  ------  -----  -----------------------------
// X                X       NULL   Error, invalid input
//
// X                SET     SET    New node becomes child of parent
//
// NULL             NULL    SET    New node becomes root of tree
// SET              NULL    SET    New node becomes child of root of tree
//
int
hierarchy_insert(hierarchy_t *hierarchy, GNode *parent, obj_t *obj)
{
	int retval = PWR_RET_SUCCESS;

	TRACE2_ENTER("hierarchy = %p, parent = %p, obj = %p",
			hierarchy, parent, obj);

	if (!obj) {
		LOG_FAULT("No object provided for insert into hierarchy");
		retval = PWR_RET_FAILURE;
		goto error_return;
	}

	LOG_DBG("Inserting object %s into hierarchy", obj->name);

	// Allocate the gnode for the object being inserted
	obj->gnode = g_node_new(obj);
	if (!obj->gnode) {
		LOG_FAULT("Failed to alloc hierarchy gnode for %s", obj->name);
		retval = PWR_RET_FAILURE;
		goto error_return;
	}

	// If parent available, make child one of its children.
	// Else make child the root of the hierarchy tree.
	if (parent) {
		g_node_append(parent, obj->gnode);
	} else {
		hierarchy->tree = obj->gnode;
	}

	// Insert the object into the hierarchy name map
	// and record the OS identifier in the metadata.
	g_hash_table_insert(hierarchy->map, obj->name, obj);

error_return:
	TRACE2_EXIT("result = %d", retval);

	return retval;
}

hierarchy_t *
new_hierarchy(void)
{
	// Allocate hierarchy with initialized to 0's
	hierarchy_t *hierarchy = g_new0(hierarchy_t, 1);

	TRACE2_ENTER("");

	// The name map will be used to deallocate the resources for
	// the power objects. The tree is only used to maintain the
	// hierarchical relationship among the power objects.
	//
	// The tree should be destroyed before the name map.

	// The obj_destroy_callback will ensure all of the resources
	// are deallocated for the power objects when they are
	// removed from the name map.
	hierarchy->map = g_hash_table_new_full(g_str_hash, g_str_equal,
			NULL, obj_destroy_callback);


	// Request plugin construct hierarchy
	if (plugin->construct_hierarchy(hierarchy) != 0) {
		del_hierarchy(hierarchy);
		hierarchy = NULL;
	}

	return hierarchy;
}



void
del_hierarchy(hierarchy_t *hierarchy)
{
	TRACE2_ENTER("hierarchy = %p", hierarchy);

	if (!hierarchy) {
		TRACE2_EXIT("");
		return;
	}

	plugin->destruct_hierarchy(hierarchy);

	if (hierarchy->tree) {
		g_node_destroy(hierarchy->tree);
		hierarchy->tree = NULL;
	}

	// Delete the name map, since name map hash table was
	// created using g_hash_table_new_full() all the
	// keys and values are destroyed by the provided
	// destroy functions.
	if (hierarchy->map) {
		g_hash_table_destroy(hierarchy->map);
		hierarchy->map = NULL;
	}

	g_free(hierarchy);

	TRACE2_EXIT("");
}



//----------------------------------------------------------------------//
//				PLUGIN					//
//----------------------------------------------------------------------//

plugin_t *plugin = NULL;

plugin_t *
new_plugin(void)
{
	plugin_t *plugin = NULL;

	TRACE2_ENTER("");

	plugin = g_new0(plugin_t, 1);
	if (!plugin)
		goto done;

	// Note for future library developers:
	//
	// This is the location where the architecture specific
	// plugin code could be swapped out. Currently it is
	// coded to use the x86 plugin.

	if (x86_construct_plugin(plugin) != 0) {
		del_plugin(plugin);
		plugin = NULL;
	}

done:
	TRACE2_EXIT("plugin = %p", plugin);

	return plugin;
}

void
del_plugin(plugin_t *plugin)
{
	TRACE2_ENTER("plugin = %p", plugin);

	if (plugin) {
		if (plugin->destruct) {
			plugin->destruct(plugin);
		}

		g_free(plugin);
	}

	TRACE2_EXIT("");
}
