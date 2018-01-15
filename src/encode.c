
/*
 * Adapted from:
 */

/*
 * QR Code generator library (C)
 *
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/qr-code-generator-library
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#define _POSIX_C_SOURCE 2

#include <unistd.h>

#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"
#include "seg.h"

static inline unsigned
count_align(unsigned ver)
{
	unsigned n;

	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	n = ver / 7 + 2;
	assert(n <= QR_ALIGN_MAX);

	return n;
}

/*
 * Returns the number of data bits that can be stored in a QR Code of the given version number, after
 * all function modules are excluded. This includes remainder bits, so it might not be a multiple of 8.
 * The result is in the range [208, 29648]. This could be implemented as a 40-entry lookup table.
 */
unsigned
count_data_bits(unsigned ver)
{
	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	unsigned n = (16 * ver + 128) * ver + 64;
	if (ver >= 2) {
		unsigned na = count_align(ver);
		n -= (25 * na - 10) * na - 55;
		if (ver >= 7) {
			n -= 18 * 2;  // Subtract version information
		}
	}

	return n;
}

/*
 * Returns the number of 8-bit codewords that can be used for storing data (not ECL),
 * for the given version number and error correction level. The result is in the range [9, 2956].
 */
static int
count_codewords(unsigned ver, enum qr_ecl ecl)
{
	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);
	assert(ecl < 4);

	return count_data_bits(ver) / 8
		- ECL_CODEWORDS_PER_BLOCK[ver][ecl] * NUM_ERROR_CORRECTION_BLOCKS[ver][ecl];
}

/*
 * Reed-Solomon ECL generator functions
 */

/*
 * The product of the two given field elements modulo GF(2^8 / 0x11D).
 * All inputs are valid.
 * This could be implemented as a 256 * 256 lookup table.
 */
static uint8_t
finiteFieldMul(uint8_t x, uint8_t y)
{
	uint8_t z = 0;

	/* Russian peasant multiplication */
	for (int i = 7; i >= 0; i--) {
		z = (z << 1) ^ ((z >> 7) * 0x11D);
		z ^= ((y >> i) & 1) * x;
	}

	return z;
}

/*
 * Calculates the Reed-Solomon generator polynomial of the given degree,
 * storing in r[0 : degree].
 */
static void
reed_solomon_generator(int degree, uint8_t *r)
{
	assert(1 <= degree && degree <= 30);

	// Start with the monomial x^0
	memset(r, 0, degree);
	r[degree - 1] = 1;

	/*
	 * Compute the product polynomial (x - r^0) * (x - r^1) * (x - r^2) * ... * (x - r^{degree-1}),
	 * drop the highest term, and store the rest of the coefficients in order of descending powers.
	 * Note that r = 0x02, which is a generator element of this field GF(2^8/0x11D).
	 */
	uint8_t root = 1;
	for (int i = 0; i < degree; i++) {
		// Multiply the current product by (x - r^i)
		for (int j = 0; j < degree; j++) {
			r[j] = finiteFieldMul(r[j], root);
			if (j + 1 < degree) {
				r[j] ^= r[j + 1];
			}
		}

		root = finiteFieldMul(root, 0x02);
	}
}

/*
 * Calculates the remainder of the polynomial data[0 : dataLen]
 * when divided by the generator[0 : degree],
 * where all polynomials are in big endian and the generator has an
 * implicit leading 1 term, storing the result in result[0 : degree].
 */
static void
reed_solomon_remainder(const void *data, size_t dataLen,
	const uint8_t generator[], size_t degree, uint8_t *r)
{
	const uint8_t *p = data;

	assert(1 <= degree && degree <= 30);

	// Perform polynomial division
	memset(r, 0, degree);
	for (size_t i = 0; i < dataLen; i++) {
		uint8_t factor = p[i] ^ r[0];
		memmove(&r[0], &r[1], degree - 1);
		r[degree - 1] = 0;
		for (size_t j = 0; j < degree; j++) {
			r[j] ^= finiteFieldMul(generator[j], factor);
		}
	}
}

/*
 * Appends error correction bytes to each block of the given data array, then interleaves bytes
 * from the blocks and stores them in the result array. data[0 : rawCodewords - totalEcc] contains
 * the input data. data[rawCodewords - totalEcc : rawCodewords] is used as a temporary work area
 * and will be clobbered by this function. The final answer is stored in result[0 : rawCodewords].
 */
static void
append_ecl(void *data, unsigned ver, enum qr_ecl ecl, uint8_t result[])
{
	uint8_t *p = data;

	assert(0 <= ecl && ecl < 4);
	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	// Calculate parameter numbers
	int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ver][ecl];
	int blockEccLen = ECL_CODEWORDS_PER_BLOCK[ver][ecl];
	int rawCodewords = count_data_bits(ver) / 8;
	int dataLen = rawCodewords - blockEccLen * numBlocks;
	int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	int ecc_bs = rawCodewords / numBlocks;
	int shortBlockDataLen = ecc_bs - blockEccLen;
//fprintf(stderr, "\nnumBlocks=%d - numshortBlocks=%d = %d\n", numBlocks, numShortBlocks, numBlocks - numShortBlocks);

	// Split data into blocks and append ECL after all data
	uint8_t generator[30];
	reed_solomon_generator(blockEccLen, generator);
	for (int i = 0, j = dataLen, k = 0; i < numBlocks; i++) {
		int blockLen = shortBlockDataLen;
		if (i >= numShortBlocks) {
			blockLen++;
		}
		reed_solomon_remainder(&p[k], blockLen, generator, blockEccLen, &p[j]);
		j += blockEccLen;
		k += blockLen;
	}

	// Interleave (not concatenate) the bytes from every block into a single sequence
	for (int i = 0, k = 0; i < numBlocks; i++) {
		for (int j = 0, l = i; j < shortBlockDataLen; j++, k++, l += numBlocks) {
			result[l] = p[k];
		}
		if (i >= numShortBlocks) {
			k++;
		}
	}
	for (int i = numShortBlocks, k = (numShortBlocks + 1) * shortBlockDataLen, l = numBlocks * shortBlockDataLen;
			i < numBlocks; i++, k += shortBlockDataLen + 1, l++) {
		result[l] = p[k];
	}
	for (int i = 0, k = dataLen; i < numBlocks; i++) {
		for (int j = 0, l = dataLen + i; j < blockEccLen; j++, k++, l += numBlocks)
			result[l] = p[k];
	}
}

/*
 * Calculates the positions of alignment patterns in ascending order
 * for the given version number, storing them to the given array.
 * Returns the number of alignment patterns, which is the used
 * array length, in the range [0, 7].
 */
unsigned
getAlignmentPatternPositions(unsigned ver, unsigned a[static QR_ALIGN_MAX])
{
	unsigned i, step, pos;
	unsigned n;

	if (ver == 1) {
		return 0;
	}

	n = count_align(ver);
	if (ver != 32) {
		// ceil((size - 13) / (2 * n - 2)) * 2
		step = (ver * 4 + n * 2 + 1) / (2 * n - 2) * 2;
	} else {
		step = 26;
	}

	for (i = n - 1, pos = ver * 4 + 10; i >= 1; i--, pos -= step) {
		a[i] = pos;
	}

	a[0] = 6;

	return n;
}

/*
 * Clears the given QR Code grid with white modules for the given
 * version's size, then marks every function module as v.
 */
void
draw_init(unsigned ver, struct qr *q)
{
	assert(q != NULL);

	// Initialize QR Code
	q->size = QR_SIZE(ver);
	memset(q->map, 0, QR_BUF_LEN(ver));

	// Fill horizontal and vertical timing patterns
	fill(6, 0, 1, q->size, q);
	fill(0, 6, q->size, 1, q);

	// Fill 3 finder patterns (all corners except bottom right) and format bits
	fill(0, 0, 9, 9, q);
	fill(q->size - 8, 0, 8, 9, q);
	fill(0, q->size - 8, 9, 8, q);

	// Fill numerous alignment patterns
	unsigned alignPatPos[QR_ALIGN_MAX] = { 0 };
	int n = getAlignmentPatternPositions(ver, alignPatPos);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))
				continue;  // Skip the three finder corners
			else
				fill(alignPatPos[i] - 2, alignPatPos[j] - 2, 5, 5, q);
		}
	}

	// Fill version blocks
	if (ver >= 7) {
		fill(q->size - 11, 0, 3, 6, q);
		fill(0, q->size - 11, 6, 3, q);
	}
}

/*
 * Draws white function modules and possibly some v modules onto the given QR Code, without changing
 * non-function modules. This does not draw the format bits. This requires all function modules to be previously
 * marked v (namely by draw_init()), because this may skip redrawing v function modules.
 */
static void
draw_white_function_modules(struct qr *q, unsigned ver)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	// Draw horizontal and vertical timing patterns
	for (size_t i = 7; i < q->size - 7; i += 2) {
		qr_set_module(q, 6, i, false);
		qr_set_module(q, i, 6, false);
	}

	// Draw 3 finder patterns (all corners except bottom right; overwrites some timing modules)
	for (int i = -4; i <= 4; i++) {
		for (int j = -4; j <= 4; j++) {
			int dist = abs(i);
			if (abs(j) > dist)
				dist = abs(j);
			if (dist == 2 || dist == 4) {
				set_module_bounded(q, 3 + j, 3 + i, false);
				set_module_bounded(q, q->size - 4 + j, 3 + i, false);
				set_module_bounded(q, 3 + j, q->size - 4 + i, false);
			}
		}
	}

	// Draw numerous alignment patterns
	unsigned alignPatPos[QR_ALIGN_MAX] = { 0 };
	int n = getAlignmentPatternPositions(ver, alignPatPos);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))
				continue;  // Skip the three finder corners
			else {
				for (int k = -1; k <= 1; k++) {
					for (int l = -1; l <= 1; l++)
						qr_set_module(q, alignPatPos[i] + l, alignPatPos[j] + k, k == 0 && l == 0);
				}
			}
		}
	}

	// Draw version blocks
	if (ver >= 7) {
		// Calculate error correction code and pack bits
		int rem = ver;  // version is uint6, in the range [7, 40]
		for (int i = 0; i < 12; i++)
			rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
		long data = (long) ver << 12 | rem;  // uint18
		assert(data >> 18 == 0);

		// Draw two copies
		for (int i = 0; i < 6; i++) {
			for (int j = 0; j < 3; j++) {
				int k = q->size - 11 + j;
				qr_set_module(q, k, i, data & 1);
				qr_set_module(q, i, k, data & 1);
				data >>= 1;
			}
		}
	}
}

/*
 * Draws two copies of the format bits (with its own error correction code) based
 * on the given mask and error correction level. This always draws all modules of
 * the format bits, unlike drawWhiteFunctionModules() which might skip v modules.
 */
static void
draw_format(enum qr_ecl ecl, enum qr_mask mask, struct qr *q)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	// Calculate error correction code and pack bits
	assert(0 <= mask && mask <= 7);
	int data = (int) ecl;
	data = data << 3 | mask;  // ecl-derived value is uint2, mask is uint3
	int rem = data;
	for (int i = 0; i < 10; i++)
		rem = (rem << 1) ^ ((rem >> 9) * 0x537);
	data = data << 10 | rem;
	data ^= 0x5412;  // uint15
	assert(data >> 15 == 0);

	// Draw first copy
	for (int i = 0; i <= 5; i++)
		qr_set_module(q, 8, i, (data >> i) & 1);
	qr_set_module(q, 8, 7, (data >> 6) & 1);
	qr_set_module(q, 8, 8, (data >> 7) & 1);
	qr_set_module(q, 7, 8, (data >> 8) & 1);
	for (int i = 9; i < 15; i++)
		qr_set_module(q, 14 - i, 8, (data >> i) & 1);

	// Draw second copy
	for (int i = 0; i <= 7; i++)
		qr_set_module(q, q->size - 1 - i, 8, (data >> i) & 1);
	for (int i = 8; i < 15; i++)
		qr_set_module(q, 8, q->size - 15 + i, (data >> i) & 1);
	qr_set_module(q, 8, q->size - 8, true);
}

/*
 * Draws the raw codewords (including data and ECL) onto the given QR Code.
 * This requires the initial state of the QR Code to be v at function modules
 * and white at codeword modules (including unused remainder bits).
 */
static void
draw_codewords(const void *data, size_t len, struct qr *q)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	const uint8_t *p = data;
	unsigned i = 0;  // Bit index into the data

	// Do the funny zigzag scan
	for (int right = q->size - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
		if (right == 6)
			right = 5;

		for (size_t vert = 0; vert < q->size; vert++) {  // Vertical counter
			for (int j = 0; j < 2; j++) {
				unsigned x = right - j;  // Actual x coordinate
				bool upward = ((right + 1) & 2) == 0;
				unsigned y = upward ? q->size - 1 - vert : vert;  // Actual y coordinate
				if (!qr_get_module(q, x, y) && i < len * 8) {
					bool v = (p[BM_BYTE(i)] >> (7 - BM_BIT(i))) & 1;
					qr_set_module(q, x, y, v);
					i++;
				}
				// If there are any remainder bits (0 to 7), they are already
				// set to 0/false/white when the grid of modules was initialized
			}
		}
	}
	assert(i == len * 8);
}

/*
 * Calculates and returns the penalty score based on state of the given QR Code's current modules.
 * This is used by the automatic mask choice algorithm to find the mask pattern that yields the lowest score.
 */
static long
penalty(const struct qr *q)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

#define PENALTY_N1 3
#define PENALTY_N2 3
#define PENALTY_N3 40
#define PENALTY_N4 10

	long result = 0;

	// Adjacent modules in row having same color
	for (unsigned y = 0; y < q->size; y++) {
		bool colorX;
		for (unsigned x = 0, runX; x < q->size; x++) {
			if (x == 0 || qr_get_module(q, x, y) != colorX) {
				colorX = qr_get_module(q, x, y);
				runX = 1;
			} else {
				runX++;
				if (runX == 5)
					result += PENALTY_N1;
				else if (runX > 5)
					result++;
			}
		}
	}
	// Adjacent modules in column having same color
	for (unsigned x = 0; x < q->size; x++) {
		bool colorY;
		for (unsigned y = 0, runY; y < q->size; y++) {
			if (y == 0 || qr_get_module(q, x, y) != colorY) {
				colorY = qr_get_module(q, x, y);
				runY = 1;
			} else {
				runY++;
				if (runY == 5)
					result += PENALTY_N1;
				else if (runY > 5)
					result++;
			}
		}
	}

	// 2*2 blocks of modules having same color
	for (unsigned y = 0; y < q->size - 1; y++) {
		for (unsigned x = 0; x < q->size - 1; x++) {
			bool  color = qr_get_module(q, x, y);
			if (  color == qr_get_module(q, x + 1, y) &&
			      color == qr_get_module(q, x, y + 1) &&
			      color == qr_get_module(q, x + 1, y + 1))
				result += PENALTY_N2;
		}
	}

	// Finder-like pattern in rows
	for (unsigned y = 0; y < q->size; y++) {
		for (unsigned x = 0, bits = 0; x < q->size; x++) {
			bits = ((bits << 1) & 0x7FF) | (qr_get_module(q, x, y) ? 1 : 0);
			if (x >= 10 && (bits == 0x05D || bits == 0x5D0))  // Needs 11 bits accumulated
				result += PENALTY_N3;
		}
	}
	// Finder-like pattern in columns
	for (unsigned x = 0; x < q->size; x++) {
		for (unsigned y = 0, bits = 0; y < q->size; y++) {
			bits = ((bits << 1) & 0x7FF) | (qr_get_module(q, x, y) ? 1 : 0);
			if (y >= 10 && (bits == 0x05D || bits == 0x5D0))  // Needs 11 bits accumulated
				result += PENALTY_N3;
		}
	}

	// Balance of v and white modules
	unsigned v = 0;
	for (unsigned y = 0; y < q->size; y++) {
		for (unsigned x = 0; x < q->size; x++) {
			if (qr_get_module(q, x, y))
				v++;
		}
	}

	size_t total = q->size * q->size;
	// Find smallest k such that (45-5k)% <= dark/total <= (55+5k)%
	for (unsigned k = 0; v * 20L < (9L - k) * total || v * 20L > (11L + k) * total; k++) {
		result += PENALTY_N4;
	}

	return result;
}

/*
 * Renders a QR Code symbol representing the given data segments
 * with the given encoding parameters.
 * Returns true if QR Code creation succeeded, or false if the data is
 * too long to fit in the range of versions.
 *
 * The smallest possible QR Code version within the given range is
 * automatically chosen for the output.
 *
 * This function allows the user to create a custom sequence of segments
 * that switches between modes (such as alphanumeric and binary) to
 * encode text more efficiently.
 *
 * This function is considered to be lower level than simply encoding text
 * or binary data.
 *
 * To save memory, the segments' data buffers can alias/overlap tmp,
 * and will result in them being clobbered, but the QR Code output will
 * still be correct. But the q->map array must not overlap tmp or any
 * segment's data buffer.
 *
 * QR_MODE_ALNUM/QR_MODE_NUMERIC:
 *
 * - The input string must be encoded in UTF-8 and be nul-terminated.
 * - The arrays tmp and q->map must each have a length
 *   of at least QR_BUF_LEN(max).
 * - After the function returns, tmp contains no useful data.
 * - In the most optimistic case, a QR Code at version 40 with low ECL
 *   can hold any alphanumeric string up to 4296 characters,
 *   or any digit string up to 7089 characters.
 *   These numbers represent the hard upper limit of the QR Code standard.
 *
 * QR_MODE_BYTE:
 *
 * - The input array range a[0 : dataLen] should normally be
 *   valid UTF-8 text, but is not required by the QR Code standard.
 * - The arrays tmp and q->map must each have a length
 *   of at least QR_BUF_LEN(max).
 * - After the function returns, the contents of p may have changed,
 *   and does not represent useful data anymore.
 * - If successful, the resulting QR Code will use byte mode to encode the data.
 * - In the most optimistic case, a QR Code at version 40 with low ECL can hold any byte
 *   sequence up to length 2953. This is the hard upper limit of the QR Code standard.
 *
 * Please consult the QR Code specification for information on
 * data capacities per version, ECL level, and text encoding mode.
 */
bool
qr_encode(struct qr_segment * const a[], size_t n,
	enum qr_ecl ecl,
	unsigned min, unsigned max,
	int mask,
	bool boost_ecl,
	void *tmp, struct qr *q)
{
	assert(a != NULL || n == 0);
	assert(QR_VER_MIN <= min && min <= max && max <= QR_VER_MAX);
	assert(0 <= ecl && ecl <= 3);
	assert(-1 <= mask && mask <= 7);

	// Find the minimal version number to use
	unsigned ver;
	int dataUsedBits;
	for (ver = min; ; ver++) {
		int dataCapacityBits = count_codewords(ver, ecl) * 8;  // Number of data bits available
		dataUsedBits = count_total_bits(a, n, ver);
		if (dataUsedBits != -1 && dataUsedBits <= dataCapacityBits)
			break;  // This version number is found to be suitable
		if (ver >= max) {  // All versions in the range could not fit the given data
			errno = EMSGSIZE;
			return false;
		}
	}
	assert(dataUsedBits != -1);

	// Increase the error correction level while the data still fits in the current version number
	if (boost_ecl) {
		const enum qr_ecl e[] = {
			QR_ECL_LOW,
			QR_ECL_MEDIUM,
			QR_ECL_QUARTILE,
			QR_ECL_HIGH
		};

		for (size_t i = 0; i < sizeof e / sizeof *e; i++) {
			if (dataUsedBits <= count_codewords(ver, e[i]) * 8) {
				ecl = e[i];
			}
		}
	}

	/*
	 * QR 2005 6.4.8.1 FNC1 in first position
	 * "... shall only be used once in a symbol ..."
	 * "... shall be placed immediately before the first mode indicator used
	 * for efficient data encoding (Numeric, Alphanumeric, Byte or Kanji),
	 * and after any ECI or Structured Append header."
	 *
	 * FNC2 is the same, except followed by a byte defined with some external
	 * meaning registered with AIM.
	 */
	/*
	 * TODO: implement as state kept when iterating over segments below,
	 * and emit these at the appropriate point.
	 */

	// Create the data bit string by concatenating all segments
	size_t dataCapacityBits = count_codewords(ver, ecl) * 8;
	memset(q->map, 0, QR_BUF_LEN(ver));
	size_t count = 0;
	for (size_t i = 0; i < n; i++) {
		size_t len;

		switch (a[i]->mode) {
		case QR_MODE_BYTE:
			len = BM_LEN(a[i]->u.m.bits);
			break;

		case QR_MODE_NUMERIC:
		case QR_MODE_ALNUM:
		case QR_MODE_KANJI:
			len = strlen(a[i]->u.s);
			break;

		case QR_MODE_ECI:
			len = 0;
			return 0;

		default:
			assert(false);
		}

		append_bits(a[i]->mode, 4, q->map, &count);
		append_bits(len, count_char_bits(a[i]->mode, ver), q->map, &count);

		for (size_t j = 0; j < a[i]->m.bits; j++) {
			append_bits((((const uint8_t *) a[i]->m.data)[BM_BYTE(j)] >> (7 - BM_BIT(j))) & 1, 1, q->map, &count);
		}
	}

	/*
	 * QR 2005 6.4.9 Terminator "The end of data in the symbol is signalled
	 * by the Terminator sequence of 0 bits, ... following the final mode segment.
	 * ... shall be omitted if the data bit stream completely fills the capacity
	 * of the symbol, or abbreviated if the remaining capacity of the symbol is
	 * less than 4 bits."
	 */
	// pad up to a byte if applicable
	int terminatorBits = dataCapacityBits - count;
	if (terminatorBits > 4)
		terminatorBits = 4;
	append_bits(0, terminatorBits, q->map, &count);
	append_bits(0, (8 - count % 8) % 8, q->map, &count);

	/*
	 * QR 2005 6.4.10 "The message bit stream shall then be extended to fill
	 * the data capacity ... by adding the Pad Codewords 11101100 and 00010001
	 * alternately."
	 */
	for (uint8_t padByte = 0xEC; count < dataCapacityBits; padByte ^= 0xEC ^ 0x11)
		append_bits(padByte, 8, q->map, &count);
	assert(count % 8 == 0);

	// Draw function and data codeword modules
	append_ecl(q->map, ver, ecl, tmp);
	draw_init(ver, q);
	draw_codewords(tmp, count_data_bits(ver) / 8, q);
	draw_white_function_modules(q, ver);

	// Handle masking
	if (mask == QR_MASK_AUTO) {
		long curr = LONG_MAX;
		for (int i = 0; i < 8; i++) {
			draw_format(ecl, i, q);
			qr_apply_mask(q, i);
			long w = penalty(q);
			if (w < curr) {
				mask = i;
				curr = w;
			}
			qr_apply_mask(q, i);  // Undoes the mask due to XOR
		}
	}

	assert(0 <= (int) mask && (int) mask <= 7);
	draw_format(ecl, mask, q);
	qr_apply_mask(q, mask);

	return true;
}

