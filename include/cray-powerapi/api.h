/*
 * Copyright (c) 2015-2016, 2018, Cray Inc.
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
 * This file contains base PM API function prototypes.
 */

#ifndef _PWR_API_H
#define _PWR_API_H

#include <time.h>

#include <cray-powerapi/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Functions to get the major and minor portions of the specification version.
 */
int          PWR_GetMajorVersion(void);
int          PWR_GetMinorVersion(void);

/*
 * Initialization Functions.
 */
int          PWR_CntxtInit(PWR_CntxtType, PWR_Role, const char *, PWR_Cntxt *);
int          PWR_CntxtDestroy(PWR_Cntxt);

/*
 * Hierarchy Navigation Functions.
 */
int          PWR_CntxtGetEntryPoint(PWR_Cntxt, PWR_Obj *);
int          PWR_ObjGetType(PWR_Obj, PWR_ObjType *);
int          PWR_ObjGetName(PWR_Obj, char *, size_t);
int          PWR_ObjGetParent(PWR_Obj, PWR_Obj *);
int          PWR_ObjGetChildren(PWR_Obj, PWR_Grp *);
int          PWR_CntxtGetObjByName(PWR_Cntxt, const char *, PWR_Obj *);

/*
 * Group Functions.
 */
int          PWR_GrpCreate(PWR_Cntxt, PWR_Grp *);
int          PWR_GrpDestroy(PWR_Grp);
int          PWR_GrpAddObj(PWR_Grp, PWR_Obj);
int          PWR_GrpRemoveObj(PWR_Grp, PWR_Obj);
int          PWR_GrpGetNumObjs(PWR_Grp);
int          PWR_GrpGetObjByIndx(PWR_Grp, int, PWR_Obj *);
int          PWR_GrpDuplicate(PWR_Grp, PWR_Grp *);
int          PWR_GrpUnion(PWR_Grp, PWR_Grp, PWR_Grp *);
int          PWR_GrpIntersection(PWR_Grp, PWR_Grp, PWR_Grp *);
int          PWR_GrpDifference(PWR_Grp, PWR_Grp, PWR_Grp *);
int          PWR_GrpSymDifference(PWR_Grp, PWR_Grp, PWR_Grp *);
int          PWR_CntxtGetGrpByName(PWR_Cntxt, const char *, PWR_Grp *);

#define CRAY_NAMED_GRP_HTS     "all_ht"
#define CRAY_NAMED_GRP_CORES   "all_core"
#define CRAY_NAMED_GRP_SOCKETS "all_socket"
#define CRAY_NAMED_GRP_MEMS    "all_mem"

/*
 * Attribute Functions.
 */
int          PWR_ObjAttrGetValue(PWR_Obj, PWR_AttrName, void *, PWR_Time *);
int          PWR_ObjAttrSetValue(PWR_Obj, PWR_AttrName, const void *);
int          PWR_ObjAttrGetValues(PWR_Obj, int, const PWR_AttrName[],
                                  void *, PWR_Time[], PWR_Status);
int          PWR_ObjAttrSetValues(PWR_Obj, int, const PWR_AttrName[],
                                  const void *, PWR_Status);
int          PWR_ObjAttrIsValid(PWR_Obj, PWR_AttrName);

int          PWR_GrpAttrGetValue(PWR_Grp, PWR_AttrName, void *, PWR_Time[],
                                 PWR_Status);
int          PWR_GrpAttrSetValue(PWR_Grp, PWR_AttrName, const void *,
                                 PWR_Status);
int          PWR_GrpAttrGetValues(PWR_Grp, int, const PWR_AttrName[],
                                  void *, PWR_Time[], PWR_Status);
int          PWR_GrpAttrSetValues(PWR_Grp, int, const PWR_AttrName[],
                                  const void *, PWR_Status);

/*
 * Metadata Functions.
 */
int          PWR_ObjAttrGetMeta(PWR_Obj, PWR_AttrName, PWR_MetaName, void *);
int          PWR_ObjAttrSetMeta(PWR_Obj, PWR_AttrName, PWR_MetaName,
                                const void *);
int          PWR_MetaValueAtIndex(PWR_Obj, PWR_AttrName, unsigned int, void *,
                                  char *);

/*
 * Status Functions.
 */
int          PWR_StatusCreate(PWR_Cntxt, PWR_Status *);
int          PWR_StatusDestroy(PWR_Status);
int          PWR_StatusPopError(PWR_Status, PWR_AttrAccessError *);

/*
 * Statistics Functions.
 */
int          PWR_ObjGetStat(PWR_Obj, PWR_AttrName, PWR_AttrStat,
                            PWR_TimePeriod *, double *);
int          PWR_GrpGetStats(PWR_Grp, PWR_AttrName, PWR_AttrStat,
                             PWR_TimePeriod *, double[], PWR_TimePeriod[]);
int          PWR_ObjCreateStat(PWR_Obj, PWR_AttrName, PWR_AttrStat, PWR_Stat *);
int          PWR_GrpCreateStat(PWR_Grp, PWR_AttrName, PWR_AttrStat, PWR_Stat *);
int          PWR_StatDestroy(PWR_Stat);
int          PWR_StatStart(PWR_Stat);
int          PWR_StatStop(PWR_Stat);
int          PWR_StatClear(PWR_Stat);
int          PWR_StatGetValue(PWR_Stat, double *, PWR_TimePeriod *);
int          PWR_StatGetValues(PWR_Stat, double[], PWR_TimePeriod[]);
int          PWR_StatGetReduce(PWR_Stat, PWR_AttrStat, int *, double *,
                               PWR_Time *);
int          PWR_GrpGetReduce(PWR_Grp, PWR_AttrName, PWR_AttrStat, PWR_AttrStat,
                              PWR_TimePeriod, int *, double *,
                              PWR_TimePeriod *);

/*
 * Report Functions.
 */
int          PWR_GetReportByID(PWR_Cntxt, const char *, PWR_ID, PWR_AttrName,
                               PWR_AttrStat, double *, PWR_TimePeriod *);

/*
 * Operating System, Hardware Interface.
 */
int          PWR_StateTransitDelay(PWR_Obj, PWR_OperState, PWR_OperState,
                                   PWR_Time *);

/*
 * Monitor and Control, Hardware Interface.
 */

/*
 * Application, Operating System Interface.
 */
int          PWR_AppHintCreate(PWR_Obj, const char *, uint64_t *,
                               PWR_RegionHint, PWR_RegionIntensity);
int          PWR_AppHintDestroy(uint64_t);
int          PWR_AppHintStart(uint64_t);
int          PWR_AppHintStop(uint64_t);
int          PWR_AppHintProgress(uint64_t, double);

int          PWR_GetSleepState(PWR_Obj, PWR_SleepState *);
int          PWR_SetSleepStateLimit(PWR_Obj, PWR_SleepState);
int          PWR_WakeUpLatency(PWR_Obj, PWR_SleepState, PWR_Time *);
int          PWR_RecommendSleepState(PWR_Obj, PWR_Time, PWR_SleepState *);

int          PWR_GetPerfState(PWR_Obj, PWR_PerfState *);
int          PWR_SetPerfState(PWR_Obj, PWR_PerfState);

/*
 * User, Resource Manager Interface.
 */

/*
 * Resource Manager, Operating System Interface.
 */

/*
 * Resource Manager, Monitor and Control Interface.
 */

/*
 * Administrator, Monitor and Control Interface.
 */

/*
 * HPCS Manager, Resource Manager Interface.
 */

/*
 * Accounting, Monitor and Control Interface.
 */

/*
 * User, Monitor and Control Interface.
 */

/*
 * Cray Specific Interface
 */
int CRAYPWR_AttrGetCount(PWR_ObjType obj, size_t *value);
int CRAYPWR_AttrGetList(PWR_ObjType obj, size_t len, const char **str_list,
                        int *val_list);
int CRAYPWR_AttrGetName(PWR_AttrName attr, char *buf, size_t max);
PWR_AttrName CRAYPWR_AttrGetEnum(const char *attrname);

#ifdef __cplusplus
}
#endif

#endif /* _PWR_API_H */
