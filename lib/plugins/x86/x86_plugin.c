/*
 * Copyright (c) 2016-2018, Cray Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _GNU_SOURCE // Enable GNU extensions

#include <glib.h>

#include <cray-powerapi/types.h>
#include <log.h>

#include "object.h"
#include "attributes.h"
#include "hierarchy.h"
#include "x86_hierarchy.h"
#include "x86_obj.h"
#include "x86_plugin.h"
#include "x86_paths.h"

#include "../common/file.h"


static const struct node_ops x86_node_ops = {
	// Attribute functions
	.get_power = x86_node_get_power,
	.get_power_limit_max = x86_node_get_power_limit_max,
	.get_energy = x86_node_get_energy,

	// Metadata functions
	.get_meta = x86_node_get_meta,
	.set_meta = x86_node_set_meta,
	.get_meta_at_index = x86_node_get_meta_at_index
};

static const struct socket_ops x86_socket_ops = {
	// Attribute functions
	.get_power = x86_socket_get_power,
	.get_power_limit_max = x86_socket_get_power_limit_max,
	.set_power_limit_max = x86_socket_set_power_limit_max,
	.get_energy = x86_socket_get_energy,
	.get_throttled_time = x86_socket_get_throttled_time,
	.get_temp = x86_socket_get_temp,

	// Metadata functions
	.get_meta = x86_socket_get_meta,
	.set_meta = x86_socket_set_meta,
	.get_meta_at_index = x86_socket_get_meta_at_index
};

static const struct mem_ops x86_mem_ops = {
	// Attribute functions
	.get_power = x86_mem_get_power,
	.get_power_limit_max = x86_mem_get_power_limit_max,
	.set_power_limit_max = x86_mem_set_power_limit_max,
	.get_energy = x86_mem_get_energy,
	.get_throttled_time = x86_mem_get_throttled_time,

	// Metadata functions
	.get_meta = x86_mem_get_meta,
	.set_meta = x86_mem_set_meta,
	.get_meta_at_index = x86_mem_get_meta_at_index
};

static const struct pplane_ops x86_pplane_ops = {
	// Attribute functions
	.get_power = x86_pplane_get_power,
	.get_energy = x86_pplane_get_energy,

	// Metadata functions
	.get_meta = x86_pplane_get_meta,
	.set_meta = x86_pplane_set_meta,
	.get_meta_at_index = x86_pplane_get_meta_at_index
};

static const struct core_ops x86_core_ops = {
	// Attribute functions
	.get_temp = x86_core_get_temp,

	// Metadata functions
	.get_meta = x86_core_get_meta,
	.set_meta = x86_core_set_meta,
	.get_meta_at_index = x86_core_get_meta_at_index
};

static const struct ht_ops x86_ht_ops = {
	// Attribute functions
	.get_cstate_limit = x86_ht_get_cstate_limit,
	.set_cstate_limit = x86_ht_set_cstate_limit,
	.get_freq = x86_ht_get_freq,
	.get_freq_req = x86_ht_get_freq_req,
	.set_freq_req = x86_ht_set_freq_req,
	.get_freq_limit_min = x86_ht_get_freq_limit_min,
	.set_freq_limit_min = x86_ht_set_freq_limit_min,
	.get_freq_limit_max = x86_ht_get_freq_limit_max,
	.set_freq_limit_max = x86_ht_set_freq_limit_max,
	.get_governor = x86_ht_get_governor,
	.set_governor = x86_ht_set_governor,

	// Metadata functions
	.get_meta = x86_ht_get_meta,
	.set_meta = x86_ht_set_meta,
	.get_meta_at_index = x86_ht_get_meta_at_index
};

/*
 * Statically initialize the x86 system files catalog. Note that we have to
 * initialize the common elements here as well, which are hidden behind an
 * additional tag, so there are two init macros.
 *
 * This creates lines that look like this:
 *
 * .hdr.num_cstates_path =
 *      	{"num_cstates_path","/sys/kernel/pm_api/num_cstates"},
 * ...
 * .sysfs_kernel = {"sysfs_kernel", "/sys/kernel/pm_api",
 *
 * Thus, we can go directly to the field in compiled code, OR we can search
 * through the list for the name of the field.
 */
#define _PROCFS         "/proc"
#define	_SYSFS_KERNEL	"/sys/kernel/pm_api"
#define _SYSFS_CPU	"/sys/devices/system/cpu"
#define _SYSFS_PM_CNTRS	"/sys/cray/pm_counters"
#define _SYSFS_RAPL	"/sys/class/powercap/intel-rapl"
#define	_SYSFS_HWMON	"/sys/class/hwmon"

#define	_ini0(_n_,v)	.hdr._n_ = {#_n_, v}
#define	_ini(_n_,v)	._n_ = {#_n_, v}

static x86_sysfiles_t x86_sysfile_catalog = {
	_ini0(num_cstates_path, _SYSFS_KERNEL "/num_cstates"),
	_ini0(cstate_limit_path, _SYSFS_KERNEL "/cstate_limit"),
	_ini0(cstate_latency_path, _SYSFS_CPU "/cpu0/cpuidle/state%d/latency"),
	_ini0(avail_freqs_path, _SYSFS_CPU "/cpu0/cpufreq/scaling_available_frequencies"),
	_ini0(curr_freq_path, _SYSFS_CPU "/cpu0/cpufreq/scaling_cur_freq"),
	_ini0(max_freq_path, _SYSFS_CPU "/cpu0/cpufreq/scaling_max_freq"),
	_ini0(min_freq_path, _SYSFS_CPU "/cpu0/cpufreq/scaling_min_freq"),
	_ini0(kernel_max_path, _SYSFS_CPU "/kernel_max"),
	_ini0(cpu_possible_path, _SYSFS_CPU "/possible"),
	_ini0(cpu_present_path, _SYSFS_CPU "/present"),
	_ini0(cpu_online_path, _SYSFS_CPU "/online"),

	_ini(procfs_cpuinfo, _PROCFS "/cpuinfo"),
	_ini(procfs_cname, _PROCFS "/cray_xt/cname"),
	_ini(procfs_nid, _PROCFS "/cray_xt/nid"),

	_ini(sysfs_kernel, _SYSFS_KERNEL),
	_ini(sysfs_cpu, _SYSFS_CPU),
	_ini(sysfs_pm_cntrs, _SYSFS_PM_CNTRS),
	_ini(sysfs_rapl, _SYSFS_RAPL),
	_ini(sysfs_hwmon, _SYSFS_HWMON),

	_ini(node_power_path, _SYSFS_PM_CNTRS "/power"),
	_ini(node_power_cap_path, _SYSFS_PM_CNTRS "/power_cap"),
	_ini(node_energy_path, _SYSFS_PM_CNTRS "/energy"),
	_ini(node_cpu_power_path, _SYSFS_PM_CNTRS "/cpu_power"),
	_ini(node_cpu_energy_path, _SYSFS_PM_CNTRS "/cpu_energy"),
	_ini(node_mem_power_path, _SYSFS_PM_CNTRS "/memory_power"),
	_ini(node_mem_energy_path, _SYSFS_PM_CNTRS "/memory_energy"),
	_ini(node_update_rate_path, _SYSFS_PM_CNTRS "/raw_scan_hz"),

	_ini(topology_path, _SYSFS_CPU "/cpu%lu/topology/%s"),

	_ini(ht_freq_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_cur_freq"),
	_ini(ht_freq_req_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_setspeed"),
	_ini(ht_freq_limit_min_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_min_freq"),
	_ini(ht_freq_limit_max_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_max_freq"),
	_ini(ht_freq_limit_list_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_available_frequencies"),
	_ini(ht_cstate_path, _SYSFS_CPU "/cpu%lu/cpuidle"),
	_ini(ht_cstate_limit_path, _SYSFS_CPU "/cpu%lu/cpuidle/state%lu/disable"),
	_ini(ht_governor_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_governor"),
		_ini(ht_governor_list_path, _SYSFS_CPU "/cpu%lu/cpufreq/scaling_available_governors"),

	_ini(msr_path, _SYSFS_CPU "/cpu%lu/msr/%xr"),
	_ini(msr_pkg_power_sku_unit_path, _SYSFS_CPU "/cpu%lu/msr/606r"),
	_ini(msr_pkg_rapl_perf_status_path, _SYSFS_CPU "/cpu%lu/msr/613r"),
	_ini(msr_ddr_rapl_perf_status_path, _SYSFS_CPU "/cpu%lu/msr/61br"),

	_ini(rapl_pkg_name_path, _SYSFS_RAPL "/intel-rapl:%lu/name"),
	_ini(rapl_sub_name_path, _SYSFS_RAPL "/intel-rapl:%lu/intel-rapl:%lu:%lu/name"),
	_ini(rapl_pkg_energy_path, _SYSFS_RAPL "/intel-rapl:%lu/energy_uj"),
	_ini(rapl_pkg_energy_max_path, _SYSFS_RAPL "/intel-rapl:%lu/max_energy_range_uj"),
	_ini(rapl_sub_energy_path, _SYSFS_RAPL "/intel-rapl:%lu/intel-rapl:%lu:%lu/energy_uj"),
	_ini(rapl_sub_energy_max_path, _SYSFS_RAPL "/intel-rapl:%lu/intel-rapl:%lu:%lu/max_energy_range_uj"),
	_ini(rapl_pkg_power_limit_path, _SYSFS_RAPL "/intel-rapl:%lu/constraint_0_power_limit_uw"),
	_ini(rapl_pkg_power_limit_max_path, _SYSFS_RAPL "/intel-rapl:%lu/constraint_0_max_power_uw"),
	_ini(rapl_sub_power_limit_path, _SYSFS_RAPL "/intel-rapl:%lu/intel-rapl:%lu:%lu/constraint_0_power_limit_uw"),
	_ini(rapl_sub_power_limit_max_path, _SYSFS_RAPL  "/intel-rapl:%lu/intel-rapl:%lu:%lu/constraint_0_max_power_uw"),
	_ini(rapl_pkg_time_window_path, _SYSFS_RAPL "/intel-rapl:%lu/constraint_0_time_window_us"),
	_ini(rapl_sub_time_window_path, _SYSFS_RAPL "/intel-rapl:%lu/intel-rapl:%lu:%lu/constraint_0_time_window_us"),


	.end_of_data = { NULL, NULL }
};


static int
x86_read_node_metadata(void)
{
	int status = PWR_RET_SUCCESS;
	char *cname = NULL;
	uint64_t nid = 0;

	TRACE2_ENTER("");

	// read the node cname and nid value
	status = read_string_from_file(PROCFS_CNAME, &cname, NULL);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Failed to read cname for node");
		goto status_return;
	}

	status = read_uint64_from_file(PROCFS_NID, &nid, NULL);
	if (status != PWR_RET_SUCCESS) {
		LOG_FAULT("Failed to read nid for node");
		goto status_return;
	}

	x86_metadata.node_vendor_info = g_strdup_printf("Cray Inc. "
			"node %s, nid%05lu", cname, nid);
	if (!x86_metadata.node_vendor_info) {
		LOG_FAULT("Failed to allocate vendor_info for node");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

status_return:
	g_free(cname);

	TRACE2_EXIT("status = %d", status);
	return status;
}

static int
x86_read_pm_counters_metadata(void)
{
	uint64_t ival = 0;
	int status = read_uint64_from_file(NODE_UPDATE_RATE_PATH, &ival, NULL);

	if (status) {
		LOG_FAULT("Failed to read sample rate from %s",
				NODE_UPDATE_RATE_PATH);
	} else {
		// Save the number of pm_counter updates per second,
		x86_metadata.pm_counters_update_rate = ival;

		// Save time between pm_counter updates in nanoseconds
		x86_metadata.pm_counters_time_window = NSEC_PER_SEC / ival;
	}

	return status;
}

static int
x86_construct_hierarchy(hierarchy_t *hierarchy)
{
	int status = PWR_RET_SUCCESS;
	node_t *node = NULL;
	pplane_t *pplane = NULL;
	GNode *root = NULL;
	uint64_t node_id = 0;
	uint64_t pplane_id = 0;

	TRACE2_ENTER("hierarchy = %p", hierarchy);

	if (hierarchy->tree != NULL || g_hash_table_size(hierarchy->map) != 0) {
		LOG_FAULT("Construct hierarhcy failed, tree was not empty");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// (re)set the metadata to zeros
	memset(&x86_metadata, 0, sizeof(x86_metadata));

	// Read in the pm_counters metadata so it is available to 
	// objects populating the hierarchy.
	status = x86_read_pm_counters_metadata();
	if (status) {
		LOG_FAULT("Failed to read pm_counters metadata");
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	// Add node object to the hierarchy tree. Since this should be the
	// first insert, it will be the root node.
	node_id = 0;
	node = new_node(node_id, "node.%lu", node_id);
	if (!node) {
		LOG_FAULT("Failed to alloc node.%.lu", node_id);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	status = hierarchy_insert(hierarchy, NULL, &node->obj);
	if (status) {
		LOG_FAULT("Failed to insert %s into hierarchy", node->obj.name);
		goto status_return;
	}
	x86_read_node_metadata();	// read node-specific metadata

	// Node inserted into hierarchy, drop the reference.
	root = node->obj.gnode;
	node = NULL;

	// Create the power planes to represent the node-wide
	// pm_counters for cpu and memory. They are inserted
	// as children of the node added in the above block, so the
	// names reflect the node. Each pplane object is given a
	// unique os_id for PWR_OBJ_POWER_PLANE objects.
	pplane_id = 0;
	pplane = new_pplane(pplane_id, "pm_counters.cpu.%lu", node_id);
	if (!pplane) {
		LOG_FAULT("Failed to alloc pm_counters.cpu.%lu", node_id);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	pplane->sub_type = PWR_OBJ_SOCKET;
	status = hierarchy_insert(hierarchy, root, &pplane->obj);
	if (status) {
		LOG_FAULT("Failed to insert %s into hierarchy",
				pplane->obj.name);
		goto status_return;
	}

	// Power Plane inserted into hierarchy, drop the reference.
	pplane = NULL;

	// Get next os_id for a power plane
	pplane_id = 1;
	pplane = new_pplane(pplane_id, "pm_counters.mem.%lu", node_id);
	if (!pplane) {
		LOG_FAULT("Failed to alloc pm_counters.mem.%lu", node_id);
		status = PWR_RET_FAILURE;
		goto status_return;
	}
	pplane->sub_type = PWR_OBJ_MEM;
	status = hierarchy_insert(hierarchy, root, &pplane->obj);
	if (status) {
		LOG_FAULT("Failed to insert %s into hierarchy",
				pplane->obj.name);
		goto status_return;
	}

	// Power Plane inserted into hierarchy, drop the reference.
	pplane = NULL;

	status = x86_read_hierarchy(hierarchy);

status_return:
	// If there was an error, clean up any objects not
	// inserted into the hierarchy.
	if (status) {
		if (node)
			del_node(node);
		if (pplane)
			del_pplane(pplane);
	}

	TRACE2_EXIT("status = %d", status);
	return status;
}

static int
x86_destruct_hierarchy(hierarchy_t *hierarchy)
{
	x86_metadata_t *md = &x86_metadata;

	TRACE2_ENTER("hierarchy = %p", hierarchy);

	// Clean up all memory allocations
	pwr_list_free_uint64(&md->ht_cstate);

	pwr_list_free_double(&md->ht_freq);

	pwr_list_free_string(&md->ht_gov);

	// Zero the struct
	memset(&x86_metadata, 0, sizeof(x86_metadata));

	TRACE2_EXIT("");

	return 0;
}

static int
x86_destruct_node(node_t *node)
{
	x86_del_node(node);

	node->ops = NULL;

	return 0;
}

static int
x86_construct_node(node_t *node)
{
	if (x86_new_node(node) != 0) {
		return 1;
	}

	node->ops = &x86_node_ops;

	return 0;
}

static int
x86_destruct_socket(socket_t *socket)
{
	x86_del_socket(socket);

	socket->ops = NULL;

	return 0;
}

static int
x86_construct_socket(socket_t *socket)
{
	int status = PWR_RET_SUCCESS;

	if (x86_new_socket(socket) != 0) {
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	socket->ops = &x86_socket_ops;

status_return:
	if (status) {
		if (socket)
			x86_destruct_socket(socket);
	}

	return status;
}

static int
x86_destruct_mem(mem_t *mem)
{
	x86_del_mem(mem);

	mem->ops = NULL;

	return 0;
}

static int
x86_construct_mem(mem_t *mem)
{
	int status = PWR_RET_SUCCESS;

	if (x86_new_mem(mem) != 0) {
		status = PWR_RET_FAILURE;
		goto status_return;
	}

	mem->ops = &x86_mem_ops;

status_return:
	if (status) {
		if (mem)
			x86_destruct_mem(mem);
	}

	return status;
}

static int
x86_destruct_pplane(pplane_t *pplane)
{
	x86_del_pplane(pplane);

	pplane->ops = NULL;

	return 0;
}

static int
x86_construct_pplane(pplane_t *pplane)
{
	if (x86_new_pplane(pplane) != 0) {
		return 1;
	}

	pplane->ops = &x86_pplane_ops;

	return 0;
}

static int
x86_destruct_core(core_t *core)
{
	x86_del_core(core);

	core->ops = NULL;

	return 0;
}

static int
x86_construct_core(core_t *core)
{
	if (x86_new_core(core) != 0) {
		return 1;
	}

	core->ops = &x86_core_ops;

	return 0;
}

static int
x86_destruct_ht(ht_t *ht)
{
	x86_del_ht(ht);

	ht->ops = NULL;

	return 0;
}

static int
x86_construct_ht(ht_t *ht)
{
	if (x86_new_ht(ht) != 0) {
		return 1;
	}

	ht->ops = &x86_ht_ops;

	return 0;
}

int
x86_construct_plugin(plugin_t *plugin)
{
	plugin->sysfile_catalog = (sysentry_t *)&x86_sysfile_catalog;

	plugin->construct_hierarchy = x86_construct_hierarchy;
	plugin->destruct_hierarchy = x86_destruct_hierarchy;

	plugin->construct_node = x86_construct_node;
	plugin->destruct_node = x86_destruct_node;

	plugin->construct_socket = x86_construct_socket;
	plugin->destruct_socket = x86_destruct_socket;

	plugin->construct_mem = x86_construct_mem;
	plugin->destruct_mem = x86_destruct_mem;

	plugin->construct_pplane = x86_construct_pplane;
	plugin->destruct_pplane = x86_destruct_pplane;

	plugin->construct_core = x86_construct_core;
	plugin->destruct_core = x86_destruct_core;

	plugin->construct_ht = x86_construct_ht;
	plugin->destruct_ht = x86_destruct_ht;

	return 0;
}
