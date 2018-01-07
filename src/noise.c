
#define _XOPEN_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"

void
qr_noise(struct qr *q, size_t n, long seed, bool skip_reserved)
{
	const size_t bits = q->size * q->size;
	uint8_t buf[QR_BUF_LEN_MAX] = { 0 };
	uint8_t noise[QR_BUF_LEN_MAX] = { 0 };
	struct qr reserved;
	size_t i;

	const unsigned ver = QR_VER(q->size);

	reserved.size = q->size;
	reserved.map  = buf;

	/* TODO: merge with reserved_module() */
	/* TODO: generalise to an enum describing regions; format, ecc, alignments, data, etc. mask together */
	draw_init(ver, &reserved);

	srand48(seed);

	/*
	 * Alternate implementation for bounded runtime: set (1 << n) - 1
	 * and shuffle by fisher-yates to distribute those n bits across
	 * the bitmap. But that makes it difficult to skip_reserved.
	 */

	while (n > 0) {
		i = lrand48() % (bits + 1);

		if (skip_reserved) {
			if (BM_GET(reserved.map, i)) {
				continue;
			}
		}

		/* TODO: BM_FLIP */
		if (BM_GET(noise, i)) {
			BM_CLR(noise, i);
		} else {
			BM_SET(noise, i);
		}
		n--;
	}

	for (i = 0; i < QR_BUF_LEN(ver); i++) {
		q->map[i] ^= noise[i];
	}
}

