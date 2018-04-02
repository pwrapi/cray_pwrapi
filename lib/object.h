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
 * This file contains the structure definitions and prototypes for internal
 * use of power objects and object hieararchies.
 */

#ifndef _PWR_OBJECT_H
#define _PWR_OBJECT_H

#include <stdint.h>
#include <time.h>

#include <cray-powerapi/types.h>

#include <glib.h>

#include "typedefs.h"
#include "bitmask.h"
#include "context.h"
#include "ipc.h"
#include "opaque.h"
#include "statistics.h"


// PWR_ObjType values mapped to name strings
extern const char *obj_names[PWR_NUM_OBJ_TYPES];

// PWR_ObjType values mapped to description strings
extern const char *obj_descriptions[PWR_NUM_OBJ_TYPES];

// PWR_AttrName values mapped to name strings
extern const char *attr_names[PWR_NUM_ATTR_NAMES];

// PWR_AttrName values mapped to description strings
extern const char *attr_descriptions[PWR_NUM_ATTR_NAMES];


// Bitmasks to identify what attributes are supported by
// each of the hierarchy object types in the implementation.
extern const DEFINE_BITMASK(node_attr_bitmask);
extern const DEFINE_BITMASK(socket_attr_bitmask);
extern const DEFINE_BITMASK(mem_attr_bitmask);
extern const DEFINE_BITMASK(pplane_attr_bitmask);
extern const DEFINE_BITMASK(core_attr_bitmask);
extern const DEFINE_BITMASK(ht_attr_bitmask);


struct node_ops {
    // Attribute functions
    int (*get_power) (node_t *self, double *value, struct timespec *ts);
    int (*get_power_limit_max) (node_t *self, double *value,
					struct timespec *ts);
    int (*get_energy) (node_t *self, double *value, struct timespec *ts);

    // Metadata functions
    int (*get_meta) (node_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (node_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (node_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};

struct socket_ops {
    // Attribute functions
    int (*get_power) (socket_t *self, double *value, struct timespec *ts);
    int (*get_power_limit_max) (socket_t *self, double *value,
					struct timespec *ts);
    int (*set_power_limit_max) (socket_t *self, ipc_t *ipc,
					const double *value;);
    int (*get_energy) (socket_t *self, double *value, struct timespec *ts);
    int (*get_throttled_time) (socket_t *self, uint64_t *value,
					struct timespec *ts);
    int (*get_temp) (socket_t *self, double *value, struct timespec *ts);

    // Metadata functions
    int (*get_meta) (socket_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (socket_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (socket_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};

struct mem_ops {
    // Attribute functions
    int (*get_power) (mem_t *self, double *value, struct timespec *ts);
    int (*get_power_limit_max) (mem_t *self, double *value,
					struct timespec *ts);
    int (*set_power_limit_max) (mem_t *self, ipc_t *ipc,
					const double *value);
    int (*get_energy) (mem_t *self, double *value, struct timespec *ts);
    int (*get_throttled_time) (mem_t *self, uint64_t *value,
					struct timespec *ts);

    // Metadata functions
    int (*get_meta) (mem_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (mem_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (mem_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};

struct pplane_ops {
    // Attribute functions
    int (*get_power) (pplane_t *self, double *value, struct timespec *ts);
    int (*get_energy) (pplane_t *self, double *value, struct timespec *ts);

    // Metadata functions
    int (*get_meta) (pplane_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (pplane_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (pplane_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};

struct core_ops {
    // Attribute functions
    int (*get_temp) (core_t *self, double *value, struct timespec *ts);

    // Metadata functions
    int (*get_meta) (core_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (core_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (core_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};

struct ht_ops {
    // Attribute functions
    int (*get_cstate_limit) (ht_t *self, uint64_t *value,
					struct timespec *ts);
    int (*set_cstate_limit) (ht_t *self, ipc_t *ipc,
					const uint64_t *value);
    int (*get_freq) (ht_t *self, double *value, struct timespec *ts);
    int (*get_freq_req) (ht_t *self, double *value, struct timespec *ts);
    int (*set_freq_req) (ht_t *self, ipc_t *ipc, const double *value);
    int (*get_freq_limit_min) (ht_t *self, double *value,
					struct timespec *ts);
    int (*set_freq_limit_min) (ht_t *self, ipc_t *ipc,
					const double *value);
    int (*get_freq_limit_max) (ht_t *self, double *value,
					struct timespec *ts);
    int (*set_freq_limit_max) (ht_t *self, ipc_t *ipc,
					const double *value);
    int (*get_governor) (ht_t *self, uint64_t *value, struct timespec *ts);
    int (*set_governor) (ht_t *self, ipc_t *ipc, const uint64_t *value);

    // Metadata functions
    int (*get_meta) (ht_t *self, PWR_AttrName attr,
			PWR_MetaName meta, void *value);
    int (*set_meta) (ht_t *self, ipc_t *ipc, PWR_AttrName attr,
			PWR_MetaName meta, const void *value);
    int (*get_meta_at_index) (ht_t *self,
			PWR_AttrName attr, unsigned int index, void *value,
			char *value_str);
};


// Generic object type. Must be the first data member
// in every specific object struct.
// See typedefs.h for:
// typedef struct obj_s obj_t;
struct obj_s {
	opaque_ref_t	 opaque;	// Always first: opaque reference
	PWR_ObjType	 type;
	uint64_t	 os_id;
	char		*name;
	GSequence	*hints;
	GNode		*gnode;
};

#define to_obj(x)	((obj_t *) x)
void    obj_destroy_callback(gpointer data);

// Node object type
// See typedefs.h for:
// typedef struct node_s node_t;
struct node_s {
    obj_t	obj;

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct node_ops *ops;
};

node_t  *new_node(uint64_t id, const char *name_fmt, ...);
void    del_node(node_t *);
#define to_node(x)	((node_t *) x)


// Socket object type
// See typedefs.h for:
// typedef struct socket_s socket_t;
struct socket_s {
    obj_t	obj;
    uint64_t	ht_id;		// for operations through an ht interface

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct socket_ops *ops;
};

socket_t *new_socket(uint64_t id, const char *name_fmt, ...);
void     del_socket(socket_t *);
#define  to_socket(x)	((socket_t *) x)


// Memory object type
// See typedefs.h for:
// typedef struct mem_s mem_t;
struct mem_s {
    obj_t	obj;
    uint64_t	ht_id;		// for operations through an ht interface

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct mem_ops *ops;
};

mem_t   *new_mem(uint64_t id, const char *name_fmt, ...);
void    del_mem(mem_t *);
#define to_mem(x)	((mem_t *) x)


// Power Plane object type
// See typedefs.h for:
// typedef struct pplane_s pplane_t;
struct pplane_s {
    obj_t	obj;
    PWR_ObjType	sub_type;	// PWR_OBJ_MEM or PWR_OBJ_SOCKET (CPU)

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct pplane_ops *ops;
};

pplane_t *new_pplane(uint64_t id, const char *name_fmt, ...);
void     del_pplane(pplane_t *);
#define  to_pplane(x)	((pplane_t *) x)


// Core object type
// See typedefs.h for:
// typedef struct core_s core_t;
struct core_s {
    obj_t	obj;
    uint64_t	socket_id;	// socket id for this core

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct core_ops *ops;
};

core_t  *new_core(uint64_t id, const char *name_fmt, ...);
void    del_core(core_t *);
#define to_core(x)	((core_t *) x)


// Hardware Thread object type
// See typedefs.h for:
// typedef struct ht_s ht_t;
struct ht_s {
    obj_t	obj;

    // ----------------- //
    // plugin interfaces //
    // ----------------- //

    void	*plugin_data;

    const struct ht_ops *ops;
};

ht_t    *new_ht(uint64_t id, const char *name_fmt, ...);
void    del_ht(ht_t *);
#define to_ht(x)	((ht_t *) x)


// --------------------------------------------- //
// Object attribute get/set function prototypes. //
// --------------------------------------------- //

int	node_attr_get_value(node_t *node, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	node_attr_set_value(node_t *node, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

int	socket_attr_get_value(socket_t *socket, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	socket_attr_set_value(socket_t *socket, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

int	mem_attr_get_value(mem_t *mem, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	mem_attr_set_value(mem_t *mem, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

int	pplane_attr_get_value(pplane_t *pplane, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	pplane_attr_set_value(pplane_t *pplane, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

int	core_attr_get_value(core_t *core, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	core_attr_set_value(core_t *core, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

int	ht_attr_get_value(ht_t *ht, PWR_AttrName attr,
		void *value, struct timespec *tspec);
int	ht_attr_set_value(ht_t *ht, ipc_t *ipc, PWR_AttrName attr,
		const void *value);

#endif /* _PWR_OBJECT_H */
