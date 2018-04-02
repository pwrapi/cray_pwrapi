/*
 * Copyright (c) 2016-2017, Cray Inc.
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
 */

#ifndef _PWR_PLUGINS_COMMON_PATHS_H
#define _PWR_PLUGINS_COMMON_PATHS_H

#include <plugin.h>
#include <utility.h>

typedef struct {
	sysentry_t num_cstates_path;
	sysentry_t cstate_limit_path;
	sysentry_t cstate_latency_path;                 // %d=statenum
	sysentry_t avail_freqs_path;
	sysentry_t curr_freq_path;
	sysentry_t max_freq_path;
	sysentry_t min_freq_path;
	sysentry_t kernel_max_path;
	sysentry_t cpu_possible_path;
	sysentry_t cpu_present_path;
	sysentry_t cpu_online_path;
} sysfiles_t;

#define SYSFILES		((sysfiles_t *)plugin->sysfile_catalog)

#define NUM_CSTATES_PATH	SYSFILES->num_cstates_path.val
#define CSTATE_LIMIT_PATH	SYSFILES->cstate_limit_path.val
#define CSTATE_LATENCY_PATH	SYSFILES->cstate_latency_path.val

#define AVAIL_FREQS_PATH	SYSFILES->avail_freqs_path.val
#define CUR_FREQ_PATH		SYSFILES->curr_freq_path.val
#define MAX_FREQ_PATH		SYSFILES->max_freq_path.val
#define MIN_FREQ_PATH		SYSFILES->min_freq_path.val
#define KERNEL_MAX_PATH		SYSFILES->kernel_max_path.val
#define CPU_POSSIBLE_PATH	SYSFILES->cpu_possible_path.val
#define CPU_PRESENT_PATH	SYSFILES->cpu_present_path.val
#define CPU_ONLINE_PATH		SYSFILES->cpu_online_path.val

#endif /* _PWR_PLUGINS_COMMON_PATHS_H */


