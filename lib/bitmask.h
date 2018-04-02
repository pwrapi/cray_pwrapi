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

#ifndef _PWR_BITMASK_H
#define _PWR_BITMASK_H

#include <stdint.h>

//------------------------------------------------------------------------
// BITMASK: types, macros and prototypes
//------------------------------------------------------------------------


// A bitmask can be of arbitrary length. The length of the space
// available for the mask will always be multiple of the word size.
// The bitmask may use less than the available size.
//
// A bitmask is composed of 1 or more bitblocks. Each bitblock is composed of
// the bits that fit within a word.

//--------------------------//
// BITBLOCK TYPE and MACROS //
//--------------------------//

// Make bitblock_t the unsigned int type for the word size of the
// target system. Used for the array of blocks of bits making up the
// mask and the count of bits used for the mask within the array.
typedef uint64_t bitblock_t;

#define BITBLOCK_WIDTH	(sizeof(bitblock_t) * 8)

// Get index of bitblock holding bit x
#define BITBLOCK_INDEX(bit) \
	( (bit) / BITBLOCK_WIDTH )

// Number of bitblocks needed to hold x bits
#define BITBLOCK_NUM(bits) \
	( BITBLOCK_INDEX(bits) + 1 )

// Number of bytes in bitblocks to hold a mask of x bits.
#define BITBLOCK_BYTES(bits) \
	( BITBLOCK_NUM(bits) * sizeof(bitblock_t) )

// Get a bitblock mask with only bit x set
#define BITBLOCK_MASK(bit) \
	( (bitblock_t)1 << ( (bit) % BITBLOCK_WIDTH ) )

//-------------------------//
// BITMASK TYPE and MACROS //
//-------------------------//
typedef struct {
	bitblock_t used;	 // Number of used bits in bitmask
	bitblock_t bitblock[1];  // Blocks of bits allocated for bitmask

	// Additional space allocated for bitblock beyond first entry...
	// Only allowed for heap allocated bitmasks, never for stack or
	// data block declared bitmasks.

} bitmask_t;

size_t		bitmask_num_set(const bitmask_t *bitmask);
bitmask_t *	new_bitmask(size_t bits);
void		del_bitmask(bitmask_t *bitmask);
void		dbg_bitmask(const char *label, const bitmask_t *bitmask);


// Declare a bitmask with auto storage handling (non-static).
// WARNING: limited to only one bitblock for the mask.
#define DECLARE_BITMASK_AUTO(name, bits, value) \
	bitmask_t name = {			\
		.used = (bits),			\
		.bitblock = { (value) }		\
	}

#define DEFINE_BITMASK(name)	bitmask_t name


// Allocate/deallocate a bitmask from dynamic storage
#define NEW_BITMASK(bits) new_bitmask(bits)
#define DEL_BITMASK(bitmask) del_bitmask(bitmask)


// Number of bytes in bitmast_t struct to hold bitmask of x bits
#define BITMASK_BYTES(bitmask) \
	  ( sizeof(bitmask_t) + (BITBLOCK_BYTES((bitmask)->used) - sizeof(bitblock_t )) )

// Set bit in bitmask
#define BITMASK_SET(bitmask, bit) \
	( (bitmask)->bitblock[BITBLOCK_INDEX(bit)] |= BITBLOCK_MASK(bit) )

// Clear bit in bitmask
#define BITMASK_CLEAR(bitmask, bit) \
	( (bitmask)->bitblock[BITBLOCK_INDEX(bit)] &= ~BITBLOCK_MASK(bit) )

// Test if bit is set in bitmask
#define BITMASK_TEST(bitmask, bit) \
	( (bitmask)->bitblock[BITBLOCK_INDEX(bit)] & BITBLOCK_MASK(bit) )

// Clear all bits in bitmask
#define BITMASK_CLEARALL(bitmask)				\
	do {							\
		size_t _i;					\
		size_t _m = BITBLOCK_NUM((bitmask)->used);	\
		for (_i = 0; _i < _m; ++_i)			\
			(bitmask)->bitblock[_i] = (bitblock_t)0;	\
	} while (0)

// Set all bits in bitmask
#define BITMASK_SETALL(bitmask)					\
	do {							\
		size_t _i;					\
		size_t _m = BITBLOCK_NUM((bitmask)->used);	\
		for (_i = 0; _i < _m; ++_i)			\
			(bitmask)->bitblock[_i] = (bitblock_t)-1;	\
	} while (0)

// Get number of bits set in bitmask
#define BITMASK_NUM_BITS_SET(bitmask)		\
	bitmask_num_set(bitmask)

// Get number of bits clear in bitmask
#define BITMASK_NUM_BITS_CLEAR(bitmask)		\
	((bitmask)->used - bitmask_num_set(bitmask))

// Get number of bits used in bitmask
#define BITMASK_NUM_BITS_USED(bitmask)		\
	((bitmask)->used)

#endif // _PWR_BITMASK_H
