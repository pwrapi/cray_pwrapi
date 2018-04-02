/*
 * Copyright (c) 2015-2017, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.

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
 * use of the statistics interface.
 */

#ifndef _PWR_STATISTICS_H
#define _PWR_STATISTICS_H

#include <glib.h>

#include "typedefs.h"
#include "opaque.h"


//
// stat_t is the internal implementation of PWR_Stat opaque type.
//

// See typedefs.h for:
// typedef struct stat_s stat_t;
struct stat_s {
	opaque_ref_t	opaque;	// Always first: opaque reference
	gpointer	context_key; // Context statistic was created under
	GList		*link; // Link into parent object/group's list of stats
	GList		*ctx_link; // Link into context's list of stats

	PWR_Obj		obj;
	PWR_Grp		grp;
	PWR_AttrName	attr;
	PWR_AttrStat	stat;
	double		sample_rate;
	int		objcount; // 1 for an object, group size for groups

	PWR_Time	start;
	PWR_Time	stop;
	double		*values;
	PWR_Time	*instants;
	GMutex		val_lock; // lock to access values and instants

	GThread		*thread;
	bool		die;
};

stat_t *new_stat(void);
void del_stat(stat_t *);
void stat_destroy_callback(gpointer);
void stat_invalidate_callback(gpointer);

#endif /* _PWR_STATISTICS_H */
