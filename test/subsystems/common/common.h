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
 */

#ifndef __TEST_LIB_COMMON_H
#define __TEST_LIB_COMMON_H

#include <cray-powerapi/api.h>

/*
 * EC_* - Common test exit codes.
 */
#define EC_SUCCESS			0
#define EC_CNTXT_CREATE			1
#define EC_CNTXT_DESTROY		2
#define EC_CNTXT_GET_ENTRY_POINT	3
#define EC_CNTXT_GET_GROUP_BY_NAME	4
#define EC_OBJ_GET_TYPE			10
#define EC_OBJ_GET_NAME			11
#define EC_OBJ_ATTR_GET_VALUE		12
#define EC_OBJ_ATTR_SET_VALUE		13
#define EC_OBJ_GET_CHILDREN		14
#define EC_OBJ_TYPE_COMPARE		15
#define EC_GROUP_CREATE			20
#define EC_GROUP_DESTROY		21
#define EC_GROUP_ADD			22
#define EC_GROUP_REMOVE			23
#define EC_GROUP_GET_NUM_OBJS		24
#define EC_GROUP_GET_OBJ_BY_IDX		25
#define EC_GROUP_NUM_OBJS_COMPARE	26
#define EC_GROUP_DUPLICATE		27
#define EC_GROUP_UNION			28
#define EC_GROUP_INTERSECTION		29
#define EC_GROUP_DIFFERENCE		30
#define EC_GROUP_SYM_DIFFERENCE		31
#define EC_STAT_CREATE_OBJ		32
#define EC_STAT_CREATE_GRP		33
#define EC_STAT_DESTROY			34
#define EC_STAT_START			35
#define EC_STAT_STOP			36
#define EC_STAT_CLEAR			37
#define EC_STAT_GET_VALUE		38
#define EC_STAT_GET_VALUES		39
#define EC_STAT_GET_REDUCE		40
#define EC_NO_HT_OBJ			41
#define EC_CNTXT_GET_OBJ_BY_NAME	42
#define	EC_HINT_CREATE			43
#define	EC_HINT_DESTROY			44
#define	EC_HINT_START			45
#define	EC_HINT_STOP			46
#define	EC_HINT_PROGRESS		47
#define	EC_HINT_LOGERROR		48
#define EC_APPOS_GET_SLEEP_STATE	49
#define EC_APPOS_SET_SLEEP_STATE_LIMIT	50
#define EC_APPOS_WAKEUP_LATENCY		51
#define EC_APPOS_RECOMMEND_SLEEP_STATE	52
#define EC_APPOS_GET_PERF_STATE		53
#define EC_APPOS_SET_PERF_STATE		54

#define EC_TEST_UNIQUE_START		64	// unique exit codes start here

void check_int_equal(int value, int expected, int exit_code);
void check_int_greater_than(int value, int target, int exit_code);
void check_int_greater_than_equal(int value, int target, int exit_code);
void check_double_equal(double value, double expected, int exit_code);
void check_double_greater_than(double value, double target, int exit_code);
void check_double_greater_than_equal(double value, double target, int exit_code);

void find_objects_of_type(PWR_Obj parent, PWR_ObjType find_type,
		PWR_Grp result);
void get_socket_obj(PWR_Cntxt ctx, PWR_Obj entry, PWR_Obj *obj);
void get_ht_obj(PWR_Cntxt ctx, PWR_Obj entry, PWR_Obj *obj);

void TST_CntxtInit(PWR_CntxtType type, PWR_Role role, const char *name,
		PWR_Cntxt *ctx, int expected_retval);
void TST_CntxtDestroy(PWR_Cntxt *ctx, int expected_retval);
void TST_CntxtGetEntryPoint(PWR_Cntxt ctx, PWR_Obj *entry_point,
		int expected_retval);

void TST_ObjGetType(PWR_Obj object, PWR_ObjType *type, int expected_retval);
void TST_ObjGetName(PWR_Obj object, char *buf, size_t len,
		int expected_retval);
void TST_ObjAttrGetValue(PWR_Obj object, PWR_AttrName attr, void *value,
		PWR_Time *ts, int expected_retval);
void TST_ObjAttrSetValue(PWR_Obj object, PWR_AttrName attr, const void *value,
		int expected_retval);
void TST_ObjGetChildren(PWR_Obj object, PWR_Grp *group, int expected_retval);

void TST_GrpCreate(PWR_Cntxt context, PWR_Grp *group, int expected_retval);
void TST_GrpDestroy(PWR_Grp group, int expected_retval);
void TST_GrpDuplicate(PWR_Grp group1, PWR_Grp *group2, int expected_retval);
void TST_GrpAddObj(PWR_Grp group, PWR_Obj object, int expected_retval);
void TST_GrpRemoveObj(PWR_Grp group, PWR_Obj object, int expected_retval);
void TST_GrpGetNumObjs(PWR_Grp group, unsigned int *num_objects,
		int expected_retval);
void TST_GrpGetObjByIndx(PWR_Grp group, int idx, PWR_Obj *object,
		int expected_retval);
void TST_CntxtGetGrpByName(PWR_Cntxt context, const char *name,
	 		   PWR_Grp *group, int expected_retval);
void TST_GrpUnion(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
		  int expected_retval);
void TST_GrpIntersection(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
			 int expected_retval);
void TST_GrpDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
		       int expected_retval);
void TST_GrpSymDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
			  int expected_retval);
void TST_StatCreateObj(PWR_Obj object, PWR_AttrName name,
		       PWR_AttrStat statistic, PWR_Stat *stat,
		       int expected_retval);
void TST_StatCreateGrp(PWR_Grp group, PWR_AttrName name,
		       PWR_AttrStat statistic, PWR_Stat *stat,
		       int expected_retval);
void TST_StatDestroy(PWR_Stat stat, int expected_retval);
void TST_StatStart(PWR_Stat stat, int expected_retval);
void TST_StatStop(PWR_Stat stat, int expected_retval);
void TST_StatClear(PWR_Stat stat, int expected_retval);
void TST_StatGetValue(PWR_Stat stat, double *value, PWR_TimePeriod *times,
		      int expected_retval);
void TST_StatGetValues(PWR_Stat stat, double value[], PWR_TimePeriod times[],
		       int expected_retval);
void TST_StatGetReduce(PWR_Stat stat, PWR_AttrStat reduceOp, int *index,
		       double *value, PWR_Time *time, int expected_retval);
void TST_CntxtGetObjByName(PWR_Cntxt context, const char *name, PWR_Obj *objectp,
		int expected_retval);
void TST_AppHintCreate(PWR_Obj object, const char *name, uint64_t *hintidp,
		PWR_RegionHint hint, PWR_RegionIntensity level, int expected_retval);
void TST_AppHintDestroy(uint64_t hintid, int expected_retval);
void TST_AppHintStart(uint64_t hintid, int expected_retval);
void TST_AppHintStop(uint64_t hintid, int expected_retval);
void TST_AppHintProgress(uint64_t hintid, double progress, int expected_retval);
void TST_GetSleepState(PWR_Obj obj, PWR_SleepState *state, int expected_retval);
void TST_SetSleepStateLimit(PWR_Obj obj, PWR_SleepState state,
		int expected_retval);
void TST_WakeUpLatency(PWR_Obj obj, PWR_SleepState state, PWR_Time *latency,
		int expected_retval);
void TST_RecommendSleepState(PWR_Obj obj, PWR_Time latency,
		PWR_SleepState *state, int expected_retval);
void TST_GetPerfState(PWR_Obj obj, PWR_PerfState *state, int expected_retval);
void TST_SetPerfState(PWR_Obj obj, PWR_PerfState state, int expected_retval);

#endif /* ifndef __TEST_LIB_COMMON_H */
