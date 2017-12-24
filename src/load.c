
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include <qr.h>
#include <io.h>

#define MAXLINE 1024

enum {
	PBM_ASCII  = 1,
	PBM_BINARY = 4
};

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

/*
 * Read the header contents of a PBM/PGM/PPM/PFM file up to the point of
 * extracting its type. Valid types for a PNM image are as follows:
 *   PBM_ASCII     =  1
 *   PBM_BINARY    =  4
 *
 * The result (pnm_type) is returned.
 */
static int
get_pnm_type(FILE *f)
{
	int flag=0;
	int pnm_type=0;
	unsigned int i;
	char magic[MAXLINE];
	char line[MAXLINE];

	/* Read the PNM/PFM file header. */
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
			sscanf(line, "%s", magic);
			break;
		}
	}

	if (strcmp(magic, "P1") == 0) {
		pnm_type = PBM_ASCII;
	} else if (strcmp(magic, "P4") == 0) {
		pnm_type = PBM_BINARY;
	} else {
		fprintf(stderr, "Error: Unknown PNM/PFM file; wrong magic number!\n");
		exit(1);
	}

	return (pnm_type);
}

bool
qr_load_pbm(FILE *f, struct qr *q, bool invert)
{
	int enable_ascii=0;
	int pnm_type;
	int x_dim, y_dim;
	int *img_data;

	img_data = NULL;

	pnm_type = get_pnm_type(f);
	fprintf(stderr, "Info: pnm_type = %d\n", pnm_type);
	rewind(f);

	if (pnm_type != PBM_ASCII && pnm_type != PBM_BINARY) {
		return false;
	}

	read_pbm_header(f, &x_dim, &y_dim, &enable_ascii);

	img_data = malloc((x_dim * y_dim) * sizeof(int));
	if (img_data == NULL) {
		return false;
	}

	read_pbm_data(f, img_data, enable_ascii);

	/* TODO: validate border */
	(void) q;
	(void) invert;

	/* TODO: set size */
	/* TODO: set modules, skip border */

	free(img_data);

	return true;
}
