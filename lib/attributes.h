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
 * use of the attributes interface.
 */

#ifndef _PWR_ATTRIBUTES_H
#define _PWR_ATTRIBUTES_H

#include <cray-powerapi/types.h>
#include <opaque.h>

#include "typedefs.h"

//
// status_t is the internal implementation of PWR_Status opaque type.
//
// see typedefs.h for:
// typedef struct status_s status_t;
struct status_s {
    opaque_ref_t    opaque;		// Always first: opaque reference
    gpointer        context_key;	// Context status was created under
    GList           *link;		// Link into the context's status list
    GQueue          *list;		// Collection of PWR_AttrAccessError objects
};

/*
 * attr_dval_t - Every attribute that has a small set of discrete valid values
 *		 will have an attr_dval_t for each of them.
 */
typedef struct attr_dval_s {
	void	   *dval;		// actual discrete value
	const char *name;		// ASCII printable string of discrete value
} attr_dval_t;

/*
 * attr_t - Every attribute in PWR_AttrName has an associated attr_t.
 */
typedef struct attr_s {
	int		valid;		// valid for current object?
	const char	*name;		// ASCII readable attribute name
	int		num_dvals;	// number of discrete values (if any)
	attr_dval_t	**dvals;	// array of all discrete values
} attr_t;

void attr_debug(void);

status_t *new_status(void);
void del_status(status_t *stat);
void status_destroy_callback(gpointer data);

#endif /* _PWR_ATTRIBUTES_H */

