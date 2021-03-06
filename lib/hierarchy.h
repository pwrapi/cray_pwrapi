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
 * use of power objects and object hieararchies.
 */

#ifndef _PWR_HIERARCHY_H
#define _PWR_HIERARCHY_H

#include <cray-powerapi/types.h>

#include <glib.h>

#include "typedefs.h"
#include "plugin.h"


// See typedefs.h for:
// typedef struct hierarchy_s hierarchy_t;
struct hierarchy_s {
	GNode		*tree;		// root of N-ary tree
	GHashTable	*map;		// name to object map
};

hierarchy_t *new_hierarchy(void);
void	    del_hierarchy(hierarchy_t *hierarchy);
void	    hierarchy_debug(hierarchy_t *hierarchy);
int	    hierarchy_insert(hierarchy_t *hierarchy, GNode *parent, obj_t *obj);
int	    hierarchy_remove(hierarchy_t *hierarchy, obj_t *obj);

#endif /* _PWR_HIERARCHY_H */
