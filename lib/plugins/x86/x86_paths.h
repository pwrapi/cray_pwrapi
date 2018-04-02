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
 */

//
// This file contains interfaces to construct/destruct the plugin
// specific parts of the various power object types.
//
#ifndef _X86_PATHS_H
#define _X86_PATHS_H

#include "../common/paths.h"

typedef struct {
	// Generic (all-architecture) element
	sysfiles_t hdr;

	// Paths to procfs data
	sysentry_t procfs_cpuinfo;
	sysentry_t procfs_cname;
	sysentry_t procfs_nid;

	// Directory paths which can be useful for testing
	sysentry_t sysfs_kernel;
	sysentry_t sysfs_cpu;
	sysentry_t sysfs_pm_cntrs;
	sysentry_t sysfs_rapl;
	sysentry_t sysfs_hwmon;

	// Node paths
	sysentry_t node_power_path;
	sysentry_t node_power_cap_path;
	sysentry_t node_energy_path;
	sysentry_t node_cpu_power_path;
	sysentry_t node_cpu_energy_path;
	sysentry_t node_mem_power_path;
	sysentry_t node_mem_energy_path;
	sysentry_t node_update_rate_path;

	// Topology path
	sysentry_t topology_path;                       // %lu=cpunum, %s=subpath

	// HT paths
	sysentry_t ht_freq_path;                        // %lu=cpunum
	sysentry_t ht_freq_req_path;                    // %lu=cpunum
	sysentry_t ht_freq_limit_min_path;              // %lu=cpunum
	sysentry_t ht_freq_limit_max_path;              // %lu=cpunum
	sysentry_t ht_freq_limit_list_path;             // %lu=cpunum
	sysentry_t ht_cstate_path;                      // %lu=cpunum
	sysentry_t ht_cstate_limit_path;                // %lu=cpunum, %lu=statenum
	sysentry_t ht_governor_path;                    // %lu=cpunum
	sysentry_t ht_governor_list_path;               // %lu=cpunum

	// MSR paths
	sysentry_t msr_path;                            // %lu=cpunum, %x=register
	sysentry_t msr_pkg_power_sku_unit_path;         // %lu=cpunum
	sysentry_t msr_pkg_rapl_perf_status_path;       // %lu=cpunum
	sysentry_t msr_ddr_rapl_perf_status_path;       // %lu=cpunum

	// RAPL paths
	sysentry_t rapl_pkg_name_path;                  // %lu=raplid
	sysentry_t rapl_sub_name_path;                  // %lu=raplid, %lu=intelid, %ld=intelsub
	sysentry_t rapl_pkg_energy_path;                // %lu=raplid
	sysentry_t rapl_pkg_energy_max_path;            // %lu=raplid
	sysentry_t rapl_sub_energy_path;                // %lu=raplid, %lu=intelid, %ld=intelsub
	sysentry_t rapl_sub_energy_max_path;            // %lu=raplid
	sysentry_t rapl_pkg_power_limit_path;           // %lu=raplid
	sysentry_t rapl_pkg_power_limit_max_path;       // %lu=raplid
	sysentry_t rapl_sub_power_limit_path;           // %lu=raplid, %lu=intelid, %ld=intelsub
	sysentry_t rapl_sub_power_limit_max_path;       // %lu=raplid, %lu=intelid, %ld=intelsub
	sysentry_t rapl_pkg_time_window_path;           // %lu=raplid
	sysentry_t rapl_sub_time_window_path;           // %lu=raplid, %lu=intelid, %ld=intelsub

	// Required final entry
	sysentry_t end_of_data;
} x86_sysfiles_t;

#define X86_SYSFILES	((x86_sysfiles_t *)plugin->sysfile_catalog)

#define PROCFS_CPUINFO			X86_SYSFILES->procfs_cpuinfo.val
#define PROCFS_CNAME			X86_SYSFILES->procfs_cname.val
#define PROCFS_NID			X86_SYSFILES->procfs_nid.val

#define SYSFS_KERNEL			X86_SYSFILES->sysfs_kernel.val
#define SYSFS_CPU			X86_SYSFILES->sysfs_cpu.val
#define SYSFS_PM_CNTRS			X86_SYSFILES->sysfs_pm_cntrs.val
#define SYSFS_RAPL			X86_SYSFILES->sysfs_rapl.val
#define SYSFS_HWMON			X86_SYSFILES->sysfs_hwmon.val

#define NODE_POWER_PATH			X86_SYSFILES->node_power_path.val
#define NODE_POWER_CAP_PATH		X86_SYSFILES->node_power_cap_path.val
#define NODE_ENERGY_PATH		X86_SYSFILES->node_energy_path.val
#define NODE_CPU_POWER_PATH		X86_SYSFILES->node_cpu_power_path.val
#define NODE_CPU_ENERGY_PATH		X86_SYSFILES->node_cpu_energy_path.val
#define NODE_MEM_POWER_PATH		X86_SYSFILES->node_mem_power_path.val
#define NODE_MEM_ENERGY_PATH		X86_SYSFILES->node_mem_energy_path.val
#define NODE_UPDATE_RATE_PATH           X86_SYSFILES->node_update_rate_path.val

#define TOPOLOGY_PATH			X86_SYSFILES->topology_path.val

#define HT_FREQ_PATH			X86_SYSFILES->ht_freq_path.val
#define HT_FREQ_REQ_PATH		X86_SYSFILES->ht_freq_req_path.val
#define HT_FREQ_LIMIT_MIN_PATH		X86_SYSFILES->ht_freq_limit_min_path.val
#define HT_FREQ_LIMIT_MAX_PATH		X86_SYSFILES->ht_freq_limit_max_path.val
#define HT_FREQ_LIMIT_LIST_PATH		X86_SYSFILES->ht_freq_limit_list_path.val
#define HT_CSTATE_PATH			X86_SYSFILES->ht_cstate_path.val
#define HT_CSTATE_LIMIT_PATH		X86_SYSFILES->ht_cstate_limit_path.val
#define HT_GOVERNOR_PATH		X86_SYSFILES->ht_governor_path.val
#define HT_GOVERNOR_LIST_PATH		X86_SYSFILES->ht_governor_list_path.val

#define MSR_PATH			X86_SYSFILES->msr_path.val
#define MSR_PKG_POWER_SKU_UNIT_PATH	X86_SYSFILES->msr_pkg_power_sku_unit_path.val
#define MSR_PKG_RAPL_PERF_STATUS_PATH	X86_SYSFILES->msr_pkg_rapl_perf_status_path.val
#define MSR_DDR_RAPL_PERF_STATUS_PATH	X86_SYSFILES->msr_ddr_rapl_perf_status_path.val

#define RAPL_PKG_NAME_PATH		X86_SYSFILES->rapl_pkg_name_path.val
#define RAPL_SUB_NAME_PATH		X86_SYSFILES->rapl_sub_name_path.val
#define RAPL_PKG_ENERGY_PATH		X86_SYSFILES->rapl_pkg_energy_path.val
#define RAPL_PKG_ENERGY_MAX_PATH	X86_SYSFILES->rapl_pkg_energy_max_path.val
#define RAPL_SUB_ENERGY_PATH		X86_SYSFILES->rapl_sub_energy_path.val
#define RAPL_SUB_ENERGY_MAX_PATH	X86_SYSFILES->rapl_sub_energy_max_path.val
#define RAPL_PKG_POWER_LIMIT_PATH	X86_SYSFILES->rapl_pkg_power_limit_path.val
#define RAPL_PKG_POWER_LIMIT_MAX_PATH	X86_SYSFILES->rapl_pkg_power_limit_max_path.val
#define RAPL_SUB_POWER_LIMIT_PATH	X86_SYSFILES->rapl_sub_power_limit_path.val
#define RAPL_SUB_POWER_LIMIT_MAX_PATH	X86_SYSFILES->rapl_sub_power_limit_max_path.val
#define RAPL_PKG_TIME_WINDOW_PATH	X86_SYSFILES->rapl_pkg_time_window_path.val
#define RAPL_SUB_TIME_WINDOW_PATH	X86_SYSFILES->rapl_sub_time_window_path.val

#endif	// _X86_PATHS_H

