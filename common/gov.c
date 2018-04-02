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

#include <string.h>

#include <common.h>
#include <log.h>

PWR_AttrGov
pwr_string_to_gov(const char *str)
{
	PWR_AttrGov gov = PWR_GOV_INVALID;

	TRACE2_ENTER("str = '%s'", str);

	if (strcmp(str, "ondemand") == 0) {
		gov = PWR_GOV_LINUX_ONDEMAND;
	} else if (strcmp(str, "performance") == 0) {
		gov = PWR_GOV_LINUX_PERFORMANCE;
	} else if (strcmp(str, "conservative") == 0) {
		gov = PWR_GOV_LINUX_CONSERVATIVE;
	} else if (strcmp(str, "powersave") == 0) {
		gov = PWR_GOV_LINUX_POWERSAVE;
	} else if (strcmp(str, "userspace") == 0) {
		gov = PWR_GOV_LINUX_USERSPACE;
	}

	TRACE2_EXIT("gov = %d", gov);

	return gov;
}

const char *
pwr_gov_to_string(PWR_AttrGov gov)
{
	const char *str = NULL;

	TRACE2_ENTER("gov = %d", gov);

	switch (gov) {
	case PWR_GOV_LINUX_ONDEMAND:
		str = "ondemand";
		break;
	case PWR_GOV_LINUX_PERFORMANCE:
		str = "performance";
		break;
	case PWR_GOV_LINUX_CONSERVATIVE:
		str = "conservative";
		break;
	case PWR_GOV_LINUX_POWERSAVE:
		str = "powersave";
		break;
	case PWR_GOV_LINUX_USERSPACE:
		str = "userspace";
		break;
	default:
		str = "INVALID";
		break;
	}

	TRACE2_EXIT("str = '%s'", str);

	return str;
}

