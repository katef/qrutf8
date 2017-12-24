
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

#include <qr.h>
#include <io.h>

#define MAXLINE 1024

/*
 * Read the data contents of a PBM (portable bit map) file.
 */
static void
read_pbm_data(FILE *f, int *img_in, int is_ascii)
{
	int i=0, c;
	int lum_val;
	int k;

	/* Read the rest of the PBM file. */
	while ((c = fgetc(f)) != EOF) {
		ungetc(c, f);

		if (is_ascii == 1) {
			if (fscanf(f, "%d", &lum_val) != 1) return;
			img_in[i++] = lum_val;
		} else {
			lum_val = fgetc(f);
			/* Decode the image contents byte-by-byte. */
			for (k = 0; k < 8; k++) {
				img_in[i++] = (lum_val >> (7-k)) & 0x1;
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
read_pbm_header(FILE *f, int *img_xdim, int *img_ydim, int *is_ascii)
{
	int flag=0;
	int x_val, y_val;
	unsigned int i;
	char magic[MAXLINE];
	char line[MAXLINE];
	int count=0;

	/* Read the PBM file header. */
	while (fgets(line, MAXLINE, f) != NULL) {
		flag = 0;
		for (i = 0; i < strlen(line); i++) {
			if (isgraph(line[i])) {
				if ((line[i] == '#') && (flag == 0)) {
					flag = 1;
				}
			}
		}
		if (flag == 0) {
			if (count == 0) {
				count += sscanf(line, "%s %d %d", magic, &x_val, &y_val);
			} else if (count == 1) {
				count += sscanf(line, "%d %d", &x_val, &y_val);
			} else if (count == 2) {
				count += sscanf(line, "%d", &y_val);
			}
		}
		if (count == 3) {
			break;
		}
	}

	if (strcmp(magic, "P1") == 0) {
		*is_ascii = 1;
	} else if (strcmp(magic, "P4") == 0) {
		*is_ascii = 0;
	} else {
		fprintf(stderr, "Error: Input file not in PBM format!\n");
		exit(1);
	}

	fprintf(stderr, "Info: magic=%s, x_val=%d, y_val=%d\n", magic, x_val, y_val);

	*img_xdim   = x_val;
	*img_ydim   = y_val;
}

static bool
quiet(int x_dim, int y_dim, size_t border, const int *img_data)
{
	size_t x, y;

	assert(x_dim == y_dim);
	assert(border <= (size_t) x_dim);
	assert(img_data != NULL);

	for (y = 0; y < (size_t) y_dim; y++) {
		for (x = 0; x < border; x++) {
			if (img_data[y * y_dim + x] || img_data[y * y_dim + (x_dim - 1 - x)]) {
				return false;
			}
		}
	}

	for (y = 0; y < border; y++) {
		for (x = 0; x < (size_t) x_dim; x++) {
			if (img_data[y * y_dim + x] || img_data[(y_dim - 1 - y) * y_dim + x]) {
				return false;
			}
		}
	}

	return true;
}

bool
qr_load_pbm(FILE *f, struct qr *q, bool invert)
{
	int enable_ascii=0;
	int x_dim, y_dim;
	int *img_data;
	size_t border;

	read_pbm_header(f, &x_dim, &y_dim, &enable_ascii);

	if (x_dim != y_dim) {
		return false;
	}

	img_data = malloc((x_dim * y_dim) * sizeof(int));
	if (img_data == NULL) {
		return false;
	}

	read_pbm_data(f, img_data, enable_ascii);

	{
		size_t i;

		border = 0;

		for (i = 0; i < (size_t) x_dim; i++) {
			if (img_data[i * y_dim + i]) {
				break;
			}

			border++;
		}
	}

	/* XXX: heed invert */
	(void) invert;

	if (!quiet(x_dim, y_dim, border, img_data)) {
		fprintf(stderr, "pixel in quiet zone\n"); /* XXX: error enum */
		goto error;
	}

	q->size = x_dim - border * 2;

	{
		size_t x, y;

		for (y = border; y < (size_t) y_dim - border; y++) {
			for (x = border; x < (size_t) x_dim - border; x++) {
				bool v;

				v = img_data[y * y_dim + x];

				qr_set_module(q, x - border, y - border, v);
			}
		}
	}

	free(img_data);

	return true;

error:

	free(img_data);

	return false;
}

