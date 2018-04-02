/*
 * Copyright (c) 2015-2016, Cray Inc.
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
 * use of the groups interface.
 */

#ifndef _PWR_GROUP_H
#define _PWR_GROUP_H

#include <stdbool.h>

#include <glib.h>

#include "typedefs.h"
#include "object.h"
#include "opaque.h"
#include "statistics.h"

//
// group_t is the internal implementation of PWR_Grp opaque type.
//
// See typedefs.h for:
// typedef struct group_s group_t;
struct group_s {
	opaque_ref_t	 opaque;	// Always first: opaque reference
	gpointer	 context_key;	// Context group was created under
	GList		*link;		// Link into the context's group list
	GSequence	*list;		// Collection of objects in group
	GList		*stat_list;	// List of statistics for this group
};

group_t	*new_group(void);
void	 del_group(group_t *group);

bool	 group_insert_obj(group_t *group, obj_t *obj);
bool	 group_remove_obj(group_t *group, obj_t *obj);

void	 group_insert_callback(GNode *gnode, gpointer data);
void	 group_destroy_callback(gpointer data);

stat_t	*group_new_statistic(group_t *group);
void	 group_del_statistic(group_t *group, stat_t *stat);

#endif /* _PWR_GROUP_H */

