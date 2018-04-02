/*
 * Copyright (c) 2017, Cray Inc.
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
 * use of power objects and object hieararchies.
 */

#ifndef _PWR_PLUGIN_H
#define _PWR_PLUGIN_H

#include <cray-powerapi/types.h>

#include "typedefs.h"
#include "hierarchy.h"
#include "object.h"


//------------------------------------------------------------------------
// PLUGIN: Macros, definitions and prototypes for plugins
//------------------------------------------------------------------------

// See typedefs.h for:
// typedef struct plugin_s plugin_t;
struct plugin_s {
	//------------------------------------------------------//
	//	Hierarchy Plugin Data & Functions		//
	//------------------------------------------------------//
	void *plugin_data;

	// System file names
	sysentry_t *sysfile_catalog;

	// Function pointers
	int (*destruct) (plugin_t *plugin);

	int (*construct_hierarchy) (hierarchy_t *hierarchy);
	int (*destruct_hierarchy) (hierarchy_t *hierarchy);

	int (*construct_node) (node_t *node);
	int (*destruct_node) (node_t *node);

	int (*construct_socket) (socket_t *socket);
	int (*destruct_socket) (socket_t *socket);

	int (*construct_mem) (mem_t *mem);
	int (*destruct_mem) (mem_t *mem);

	int (*construct_pplane) (pplane_t *pplane);
	int (*destruct_pplane) (pplane_t *pplane);

	int (*construct_core) (core_t *core);
	int (*destruct_core) (core_t *core);

	int (*construct_ht) (ht_t *ht);
	int (*destruct_ht) (ht_t *ht);
};

plugin_t *new_plugin(void);
void     del_plugin(plugin_t *plugin);

extern plugin_t *plugin;

#endif /* _PWR_PLUGIN_H */

