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
#include <sys/time.h>

#ifdef USE_CBLAS
#include "cblas.h"
#elif USE_NVBLAS
#include "nvblas.h"
#elif USE_MKL
#include "mkl.h"
#endif

#define DGEMM_RESTRICT __restrict__

// ------------------------------------------------------- //
// Function: get_seconds
//
// Vendor may modify this call to provide higher resolution
// timing if required
// ------------------------------------------------------- //
double get_seconds()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	const double seconds = (double)now.tv_sec;
	const double usec = (double)now.tv_usec;

	return seconds + (usec * 1.0e-6);
}

// ------------------------------------------------------- //
// Function: main
// ------------------------------------------------------- //
int mt_dgemm_main(int argc, char *argv[])
{

	int N = 256;
	int repeats = 30;

	double alpha = 1.0;
	double beta = 1.0;

	if (argc > 1) {
		N = atoi(argv[1]);
		printf("Matrix size input by command line: %d\n", N);

		if (argc > 2) {
			repeats = atoi(argv[2]);

			if (repeats < 30) {
				fprintf(stderr,
					"Error: repeats must be at least 30, "
					"setting is: %d\n",
					repeats);
				exit(-1);
			}

			printf("Repeat multiply %d times.\n", repeats);

			if (argc > 3) {
				alpha = (double)atof(argv[3]);

				if (argc > 4) {
					beta = (double)atof(argv[4]);
				}
			}
		} else {
			printf("Repeat multiply defaulted to %d\n", repeats);
		}
	} else {
		printf("Matrix size defaulted to %d\n", N);
	}

	printf("Alpha =    %f\n", alpha);
	printf("Beta  =    %f\n", beta);

	if (N < 128) {
		printf("Error: N (%d) is less than 128, the matrix is too "
		       "small.\n",
		       N);
		exit(-1);
	}

	printf("Allocating Matrices...\n");

	double *DGEMM_RESTRICT matrixA =
	    (double *)malloc(sizeof(double) * N * N);
	double *DGEMM_RESTRICT matrixB =
	    (double *)malloc(sizeof(double) * N * N);
	double *DGEMM_RESTRICT matrixC =
	    (double *)malloc(sizeof(double) * N * N);

	printf("Allocation complete, populating with values...\n");

	int i, j, k, r;

#pragma omp parallel for
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			matrixA[i * N + j] = 2.0;
			matrixB[i * N + j] = 0.5;
			matrixC[i * N + j] = 1.0;
		}
	}

	printf("Performing multiplication...\n");

	const double start = get_seconds();

	// ------------------------------------------------------- //
	// VENDOR NOTIFICATION: START MODIFIABLE REGION
	//
	// Vendor is able to change the lines below to call optimized
	// DGEMM or other matrix multiplication routines. Do *NOT*
	// change any lines above this statement.
	// ------------------------------------------------------- //

	double sum = 0;

	// Repeat multiple times
	for (r = 0; r < repeats; r++) {
#if defined(USE_MKL) || defined(USE_CBLAS)
		cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, N, N, N,
			    alpha, matrixA, N, matrixB, N, beta, matrixC, N);
#elif defined(USE_NVBLAS)
		char transA = 'N';
		char transB = 'N';

		dgemm(&transA, &transB, &N, &N, &N, &alpha, matrixA, &N,
		      matrixB, &N, &beta, matrixC, &N);
#else
#pragma omp parallel for private(sum)
		for (i = 0; i < N; i++) {
			for (j = 0; j < N; j++) {
				sum = 0;

				for (k = 0; k < N; k++) {
					sum += matrixA[i * N + k] *
					       matrixB[k * N + j];
				}

				matrixC[i * N + j] =
				    (alpha * sum) + (beta * matrixC[i * N + j]);
			}
		}
#endif
	}

	const double end = get_seconds();

	printf("Calculating matrix check...\n");

	double final_sum = 0;
	long long int count = 0;

#pragma omp parallel for reduction(+ : final_sum, count)
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			final_sum += matrixC[i * N + j];
			count++;
		}
	}

	double N_dbl = (double)N;
	double matrix_memory = (3 * N_dbl * N_dbl) * ((double)sizeof(double));

	printf("\n");
	printf("==============================================================="
	       "\n");

	const double count_dbl = (double)count;
	const double scaled_result = (final_sum / (count_dbl * repeats));

	printf("Final Sum is:         %f\n", scaled_result);

	const double check_sum = N_dbl + (1.0 / (double)(repeats));
	const double allowed_margin = 1.0e-8;

	if ((check_sum >= (scaled_result - allowed_margin)) &&
	    (check_sum <= (scaled_result + allowed_margin))) {
		printf(" -> Solution check PASSED successfully.\n");
	} else {
		printf(" -> Solution check FAILED.\n");
	}

	printf("Memory for Matrices:  %f MB\n",
	       (matrix_memory / (1024 * 1024)));

	const double time_taken = (end - start);

	printf("Multiply time:        %f seconds\n", time_taken);

	const double flops_computed =
	    (N_dbl * N_dbl * N_dbl * 2.0 * (double)(repeats)) +
	    (N_dbl * N_dbl * 2 * (double)(repeats));

	printf("FLOPs computed:       %f\n", flops_computed);
	printf("GFLOP/s rate:         %f GF/s\n",
	       (flops_computed / time_taken) / 1000000000.0);

	printf("==============================================================="
	       "\n");
	printf("\n");

	free(matrixA);
	free(matrixB);
	free(matrixC);

	return 0;
}
