/*
 * Copyright (c) 2017, Cray Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stats.h"

//
// create_grp_stat_array - Initialize PWR_Stat array for a given group. If the
//                         group is of size 1, then create an object stat for
//                         that single object (necessary for avoiding NAN 
//                         standard deviations)
//
// Argument(s):
//
//	arr     - pointer to array of PWR_Stat of length stat_MAX
//	my_grp  - the group pertaining to the stat
//	my_attr - the attribute of the stat
//	grp_sz  - the size of the group my_grp
//
// Return Code(s):
//
//	int
//
static int create_grp_stat_array(PWR_Stat *arr, PWR_Grp my_grp,
				 PWR_AttrName my_attr, int grp_sz)
{
	int i = 0;
	int ret = 0;
	PWR_Obj my_obj;
	if (grp_sz == 1) {
		// Get the only object in the group
		ret = PWR_GrpGetObjByIndx(my_grp, 0, &my_obj);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_GrpGetObjByIndx() failed: %d\n",
				__func__, ret);
			return -1;
		}
	}
	for (i = 0; i < stat_MAX; i++) {
		if (grp_sz > 1) {
			ret = PWR_GrpCreateStat(my_grp, my_attr, map_stat[i], &arr[i]);
		} else {
			ret = PWR_ObjCreateStat(my_obj, my_attr, map_stat[i], &arr[i]);
		}
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_GrpCreateStat() failed for "
				"PWR_AttrStat %i: %i\n",
				__func__, map_stat[i], ret);
			return -1;
		}
	}
	return ret;
}

//
// init_stats - Initialize PWR_Stat arrays
//
// Argument(s):
//
//	ctx  - pointer tp the PWR_Cntxt to use
//	grps - a pointer to an array of groups of length PWR_NUM_OBJ_TYPES
//	s    - pointer to run stats data
//
// Return Code(s):
//
//	int
//
int init_stats(PWR_Cntxt *ctx, PWR_Grp *grps, run_stats_type *s)
{
	int i = 0;
	int ret = 0;
	PWR_Obj obj;

	// Init node stats (get node object and create)
	ret = PWR_CntxtGetObjByName(*ctx, "node.0", &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_CntxtGetObjByName failed: %i\n",
			__func__, ret);
		return -1;
	}
	for (i = 0; i < stat_MAX; i++) {
		ret = PWR_ObjCreateStat(obj, PWR_ATTR_POWER, map_stat[i],
					&s->node_pwr[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_ObjCreateStat() failed for "
				"PWR_AttrStat %i: %i\n",
				__func__, map_stat[i], ret);
			return -1;
		}
	}

	if ((s->socket_cnt = PWR_GrpGetNumObjs(grps[PWR_OBJ_SOCKET])) == PWR_RET_FAILURE) {
		fprintf(stderr,
			"%s: PWR_GrpGetNumObjs() failed for group ObjType %i\n",
			__func__, PWR_OBJ_SOCKET);
		return -1;
	}
	if ((s->mem_cnt = PWR_GrpGetNumObjs(grps[PWR_OBJ_MEM])) == PWR_RET_FAILURE) {
		fprintf(stderr,
			"%s: PWR_GrpGetNumObjs() failed for group ObjType %i\n",
			__func__, PWR_OBJ_MEM);
		return -1;
	}
	if ((s->core_cnt = PWR_GrpGetNumObjs(grps[PWR_OBJ_CORE])) == PWR_RET_FAILURE) {
		fprintf(stderr,
			"%s: PWR_GrpGetNumObjs() failed for group ObjType %i\n",
			__func__, PWR_OBJ_CORE);
		return -1;
	}

	// Init group stats
	if (create_grp_stat_array(s->socket_pwr, grps[PWR_OBJ_SOCKET],
				  PWR_ATTR_POWER, s->socket_cnt)) {
		fprintf(stderr,
			"%s: Failed to create socket power PWR_Stats\n");
		ret = -1;
	}
	if (create_grp_stat_array(s->socket_energy, grps[PWR_OBJ_SOCKET],
				  PWR_ATTR_ENERGY, s->socket_cnt)) {
		fprintf(stderr,
			"%s: Failed to create socket energy PWR_Stats\n");
		ret = -1;
	}
	if (create_grp_stat_array(s->socket_temp, grps[PWR_OBJ_SOCKET],
				  PWR_ATTR_TEMP, s->socket_cnt)) {
		fprintf(stderr, "%s: Failed to create socket temp PWR_Stats\n");
		ret = -1;
	}
	if (create_grp_stat_array(s->mem_pwr, grps[PWR_OBJ_MEM],
				  PWR_ATTR_POWER, s->mem_cnt)) {
		fprintf(stderr, "%s: Failed to create mem power PWR_Stats\n");
		ret = -1;
	}
	if (create_grp_stat_array(s->mem_energy, grps[PWR_OBJ_MEM],
				  PWR_ATTR_ENERGY, s->mem_cnt)) {
		fprintf(stderr, "%s: Failed to create mem energy PWR_Stats\n");
		ret = -1;
	}
	if (create_grp_stat_array(s->core_temp, grps[PWR_OBJ_CORE],
				  PWR_ATTR_TEMP, s->core_cnt)) {
		fprintf(stderr, "%s: Failed to create core temp PWR_Stats\n");
		ret = -1;
	}
	return ret;
}

//
// start_stat_array - Start all stats in a given PWR_Stat array
//
// Argument(s):
//
//	arr     - pointer to array of PWR_Stat of length stat_MAX
//
// Return Code(s):
//
//	int
//
static int start_stat_array(PWR_Stat *arr)
{
	int i, ret;
	for (i = 0; i < stat_MAX; i++) {
		ret = PWR_StatStart(arr[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatStart() failed: %i\n",
				__func__, ret);
			return -1;
		}
	}
	return ret;
}

//
// start_stats - Start all stats in a run_stats_type data structure
//
// Argument(s):
//
//	s - pointer to run stats data
//
// Return Code(s):
//
//	int
//
int start_stats(run_stats_type *s)
{
	int ret = 0;
	if (start_stat_array(s->node_pwr)) {
		fprintf(stderr, "%s: Failed to start node power PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->socket_pwr)) {
		fprintf(stderr, "%s: Failed to start socket power PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->socket_energy)) {
		fprintf(stderr,
			"%s: Failed to start socket energy PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->socket_temp)) {
		fprintf(stderr, "%s: Failed to start socket temp PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->mem_pwr)) {
		fprintf(stderr, "%s: Failed to start mem power PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->mem_energy)) {
		fprintf(stderr, "%s: Failed to start mem energy PWR_Stats\n");
		ret = -1;
	}
	if (start_stat_array(s->core_temp)) {
		fprintf(stderr, "%s: Failed to start core temp PWR_Stats\n");
		ret = -1;
	}
	return ret;
}

//
// stop_stat_array - Stop all stats in a given PWR_Stat array
//
// Argument(s):
//
//	arr     - pointer to array of PWR_Stat of length stat_MAX
//
// Return Code(s):
//
//	int
//
static int stop_stat_array(PWR_Stat *arr)
{
	int i, ret;
	for (i = 0; i < stat_MAX; i++) {
		ret = PWR_StatStop(arr[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatStop() failed: %i\n",
				__func__, ret);
			return -1;
		}
	}
	return ret;
}

//
// stop_stats - Stop all stats in a run_stats_type data structure
//
// Argument(s):
//
//	s - pointer to run stats data
//
// Return Code(s):
//
//	int
//
int stop_stats(run_stats_type *s)
{
	int ret = 0;
	if (stop_stat_array(s->node_pwr)) {
		fprintf(stderr, "%s: Failed to stop node power PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->socket_pwr)) {
		fprintf(stderr, "%s: Failed to stop socket power PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->socket_energy)) {
		fprintf(stderr, "%s: Failed to stop socket energy PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->socket_temp)) {
		fprintf(stderr, "%s: Failed to stop socket temp PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->mem_pwr)) {
		fprintf(stderr, "%s: Failed to stop mem power PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->mem_energy)) {
		fprintf(stderr, "%s: Failed to stop mem energy PWR_Stats\n");
		ret = -1;
	}
	if (stop_stat_array(s->core_temp)) {
		fprintf(stderr, "%s: Failed to stop core temp PWR_Stats\n");
		ret = -1;
	}
	return ret;
}

//
// cleanup_stat_array - Cleanup/destroy all stats in a given PWR_Stat array
//
// Argument(s):
//
//	arr     - pointer to array of PWR_Stat of length stat_MAX
//
// Return Code(s):
//
//	int
//
static int cleanup_stat_array(PWR_Stat *arr)
{
	int i, ret;
	for (i = 0; i < stat_MAX; i++) {
		ret = PWR_StatDestroy(arr[i]);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatDestroy() failed: %i\n",
				__func__, ret);
			return -1;
		}
	}
	return ret;
}

//
// cleanup_stats - Cleanup/destroy all stats in a run_stats_type data structure
//
// Argument(s):
//
//	s - pointer to run stats data
//
// Return Code(s):
//
//	int
//
int cleanup_stats(run_stats_type *s)
{
	int ret = 0;
	if (cleanup_stat_array(s->node_pwr)) {
		fprintf(stderr, "%s: Failed to cleanup node power PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->socket_pwr)) {
		fprintf(stderr,
			"%s: Failed to cleanup socket power PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->socket_energy)) {
		fprintf(stderr,
			"%s: Failed to cleanup socket energy PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->socket_temp)) {
		fprintf(stderr,
			"%s: Failed to cleanup socket temp PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->mem_pwr)) {
		fprintf(stderr, "%s: Failed to cleanup mem power PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->mem_energy)) {
		fprintf(stderr, "%s: Failed to cleanup mem energy PWR_Stats\n");
		ret = -1;
	}
	if (cleanup_stat_array(s->core_temp)) {
		fprintf(stderr, "%s: Failed to cleanup core temp PWR_Stats\n");
		ret = -1;
	}
	return ret;
}
