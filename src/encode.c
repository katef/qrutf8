
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
#include <ctype.h>

#include <qr.h>

#include "internal.h"

/*
 * The mask pattern used in a QR Code symbol.
 */
enum qr_mask {
	// A special value to tell the QR Code encoder to
	// automatically select an appropriate mask pattern
	QR_MASK_AUTO = -1,

	// The eight actual mask patterns
	QR_MASK_0 = 0,
	QR_MASK_1,
	QR_MASK_2,
	QR_MASK_3,
	QR_MASK_4,
	QR_MASK_5,
	QR_MASK_6,
	QR_MASK_7
};

static const char ALNUM_CHARSET[] =
	"0123456789"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	" $%*+-./:";

static unsigned int
charset_index(const char *charset, char c)
{
	const char *p;

	p = strchr(charset, c);
	assert(p != NULL);

	return p - charset;
}

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
static unsigned
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
 * Returns the number of data bits needed to represent a segment
 * containing the given number of characters using the given mode. Notes:
 * - Returns -1 on failure, i.e. len > INT16_MAX or
 *   the number of needed bits exceeds INT16_MAX (i.e. 32767).
 * - Otherwise, all valid results are in the range [0, INT16_MAX].
 * - For byte mode, len measures the number of bytes, not Unicode code points.
 * - For ECI mode, len must be 0, and the worst-case number of bits is returned.
 *   An actual ECI segment can have shorter data. For non-ECI modes, the result is exact.
 */
static int
count_seg_bits(enum qr_mode mode, size_t len)
{
	const int LIMIT = INT16_MAX;  // Can be configured as high as INT_MAX

	if (len > (size_t) LIMIT) {
		errno = EMSGSIZE;
		return -1;
	}

	int n = len;
	int r = -2;

	switch (mode) {
		int v;

	case QR_MODE_NUMERIC:
		// n * 3 + ceil(n / 3)
		if (n > LIMIT / 3)
			goto overflow;
		r = n * 3;
		v = n / 3 + !!(n % 3);
		if (v > LIMIT - r)
			goto overflow;
		r += v;
		break;

	case QR_MODE_ALNUM:
		// n * 5 + ceil(n / 2)
		if (n > LIMIT / 5)
			goto overflow;
		r = n * 5;
		v = n / 2 + n % 2;
		if (v > LIMIT - r)
			goto overflow;
		r += v;
		break;

	case QR_MODE_BYTE:
		if (n > LIMIT / 8)
			goto overflow;
		r = n * 8;
		break;

	case QR_MODE_KANJI:
		if (n > LIMIT / 13)
			goto overflow;
		r = n * 13;
		break;

	case QR_MODE_ECI:
		if (len != 0) {
			goto overflow;
		}
		r = 3 * 8;
		break;
	}

	assert(0 <= r && r <= LIMIT);

	return r;

overflow:

	errno = ERANGE;

	return -1;
}

/*
 * Returns the bit width of the segment character count field
 * for the given mode at the given version number.
 *
 * The result is in the range [0, 16].
 */
static int
count_char_bits(enum qr_mode mode, unsigned ver)
{
	int i = -1;  // Dummy value

	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	if      ( 1 <= ver && ver <=  9) { i = 0; }
	else if (10 <= ver && ver <= 26) { i = 1; }
	else if (27 <= ver && ver <= 40) { i = 2; }
	else { assert(false); }

	switch (mode) {
	case QR_MODE_NUMERIC: { return (const int []) { 10, 12, 14 } [i]; }
	case QR_MODE_ALNUM:   { return (const int []) {  9, 11, 13 } [i]; }
	case QR_MODE_BYTE:    { return (const int []) {  8, 16, 16 } [i]; }
	case QR_MODE_KANJI:   { return (const int []) {  8, 10, 12 } [i]; }

	case QR_MODE_ECI:
		return 0;

	default:
		assert(false);
	}

	return -1;  // Dummy value
}

/*
 * Returns the number of bits needed to encode the given list of segments
 * at the given version.
 *
 * The result is in the range [0, 32767] if successful.
 * Otherwise, -1 is returned if any segment has more characters than allowed
 * by that segment's mode's character count field at the version,
 * or if the actual answer exceeds INT16_MAX.
 */
static int
count_total_bits(const struct qr_segment segs[], size_t n, unsigned ver)
{
	int len = 0;

	assert(segs != NULL || n == 0);
	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	for (size_t i = 0; i < n; i++) {
		assert(segs[i].len <= INT16_MAX);
		assert(segs[i].count <= INT16_MAX);

		int ccbits = count_char_bits(segs[i].mode, ver);
		assert(0 <= ccbits && ccbits <= 16);

		// Fail if segment length value doesn't fit in the length field's bit-width
		/* XXX: i don't understand why this is neccessary; remove .len from the encoder? */
		if (segs[i].len >= (1UL << ccbits))
			return -1;

		long tmp = 4L + ccbits + segs[i].count;
		if (tmp > INT16_MAX - len)
			return -1;

		len += tmp;
	}

	assert(0 <= len && len <= INT16_MAX);

	return len;
}

/*
 * Returns the number of bytes (uint8_t) needed for the data buffer of a segment
 * containing the given number of characters using the given mode. Notes:
 * - Returns SIZE_MAX on failure, i.e. len > INT16_MAX or
 *   the number of needed bits exceeds INT16_MAX (i.e. 32767).
 * - Otherwise, all valid results are in the range [0, ceil(INT16_MAX / 8)], i.e. at most 4096.
 * - It is okay for the user to allocate more bytes for the buffer than needed.
 * - For byte mode, len measures the number of bytes, not Unicode code points.
 * - For ECI mode, len must be 0, and the worst-case number of bytes is returned.
 *   An actual ECI segment can have shorter data. For non-ECI modes, the result is exact.
 */
size_t
qr_calcSegmentBufferSize(enum qr_mode mode, size_t len)
{
	int n;

	n = count_seg_bits(mode, len);
	if (n == -1) {
		return SIZE_MAX;
	}

	assert(0 <= n && n <= INT16_MAX);

	return BM_LEN((size_t) n);
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
 * Appends the given sequence of bits to the given byte-based bit buffer,
 * increasing the bit length.
 */
static void
append_bits(unsigned v, size_t n, void *buf, size_t *count)
{
	assert(n <= 16 && v >> n == 0);

	for (int i = n - 1; i >= 0; i--, (*count)++) {
		((uint8_t *) buf)[BM_BYTE(*count)] |= ((v >> i) & 1) << (7 - BM_BIT(*count));
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
 * Returns a segment representing the given binary data encoded in byte mode.
 */
struct qr_segment
qr_make_bytes(const void *data, size_t len)
{
	struct qr_segment seg;
	int count;

	assert(data != NULL || len == 0);

	count = count_seg_bits(QR_MODE_BYTE, len);
	assert(count != -1);

	seg.mode  = QR_MODE_BYTE;
	seg.len   = len;
	seg.data  = data;
	seg.count = count;

	return seg;
}

/*
 * Returns a segment representing the given string of decimal digits encoded in numeric mode.
 */
struct qr_segment
qr_make_numeric(const char *s, void *buf)
{
	struct qr_segment seg;
	const char *p;
	int count;
	size_t len;
	size_t rcount;

	assert(s != NULL);
	assert(buf != NULL || strlen(s) == 0);

	len = strlen(s);

	count = count_seg_bits(QR_MODE_NUMERIC, len);
	assert(count != -1);

	if (count > 0) {
		memset(buf, 0, BM_LEN(count));
	}

	rcount = 0;

	unsigned n = 0;
	int digits = 0;
	for (p = s; *p != '\0'; p++) {
		assert('0' <= *p && *p <= '9');
		n *= 10;
		n += (*p - '0');
		digits++;
		if (digits == 3) {
			append_bits(n, 10, buf, &rcount);
			n = 0;
			digits = 0;
		}
	}

	if (digits > 0) {  // 1 or 2 digits remaining
		append_bits(n, digits * 3 + 1, buf, &rcount);
	}

	assert(rcount == (size_t) count);
	/* XXX: then why memset at the start? */

	seg.mode  = QR_MODE_NUMERIC;
	seg.len   = len;
	seg.data  = buf;
	seg.count = rcount;

	return seg;
}

/*
 * Returns a segment representing the given text string encoded in alphanumeric mode.
 * The characters allowed are: 0 to 9, A to Z (uppercase only), space,
 * dollar, percent, asterisk, plus, hyphen, period, slash, colon.
 */
struct qr_segment
qr_make_alnum(const char *s, void *buf)
{
	struct qr_segment seg;
	const char *p;
	size_t rcount;
	int count;

	size_t len = strlen(s);

	assert(s != NULL);

	count = count_seg_bits(QR_MODE_ALNUM, len);
	assert(count != -1);

	if (count > 0) {
		memset(buf, 0, BM_LEN(count));
	}

/* TODO: centralise with digits encoding; this is just base 45 */

	rcount = 0;

	unsigned accumData = 0;
	int accumCount = 0;
	for (p = s; *p != '\0'; p++) {
		accumData = accumData * 45 + charset_index(ALNUM_CHARSET, *p);
		accumCount++;
		if (accumCount == 2) {
			append_bits(accumData, 11, buf, &rcount);
			accumData  = 0;
			accumCount = 0;
		}
	}

	if (accumCount > 0) { // 1 character remaining
		append_bits(accumData, 6, buf, &rcount);
	}

	assert(rcount == (size_t) count);
	/* XXX: then why memset at the start? */

	seg.mode  = QR_MODE_ALNUM;
	seg.len   = len;
	seg.data  = buf;
	seg.count = rcount;

	return seg;
}

/*
 * Returns a segment representing an Extended Channel Interpretation
 * (ECI) designator with the given assignment value.
 */
struct qr_segment
qr_make_eci(long assignVal, void *buf)
{
	struct qr_segment seg;
	size_t rcount;

	rcount = 0;

	if (0 <= assignVal && assignVal < (1 << 7)) {
		memset(buf, 0, 1);
		append_bits(assignVal, 8, buf, &rcount);
	} else if ((1 << 7) <= assignVal && assignVal < (1 << 14)) {
		memset(buf, 0, 2);
		append_bits(2, 2, buf, &rcount);
		append_bits(assignVal, 14, buf, &rcount);
	} else if ((1 << 14) <= assignVal && assignVal < 1000000L) {
		memset(buf, 0, 3);
		append_bits(6, 3, buf, &rcount);
		append_bits(assignVal >> 10, 11, buf, &rcount);
		append_bits(assignVal & 0x3FF, 10, buf, &rcount);
	} else {
		assert(false);
	}

	seg.mode  = QR_MODE_ECI;
	seg.len   = 0;
	seg.data  = buf;
	seg.count = rcount;

	return seg;
}

/*
 * Tests whether the given string can be encoded as a segment in alphanumeric mode.
 */
bool
qr_isalnum(const char *s)
{
	const char *p;

	assert(s != NULL);

	for (p = s; *p != '\0'; p++) {
		if (strchr(ALNUM_CHARSET, *p) == NULL) {
			return false;
		}
	}

	return true;
}

/*
 * Tests whether the given string can be encoded as a segment in numeric mode.
 */
bool
qr_isnumeric(const char *s)
{
	const char *p;

	assert(s != NULL);

	for (p = s; *p != '\0'; p++) {
		if (!isdigit((unsigned char) *p)) {
			return false;
		}
	}

	return true;
}

/*---- Modules (pixels) ----*/

bool
qr_get_module(const struct qr *q, unsigned x, unsigned y)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));
	assert(x < q->size && y < q->size);

	return BM_GET(q->map, y * q->size + x);
}

void
qr_set_module(struct qr *q, unsigned x, unsigned y, bool v)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));
	assert(x < q->size && y < q->size);

	if (v) {
		BM_SET(q->map, y * q->size + x);
	} else {
		BM_CLR(q->map, y * q->size + x);
	}
}

// Sets the module at the given coordinates, doing nothing if out of bounds.
static void
set_module_bounded(struct qr *q, unsigned x, unsigned y, bool v)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	if (x < q->size && y < q->size) {
		qr_set_module(q, x, y, v);
	}
}

// Sets every pixel in the range [left : left + width] * [top : top + height] to v.
static void
fill(unsigned left, unsigned top, unsigned width, unsigned height, struct qr *q)
{
	assert(q != NULL);

	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			qr_set_module(q, left + x, top + y, true);
		}
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
	int data = -1;  // Dummy value
	switch (ecl) {
	case QR_ECL_LOW     :  data = 1;  break;
	case QR_ECL_MEDIUM  :  data = 0;  break;
	case QR_ECL_QUARTILE:  data = 3;  break;
	case QR_ECL_HIGH    :  data = 2;  break;
	default:  assert(false);
	}
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
 * XORs the data modules in this QR Code with the given mask pattern. Due to XOR's mathematical
 * properties, calling apply_mask(..., m) twice with the same value is equivalent to no change at all.
 * This means it is possible to apply a mask, undo it, and try another mask. Note that a final
 * well-formed QR Code symbol needs exactly one mask applied (not zero, not two, etc.).
 */
static void
apply_mask(const uint8_t *functionModules, struct qr *q, enum qr_mask mask)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));
	assert(0 <= mask && mask <= 7);  // Disallows QR_MASK_AUTO

	for (unsigned y = 0; y < q->size; y++) {
		for (unsigned x = 0; x < q->size; x++) {
			struct qr tmp;

			tmp.size = q->size;
			tmp.map  = (void *) functionModules;

			if (qr_get_module(&tmp, x, y)) {
				continue;
			}

			bool invert = false;  // Dummy value
			switch (mask) {
			case QR_MASK_0: invert = (x + y) % 2 == 0;                   break;
			case QR_MASK_1: invert = y % 2 == 0;                         break;
			case QR_MASK_2: invert = x % 3 == 0;                         break;
			case QR_MASK_3: invert = (x + y) % 3 == 0;                   break;
			case QR_MASK_4: invert = (x / 3 + y / 2) % 2 == 0;           break;
			case QR_MASK_5: invert = x * y % 2 + x * y % 3 == 0;         break;
			case QR_MASK_6: invert = (x * y % 2 + x * y % 3) % 2 == 0;   break;
			case QR_MASK_7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;

			case QR_MASK_AUTO:
				assert(!"unreached");
				break;
			}

			qr_set_module(q, x, y, qr_get_module(q, x, y) ^ invert);
		}
	}
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

/*---- High-level QR Code encoding functions ----*/

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
 */
bool
qr_encode_segments(const struct qr_segment segs[], size_t len, enum qr_ecl ecl,
	unsigned min, unsigned max, int mask, bool boost_ecl, void *tmp, struct qr *q)
{
	assert(segs != NULL || len == 0);
	assert(QR_VER_MIN <= min && min <= max && max <= QR_VER_MAX);
	assert(0 <= ecl && ecl <= 3 && -1 <= mask && mask <= 7);

	// Find the minimal version number to use
	unsigned ver;
	int dataUsedBits;
	for (ver = min; ; ver++) {
		int dataCapacityBits = count_codewords(ver, ecl) * 8;  // Number of data bits available
		dataUsedBits = count_total_bits(segs, len, ver);
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
		for (int i = QR_ECL_MEDIUM; i <= QR_ECL_HIGH; i++) {
			if (dataUsedBits <= count_codewords(ver, i) * 8) {
				ecl = i;
			}
		}
	}

	// Create the data bit string by concatenating all segments
	size_t dataCapacityBits = count_codewords(ver, ecl) * 8;
	memset(q->map, 0, QR_BUF_LEN(ver));
	size_t count = 0;
	for (size_t i = 0; i < len; i++) {
		const struct qr_segment *seg = &segs[i];
		append_bits(seg->mode, 4, q->map, &count);
		append_bits(seg->len, count_char_bits(seg->mode, ver), q->map, &count);
		for (size_t j = 0; j < seg->count; j++) {
			append_bits((((const uint8_t *) seg->data)[BM_BYTE(j)] >> (7 - BM_BIT(j))) & 1, 1, q->map, &count);
		}
	}

	// Add terminator and pad up to a byte if applicable
	int terminatorBits = dataCapacityBits - count;
	if (terminatorBits > 4)
		terminatorBits = 4;
	append_bits(0, terminatorBits, q->map, &count);
	append_bits(0, (8 - count % 8) % 8, q->map, &count);

	// Pad with alternate bytes until data capacity is reached
	for (uint8_t padByte = 0xEC; count < dataCapacityBits; padByte ^= 0xEC ^ 0x11)
		append_bits(padByte, 8, q->map, &count);
	assert(count % 8 == 0);

	// Draw function and data codeword modules
	struct qr qtmp;
	qtmp.map = tmp;
	append_ecl(q->map, ver, ecl, qtmp.map);
	draw_init(ver, q);
	draw_codewords(qtmp.map, count_data_bits(ver) / 8, q);
	draw_white_function_modules(q, ver);
	draw_init(ver, &qtmp);

	// Handle masking
	if (mask == QR_MASK_AUTO) {  // Automatically choose best mask
		long curr = LONG_MAX;
		for (int i = 0; i < 8; i++) {
			draw_format(ecl, i, q);
			apply_mask(qtmp.map, q, i);
			long n = penalty(q);
			if (n < curr) {
				mask = i;
				curr = n;
			}
			apply_mask(qtmp.map, q, i);  // Undoes the mask due to XOR
		}
	}

	assert(0 <= (int) mask && (int) mask <= 7);
	draw_format(ecl, mask, q);
	apply_mask(qtmp.map, q, mask);

	return true;
}

/*
 * Encodes the given text string to a QR Code symbol, returning true if encoding succeeded.
 * If the data is too long to fit in any version in the given range
 * at the given ECL level, then false is returned.
 * - The input tring ust be encoded in UTF-8 and be nul-terminated.
 * - The variables ecl and mask must correspond to enum constant values.
 * - Requires 1 <= min <= max <= 40.
 * - The arrays tmp and q->map must each have a length
 *   of at least QR_BUF_LEN(max).
 * - After the function returns, tmp contains no useful data.
 * - If successful, the resulting QR Code may use numeric,
 *   alphanumeric, or byte mode to encode the text.
 * - In the most optimistic case, a QR Code at version 40 with low ECL
 *   can hold any UTF-8 string up to 2953 bytes, or any alphanumeric string
 *   up to 4296 characters, or any digit string up to 7089 characters.
 *   These numbers represent the hard upper limit of the QR Code standard.
 * - Please consult the QR Code specification for information on
 *   data capacities per version, ECL level, and text encoding mode.
 */
bool
qr_encode_str(const char *s, void *tmp, struct qr *q,
	enum qr_ecl ecl, unsigned min, unsigned max, enum qr_mask mask, bool boost_ecl)
{
	struct qr_segment seg;

	size_t sLen = strlen(s);
	if (sLen == 0)
		return qr_encode_segments(NULL, 0, ecl, min, max, mask, boost_ecl, tmp, q);
	size_t bufLen = QR_BUF_LEN(max);

	if (qr_isnumeric(s)) {
		if (qr_calcSegmentBufferSize(QR_MODE_NUMERIC, sLen) > bufLen)
			goto error;
		seg = qr_make_numeric(s, tmp);
	} else if (qr_isalnum(s)) {
		if (qr_calcSegmentBufferSize(QR_MODE_ALNUM, sLen) > bufLen)
			goto error;
		seg = qr_make_alnum(s, tmp);
	} else {
		if (sLen > bufLen)
			goto error;
		seg = qr_make_bytes(s, sLen);
	}

	return qr_encode_segments(&seg, 1, ecl, min, max, mask, boost_ecl, tmp, q);

error:

	errno = EMSGSIZE;

	return false;
}

/*
 * Encodes the given binary data to a QR Code symbol, returning true if encoding succeeded.
 * If the data is too long to fit in any version in the given range
 * at the given ECL level, then false is returned.
 * - The input array range a[0 : dataLen] should normally be
 *   valid UTF-8 text, but is not required by the QR Code standard.
 * - The variables ecl and mask must correspond to enum constant values.
 * - Requires 1 <= min <= max <= 40.
 * - The arrays tmp and q->map must each have a length
 *   of at least QR_BUF_LEN(max).
 * - After the function returns, the contents of p may have changed,
 *   and does not represent useful data anymore.
 * - If successful, the resulting QR Code will use byte mode to encode the data.
 * - In the most optimistic case, a QR Code at version 40 with low ECL can hold any byte
 *   sequence up to length 2953. This is the hard upper limit of the QR Code standard.
 * - Please consult the QR Code specification for information on
 *   data capacities per version, ECL level, and text encoding mode.
 */
bool
qr_encode_bytes(const void *data, size_t len, void *tmp, struct qr *q,
	enum qr_ecl ecl, unsigned min, unsigned max, enum qr_mask mask, bool boost_ecl)
{
	struct qr_segment seg;

	seg.mode  = QR_MODE_BYTE;
	int count = count_seg_bits(seg.mode, len);
	if (count == -1) {
		return false;
	}

	seg.len   = len;
	seg.data  = data;
	seg.count = count;

	return qr_encode_segments(&seg, 1, ecl, min, max, mask, boost_ecl, tmp, q);
}

