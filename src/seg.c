
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

#include <eci.h>
#include <qr.h>

#include "internal.h"
#include "util.h"
#include "seg.h"

size_t
seg_len(struct qr_segment * const a[], size_t n)
{
	size_t len;
	size_t j;

	assert(a != NULL);

	len = 0;

	for (j = 0; j < n; j++) {
		switch (a[j]->mode) {
		case QR_MODE_BYTE:
			len += a[j]->u.m.bits * 8; // padded to a byte
			break;

		case QR_MODE_NUMERIC:
		case QR_MODE_ALNUM:
		case QR_MODE_KANJI:
			len += strlen(a[j]->u.s);
			break;

		case QR_MODE_ECI:
			break;
		}
	}

	return len;
}

bool
seg_cmp(
	struct qr_segment * const a[], size_t an,
	struct qr_segment * const b[], size_t bn)
{
	size_t j;

	assert(a != NULL);
	assert(b != NULL);

	if (an != bn) {
		return false;
	}

	if (seg_len(a, an) != seg_len(b, bn)) {
		return false;
	}

	for (j = 0; j < an; j++) {
		if (a[j]->mode != b[j]->mode) {
			return false;
		}

		switch (b[j]->mode) {
		case QR_MODE_BYTE:
			if (a[j]->u.m.bits != b[j]->u.m.bits) {
				return false;
			}

			if (0 != memcmp(a[j]->u.m.data, b[j]->u.m.data, BM_LEN(b[j]->u.m.bits))) {
				return false;
			}
			break;

		case QR_MODE_NUMERIC:
		case QR_MODE_ALNUM:
		case QR_MODE_KANJI:
			if (strlen(a[j]->u.s) != strlen(b[j]->u.s)) {
				return false;
			}

			if (0 != strcmp(a[j]->u.s, b[j]->u.s)) {
				return false;
			}
			break;

		case QR_MODE_ECI:
			if (a[j]->u.eci != b[j]->u.eci) {
				return false;
			}
			break;
		}
	}

	return true;
}

static unsigned int
charset_index(const char *charset, char c)
{
	const char *p;

	p = strchr(charset, c);
	assert(p != NULL);

	return p - charset;
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

	/* XXX: why return the worst case here? */
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
int
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
int
count_total_bits(struct qr_segment * const a[], size_t n, unsigned ver)
{
	int len = 0;

	assert(a != NULL || n == 0);
	assert(QR_VER_MIN <= ver && ver <= QR_VER_MAX);

	for (size_t i = 0; i < n; i++) {
		assert(a[i]->m.bits <= INT16_MAX);

		int ccbits = count_char_bits(a[i]->mode, ver);
		assert(0 <= ccbits && ccbits <= 16);

		// Fail if segment length value doesn't fit in the length field's bit-width
		switch (a[i]->mode) {
		case QR_MODE_BYTE:
			assert(a[i]->u.m.bits <= INT16_MAX);
			if (a[i]->u.m.bits * 8 >= (1UL << ccbits))
				return -1;
			break;

		case QR_MODE_NUMERIC:
		case QR_MODE_ALNUM:
		case QR_MODE_KANJI:
			if (strlen(a[i]->u.s) >= (1UL << ccbits))
				return -1;
			break;

		case QR_MODE_ECI:
			break;
		}

		long tmp = 4L + ccbits + a[i]->m.bits;
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

struct qr_segment *
qr_make_bytes(const void *data, size_t len)
{
	struct qr_segment *seg;
	int count;

	assert(data != NULL);
	assert(len <= sizeof seg->u.m.data);

	seg = malloc(sizeof *seg);
	if (seg == NULL) {
		return NULL;
	}

	memcpy(seg->u.m.data, data, len);
	seg->u.m.bits = len * 8;

	count = count_seg_bits(QR_MODE_BYTE, len);
	assert(count != -1);

	seg->mode   = QR_MODE_BYTE;
	seg->m.bits = count;
	memcpy(seg->m.data, data, len);

	return seg;
}

struct qr_segment *
qr_make_numeric(const char *s)
{
	struct qr_segment *seg;
	void *data;
	const char *p;
	int count;
	size_t len;
	size_t rcount;

	assert(s != NULL);

	len = strlen(s);
	assert(len <= sizeof seg->u.s - 1);

	count = count_seg_bits(QR_MODE_NUMERIC, len);
	assert(count != -1);

	seg = malloc(sizeof *seg + BM_LEN(count));
	if (seg == NULL) {
		return NULL;
	}

	data = (char *) seg + sizeof *seg;

	strcpy(seg->u.s, s);

	if (count > 0) {
		memset(data, 0, BM_LEN(count));
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
			append_bits(n, 10, data, &rcount);
			n = 0;
			digits = 0;
		}
	}

	if (digits > 0) {  // 1 or 2 digits remaining
		append_bits(n, digits * 3 + 1, data, &rcount);
	}

	assert(rcount == (size_t) count);
	/* XXX: then why memset at the start? */

	seg->mode   = QR_MODE_NUMERIC;
	seg->m.bits = rcount;
	memcpy(seg->m.data, data, BM_LEN(seg->m.bits));

	return seg;
}

struct qr_segment *
qr_make_alnum(const char *s)
{
	struct qr_segment *seg;
	void *data;
	const char *p;
	size_t rcount;
	size_t len;
	int count;

	assert(s != NULL);

	len = strlen(s);
	assert(len <= sizeof seg->u.s - 1);

	count = count_seg_bits(QR_MODE_ALNUM, len);
	assert(count != -1);

	seg = malloc(sizeof *seg + BM_LEN(count));
	if (seg == NULL) {
		return NULL;
	}

	data = (char *) seg + sizeof *seg;

	strcpy(seg->u.s, s);

	if (count > 0) {
		memset(data, 0, BM_LEN(count));
	}

/* TODO: centralise with digits encoding; this is just base 45 */

	rcount = 0;

	unsigned accumData = 0;
	int accumCount = 0;
	for (p = s; *p != '\0'; p++) {
		accumData = accumData * 45 + charset_index(ALNUM_CHARSET, *p);
		accumCount++;
		if (accumCount == 2) {
			append_bits(accumData, 11, data, &rcount);
			accumData  = 0;
			accumCount = 0;
		}
	}

	if (accumCount > 0) { // 1 character remaining
		append_bits(accumData, 6, data, &rcount);
	}

	assert(rcount == (size_t) count);
	/* XXX: then why memset at the start? */

	seg->mode   = QR_MODE_ALNUM;
	seg->m.bits = rcount;
	memcpy(seg->m.data, data, BM_LEN(seg->m.bits));

	return seg;
}

struct qr_segment *
qr_make_eci(long assignVal)
{
	struct qr_segment *seg;
	void *data;
	size_t rcount;

	seg = malloc(sizeof *seg + 3);
	if (seg == NULL) {
		return NULL;
	}

	data = (char *) seg + sizeof *seg;

	rcount = 0;

	if (0 <= assignVal && assignVal < (1 << 7)) {
		memset(data, 0, 1);
		append_bits(assignVal, 8, data, &rcount);
	} else if ((1 << 7) <= assignVal && assignVal < (1 << 14)) {
		memset(data, 0, 2);
		append_bits(2, 2, data, &rcount);
		append_bits(assignVal, 14, data, &rcount);
	} else if ((1 << 14) <= assignVal && assignVal < 1000000L) {
		memset(data, 0, 3);
		append_bits(6, 3, data, &rcount);
		append_bits(assignVal >> 10, 11, data, &rcount);
		append_bits(assignVal & 0x3FF, 10, data, &rcount);
	} else {
		assert(false);
	}

	seg->mode   = QR_MODE_ECI;
	seg->u.eci  = (enum eci) assignVal;
	seg->m.bits = rcount;
	memcpy(seg->m.data, data, BM_LEN(seg->m.bits));

	return seg;
}

struct qr_segment *
qr_make_any(const char *s)
{
	struct qr_segment *seg;

	assert(s != NULL);

	if (qr_isnumeric(s)) {
		seg = qr_make_numeric(s);
	} else if (qr_isalnum(s)) {
		seg = qr_make_alnum(s);
	} else {
		seg = qr_make_bytes(s, strlen(s));
	}

	return seg;
}

void
seg_free(struct qr_segment *seg)
{
	assert(seg != NULL);

	free(seg);
}

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

void
seg_print(FILE *f, size_t n, struct qr_segment * const a[])
{
	size_t j;

	assert(f != NULL);
	assert(a != NULL);

	enum eci eci = ECI_DEFAULT;

	printf("    Segments x%zu {\n", n);
	for (j = 0; j < n; j++) {
		const char *dts;

		switch (a[j]->mode) {
		case QR_MODE_NUMERIC: dts = "NUMERIC"; break;
		case QR_MODE_ALNUM:   dts = "ALNUM";   break;
		case QR_MODE_BYTE:    dts = "BYTE";    break;
		case QR_MODE_KANJI:   dts = "KANJI";   break;
		case QR_MODE_ECI:     dts = "ECI";     break;
		default: dts = "?"; break;
		}

		printf("    %zu: mode=%d (%s)\n", j, a[j]->mode, dts);

		switch (a[j]->mode) {
		case QR_MODE_NUMERIC:
		case QR_MODE_ALNUM:
		case QR_MODE_KANJI:
			/* TODO: iconv here, per eci */
			(void) eci;

			printf("      source string: len=%zu bytes\n", strlen(a[j]->u.s));
			if (qr_isalnum(a[j]->u.s) || qr_isnumeric(a[j]->u.s)) {
				printf("      \"%s\"\n", a[j]->u.s);
			} else {
				hexdump(stdout, (void *) a[j]->u.s, strlen(a[j]->u.s));
			}
			break;

		case QR_MODE_BYTE:
			/* TODO: iconv here, per eci */
			(void) eci;

			printf("      source string: len=%zu bytes\n", BM_LEN(a[j]->u.m.bits));
			hexdump(stdout, (void *) a[j]->u.m.data, BM_LEN(a[j]->u.m.bits));
			break;

		case QR_MODE_ECI:
			printf("      eci: %u\n", a[j]->u.eci);
			eci = a[j]->u.eci;
			break;

		default:
			break;
		}

		printf("      encoded data: %zu bits\n", a[j]->m.bits);
		hexdump(stdout, a[j]->m.data, BM_LEN(a[j]->m.bits));
	}
	printf("    }\n");
	printf("    Segments total data length: %zu\n", seg_len(a, n));
}

