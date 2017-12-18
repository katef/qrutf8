
/* XXX: placeholder */
#ifndef VERSION_DB_C
#define VERSION_DB_C

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

static const int8_t ECL_CODEWORDS_PER_BLOCK[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	// 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{ -1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // Low
	{ -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28 }, // Medium
	{ -1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // Quartile
	{ -1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // High
};

static const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	// 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{ -1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25 }, // Low
	{ -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49 }, // Medium
	{ -1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68 }, // Quartile
	{ -1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81 }, // High
};

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

/************************************************************************
 * QR-code version information database
 */

struct quirc_version_info {
	int				data_bytes;
};

static const struct quirc_version_info quirc_version_db[QR_VER_MAX + 1] = {
	{ 0 },
	/* Version 1 */ {
		.data_bytes =  26, 
	},
	/* Version 2 */ {
		.data_bytes =  44, 
	},
	/* Version 3 */ {
		.data_bytes =  70, 
	},
	/* Version 4 */ {
		.data_bytes = 100,
	},
	/* Version 5 */ {
		.data_bytes = 134,
	},
	/* Version 6 */ {
		.data_bytes = 172,
	},
	/* Version 7 */ {
		.data_bytes = 196,
	},
	/* Version 8 */ {
		.data_bytes = 242,
	},
	/* Version 9 */ {
		.data_bytes = 292,
	},
	/* Version 10 */ {
		.data_bytes = 346,
	},
	/* Version 11 */ {
		.data_bytes = 404,
	},
	/* Version 12 */ {
		.data_bytes = 466,
	},
	/* Version 13 */ {
		.data_bytes = 532,
	},
	/* Version 14 */ {
		.data_bytes = 581,
	},
	/* Version 15 */ {
		.data_bytes = 655,
	},
	/* Version 16 */ {
		.data_bytes = 733,
	},
	/* Version 17 */ {
		.data_bytes = 815,
	},
	/* Version 18 */ {
		.data_bytes = 901,
	},
	/* Version 19 */ {
		.data_bytes = 991,
	},
	/* Version 20 */ {
		.data_bytes = 1085,
	},
	/* Version 21 */ {
		.data_bytes = 1156,
	},
	/* Version 22 */ {
		.data_bytes = 1258,
	},
	/* Version 23 */ {
		.data_bytes = 1364,
	},
	/* Version 24 */ {
		.data_bytes = 1474,
	},
	/* Version 25 */ {
		.data_bytes = 1588,
	},
	/* Version 26 */ {
		.data_bytes = 1706,
	},
	/* Version 27 */ {
		.data_bytes = 1828,
	},
	/* Version 28 */ {
		.data_bytes = 1921,
	},
	/* Version 29 */ {
		.data_bytes = 2051,
	},
	/* Version 30 */ {
		.data_bytes = 2185,
	},
	/* Version 31 */ {
		.data_bytes = 2323,
	},
	/* Version 32 */ {
		.data_bytes = 2465,
	},
	/* Version 33 */ {
		.data_bytes = 2611,
	},
	/* Version 34 */ {
		.data_bytes = 2761,
	},
	/* Version 35 */ {
		.data_bytes = 2876,
	},
	/* Version 36 */ {
		.data_bytes = 3034,
	},
	/* Version 37 */ {
		.data_bytes = 3196,
	},
	/* Version 38 */ {
		.data_bytes = 3362,
	},
	/* Version 39 */ {
		.data_bytes = 3532,
	},
	/* Version 40 */ {
		.data_bytes = 3706,
	}
};

#endif

