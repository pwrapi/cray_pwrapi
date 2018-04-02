/*
 * Copyright (c) 2016, Cray Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <cray-powerapi/api.h>

#include "../common/common.h"

#define EC_POWER_MAX_NO_MEMORY		64
#define EC_POWER_MAX_NO_SOCKETS		65
#define EC_POWER_MAX_NO_HTS		66
#define EC_POWER_MAX_NO_THREAD		67
#define EC_POWER_MAX_SET		68
#define EC_POWER_USE			69

#define CONTEXT_NAME		"test_attr_power_max"

static gpointer
mem_alloc(size_t size)
{
	gpointer ptr;

	printf("Allocate %zu bytes of memory: ", size);
	ptr = g_malloc0(size);
	if (!ptr) {
		printf("FAIL\n");
		exit(EC_POWER_MAX_NO_MEMORY);
	}
	printf("PASS\n");

	return ptr;
}

#define NUM_VAR (4 * 1024 * 1024)

static volatile gint thread_run = 0;
static volatile gint zero = 0;
static volatile gint *var_array;

static gpointer
worker_thread(gpointer arg)
{
	int *run = arg;
	int idx;

	g_atomic_int_add(&thread_run, 1);

	while (*run) {
		for (idx = 0; idx < NUM_VAR; idx++) {
			g_atomic_int_add(var_array + idx, 1);
		}
		g_thread_yield();
		for (idx = 0; idx < NUM_VAR; idx++) {
			if (g_atomic_int_dec_and_test(var_array + idx)) {
				g_atomic_int_add(&zero, 1);
			}
		}
		g_thread_yield();
	}

	g_atomic_int_add(&thread_run, -1);

	return NULL;
}

//
// main - Main entry point.
//
// Argument(s):
//
//	argc - Number of arguments
//	argv - Arguments
//
// Return Code(s):
//
//	int - Zero for success, non-zero for failure
//
int
main(int argc, char **argv)
{
	PWR_Cntxt context;
	PWR_Grp socket_group;
	unsigned int num_socket;
	PWR_Grp ht_group;
	unsigned int num_ht;
	int num_thread;
	PWR_Obj object;
	PWR_Time tspec;
	double *power_max_ini_array;
	double *power_max_set_array;
	double *power_use_ini_array;
	GThread **thread_array;
	double power_use;
	double power_max;
	int run;
	int idx;

	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "test_role", &context,
			PWR_RET_SUCCESS);

	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_SOCKETS, &socket_group, PWR_RET_SUCCESS);

	TST_GrpGetNumObjs(socket_group, &num_socket, PWR_RET_SUCCESS);
	printf("Verify socket group isn't empty: ");
	check_int_greater_than(num_socket, 0, EC_POWER_MAX_NO_SOCKETS);

	TST_CntxtGetGrpByName(context, CRAY_NAMED_GRP_HTS, &ht_group, PWR_RET_SUCCESS);

	TST_GrpGetNumObjs(ht_group, &num_ht, PWR_RET_SUCCESS);
	printf("Verify HT group isn't empty: ");
	check_int_greater_than(num_ht, 0, EC_POWER_MAX_NO_HTS);

	power_max_ini_array = mem_alloc(sizeof(double) * num_socket);
	power_max_set_array = mem_alloc(sizeof(double) * num_socket);
	power_use_ini_array = mem_alloc(sizeof(double) * num_socket);

	num_thread = num_ht * 5 / 2; // = 2.5
	thread_array = mem_alloc(sizeof(GThread *) * num_thread);

	var_array = mem_alloc(sizeof(*var_array) * NUM_VAR);

	for (idx = 0; idx < num_socket; idx++) {
		TST_GrpGetObjByIndx(socket_group, idx, &object, PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER_LIMIT_MAX,
				&power_max_ini_array[idx], &tspec, PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER,
				&power_use_ini_array[idx], &tspec, PWR_RET_SUCCESS);
	}

	printf("Start %d worker threads: ", num_thread);
	run = 1;
	for (idx = 0; idx < num_thread; idx++) {
		thread_array[idx] = g_thread_new("worker", worker_thread, &run);
		if (!thread_array[idx]) {
			printf("FAIL\n");
			exit(EC_POWER_MAX_NO_THREAD);
		}
	}
	printf("PASS\n");

	// wait for threads to start
	while (g_atomic_int_get(&thread_run) < num_thread) {
		g_thread_yield();
	}

	sleep(15); // give threads time to crank up power use

	for (idx = 0; idx < num_socket; idx++) {
		TST_GrpGetObjByIndx(socket_group, idx, &object, PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER,
				&power_use, &tspec, PWR_RET_SUCCESS);
		printf("Socket %d: Verify power use (%lf) is more than initial (%lf): ",
				idx, power_use, power_use_ini_array[idx]);
		check_double_greater_than(power_use, power_use_ini_array[idx], EC_POWER_USE);

		// limit power use to 80% of what is being used right now
		power_max_set_array[idx] = power_use * 0.8;

		TST_ObjAttrSetValue(object, PWR_ATTR_POWER_LIMIT_MAX,
				&power_max_set_array[idx], PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER_LIMIT_MAX,
				&power_max, &tspec, PWR_RET_SUCCESS);
		printf("Socket %d: Verify power max (%lf) is what was set (%lf): ",
				idx, power_max, power_max_set_array[idx]);
		check_double_equal(power_max, power_max_set_array[idx], EC_POWER_MAX_SET);
	}

	sleep(15); // give hardware time to limit power use

	for (idx = 0; idx < num_socket; idx++) {
		TST_GrpGetObjByIndx(socket_group, idx, &object, PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER,
				&power_use, &tspec, PWR_RET_SUCCESS);

		// allow power use to be up to 105% of max
		power_max = power_max_set_array[idx] * 1.05;
		printf("Socket %d: Verify power use (%lf) is less than max (%lf): ",
				idx, power_use, power_max);
		check_double_greater_than(power_max, power_use, EC_POWER_USE);
	}

	// wait for threads to stop
	run = 0;
	while (g_atomic_int_get(&thread_run) > 0) {
		g_thread_yield();
	}

	for (idx = 0; idx < num_socket; idx++) {
		TST_GrpGetObjByIndx(socket_group, idx, &object, PWR_RET_SUCCESS);

		TST_ObjAttrSetValue(object, PWR_ATTR_POWER_LIMIT_MAX,
				&power_max_ini_array[idx], PWR_RET_SUCCESS);

		TST_ObjAttrGetValue(object, PWR_ATTR_POWER_LIMIT_MAX,
				&power_max, &tspec, PWR_RET_SUCCESS);
		printf("Socket %d: Verify power max (%lf) is what was set (%lf): ",
				idx, power_max, power_max_ini_array[idx]);
		check_double_equal(power_max, power_max_ini_array[idx], EC_POWER_MAX_SET);
	}

	TST_GrpDestroy(socket_group, PWR_RET_SUCCESS);

	TST_GrpDestroy(ht_group, PWR_RET_SUCCESS);

	TST_CntxtDestroy(context, PWR_RET_SUCCESS);

	exit(EC_SUCCESS);
}
