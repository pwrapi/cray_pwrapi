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

#ifndef _PWR_OPAQUE_H
#define _PWR_OPAQUE_H

#include <stdint.h>
#include <stdbool.h>

#include <glib.h>

//----------------------------------------------------------------------//
// OPAQUE REFERENCES: types and prototypes				//
//----------------------------------------------------------------------//

typedef enum {
	OPAQUE_INVALID = 0,
	OPAQUE_CONTEXT,
	OPAQUE_GROUP,
	OPAQUE_OBJECT,
	OPAQUE_STATUS,
	OPAQUE_STAT,
	OPAQUE_HINT,
	OPAQUE_MAX
} opaque_type_t;

typedef void * opaque_key_t;	// Hash key type for opaque references

typedef struct {
	opaque_type_t	type;	// The data type referenced by addr
	opaque_key_t	key;	// The key to the hash table (opaque reference)
} opaque_ref_t;

typedef struct {
	GRand		*rand;
	GHashTable	*table;
} opaque_map_t;

// Global map to associate reference keys to an address of the
// represented data.
extern opaque_map_t *opaque_map;

#define OPAQUE_UPPER 0xffffffff00000000
#define OPAQUE_LOWER 0x00000000ffffffff

#define OPAQUE_GENERATE(context_key, data_key)	\
	((opaque_key_t)(((uint64_t)context_key << 32) | ((uint64_t)data_key & OPAQUE_LOWER)))

#define OPAQUE_GET_CONTEXT_KEY(opaque)	\
	((opaque_key_t)((uint64_t)opaque >> 32))

#define OPAQUE_GET_DATA_KEY(opaque)	\
	((opaque_key_t)(OPAQUE_LOWER & (uint64_t)opaque))


void		opaque_map_free(opaque_map_t *map);
opaque_map_t	*opaque_map_new(void);
opaque_key_t	opaque_map_insert(opaque_map_t *map, opaque_type_t type,
				  opaque_ref_t *opaque);
bool		opaque_map_remove(opaque_map_t *map, opaque_key_t key);
opaque_ref_t	*opaque_map_lookup(opaque_map_t *map, opaque_key_t key);
opaque_ref_t	*opaque_map_lookup_type(opaque_map_t *map, opaque_key_t key,
					opaque_type_t type);

#define opaque_map_lookup_context(m, k)	\
	((context_t *)opaque_map_lookup_type(m, k, OPAQUE_CONTEXT))
#define opaque_map_lookup_group(m, k)	\
	((group_t *)opaque_map_lookup_type(m, k, OPAQUE_GROUP))
#define opaque_map_lookup_object(m, k)	\
	((obj_t *)opaque_map_lookup_type(m, k, OPAQUE_OBJECT))
#define opaque_map_lookup_status(m, k)	\
	((status_t *)opaque_map_lookup_type(m, k, OPAQUE_STATUS))
#define opaque_map_lookup_stat(m, k)	\
	((stat_t *)opaque_map_lookup_type(m, k, OPAQUE_STAT))
#define opaque_map_lookup_hint(m, k)	\
	((hint_t *)opaque_map_lookup_type(m, k, OPAQUE_HINT))

#endif // _PWR_OPAQUE_H
