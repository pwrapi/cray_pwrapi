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
 * This file contains the functions necessary for hierarchy navigation.
 */

#include <log.h>

#include "opaque.h"

//----------------------------------------------------------------------//
// 			OPAQUE KEY MAP 					//
//----------------------------------------------------------------------//

// Global map to associate reference keys to an address of the
// represented structure.
opaque_map_t *opaque_map = NULL;

#if 0
static void
opaque_map_debug_entry(opaque_type_t type, opaque_ref_t *opaque)
{
	switch (type) {
		case OPAQUE_CONTEXT:
		{
			context_t *context = (context_t *)opaque;
			LOG_DBG("context = %p, name = '%s'", context, context->name);
			break;
		}
		case OPAQUE_OBJECT:
		{
			obj_t *obj = (obj_t *)opaque;
			LOG_DBG("object = %p, name = '%s'", obj, obj->name);
			break;
		}
		default:
		{
			LOG_DBG("unsupported: addr = %p", opaque);
			break;
		}
	}
}
#endif

static void
opaque_map_clean_entry(void *data)
{
	opaque_ref_t *opaque = (opaque_ref_t *)data;

	TRACE3_ENTER("data = %p", data);

	if (opaque) {
		opaque->type = OPAQUE_INVALID;
		opaque->key = 0;
	}

	TRACE3_EXIT("");
}

void
opaque_map_free(opaque_map_t *map)
{
	TRACE3_ENTER("map = %p", map);

	if (map) {
		g_hash_table_destroy(map->table);
		g_rand_free(map->rand);
		g_free(map);
	}

	TRACE3_EXIT("");
}

opaque_map_t *
opaque_map_new(void)
{
	opaque_map_t *map;
	int error = 0;

	TRACE2_ENTER("");

	map = g_new0(opaque_map_t, 1);
	if (!map) {
		error = 1;
		goto error_return;
	}

	map->rand = g_rand_new();
	if (!map->rand) {
		error = 1;
		goto error_return;
	}

	map->table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
			opaque_map_clean_entry);
	if (!map->table) {
		error = 1;
		goto error_return;
	}

error_return:
	if (error) {
		opaque_map_free(map);
		map = NULL;
	}

	TRACE2_EXIT("map = %p", map);

	return map;
}

opaque_ref_t *
opaque_map_lookup(opaque_map_t *map, opaque_key_t key)
{
	opaque_ref_t *opaque = NULL;

	TRACE2_ENTER("map = %p, key = %p", map, key);

	if (map) {
		opaque = g_hash_table_lookup(map->table, key);
	}

	TRACE2_EXIT("opaque = %p", opaque);

	return opaque;
}

opaque_ref_t *
opaque_map_lookup_type(opaque_map_t *map, opaque_key_t key, opaque_type_t type)
{
	opaque_ref_t *opaque;

	TRACE2_ENTER("map = %p, key = %p, type = %d", map, key, type);

	opaque = opaque_map_lookup(map, key);

	// Validate that the opaque structure is the desired type.
	if (!opaque || opaque->type != type) {
		opaque = NULL;
	}

	TRACE2_EXIT("opaque = %p", opaque);

	return opaque;
}

opaque_key_t
opaque_map_insert(opaque_map_t *map, opaque_type_t type, opaque_ref_t *opaque)
{
	gpointer key = NULL;

	TRACE3_ENTER("map = %p, type = %d, opaque = %p", map, type, opaque);

	if (!map)
		goto done;

	while (key == NULL) {
		key = GUINT_TO_POINTER(g_rand_int(map->rand));
		if (g_hash_table_lookup(map->table, key)) {
			key = NULL;
		} else {
			opaque->type = type;
			opaque->key = key;
			g_hash_table_insert(map->table, key, opaque);
		}
	}

done:
	TRACE3_EXIT("key = %p", key);

	return key;
}

bool
opaque_map_remove(opaque_map_t *map, gpointer key)
{
	bool retval = false;

	TRACE3_ENTER("map = %p, key = %p", map, key);

	if (map) {
		retval = g_hash_table_remove(map->table, key);
	}

	TRACE3_EXIT("retval = %d", retval);

	return retval;
}
