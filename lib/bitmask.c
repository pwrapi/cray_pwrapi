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

#include <log.h>

#include <glib.h>

#include "bitmask.h"

//----------------------------------------------------------------------//
// 				BITMASK 				//
//----------------------------------------------------------------------//


size_t
bitmask_num_set(const bitmask_t *bitmask)
{
	size_t i;
	size_t num = 0;

	TRACE2_ENTER("bitmask = %p", bitmask);

	for (i = 0; i < bitmask->used; ++i)
		if (BITMASK_TEST(bitmask, i))
			++num;

	TRACE2_EXIT("num = %lu", num);

	return num;
}

bitmask_t *
new_bitmask(size_t bits)
{
	bitmask_t *bitmask = NULL;

	TRACE2_ENTER("bits = %lu", bits);

	if (bits < 1) {
		// Issue error message and return NULL;
		LOG_FAULT("Illegal bitmask size!");
		goto cleanup_return;
	}

	// Allocate enough space for the bitmask_t struct and any
	// additional contiguous space needed for the bitblocks
	// array beyond the first entry.
	bitmask = g_malloc0(sizeof(bitmask_t) - sizeof(bitblock_t) +
			BITBLOCK_BYTES(bits));
	if (!bitmask) {
		// Issue error message and return NULL;
		LOG_FAULT("allocation failure!");
		goto cleanup_return;
	}

	bitmask->used = bits;

cleanup_return:
	TRACE2_EXIT("bitmask = %p", bitmask);

	return bitmask;
}

void
del_bitmask(bitmask_t *bitmask)
{
	TRACE2_ENTER("bitmask = %p", bitmask);

	g_free(bitmask);

	TRACE2_EXIT("");
}

void
dbg_bitmask(const char *label, const bitmask_t *bitmask)
{
	int i;

	LOG_DBG("%s:", label);
	LOG_DBG("    used = %lu", bitmask->used);
	for (i = 0; i < BITBLOCK_NUM(bitmask->used); i++) {
		LOG_DBG("    mask[%u] = %016lx", i, bitmask->bitblock[i]);
	}
}
