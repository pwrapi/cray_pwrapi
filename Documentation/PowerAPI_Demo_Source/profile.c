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

#include "profile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NUM_OBJ_ATTR_GET_CALLS 50000
#define NUM_OBJ_ATTR_SET_CALLS 2000
#define NUM_GRP_ATTR_GET_CALLS 100
#define NUM_GRP_ATTR_SET_CALLS 100
#define NUM_OBJ_STAT_LOOPS 20

//
// find_max - find the maximum value in an array of floats
//
// Argument(s):
//
//	arr - pointer to array of floats
//	len - length of arr
//
// Return Code(s):
//
//	float - the max value.
//
static float find_max(float *arr, int len)
{
	int i;
	float max = 0;
	for (i = 0; i < len; i++) {
		if (arr[i] > max) {
			max = arr[i];
		}
	}
	return max;
}

//
// find_min - find the minimum value in an array of floats
//
// Argument(s):
//
//	arr - pointer to array of floats
//	len - length of arr
//
// Return Code(s):
//
//	float - the min value.
//
static float find_min(float *arr, int len)
{
	int i;
	float min = INFINITY;
	for (i = 0; i < len; i++) {
		if (arr[i] < min) {
			min = arr[i];
		}
	}
	return min;
}

//
// find_avg_stdev - find the average value and standard deviation in an
//                  array of floats
//
// Argument(s):
//
//	arr - pointer to array of floats
//	len - length of arr
//	avg - pointer to float to write average value
//	std - pointer to float to write standard deviation value
//
// Return Code(s):
//
//	void
//
static void find_avg_stdev(float *arr, int len, float *avg, float *std)
{
	int i;
	float sum = 0.0;
	for (i = 0; i < len; i++) {
		sum += arr[i];
	}
	*avg = sum / (float)len;
	sum = 0.0;
	for (i = 0; i < len; i++) {
		sum += pow((arr[i] - *avg), 2);
	}
	*std = sqrt(sum / (float)len);
	return;
}

//
// get_elapsed_time - given two timevals, find the milliseconds between them
//
// Argument(s):
//
//	start_tv - start time
//	end_tv   - stop time
//
// Return Code(s):
//
//	float - milliseconds elapsed from start_tv to end_tv
//
static float get_elapsed_time(struct timeval start_tv, struct timeval end_tv)
{
	return ((end_tv.tv_sec * 1000.0 + (float)end_tv.tv_usec / 1000.0) -
		(start_tv.tv_sec * 1000.0 + (float)start_tv.tv_usec / 1000.0));
}

//
// prof_obj_attr_get - call PWR_ObjAttrGetValue() several times and report stats
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_obj_attr_get(PWR_Grp *grps)
{
	float *arr = malloc(NUM_OBJ_ATTR_GET_CALLS * sizeof(float));
	struct timeval start_tv, end_tv;
	float min_v, max_v, avg_v, std_v;
	int i = 0;
	int j = 0;
	int num_calls = 0;
	int ret;
	int num_objs = PWR_GrpGetNumObjs(grps[PWR_OBJ_NODE]);
	PWR_Obj obj;
	PWR_Time ts;
	double reading;

	for (i = 0; i < NUM_OBJ_ATTR_GET_CALLS; i++) {
		ret = PWR_GrpGetObjByIndx(grps[PWR_OBJ_NODE], j, &obj);
		if (ret != PWR_RET_SUCCESS) {
			if (ret == PWR_RET_NO_OBJ_AT_INDEX) {
				continue;
			}
			fprintf(stderr,
				"%s: PWR_GrpGetObjByIndx() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		if (++j >= num_objs) {
			j = 0;
		}
		gettimeofday(&start_tv, NULL);
		ret = PWR_ObjAttrGetValue(obj, PWR_ATTR_POWER, &reading, &ts);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_ObjAttrGetValue() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		arr[num_calls] = get_elapsed_time(start_tv, end_tv);
		num_calls++;
	}
	min_v = find_min(arr, num_calls);
	max_v = find_max(arr, num_calls);
	find_avg_stdev(arr, num_calls, &avg_v, &std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_ObjAttrGetValue",
	       num_calls, min_v, max_v, avg_v, std_v);
	free(arr);
	return ret;
}

//
// prof_obj_attr_set - call PWR_ObjAttrSetValue() several times and report stats
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_obj_attr_set(PWR_Grp *grps)
{
	float *arr = malloc(NUM_OBJ_ATTR_SET_CALLS * sizeof(float));
	struct timeval start_tv, end_tv;
	float min_v, max_v, avg_v, std_v;
	int i = 0;
	int j = 0;
	int num_calls = 0;
	int ret;
	int num_objs = PWR_GrpGetNumObjs(grps[PWR_OBJ_SOCKET]);
	PWR_Obj obj;
	double val = 0;

	for (i = 0; i < NUM_OBJ_ATTR_SET_CALLS; i++) {
		ret = PWR_GrpGetObjByIndx(grps[PWR_OBJ_SOCKET], j, &obj);
		if (ret != PWR_RET_SUCCESS) {
			if (ret == PWR_RET_NO_OBJ_AT_INDEX) {
				continue;
			}
			fprintf(stderr,
				"%s: PWR_GrpGetObjByIndx() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		if (++j >= num_objs) {
			j = 0;
		}
		gettimeofday(&start_tv, NULL);
		ret = PWR_ObjAttrSetValue(obj, PWR_ATTR_POWER_LIMIT_MAX, &val);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_ObjAttrGetValue() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		arr[num_calls] = get_elapsed_time(start_tv, end_tv);
		num_calls++;
	}
	min_v = find_min(arr, num_calls);
	max_v = find_max(arr, num_calls);
	find_avg_stdev(arr, num_calls, &avg_v, &std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_ObjAttrSetValue",
	       num_calls, min_v, max_v, avg_v, std_v);
	free(arr);
	return ret;
}

//
// prof_grp_attr_get - call PWR_GrpAttrGetValue() several times and report stats
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_grp_attr_get(PWR_Grp *grps)
{
	float *arr = malloc(NUM_GRP_ATTR_GET_CALLS * sizeof(float));
	struct timeval start_tv, end_tv;
	float min_v, max_v, avg_v, std_v;
	int i = 0;
	int j = 0;
	int num_calls = 0;
	int ret;
	int num_objs = PWR_GrpGetNumObjs(grps[PWR_OBJ_HT]);
	double *vals = (double *)malloc(num_objs * sizeof(double));
	PWR_Time *ts = (PWR_Time *)malloc(num_objs * sizeof(PWR_Time));

	for (i = 0; i < NUM_GRP_ATTR_GET_CALLS; i++) {
		gettimeofday(&start_tv, NULL);
		ret = PWR_GrpAttrGetValue(grps[PWR_OBJ_HT], PWR_ATTR_FREQ, vals,
					  ts, NULL);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_GrpAttrGetValue() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		arr[num_calls] = get_elapsed_time(start_tv, end_tv);
		num_calls++;
	}
	min_v = find_min(arr, num_calls);
	max_v = find_max(arr, num_calls);
	find_avg_stdev(arr, num_calls, &avg_v, &std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_GrpAttrGetValue",
	       num_calls, min_v, max_v, avg_v, std_v);
	free(arr);
	return ret;
}

//
// prof_grp_attr_get - call PWR_GrpAttrSetValue() several times and report stats
//                     (setting PWR_ATTR_FREQ_REQ on group of PWR_OBJ_HT objs)
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_grp_attr_set_ht(PWR_Grp *grps)
{
	float *arr = malloc(NUM_GRP_ATTR_SET_CALLS * sizeof(float));
	struct timeval start_tv, end_tv;
	float min_v, max_v, avg_v, std_v;
	int i = 0;
	int j = 0;
	int num_calls = 0;
	int ret;
	int num_objs = PWR_GrpGetNumObjs(grps[PWR_OBJ_HT]);
	int num_freqs;
	double max_freq;
	PWR_Obj obj;

	ret = PWR_GrpGetObjByIndx(grps[PWR_OBJ_HT], 0, &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_GrpGetObjByIndx() failed: %d\n",
			__func__, ret);
		exit(ret);
	}
	ret = PWR_ObjAttrGetMeta(obj, PWR_ATTR_FREQ, PWR_MD_NUM, &num_freqs);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_ObjAttrGetMeta failed: %i\n", __func__,
			ret);
		exit(ret);
	}
	ret = PWR_MetaValueAtIndex(obj, PWR_ATTR_FREQ, num_freqs - 1, &max_freq,
				   NULL);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_MetaValueAtIndex failed: %i\n",
			__func__, ret);
		exit(ret);
	}

	for (i = 0; i < NUM_GRP_ATTR_SET_CALLS; i++) {
		gettimeofday(&start_tv, NULL);
		ret = PWR_GrpAttrSetValue(grps[PWR_OBJ_HT], PWR_ATTR_FREQ_REQ,
					  &max_freq, NULL);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_GrpAttrSetValue() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		arr[num_calls] = get_elapsed_time(start_tv, end_tv);
		num_calls++;
	}
	min_v = find_min(arr, num_calls);
	max_v = find_max(arr, num_calls);
	find_avg_stdev(arr, num_calls, &avg_v, &std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n",
	       "PWR_GrpAttrSetValue (HT)", num_calls, min_v, max_v, avg_v,
	       std_v);
	free(arr);
	return ret;
}

//
// prof_grp_attr_set - call PWR_GrpAttrSetValue() several times and report stats
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_grp_attr_set(PWR_Grp *grps)
{
	float *arr = malloc(NUM_GRP_ATTR_SET_CALLS * sizeof(float));
	struct timeval start_tv, end_tv;
	float min_v, max_v, avg_v, std_v;
	int i = 0;
	int val = 0;
	int num_calls = 0;
	int ret;

	for (i = 0; i < NUM_GRP_ATTR_SET_CALLS; i++) {
		gettimeofday(&start_tv, NULL);
		ret = PWR_GrpAttrSetValue(grps[PWR_OBJ_SOCKET],
					  PWR_ATTR_POWER_LIMIT_MAX, &val, NULL);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr,
				"%s: PWR_GrpAttrSetValue() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		arr[num_calls] = get_elapsed_time(start_tv, end_tv);
		num_calls++;
	}
	min_v = find_min(arr, num_calls);
	max_v = find_max(arr, num_calls);
	find_avg_stdev(arr, num_calls, &avg_v, &std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_GrpAttrSetValue",
	       num_calls, min_v, max_v, avg_v, std_v);
	free(arr);
	return ret;
}

//
// prof_obj_stats - Create, start, stop, get value, and destroy an object
//                  PWR_Stat and report stats.
//
// Argument(s):
//
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	int
//
int prof_obj_stats(PWR_Grp *grps)
{
	float *cr_arr = malloc(NUM_OBJ_STAT_LOOPS * sizeof(float));
	float *start_arr = malloc(NUM_OBJ_STAT_LOOPS * sizeof(float));
	float *stop_arr = malloc(NUM_OBJ_STAT_LOOPS * sizeof(float));
	float *get_arr = malloc(NUM_OBJ_STAT_LOOPS * sizeof(float));
	float *des_arr = malloc(NUM_OBJ_STAT_LOOPS * sizeof(float));
	float cr_min_v, cr_max_v, cr_avg_v, cr_std_v;
	float start_min_v, start_max_v, start_avg_v, start_std_v;
	float stop_min_v, stop_max_v, stop_avg_v, stop_std_v;
	float get_min_v, get_max_v, get_avg_v, get_std_v;
	float des_min_v, des_max_v, des_avg_v, des_std_v;
	int cr_num_calls = 0;
	int start_num_calls = 0;
	int stop_num_calls = 0;
	int get_num_calls = 0;
	int des_num_calls = 0;
	struct timeval start_tv, end_tv;
	int ret;
	PWR_Obj obj;
	int i = 0;
	double val = 0.0;
	PWR_TimePeriod tp;

	ret = PWR_GrpGetObjByIndx(grps[PWR_OBJ_NODE], 0, &obj);
	if (ret != PWR_RET_SUCCESS) {
		fprintf(stderr, "%s: PWR_GrpGetObjByIndx() failed: %d\n",
			__func__, ret);
		exit(ret);
	}

	for (i = 0; i < NUM_OBJ_STAT_LOOPS; i++) {
		PWR_Stat my_stat;

		// Create stat
		gettimeofday(&start_tv, NULL);
		ret = PWR_ObjCreateStat(obj, PWR_ATTR_POWER, PWR_ATTR_STAT_AVG,
					&my_stat);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_ObjCreateStat() failed: %d\n",
				__func__, ret);
			exit(ret);
		}
		cr_arr[cr_num_calls] = get_elapsed_time(start_tv, end_tv);
		cr_num_calls++;

		// Start stat
		gettimeofday(&start_tv, NULL);
		ret = PWR_StatStart(my_stat);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatStart() failed: %i\n",
				__func__, ret);
			exit(ret);
		}
		start_arr[start_num_calls] = get_elapsed_time(start_tv, end_tv);
		start_num_calls++;

		sleep(1);

		// Stop stat
		gettimeofday(&start_tv, NULL);
		ret = PWR_StatStop(my_stat);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatStop() failed: %i\n",
				__func__, ret);
			exit(ret);
		}
		stop_arr[stop_num_calls] = get_elapsed_time(start_tv, end_tv);
		stop_num_calls++;

		// Get value of stat
		gettimeofday(&start_tv, NULL);
		ret = PWR_StatGetValue(my_stat, &val, &tp);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatGetValue() failed: %i\n",
				__func__, ret);
			exit(ret);
		}
		get_arr[get_num_calls] = get_elapsed_time(start_tv, end_tv);
		get_num_calls++;

		// Destroy stat
		gettimeofday(&start_tv, NULL);
		ret = PWR_StatDestroy(my_stat);
		gettimeofday(&end_tv, NULL);
		if (ret != PWR_RET_SUCCESS) {
			fprintf(stderr, "%s: PWR_StatDestroy() failed: %i\n",
				__func__, ret);
			exit(ret);
		}
		des_arr[des_num_calls] = get_elapsed_time(start_tv, end_tv);
		des_num_calls++;
	}

	cr_min_v = find_min(cr_arr, cr_num_calls);
	cr_max_v = find_max(cr_arr, cr_num_calls);
	find_avg_stdev(cr_arr, cr_num_calls, &cr_avg_v, &cr_std_v);
	start_min_v = find_min(start_arr, start_num_calls);
	start_max_v = find_max(start_arr, start_num_calls);
	find_avg_stdev(start_arr, start_num_calls, &start_avg_v, &start_std_v);
	stop_min_v = find_min(stop_arr, stop_num_calls);
	stop_max_v = find_max(stop_arr, stop_num_calls);
	find_avg_stdev(stop_arr, stop_num_calls, &stop_avg_v, &stop_std_v);
	get_min_v = find_min(get_arr, get_num_calls);
	get_max_v = find_max(get_arr, get_num_calls);
	find_avg_stdev(get_arr, get_num_calls, &get_avg_v, &get_std_v);
	des_min_v = find_min(des_arr, des_num_calls);
	des_max_v = find_max(des_arr, des_num_calls);
	find_avg_stdev(des_arr, des_num_calls, &des_avg_v, &des_std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_ObjCreateStat",
	       cr_num_calls, cr_min_v, cr_max_v, cr_avg_v, cr_std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_StatStart",
	       start_num_calls, start_min_v, start_max_v, start_avg_v,
	       start_std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_StatStop",
	       stop_num_calls, stop_min_v, stop_max_v, stop_avg_v, stop_std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_StatGetValue",
	       get_num_calls, get_min_v, get_max_v, get_avg_v, get_std_v);
	printf("%-24s %-10d %-10f %-10f %-10f %-10f\n", "PWR_StatDestroy",
	       des_num_calls, des_min_v, des_max_v, des_avg_v, des_std_v);
	free(cr_arr);
	free(start_arr);
	free(stop_arr);
	free(get_arr);
	free(des_arr);
	return ret;
}

//
// profile_and_report - Create, start, stop, get value, and destroy an object
//                      PWR_Stat and report stats.
//
// Argument(s):
//
//	ctx  - pointer to the PWR_Cntxt to use
//	grps - a pointer to a populated array of groups of length
//	       PWR_NUM_OBJ_TYPES
//
// Return Code(s):
//
//	void
//
void profile_and_report(PWR_Cntxt *ctx, PWR_Grp *grps)
{
	printf("Live profiling report\n=====================\n");
	printf("%-24s %-10s %-10s %-10s %-10s %-10s\n", "Function", "Num calls",
	       "Min (ms)", "Max (ms)", "Avg (ms)", "Std (ms)");
	prof_obj_attr_get(grps);
	prof_obj_attr_set(grps);
	prof_obj_stats(grps);
	prof_grp_attr_set(grps);
	prof_grp_attr_get(grps);
	prof_grp_attr_set_ht(grps);
	printf("\n\n");
}