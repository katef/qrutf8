
/* Adapted from: */

/*
 * File       : pnmio.c
 * Description: I/O facilities for PBM, PGM, PPM (PNM) binary and ASCII images.
 * Author     : Nikolaos Kavvadias <nikolaos.kavvadias@gmail.com>
 * Copyright  : (C) Nikolaos Kavvadias 2012, 2013, 2014, 2015, 2016, 2017
 * Website    : http://www.nkavvadias.com
 *
 * This file is part of libpnmio, and is distributed under the terms of the
 * Modified BSD License.
 *
 * A copy of the Modified BSD License is included with this distribution
 * in the file LICENSE.
 * libpnmio is free software: you can redistribute it and/or modify it under the
 * terms of the Modified BSD License.
 * libpnmio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the Modified BSD License for more details.
 *
 * You should have received a copy of the Modified BSD License along with
 * libpnmio. If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include <eci.h>
#include <qr.h>
#include <io.h>

#define MAXLINE 1024

/*
 * Read the data contents of a PBM (portable bit map) file.
 */
static void
read_pbm_data(FILE *f, bool *img, bool ascii)
{
	int i, c;
	int v;
	int k;

	i = 0;

	/* Read the rest of the PBM file. */
	while (c = fgetc(f), c != EOF) {
		ungetc(c, f);

		if (ascii) {
			if (fscanf(f, "%d", &v) != 1) return;
			img[i++] = v;
		} else {
			v = fgetc(f);
			for (k = 0; k < 8; k++) {
				img[i++] = (v >> (7 - k)) & 0x1;
			}
		}
	}

	fclose(f);
}

/*
 * Read the header contents of a PBM (Portable Binary Map) file.
 * An ASCII PBM image file follows the format:
 * P1
 * <X> <Y>
 * <I1> <I2> ... <IMAX>
 * A binary PBM image file uses P4 instead of P1 and
 * the data values are represented in binary.
 * NOTE1: Comment lines start with '#'.
 * NOTE2: < > denote integer values (in decimal).
 */
static void
read_pbm_header(FILE *f, size_t *width, size_t *height, bool *ascii)
{
	bool flag;
	size_t x, y;
	unsigned int i;
	char magic[MAXLINE];
	char line[MAXLINE];
	int n = 0;

	/* Read the PBM file header. */
	while (fgets(line, MAXLINE, f) != NULL) {
		flag = false;

		for (i = 0; i < strlen(line); i++) {
			if (isgraph(line[i])) {
				if ((line[i] == '#') && !flag) {
					flag = true;
				}
			}
		}

		if (!flag) {
			if (n == 0) {
				n += sscanf(line, "%s %zu %zu", magic, &x, &y);
			} else if (n == 1) {
				n += sscanf(line, "%zu %zu", &x, &y);
			} else if (n == 2) {
				n += sscanf(line, "%zu", &y);
			}
		}

		if (n == 3) {
			break;
		}
	}

	if (strcmp(magic, "P1") == 0) {
		*ascii = true;
	} else if (strcmp(magic, "P4") == 0) {
		*ascii = false;
	} else {
		fprintf(stderr, "Error: Input file not in PBM format!\n");
		exit(1);
	}

	*width  = x;
	*height = y;
}

static bool
quiet(size_t width, size_t height, size_t border, const bool *img)
{
	size_t x, y;

	assert(width == height);
	assert(border <= width);
	assert(img != NULL);

	for (y = 0; y < height; y++) {
		for (x = 0; x < border; x++) {
			if (img[y * height + x] || img[y * height + (width - 1 - x)]) {
				return false;
			}
		}
	}

	for (y = 0; y < border; y++) {
		for (x = 0; x < width; x++) {
			if (img[y * height + x] || img[(height - 1 - y) * height + x]) {
				return false;
			}
		}
	}

	return true;
}

bool
qr_load_pbm(FILE *f, struct qr *q, bool invert)
{
	bool ascii;
	size_t width, height;
	bool *img;
	size_t border;

	read_pbm_header(f, &width, &height, &ascii);

	if (width != height) {
		return false;
	}

	img = malloc((width * height) * sizeof(int));
	if (img == NULL) {
		return false;
	}

	read_pbm_data(f, img, ascii);

	{
		size_t i;

		border = 0;

		for (i = 0; i < width; i++) {
			if (img[i * height + i]) {
				break;
			}

			border++;
		}
	}

	/* XXX: heed invert */
	(void) invert;

	if (!quiet(width, height, border, img)) {
		fprintf(stderr, "pixel in quiet zone\n"); /* XXX: error enum */
		goto error;
	}

	q->size = width - border * 2;

	{
		size_t x, y;

		for (y = border; y < height - border; y++) {
			for (x = border; x < width - border; x++) {
				bool v;

				v = img[y * height + x];

				qr_set_module(q, x - border, y - border, v);
			}
		}
	}

	free(img);

	return true;

error:

	free(img);

	return false;
}

