
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
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"

bool
reserved_module(const struct qr *q, unsigned x, unsigned y)
{
	int ax = -1, ay = -1;
	size_t i;
	unsigned ver;

	ver = QR_VER(q->size);
	assert(ver >= QR_VER_MIN && ver <= QR_VER_MAX);

	/* Finder + format: top left */
	if (x < 9 && y < 9)
		return true;

	/* Finder + format: bottom left */
	if (x + 8 >= q->size && y < 9)
		return true;

	/* Finder + format: top right */
	if (x < 9 && y + 8 >= q->size)
		return true;

	/* Exclude timing patterns */
	if (x == 6 || y == 6)
		return true;

	/* Exclude ver info, if it exists. Version info sits adjacent to
	 * the top-right and bottom-left finders in three rows, bounded by
	 * the timing pattern.
	 */
	if (ver >= 7) {
		if (x < 6 && y + 11 >= q->size)
			return true;
		if (x + 11 >= q->size && y < 6)
			return true;
	}

	/* Exclude alignment patterns */
	unsigned alignPatPos[QR_ALIGN_MAX];
	size_t n = getAlignmentPatternPositions(ver, alignPatPos);
	for (i = 0; i < n; i++) {
		int p = alignPatPos[i];

		if (abs(p - x) < 3)
			ax = i;
		if (abs(p - y) < 3)
			ay = i;
	}

	if (ax >= 0 && ay >= 0) {
		i--;
		if (ax > 0 && ax < (int) i)
			return true;
		if (ay > 0 && ay < (int) i)
			return true;
		if (ay == (int) i && ax == (int) i)
			return true;
	}

	return false;
}

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
void
set_module_bounded(struct qr *q, unsigned x, unsigned y, bool v)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	if (x < q->size && y < q->size) {
		qr_set_module(q, x, y, v);
	}
}

// Sets every pixel in the range [left : left + width] * [top : top + height] to v.
void
fill(unsigned left, unsigned top, unsigned width, unsigned height, struct qr *q)
{
	assert(q != NULL);

	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			qr_set_module(q, left + x, top + y, true);
		}
	}
}

