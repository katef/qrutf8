
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
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"

static bool
mask_bit(enum qr_mask mask, unsigned x, unsigned y)
{
	assert(0 <= mask && mask <= 7);
	assert((x + 1) <= QR_SIZE(QR_VER_MAX));
	assert((y + 1) <= QR_SIZE(QR_VER_MAX));

	switch (mask) {
	case QR_MASK_0: return 0 == (y + x) % 2;
	case QR_MASK_1: return 0 == y % 2;
	case QR_MASK_2: return 0 == x % 3;
	case QR_MASK_3: return 0 == (y + x) % 3;
	case QR_MASK_4: return 0 == (y / 2 + x / 3) % 2;
	case QR_MASK_5: return 0 ==  (y * x) % 2 + (y * x) % 3;
	case QR_MASK_6: return 0 == ((y * x) % 2 + (y * x) % 3) % 2;
	case QR_MASK_7: return 0 == ((y + x) % 2 + (y * x) % 3) % 2;

	case QR_MASK_AUTO:
		assert(!"unreached");
		abort();
	}

	return false;
}

void
qr_apply_mask(struct qr *q, enum qr_mask mask)
{
	assert(q != NULL);
	assert(QR_SIZE(QR_VER_MIN) <= q->size && q->size <= QR_SIZE(QR_VER_MAX));

	for (unsigned y = 0; y < q->size; y++) {
		for (unsigned x = 0; x < q->size; x++) {
			if (reserved_module(q, x, y)) {
				continue;
			}

			qr_set_module(q, x, y, qr_get_module(q, x, y) ^ mask_bit(mask, x, y));
		}
	}
}

