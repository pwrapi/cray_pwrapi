/*
 * Copyright (c) 2017-2018, Cray Inc.
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

#ifndef _X86_OBJ_H
#define _X86_OBJ_H

#include <cray-powerapi/types.h>

#include "pwr_list.h"

#include "object.h"


//----------------------------------------------------------------------//
//			Common Types and Prototypes			//
//----------------------------------------------------------------------//

#define MSR_PKG_POWER_SKU_UNIT			0x606
#define MSR_PKG_RAPL_PERF_STATUS		0x613
#define MSR_DDR_RAPL_PERF_STATUS		0x61b
#ifdef USE_RDMSR
#define RDMSR_COMMAND		"rdmsr --processor %ld --c-language 0x%x"
#endif

#define MSR_FIELD_TIME_UNIT_MASK		0xf
#define MSR_FIELD_TIME_UNIT_SHIFT		16
#define MSR_FIELD_PKG_THROTTLE_CNTR_MASK	0xffffffff
#define MSR_FIELD_PKG_THROTTLE_CNTR_SHIFT	0
#define MSR_FIELD_DDR_THROTTLE_CNTR_MASK	0xffffffff
#define MSR_FIELD_DDR_THROTTLE_CNTR_SHIFT	0

// The maximum number of time window multiples allowed for a time window
// metadata setting. The time window will be based on a multiple of the time
// between updates for the pm_counters values.
#define MD_TIME_WINDOW_MULTIPLE_MAX		10

typedef struct x86_metadata_s {
	// Metadata to describe the frequency and time window that
	// pm_counters values are updated
	double		pm_counters_update_rate; // pm_counters raw_scan_hz
	PWR_Time	pm_counters_time_window; // time between updates in nsec

	// Hardware Thread cstate metadata
	pwr_list_uint64_t ht_cstate;

	// Hardware Thread frequency metadata
	// Used for all frequency related attributes.
	pwr_list_double_t ht_freq;

	// Hardware Thread governor metadata
	pwr_list_string_t ht_gov;

	// Node vendor data
	// Used for nodes and power planes.
	char		*node_vendor_info;

	// Socket vendor data
	// Used for sockets, mems, cores, and hardware threads.
	char		*socket_vendor_info;
} x86_metadata_t;

extern x86_metadata_t x86_metadata;

int x86_get_time_unit(uint64_t ht_id, uint64_t *value, struct timespec *ts);

int x86_find_rapl_id(uint64_t socket_id, uint64_t *rapl_pkg_id,
		uint64_t *rapl_mem_id);

double x86_cpu_power_factor(void);

int x86_get_power(const char *path, PWR_Time window, double *value,
		struct timespec *ts);

int x86_get_throttled_time(int msr, uint64_t ht_id, uint64_t *value,
		struct timespec *ts);

int x86_obj_get_meta(obj_t *obj, PWR_AttrName attr, PWR_MetaName meta,
		void *value);
int x86_obj_get_meta_at_index(obj_t *obj, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str);

//----------------------------------------------------------------------//
//		Plugin Node Object Types and Prototypes			//
//----------------------------------------------------------------------//

typedef struct {
	uint8_t dummy;
} x86_node_t;

int x86_new_node(node_t *node);
void x86_del_node(node_t *node);

// Attribute Functions
int x86_node_get_power(node_t *node, double *value, struct timespec *ts);
int x86_node_get_power_limit_max(node_t *node, double *value,
		struct timespec *ts);
int x86_node_get_energy(node_t *node, double *value, struct timespec *ts);

// Metadata Functions
int x86_node_get_meta(node_t *node, PWR_AttrName attr, PWR_MetaName meta,
		void *value);
int x86_node_set_meta(node_t *node, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_node_get_meta_at_index(node_t *node, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str);

//----------------------------------------------------------------------//
//		Plugin Socket Object Types and Prototypes		//
//----------------------------------------------------------------------//

typedef struct {
	uint64_t rapl_pkg_id;
	uint64_t temp_id;
	char *temp_input;
	char *temp_max;
	PWR_Time power_time_window_meta;
} x86_socket_t;

int x86_new_socket(socket_t *socket);
void x86_del_socket(socket_t *socket);

// Attribute Functions
int x86_socket_get_power(socket_t *socket, double *value, struct timespec *ts);
int x86_socket_get_power_limit_max(socket_t *socket, double *value,
		struct timespec *ts);
int x86_socket_set_power_limit_max(socket_t *socket, ipc_t *ipc,
		const double *value);
int x86_socket_get_energy(socket_t *socket, double *value, struct timespec *ts);
int x86_socket_get_temp(socket_t *socket, double *value, struct timespec *ts);
int x86_socket_get_throttled_time(socket_t *socket, uint64_t *value,
		struct timespec *ts);

// Metadata Functions
int x86_socket_get_meta(socket_t *socket, PWR_AttrName attr, PWR_MetaName meta,
		void *value);
int x86_socket_set_meta(socket_t *socket, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_socket_get_meta_at_index(socket_t *socket, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str);

//----------------------------------------------------------------------//
//		Plugin Memory Object Types and Prototypes		//
//----------------------------------------------------------------------//

typedef struct {
	uint64_t rapl_pkg_id;
	uint64_t rapl_mem_id;
	PWR_Time power_time_window_meta;
} x86_mem_t;

int x86_new_mem(mem_t *mem);
void x86_del_mem(mem_t *mem);

// Attribute Functions
int x86_mem_get_throttled_time(mem_t *mem, uint64_t *value,
		struct timespec *ts);
int x86_mem_get_power(mem_t *mem, double *value, struct timespec *ts);
int x86_mem_get_power_limit_max(mem_t *mem, double *value, struct timespec *ts);
int x86_mem_set_power_limit_max(mem_t *mem, ipc_t *ipc, const double *value);
int x86_mem_get_energy(mem_t *mem, double *value, struct timespec *ts);

// Metadata Functions
int x86_mem_get_meta(mem_t *mem, PWR_AttrName attr,
		PWR_MetaName meta, void *value);
int x86_mem_set_meta(mem_t *mem, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_mem_get_meta_at_index(mem_t *mem, PWR_AttrName attr, unsigned int index,
		void *value, char *value_str);

//----------------------------------------------------------------------//
//	Plugin Power Plane Object Types and Prototypes			//
//----------------------------------------------------------------------//

typedef struct {
	uint8_t dummy;
} x86_pplane_t;

int x86_new_pplane(pplane_t *pplane);
void x86_del_pplane(pplane_t *pplane);

// Attribute Functions
int x86_pplane_get_power(pplane_t *pplane, double *value, struct timespec *ts);
int x86_pplane_get_energy(pplane_t *pplane, double *value, struct timespec *ts);

// Metadata Functions
int x86_pplane_get_meta(pplane_t *pplane, PWR_AttrName attr,
		PWR_MetaName meta, void *value);
int x86_pplane_set_meta(pplane_t *pplane, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_pplane_get_meta_at_index(pplane_t *pplane, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str);

//----------------------------------------------------------------------//
//		Plugin Core Object Types and Prototypes			//
//----------------------------------------------------------------------//

typedef struct {
	uint64_t temp_id;
	char *temp_input;
	char *temp_max;
} x86_core_t;

int x86_new_core(core_t *core);
void x86_del_core(core_t *core);

// Attribute Functions
int x86_core_get_temp(core_t *core, double *value, struct timespec *ts);

// Metadata Functions
int x86_core_get_meta(core_t *core, PWR_AttrName attr,
		PWR_MetaName meta, void *value);
int x86_core_set_meta(core_t *core, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_core_get_meta_at_index(core_t *core, PWR_AttrName attr,
		unsigned int index, void *value, char *value_str);

//----------------------------------------------------------------------//
//	Plugin Hardware Thread Object Types and Prototypes		//
//----------------------------------------------------------------------//

typedef struct {
	uint8_t dummy;
} x86_ht_t;

int x86_new_ht(ht_t *ht);
void x86_del_ht(ht_t *ht);

// Attribute Functions
int x86_ht_get_cstate_limit(ht_t *ht, uint64_t *value, struct timespec *ts);
int x86_ht_set_cstate_limit(ht_t *ht, ipc_t *ipc, const uint64_t *value);
int x86_ht_get_freq(ht_t *ht, double *value, struct timespec *ts);
int x86_ht_get_freq_req(ht_t *ht, double *value, struct timespec *ts);
int x86_ht_set_freq_req(ht_t *ht, ipc_t *ipc, const double *value);
int x86_ht_get_freq_limit_min(ht_t *ht, double *value, struct timespec *ts);
int x86_ht_set_freq_limit_min(ht_t *ht, ipc_t *ipc, const double *value);
int x86_ht_get_freq_limit_max(ht_t *ht, double *value, struct timespec *ts);
int x86_ht_set_freq_limit_max(ht_t *ht, ipc_t *ipc, const double *value);
int x86_ht_get_governor(ht_t *ht, uint64_t *value, struct timespec *ts);
int x86_ht_set_governor(ht_t *ht, ipc_t *ipc, const uint64_t *value);

// Metadata Functions
int x86_ht_get_meta(ht_t *ht, PWR_AttrName attr,
		PWR_MetaName meta, void *value);
int x86_ht_set_meta(ht_t *ht, ipc_t *ipc, PWR_AttrName attr,
		PWR_MetaName meta, const void *value);
int x86_ht_get_meta_at_index(ht_t *ht, PWR_AttrName attr, unsigned int index,
		void *value, char *value_str);

#endif /* _X86_OBJ_H */
