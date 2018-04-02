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

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "an9-stream.h"
#include "mt-dgemm.h"
#include "pwrdemo.h"
#include "workload.h"

int call_dgemm_main(int N)
{
	// Argv: mt-dgemm matrix-size repeats(<30) alpha beta
	//       defaults: 256 30 1.0 1.0
	char str_N[100];
	snprintf(str_N, 100, "%d", N);
	char *dgemm_argv[] = {"mt-dgemm", str_N, NULL};
	int dgemm_argc = sizeof(dgemm_argv) / sizeof(char *) - 1;
	return mt_dgemm_main(dgemm_argc, dgemm_argv);
}

int call_stream_main() { return an9_stream_main(); }

int run_workload(demo_opt_t *opts)
{
	int rc = 0;
	char *env_omp_num_threads;
	env_omp_num_threads = getenv("OMP_NUM_THREADS");
	if (env_omp_num_threads != NULL) {
		printf("Environment has OMP_NUM_THREADS set to %s\n",
		       env_omp_num_threads);
	}

	switch (opts->workload) {
	case workload_dgemm:
		printf("Running dgemm...\n");
		call_dgemm_main(opts->dgemm_N);
		printf("... dgemm run completed.\n");
		break;
	case workload_stream:
		printf("Running stream...\n");
		call_stream_main();
		printf("... stream run completed.\n");
		break;
	default:
		fprintf(stderr, "Unrecognized workload %d\n", opts->workload);
		rc = 1;
	}
	return rc;
}
