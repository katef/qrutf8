
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <qr.h>

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
qr_print_utf8qb(FILE *f, const struct qr *q, bool wide, bool invert)
{
	size_t border;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	for (int y = -border; y < (int) (q->size + border); y += 2) {
		if (wide) {
			fprintf(f, "\033#6");
		}

		for (int x = -border; x < (int) (q->size + border); x += 2) {
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
qr_print_xpm(FILE *f, const struct qr *q, bool invert)
{
	size_t border;

	assert(f != NULL);
	assert(q != NULL);

	border = 4; /* per the spec */

	fputs("/* XPM */\n", f);
	fputs("static char *qr[] = {\n", f);
	fputs("/* columns rows colors chars-per-pixels */\n", f);
	fprintf(f, "\"%zu %zu 2 1\",\n", q->size + border * 2, q->size + border * 2);
	fprintf(f, "\"  c black\",\n");
	fprintf(f, "\"# c gray100\",\n");
	fprintf(f, "/* pixels */\n");

	for (int y = -border; y < (int) (q->size + border); y++) {
		fputc('"', f);

		for (int x = -border; x < (int) (q->size + border); x++) {
			bool v;

			if (x < 0 || x >= (int) q->size || y < 0 || y >= (int) q->size) {
				v = false;
			} else {
				v = qr_get_module(q, x, y);
			}

			if (invert) {
				v = !v;
			}

			fputc(v ? '#' : ' ', f);
		}

		fputc('"', f);
		if (y < (int) (q->size + border)) {
			fputs(",", f);
		}
		fputc('\n', f);
	}

	fputs("};", f);
}

