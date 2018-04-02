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
 * This file contains the structure definitions and prototypes for internal
 * use of contexts.
 */

#ifndef _PWR_CONTEXT_H
#define _PWR_CONTEXT_H

#include <glib.h>

#include <cray-powerapi/types.h>

#include "typedefs.h"
#include "group.h"
#include "hierarchy.h"
#include "attributes.h"
#include "ipc.h"
#include "opaque.h"
#include "statistics.h"

// Internal definition of the PWR_Cntxt opaque object.
// see typedefs.h for:
// typedef struct context_s context_t;
struct context_s {
	opaque_ref_t	opaque;		// Always first: opaque ref
	PWR_CntxtType	type;
	PWR_Role	role;
	char		*name;
	GNode		*entry_point;	// Entry point, hierarchy tree
	hierarchy_t	*hierarchy;	// Hierarchy plugin data
	GSequence	*hintnames;	// All hint names in this context, sorted
	uint64_t	hintunique;	// Unique index for name generation
	ipc_t		*ipc;		// IPC plugin data
	GList		*group_list;	// List of allocated groups
	GList		*status_list;	// List of allocated status objects
	GList		*stat_list;	// List of allocated statistics objects
};

group_t  *context_new_group(context_t *context);
void      context_del_group(context_t *context, group_t *group);
status_t *context_new_status(context_t *context);
void	  context_del_status(context_t *context, status_t *stat);
stat_t   *context_new_statistic(context_t *context);
void	  context_del_statistic(context_t *context, stat_t *stat);

#endif /* _PWR_CONTEXT_H */

