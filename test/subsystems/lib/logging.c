/*
 * Copyright (c) 2017-2018, Cray Inc.
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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <cray-powerapi/api.h>
#include "../../../include/log.h"

// Non-exported whitebox tests
extern bool test_bloat(bool enable);
extern int test_pmlog_init(void);
extern int test_pmlog_ringfill(void);
extern int test_pmlog_rotate(void);
extern int test_pmlog_messages(void);
extern int test_pmlog_twofiles(void);
extern int test_pmlog_processes(int count);
extern int test_pmlog_threads(int count);
extern int test_pmlog_exit(int signal, bool expected);
extern int test_pmlog_disable(void);

extern double test_pmlog_ringrate(int len);
extern double test_pmlog_filerate(int len);
extern double test_pmlog_msgrate(int msgtype, int len);

/**
 * Evaluate errors, compute elapsed time, and display message.
 *
 * @param tv0 - time value initialized before starting the test
 * @param errcnt - error count from test
 * @param name - name of the test
 *
 * @return int - returned errcnt value
 */
static int
_dotest(struct timeval *tv0, int errcnt, const char *name)
{
	struct timeval tv;
	struct timeval *tv1 = &tv;
	// Capture the current time, and compute difference
	gettimeofday(tv1, NULL);
	if (tv1->tv_usec < tv0->tv_usec) {
		tv1->tv_usec += 1000000;
		tv1->tv_sec  -= 1;
	}
	tv1->tv_usec -= tv0->tv_usec;
	tv1->tv_sec  -= tv0->tv_sec;
	// Pass/Fail message
	if (errcnt == 0) {
		printf("pass %-24s in %2ld.%06ld sec\n",
				name, tv1->tv_sec, tv1->tv_usec);
	} else {
		printf("fail %-24s in %2ld.%06ld sec, errcnt = %d\n",
				name, tv1->tv_sec, tv1->tv_usec, errcnt);
	}
	return errcnt;
}

// Perform a test, increment caller's 'errcnt' value
#define	dotest(func)	do {						\
				struct timeval tv0;			\
				gettimeofday(&tv0, NULL);		\
				errcnt += _dotest(&tv0, func, #func);	\
				tested = true;				\
			} while(0)

/**
 * Perform a select series of rate tests.
 *
 * @param void
 */
static void
_dorates(void)
{
	pmlog_init(NULL, 0, 0, 0, 0);
	printf("Raw ring     rate = %0.2f Mops/sec\n",
			test_pmlog_ringrate(8) / 1000000.0);
	printf("Raw file     rate = %0.2f Mops/sec\n",
			test_pmlog_filerate(8) / 1000000.0);
	printf("Msg MESSAGE  rate = %0.2f Mops/sec\n",
			test_pmlog_msgrate(LOG_TYPE_MESSAGE, 8) / 1000000.0);
	printf("Msg DEBUG1   rate = %0.2f Mops/sec\n",
			test_pmlog_msgrate(LOG_TYPE_DEBUG1, 8) / 1000000.0);
	pmlog_autoflush(true, false);
	printf("Msg DEBUG1af rate = %0.2f Mops/sec\n",
			test_pmlog_msgrate(LOG_TYPE_DEBUG1, 8) / 1000000.0);
	pmlog_term();
}

/**
 * Performance test used by Sandia. This is used to test the overhead impact of
 * the logging on a real attribute fetch.
 *
 * @param name - name of the object to sample
 * @param attr - attribute to GET
 * @param samples - sample iteration count
 *
 * @return double - averaged time per sample in usec
 */
static double
_impacttime(const char *name, PWR_AttrName attr, int samples)
{
	double usecs = 0.0;
	PWR_Cntxt cntxt;
	PWR_Obj obj;
	clock_t start;
	clock_t duration;
	int tmp = samples;
	int rc;

	pmlog_enable(-1);
	pmlog_init(NULL, 0, 0, 0, 0);
	rc = PWR_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "App", &cntxt);
	if (rc != PWR_RET_SUCCESS) {
		printf("FATAL: could not initialize context, rc=%d\n", rc);
		goto done;
	}
	rc = PWR_CntxtGetObjByName(cntxt, name, &obj);
	if (rc != PWR_RET_SUCCESS) {
		printf("FATAL: could not get object %s, rc=%d\n", name, rc);
		goto done;
	}

	start = clock();
	while (tmp--) {
		PWR_Time time;
		double value;
		rc = PWR_ObjAttrGetValue(obj, attr, &value, &time);
		if (PWR_RET_SUCCESS != rc) {
			printf("FATAL: could not read energy for object %s, rc=%d\n", name, rc);
			goto done;
		}
	}
	duration = clock() - start;
	usecs = 1000000.0 * (((double)duration / CLOCKS_PER_SEC) / samples);

	PWR_CntxtDestroy(cntxt);
done:
	pmlog_term();
	return usecs;
}

/**
 * Test the impact with logging disabled and enabled.
 *
 */
static void
_doimpact(void)
{
	setenv("PMLOG_ENABLE", "none", 1);
	printf("Log Disabled  = %0.2f usec\n",
			_impacttime("node.0", PWR_ATTR_POWER, 100000));
	setenv("PMLOG_ENABLE", "default", 1);
	printf("Log Default   = %0.2f usec\n",
			_impacttime("node.0", PWR_ATTR_POWER, 100000));
	setenv("PMLOG_ENABLE", "full", 1);
	printf("Log Fulltrace = %0.2f usec\n",
			_impacttime("node.0", PWR_ATTR_POWER, 100000));
}

/**
 * Usage message.
 *
 * Pass 'fmt' as a printf-like format string, with arguments, to report usage as
 * a syntax error with a syntax error message, and exit with a return code of 1.
 *
 * Pass 'fmt' as NULL to report usage as help, and exit with a return code of 0.
 *
 * @param fmt - error message format, or NULL if not an error
 */
static void  __attribute__((__format__(__printf__, 1, 2), noreturn))
_usage(const char *fmt, ...)
{
	if (fmt != NULL) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
	printf("Usage: logging [-bh] [action ...]\n"
			"   -h = print this help\n"
			"   -b = bloat threaded code to find holes\n"
			"        (NOTE: this inserts delays while mutexes are locked, so code will\n"
			"         run very slowly)\n"
			"   Actions:\n"
			"      init       permute initialization parameters\n"
			"      ringfill   test ring fill and empty pointers\n"
			"      rotate     test log file rotation behavior\n"
			"      messages   test message type behavior\n"
			"      processes  test inter-process locking\n"
			"      twofiles   test multiple log files\n"
			"      threads    test inter-thread locking\n"
			"      exit       test exit and signal handling\n"
			"      disable    test log disable and enable\n"
			"      rates      test performance\n"
			"      impact     test logging impact on attr GET\n"
			"      all        perform all tests\n");
	exit((fmt == NULL) ? 0 : 1);
}

/**
 * Perform selected whitebox tests.
 *
 * @param argc - argument count
 * @param argv - arguments
 *
 * @return int - 0 on success, 1 if errors seen
 */
int
main(int argc, char **argv)
{

	if (geteuid() != 0) {
		printf("SKIP logging: root permissions required for test\n");
	}

	int errcnt = 0;
	bool bloat = false;
	bool allfast = false;
	bool allslow = false;
	int opt, i;

	printf("Starting logging tests, pid = %d\n", getpid());
	opterr = 0;
	while ((opt = getopt(argc, argv, "hb")) != -1) {
		switch (opt) {
		case 'b':
			bloat = true;
			break;
		case 'h':
			_usage(NULL);
		default:
			_usage("Option -%c not recognized\n", optopt);
		}
	}
	test_bloat(bloat);
	setenv("PMLOG_ENABLE", "full", 1);
	// Default test suite (no actions specified)
	if (optind >= argc) {
		allfast = true;
	} else {
		// Advance to 'all' in argument list, if present
		for (i = optind; i < argc; i++) {
			if (!strcmp(argv[i], "all")) {
				allfast = allslow = true;
				break;
			}
		}
	}
	// Parse the arguments (if any)
	for (i = optind; allfast || allslow || i < argc; i++) {
		const char *arg = (i < argc) ? argv[i] : "";
		bool tested = false;

		if (allfast || !strcmp(arg, "init")) {
			dotest(test_pmlog_init());
		}
		if (allfast || !strcmp(arg, "ringfill")) {
			dotest(test_pmlog_ringfill());
		}
		if (allfast || !strcmp(arg, "rotate")) {
			dotest(test_pmlog_rotate());
		}
		if (allfast || !strcmp(arg, "messages")) {
			dotest(test_pmlog_messages());
		}
		if (allfast || !strcmp(arg, "twofiles")) {
			dotest(test_pmlog_twofiles());
		}
		if (allfast || !strcmp(arg, "processes")) {
			dotest(test_pmlog_processes(5));
		}
		if (allfast || !strcmp(arg, "threads")) {
			dotest(test_pmlog_threads(5));
		}
		if (allfast || !strcmp(arg, "exit")) {
			dotest(test_pmlog_exit(0, true));
			dotest(test_pmlog_exit(SIGINT, false));
			dotest(test_pmlog_exit(SIGQUIT, false));
			dotest(test_pmlog_exit(SIGSEGV, true));
		}
		if (allfast || !strcmp(arg, "disable")) {
			dotest(test_pmlog_disable());
		}
		if (allslow || !strcmp(arg, "rates")) {
			_dorates();
			tested = true;
		}
		if (allslow || !strcmp(arg, "impact")) {
			_doimpact();
			tested = true;
		}
		if (!tested) {
			_usage("Option '%s' not recognized\n", arg);
		}
		// Stop looping if we tested everything
		if (allfast || allslow)
			break;
	}

	printf("Completed with %d errors\n", errcnt);
	return (errcnt == 0) ? 0 : 1;
}


