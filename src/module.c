
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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"

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

