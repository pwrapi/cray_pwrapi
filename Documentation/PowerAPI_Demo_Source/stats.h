/*
 * Copyright (c) 2017 Cray Inc. All rights reserved.
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
 */

#include <cray-powerapi/api.h>

#ifndef __STATS_H
#define __STATS_H

// convenience enum of supported stats
enum { stat_min = 0, stat_max, stat_avg, stat_stdev, stat_MAX };

// convenience map from the enum of supported stats to PWR_ATTR_STAT_*
// analogues.
static PWR_AttrStat map_stat[stat_MAX] = {
    PWR_ATTR_STAT_MIN,
    PWR_ATTR_STAT_MAX,
    PWR_ATTR_STAT_AVG,
    PWR_ATTR_STAT_STDEV
};

// data structure to hold all workload-level stats
typedef struct run_stats_s {
	PWR_Stat node_pwr[stat_MAX];
	PWR_Stat socket_pwr[stat_MAX];
	PWR_Stat socket_energy[stat_MAX];
	PWR_Stat socket_temp[stat_MAX];
	PWR_Stat mem_pwr[stat_MAX];
	PWR_Stat mem_energy[stat_MAX];
	PWR_Stat core_temp[stat_MAX];
	int socket_cnt;
	int mem_cnt;
	int core_cnt;
} run_stats_type;

int start_stats(run_stats_type *s);
int stop_stats(run_stats_type *s);
int init_stats(PWR_Cntxt *ctx, PWR_Grp *grps, run_stats_type *s);
int cleanup_stats(run_stats_type *s);

#endif /* ifndef __STATS_H */