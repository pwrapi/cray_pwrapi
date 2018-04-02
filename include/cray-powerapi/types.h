/*
 * Copyright (c) 2015-2017, Cray Inc.
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
 * This file contains base PM API types.
*/

#ifndef _PWR_TYPES_H
#define _PWR_TYPES_H

#include <stddef.h>
#include <stdint.h>

/*
 * Version data.
 */
#define PWR_MAJOR_VERSION	(2)
#define PWR_MINOR_VERSION	(0)

/*
 * CRAY_PWR_MAX_STRING_SIZE - The maximum size of strings, including the
 * 			      null byte, used internally for PowerAPI
 * 			      data names.
 */
#define CRAY_PWR_MAX_STRING_SIZE	(64)
/*
 * PWR_MAX_STRING_LEN - The maximum length of strings that can be returned
 *			from PowerAPI calls.
 */
#define PWR_MAX_STRING_LEN	(CRAY_PWR_MAX_STRING_SIZE-1)

/*
 * Opaque types.
 */
typedef void* PWR_Cntxt;
typedef void* PWR_Grp;
typedef void* PWR_Obj;
typedef void* PWR_Status;
typedef void* PWR_Stat;

/*
 * Context types.
 */
typedef int PWR_CntxtType;
#define PWR_CNTXT_DEFAULT	(0)
#define PWR_CNTXT_VENDOR	(1)

/*
 * Roles.
 */
typedef enum {
	PWR_ROLE_APP = 0,		// Application
	PWR_ROLE_MC,			// Monitor and Control
	PWR_ROLE_OS,			// Operating System
	PWR_ROLE_USER,			// User
	PWR_ROLE_RM,			// Resource Manager
	PWR_ROLE_ADMIN,			// Administrator
	PWR_ROLE_MGR,			// HPCS Manager
	PWR_ROLE_ACC,			// Accounting
	PWR_NUM_ROLES,
	/* */
	PWR_ROLE_INVALID = -1,
	PWR_ROLE_NOT_SPECIFIED = -2,
} PWR_Role;

/*
 * Object types.
 */
typedef enum {
	PWR_OBJ_PLATFORM = 0,
	PWR_OBJ_CABINET,
	PWR_OBJ_CHASSIS,
	PWR_OBJ_BOARD,
	PWR_OBJ_NODE,
	PWR_OBJ_SOCKET,
	PWR_OBJ_CORE,
	PWR_OBJ_POWER_PLANE,
	PWR_OBJ_MEM,
	PWR_OBJ_NIC,
	PWR_OBJ_HT,
	PWR_NUM_OBJ_TYPES,
	/* */
	PWR_OBJ_INVALID = -1,
	PWR_OBJ_NOT_SPECIFIED = -2,
} PWR_ObjType;

/*
 * Attributes.
 */
typedef enum {
	PWR_ATTR_PSTATE = 0,		// uint64_t
	PWR_ATTR_CSTATE,		// uint64_t
	PWR_ATTR_CSTATE_LIMIT,		// uint64_t
	PWR_ATTR_SSTATE,		// uint64_t
	PWR_ATTR_CURRENT,		// double, amps
	PWR_ATTR_VOLTAGE,		// double, volts
	PWR_ATTR_POWER,			// double, watts
	PWR_ATTR_POWER_LIMIT_MIN,	// double, watts
	PWR_ATTR_POWER_LIMIT_MAX,	// double, watts
	PWR_ATTR_FREQ,			// double, Hz
	PWR_ATTR_FREQ_REQ,		// double, Hz
	PWR_ATTR_FREQ_LIMIT_MIN,	// double, Hz
	PWR_ATTR_FREQ_LIMIT_MAX,	// double, Hz
	PWR_ATTR_ENERGY,		// double, joules
	PWR_ATTR_TEMP,			// double, degrees Celsius
	PWR_ATTR_OS_ID,			// uint64_t
	PWR_ATTR_THROTTLED_TIME,	// uint64_t
	PWR_ATTR_THROTTLED_COUNT,	// uint64_t
	PWR_ATTR_GOV,			// uint64_t
	PWR_NUM_ATTR_NAMES,
	/* */
	PWR_ATTR_INVALID = -1,
	PWR_ATTR_NOT_SPECIFIED = -2,
} PWR_AttrName;

/*
 * Attribute data types.
 */
typedef enum {
	PWR_ATTR_DATA_DOUBLE = 0,
	PWR_ATTR_DATA_UINT64,
	PWR_NUM_ATTR_DATA_TYPES,
	/* */
	PWR_ATTR_DATA_INVALID = -1,
	PWR_ATTR_DATA_NOT_SPECIFIED = -2,
} PWR_AttrDataType;

/*
 * Attribute access errors popped from PWR_Status using PWR_StatusPopError().
 */
typedef struct {
	PWR_Obj obj;		// The object associated with the error
	PWR_AttrName name;	// The attribute associated with the error
	int index;		// The index in the output array where the
				//	error occurred
	int error;		// The error code
} PWR_AttrAccessError;

/*
 * Governor names.
 */
typedef enum {
	PWR_GOV_LINUX_ONDEMAND = 0,
	PWR_GOV_LINUX_PERFORMANCE,
	PWR_GOV_LINUX_CONSERVATIVE,
	PWR_GOV_LINUX_POWERSAVE,
	PWR_GOV_LINUX_USERSPACE,
	PWR_NUM_GOV_NAMES,
	/* */
	PWR_GOV_INVALID = -1,
	PWR_GOV_NOT_SPECIFIED = -2,
} PWR_AttrGov;

/*
 * Metadata names.
 */
typedef enum {
	PWR_MD_NUM = 0,		// uint64_t
	PWR_MD_MIN,		// either uint64_t or double, depending on
				//	attribute type
	PWR_MD_MAX,		// either uint64_t or double, depending on
				//	attribute type
	PWR_MD_PRECISION,	// uint64_t
	PWR_MD_ACCURACY,	// double
	PWR_MD_UPDATE_RATE,	// double
	PWR_MD_SAMPLE_RATE,	// double
	PWR_MD_TIME_WINDOW,	// PWR_Time
	PWR_MD_TS_LATENCY,	// PWR_Time
	PWR_MD_TS_ACCURACY,	// PWR_Time
	PWR_MD_MAX_LEN,		// uint64_t, max strlen of any returned
				//	metadata string
	PWR_MD_NAME_LEN,	// uint64_t, max strlen of PWR_MD_NAME
	PWR_MD_NAME,		// char *, C-style NULL-terminated ASCII string
	PWR_MD_DESC_LEN,	// uint64_t, max strlen of PWR_MD_DESC
	PWR_MD_DESC,		// char *, C-style NULL-terminated ASCII string
	PWR_MD_VALUE_LEN,	// uint64_t, max strlen returned by
				//	PWR_MetaValueAtIndex
	PWR_MD_VENDOR_INFO_LEN,	// uint64_t, max strlen of PWR_MD_VENDOR_INFO
	PWR_MD_VENDOR_INFO,	// char *, C-style NULL-terminated ASCII string
	PWR_MD_MEASURE_METHOD,	// uint64_t, 0/1 depending on real/model
				//	measurement
	PWR_NUM_META_NAMES,
	/* */
	PWR_MD_INVALID = -1,
	PWR_MD_NOT_SPECIFIED = -2,
} PWR_MetaName;

/*
 * Metadata data types.
 */
typedef enum {
	PWR_META_DATA_DOUBLE = 0,
	PWR_META_DATA_UINT64,
	PWR_META_DATA_TIME,
	PWR_NUM_META_DATA_TYPES,
	/* */
	PWR_META_DATA_INVALID = -1,
	PWR_META_DATA_NOT_SPECIFIED = -2,
} PWR_MetaDataType;


/*
 * Error and warning return codes.
 */
#define PWR_RET_WARN_TRUNC		 (5)
#define PWR_RET_WARN_NO_GRP_BY_NAME	 (4)
#define PWR_RET_WARN_NO_OBJ_BY_NAME	 (3)
#define PWR_RET_WARN_NO_CHILDREN	 (2)
#define PWR_RET_WARN_NO_PARENT		 (1)
#define PWR_RET_SUCCESS			 (0)
#define PWR_RET_FAILURE			(-1)
#define PWR_RET_NOT_IMPLEMENTED		(-2)
#define PWR_RET_EMPTY			(-3)
#define PWR_RET_INVALID			(-4)
#define PWR_RET_LENGTH			(-5)
#define PWR_RET_NO_ATTRIB		(-6)
#define PWR_RET_NO_META			(-7)
#define PWR_RET_READ_ONLY		(-8)
#define PWR_RET_BAD_VALUE		(-9)
#define PWR_RET_BAD_INDEX		(-10)
#define PWR_RET_OP_NOT_ATTEMPTED	(-11)
#define PWR_RET_OP_NO_PERM		(-12)
#define PWR_RET_OUT_OF_RANGE		(-13)
#define PWR_RET_NO_OBJ_AT_INDEX		(-14)

/*
 * Time related definitions.
 */
typedef uint64_t PWR_Time;
#define PWR_TIME_UNINIT		(0)
#define PWR_TIME_UNKNOWN	(0)

#define NSEC_PER_USEC		(1000UL)
#define USEC_PER_SEC		(1000000UL)
#define NSEC_PER_SEC		(NSEC_PER_USEC * USEC_PER_SEC)

/*
 * Nanoseconds is the basic unit of time. Many system time values
 * are expressed in microseconds. Need to limit the maximum allowed
 * microseconds to not overflow nanoseconds on a 64-bit platform.
 */
#define NSEC_MAX		(ULONG_MAX)
#define USEC_MAX		(NSEC_MAX/NSEC_PER_USEC)

/*
 * Timestamps.
 */
typedef struct {
	PWR_Time start;
	PWR_Time stop;
	PWR_Time instant;
} PWR_TimePeriod;

/*
 * Currently defined statistics.
 */
typedef enum {
	PWR_ATTR_STAT_MIN = 0,
	PWR_ATTR_STAT_MAX,
	PWR_ATTR_STAT_AVG,
	PWR_ATTR_STAT_STDEV,
	PWR_ATTR_STAT_CV,
	PWR_ATTR_STAT_SUM,
	PWR_NUM_ATTR_STATS,
	/* */
	PWR_ATTR_STAT_INVALID = -1,
	PWR_ATTR_STAT_NOT_SPECIFIED = -2,
} PWR_AttrStat;

/*
 * IDs.
 */
typedef enum {
	PWR_ID_USER = 0,
	PWR_ID_JOB,
	PWR_ID_RUN,
	PWR_NUM_IDS,
	/* */
	PWR_ID_INVALID = -1,
	PWR_ID_NOT_SPECIFIED = -2,
} PWR_ID;

/*
 * PWR_OperState type is used to describe the state being requested by OS
 * to Hardware interface functions that require power/performance state
 * information such as P-State, C-State information. Both c_state_num and
 * p_state_num must be provided.
 */
typedef struct {
	uint64_t c_state_num;
	uint64_t p_state_num;
} PWR_OperState;

/*
 * Power and performance hints.
 */
typedef enum {
	PWR_REGION_DEFAULT = 0,
	PWR_REGION_SERIAL,
	PWR_REGION_PARALLEL,
	PWR_REGION_COMPUTE,
	PWR_REGION_COMMUNICATE,
	PWR_REGION_IO,
	PWR_REGION_MEM_BOUND,
	PWR_REGION_GLOBAL_LOOP,
	PWR_NUM_REGION_HINTS,
	/* */
	PWR_REGION_INVALID = -1,
	PWR_REGION_NOT_SPECIFIED = -2,
} PWR_RegionHint;

/*
 * Level of intensity for a given hint.
 */
typedef enum {
	PWR_REGION_INT_HIGHEST = 0,
	PWR_REGION_INT_HIGH,
	PWR_REGION_INT_MEDIUM,
	PWR_REGION_INT_LOW,
	PWR_REGION_INT_LOWEST,
	PWR_REGION_INT_NONE,
	PWR_NUM_REGION_INTENSITIES,
	/* */
	PWR_REGION_INT_INVALID = -1,
	PWR_REGION_INT_NOT_SPECIFIED = -2,
} PWR_RegionIntensity;

/*
 * Sleep state levels.
 */
typedef enum {
	PWR_SLEEP_NO = 0,
	PWR_SLEEP_SHALLOW,
	PWR_SLEEP_MEDIUM,
	PWR_SLEEP_DEEP,
	PWR_SLEEP_DEEPEST,
	PWR_NUM_SLEEP_STATES,
	/* */
	PWR_SLEEP_INVALID = -1,
	PWR_SLEEP_NOT_SPECIFIED = -2,
} PWR_SleepState;

/*
 * Performance states hardware may be placed in.
 */
typedef enum {
	PWR_PERF_FASTEST = 0,
	PWR_PERF_FAST,
	PWR_PERF_MEDIUM,
	PWR_PERF_SLOW,
	PWR_PERF_SLOWEST,
	PWR_NUM_PERF_STATES,
	/* */
	PWR_PERF_INVALID = -1,
	PWR_PERF_NOT_SPECIFIED = -2,
} PWR_PerfState;

#endif /* _PWR_TYPES_H */
