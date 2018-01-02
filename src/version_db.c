
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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"

/*
 * Tables are indexed:
 * - Firstly by version (index 0 is for padding, set to an illegal value)
 * - Secondly by ecl (medium, low, high, quartile).
 */

const int8_t ECL_CODEWORDS_PER_BLOCK[QR_VER_MAX + 1][4] = {
	{ -1, -1, -1, -1 },
	{ 10,  7, 17, 13 },
	{ 16, 10, 28, 22 },
	{ 26, 15, 22, 18 },
	{ 18, 20, 16, 26 },
	{ 24, 26, 22, 18 },
	{ 16, 18, 28, 24 },
	{ 18, 20, 26, 18 },
	{ 22, 24, 26, 22 },
	{ 22, 30, 24, 20 },
	{ 26, 18, 28, 24 },
	{ 30, 20, 24, 28 },
	{ 22, 24, 28, 26 },
	{ 22, 26, 22, 24 },
	{ 24, 30, 24, 20 },
	{ 24, 22, 24, 30 },
	{ 28, 24, 30, 24 },
	{ 28, 28, 28, 28 },
	{ 26, 30, 28, 28 },
	{ 26, 28, 26, 26 },
	{ 26, 28, 28, 30 },
	{ 26, 28, 30, 28 },
	{ 28, 28, 24, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 26, 30, 30 },
	{ 28, 28, 30, 28 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 },
	{ 28, 30, 30, 30 }
};

const int8_t NUM_ERROR_CORRECTION_BLOCKS[QR_VER_MAX + 1][4] = {
	{ -1, -1, -1, -1 },
	{  1,  1,  1,  1 },
	{  1,  1,  1,  1 },
	{  1,  1,  2,  2 },
	{  2,  1,  4,  2 },
	{  2,  1,  4,  4 },
	{  4,  2,  4,  4 },
	{  4,  2,  5,  6 },
	{  4,  2,  6,  6 },
	{  5,  2,  8,  8 },
	{  5,  4,  8,  8 },
	{  5,  4, 11,  8 },
	{  8,  4, 11, 10 },
	{  9,  4, 16, 12 },
	{  9,  4, 16, 16 },
	{ 10,  6, 18, 12 },
	{ 10,  6, 16, 17 },
	{ 11,  6, 19, 16 },
	{ 13,  6, 21, 18 },
	{ 14,  7, 25, 21 },
	{ 16,  8, 25, 20 },
	{ 17,  8, 25, 23 },
	{ 17,  9, 34, 23 },
	{ 18,  9, 30, 25 },
	{ 20, 10, 32, 27 },
	{ 21, 12, 35, 29 },
	{ 23, 12, 37, 34 },
	{ 25, 12, 40, 34 },
	{ 26, 13, 42, 35 },
	{ 28, 14, 45, 38 },
	{ 29, 15, 48, 40 },
	{ 31, 16, 51, 43 },
	{ 33, 17, 54, 45 },
	{ 35, 18, 57, 48 },
	{ 37, 19, 60, 51 },
	{ 38, 19, 63, 53 },
	{ 40, 20, 66, 56 },
	{ 43, 21, 70, 59 },
	{ 45, 22, 74, 62 },
	{ 47, 24, 77, 65 },
	{ 49, 25, 81, 68 }
};

