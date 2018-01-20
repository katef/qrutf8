
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <eci.h>
#include <qr.h>
#include <io.h>

#include "util.h"

static void
utf8(int cp, char c[])
{
	if (cp <= 0x7f) {
		c[0] =  cp;
		return;
	}

	if (cp <= 0x7ff) {
		c[0] = (cp >>  6) + 192;
		c[1] = (cp  & 63) + 128;
		return;
	}

	if (0xd800 <= cp && cp <= 0xdfff) {
		/* invalid */
		goto error;
	}

	if (cp <= 0xffff) {
		c[0] =  (cp >> 12) + 224;
		c[1] = ((cp >>  6) &  63) + 128;
		c[2] =  (cp  & 63) + 128;
		return;
	}

	if (cp <= 0x10ffff) {
		c[0] =  (cp >> 18) + 240;
		c[1] = ((cp >> 12) &  63) + 128;
		c[2] = ((cp >>  6) &  63) + 128;
		c[3] =  (cp  & 63) + 128;
		return;
	}

error:

	fprintf(stderr, "codepoint out of range\n");
	exit(1);
}

void
qr_print_utf8qb(FILE *f, const struct qr *q, enum qr_utf8 uwidth, bool invert)
{
	size_t border;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	for (int y = -border; y < (int) (q->size + border); y += 2) {
		if (uwidth == QR_UTF8_WIDE) {
			fprintf(f, "\033#6");
		}

		for (int x = -border; x < (int) (q->size + border); x += uwidth == QR_UTF8_DOUBLE ? 1 : 2) {
			char s[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
			size_t i;
			int e;

			struct {
				unsigned x;
				unsigned y;
			} a[] = {
				{ x + 0, y + 0 },
				{ x + 1, y + 0 },
				{ x + 0, y + 1 },
				{ x + 1, y + 1 }
			};

			e = 0;

			for (i = 0; i < sizeof a / sizeof *a; i++) {
				if (a[i].x >= q->size || a[i].y >= q->size) {
					continue;
				}

				if (qr_get_module(q, a[i].x, a[i].y)) {
					e |= 1 << i;
				}
			}

			int cp[] = {
				' ',
				0x2598, 0x259D, 0x2580,
				0x2596, 0x258C, 0x259E,
				0x259B, 0x2597, 0x259A,
				0x2590, 0x259C, 0x2584,
				0x2599, 0x259F, 0x2588
			};

			assert(e < (int) (sizeof cp / sizeof *cp));

			if (invert) {
				e = (sizeof cp / sizeof *cp) - 1 - e;
			}

			utf8(cp[e], s);

			fputs(s, f);
		}

		fputs("\n", f);
	}
}

void
qr_print_pbm1(FILE *f, const struct qr *q, bool invert)
{
	size_t border;
	int x, y;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	fprintf(f, "P1\n");
	fprintf(f, "%zu %zu\n", q->size + border * 2, q->size + border * 2);

	for (y = -border; y < (int) (q->size + border); y++) {
		for (x = -border; x < (int) (q->size + border); x++) {
			bool v;

			if (x < 0 || x >= (int) q->size || y < 0 || y >= (int) q->size) {
				v = false;
			} else {
				v = qr_get_module(q, x, y);
			}

			if (invert) {
				v = !v;
			}

			fprintf(f, "%d", v ? 0 : 1);

			if (y < (int) (q->size + border)) {
				fputs(" ", f);
			}
		}

		fprintf(f, "\n");
	}
}

void
qr_print_pbm4(FILE *f, const struct qr *q, bool invert)
{
	size_t border;
	int x, y;
	uint8_t c;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	fprintf(f, "P4\n");
	fprintf(f, "%zu %zu\n", q->size + border * 2, q->size + border * 2);

	c = 0x00;

	for (y = -border; y < (int) (q->size + border); y++) {
		for (x = -border; x < (int) (q->size + border); x++) {
			bool v;

			if (x < 0 || x >= (int) q->size || y < 0 || y >= (int) q->size) {
				v = false;
			} else {
				v = qr_get_module(q, x, y);
			}

			if (invert) {
				v = !v;
			}

			c |= v;

			if (x == (int) (q->size + border) - 1) {
				/* end of row; pad to byte */
				c <<= (7 - ((x + border) & 07));

				fwrite(&c, sizeof c, 1, f);

				c = 0x00;
				continue;
			}

			if (((x + border) & 07) == 07) {
				fwrite(&c, sizeof c, 1, f);

				c = 0x00;
				continue;
			}

			c <<= 1;
		}
	}
}

void
qr_print_svg(FILE *f, const struct qr *q, bool invert)
{
	size_t border;
	int x, y;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	fprintf(f, "<?xml version='1.0' standalone='yes'?>\n");
	fprintf(f, "<svg xmlns='%s' version='1.1' width='%zu' height='%zu'>\n",
		"http://www.w3.org/2000/svg",
		q->size + border * 2,
		q->size + border * 2);

	for (y = -border; y < (int) (q->size + border); y++) {
		for (x = -border; x < (int) (q->size + border); x++) {
			bool v;

			if (x < 0 || x >= (int) q->size || y < 0 || y >= (int) q->size) {
				v = false;
			} else {
				v = qr_get_module(q, x, y);
			}

			if (v) {
				fprintf(f, "  <rect x='%zu' y='%zu' width='1' height='1' style='fill: %s; shape-rendering: crispEdges;'/>\n",
					x + border, y + border,
					invert ? "white" : "black");
			}
		}
	}

	fprintf(f, "</svg>");
}

