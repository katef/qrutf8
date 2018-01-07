
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

/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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
#include "datastream.h"

/*
 * Appends the given sequence of bits to the given byte-based bit buffer,
 * increasing the bit length.
 */
void
append_bits(unsigned v, size_t n, void *buf, size_t *count)
{
	assert(n <= 16 && v >> n == 0);

	for (int i = n - 1; i >= 0; i--, (*count)++) {
		((uint8_t *) buf)[BM_BYTE(*count)] |= ((v >> i) & 1) << (7 - BM_BIT(*count));
	}
}

static void
read_bit(const struct qr *q,
	enum qr_mask mask,
	struct datastream *ds, int i, int j)
{
	int bitpos  = BM_BIT(ds->data_bits);
	int bytepos = BM_BYTE(ds->data_bits);
	int v = qr_get_module(q, j, i);

	assert(mask >= 0 && mask <= 7);

	if (mask_bit(mask, i, j))
		v ^= 1;

	if (v)
		ds->raw[bytepos] |= (0x80 >> bitpos);

	ds->data_bits++;
}

void
read_data(const struct qr *q,
	enum qr_mask mask,
	struct datastream *ds)
{
	int y = q->size - 1;
	int x = q->size - 1;
	int dir = -1;

	assert(mask >= 0 && mask <= 7);

	while (x > 0) {
		if (x == 6)
			x--;

		if (!reserved_module(q, y, x))
			read_bit(q, mask, ds, y, x);

		if (!reserved_module(q, y, x - 1))
			read_bit(q, mask, ds, y, x - 1);

		y += dir;
		if (y < 0 || y >= (int) q->size) {
			dir = -dir;
			x -= 2;
			y += dir;
		}
	}
}

int
bits_remaining(const struct datastream *ds)
{
	return ds->data_bits - ds->ptr;
}

int
take_bits(struct datastream *ds, int len)
{
	int ret = 0;

	while (len && (ds->ptr < ds->data_bits)) {
		uint8_t b = ds->data[ds->ptr >> 3];
		int bitpos = ds->ptr & 7;

		ret <<= 1;
		if ((b << bitpos) & 0x80)
			ret |= 1;

		ds->ptr++;
		len--;
	}

	return ret;
}

