/*
 * Copyright (c) 2017, Cray Inc.
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

#include <cray-powerapi/api.h>
#include <log.h>

#include "../common/common.h"

#define	CONTEXT_NAME	"test_apphint"
#define	LOG_PATH	"/tmp/apphints.log"

int
main(int argc, char **argv)
{
	PWR_Cntxt context1 = NULL;
	PWR_Cntxt context2 = NULL;
	PWR_Obj object = NULL;
	uint64_t hintid[3];
	FILE *fid = NULL;
	char buf[4096];
	const char *expected[] = {
		"apphint start",
		"apphint progress",
		"apphint stop",
		"apphint destroy",
		""
	};
	int index = 0;

	memset(hintid, 0, sizeof(hintid));

	// First pass does not log
	unlink(LOG_PATH);
	unlink(LOG_PATH ".ctl");
	unsetenv("PWR_HINT_LOG_PATH");

	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, CONTEXT_NAME,
			&context1, PWR_RET_SUCCESS);

	// Make sure this new routine works
	TST_CntxtGetObjByName(context1, "core.0", &object, PWR_RET_WARN_NO_OBJ_BY_NAME);
	TST_CntxtGetObjByName(context1, "core.0.0", &object, PWR_RET_SUCCESS);

	// Test creation and double-deletion, automatic naming
	TST_AppHintCreate(object, NULL, &hintid[0], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[0], PWR_RET_FAILURE);

	// Test multiple creations and deletions, automatic naming
	TST_AppHintCreate(object, NULL, &hintid[0], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintCreate(object, NULL, &hintid[1], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintCreate(object, NULL, &hintid[2], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[1], PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[2], PWR_RET_SUCCESS);

	// Test naming conflict
	TST_AppHintCreate(object, "blah", &hintid[0], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintCreate(object, "blah", &hintid[1], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_FAILURE);
	TST_AppHintDestroy(hintid[0], PWR_RET_SUCCESS);

	// Run through functions for a hint
	TST_AppHintCreate(object, NULL, &hintid[0], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintStart(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintProgress(hintid[0], 0.5, PWR_RET_SUCCESS);
	TST_AppHintStop(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[0], PWR_RET_SUCCESS);
	if ((fid = fopen(LOG_PATH, "r"))) {
		fclose(fid);
		printf("fopen('%s') succeeded, should have failed: FAIL\n", LOG_PATH);
		return EC_HINT_LOGERROR;
	}

	// While previous context exists, create a new one
	setenv("PWR_HINT_LOG_PATH", LOG_PATH, 1);
	TST_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, CONTEXT_NAME,
			&context2, PWR_RET_SUCCESS);
	TST_CntxtGetObjByName(context2, "core.0.0", &object, PWR_RET_SUCCESS);

	// Run through functions for a hint
	TST_AppHintCreate(object, NULL, &hintid[0], PWR_REGION_DEFAULT, PWR_REGION_INT_NONE, PWR_RET_SUCCESS);
	TST_AppHintStart(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintProgress(hintid[0], 0.5, PWR_RET_SUCCESS);
	TST_AppHintStop(hintid[0], PWR_RET_SUCCESS);
	TST_AppHintDestroy(hintid[0], PWR_RET_SUCCESS);

	// Destroy all contexts
	TST_CntxtDestroy(context2, PWR_RET_SUCCESS);
	TST_CntxtDestroy(context1, PWR_RET_SUCCESS);

	// File should exist and contain the right data
	if (!(fid = fopen(LOG_PATH, "r"))) {
		printf("fopen('%s') failed %m: FAIL\n", LOG_PATH);
		return EC_HINT_LOGERROR;
	}
	index = 0;
	while (fgets(buf, sizeof(buf), fid)) {
		int typ;
		char *msg;

		msg = buf + strlen(buf);
		while (--msg >= buf && *msg == '\n')
			*msg = 0;

		msg = pmlog_parse(buf, NULL, NULL, NULL, NULL, &typ);
		if (!msg) {
			printf("bad format: '%s': FAIL\n", msg);
			return EC_HINT_LOGERROR;
		}
		if (typ != LOG_TYPE_MESSAGE) {
			printf("bad message type: %d: FAIL\n", typ);
			return EC_HINT_LOGERROR;
		}
		if (strncmp(msg, expected[index], strlen(expected[index]))) {
			printf("wrong message: '%s' does not match '%s': FAIL\n",
					msg, expected[index]);
			return EC_HINT_LOGERROR;
		}
		index++;
	}
	fclose(fid);
	printf("AppHints file content checked: PASS\n");

	unlink(LOG_PATH);
	unlink(LOG_PATH ".ctl");

	return 0;
}
