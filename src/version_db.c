
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

#include <qr.h>

#include "internal.h"

/*
 * Tables are indexed:
 * - Firstly by version (index 0 is for padding, set to an illegal value)
 * - Secondly by ecl (low, medium quartile, high).
 */

const int8_t ECL_CODEWORDS_PER_BLOCK[QR_VER_MAX + 1][4] = {
	{ -1, -1, -1, -1 },
	{  7, 10, 13, 17 },
	{ 10, 16, 22, 28 },
	{ 15, 26, 18, 22 },
	{ 20, 18, 26, 16 },
	{ 26, 24, 18, 22 },
	{ 18, 16, 24, 28 },
	{ 20, 18, 18, 26 },
	{ 24, 22, 22, 26 },
	{ 30, 22, 20, 24 },
	{ 18, 26, 24, 28 },
	{ 20, 30, 28, 24 },
	{ 24, 22, 26, 28 },
	{ 26, 22, 24, 22 },
	{ 30, 24, 20, 24 },
	{ 22, 24, 30, 24 },
	{ 24, 28, 24, 30 },
	{ 28, 28, 28, 28 },
	{ 30, 26, 28, 28 },
	{ 28, 26, 26, 26 },
	{ 28, 26, 30, 28 },
	{ 28, 26, 28, 30 },
	{ 28, 28, 30, 24 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 26, 28, 30, 30 },
	{ 28, 28, 28, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 },
	{ 30, 28, 30, 30 }
};

const int8_t NUM_ERROR_CORRECTION_BLOCKS[QR_VER_MAX + 1][4] = {
	{ -1, -1, -1, -1 },
	{  1,  1,  1,  1 },
	{  1,  1,  1,  1 },
	{  1,  1,  2,  2 },
	{  1,  2,  2,  4 },
	{  1,  2,  4,  4 },
	{  2,  4,  4,  4 },
	{  2,  4,  6,  5 },
	{  2,  4,  6,  6 },
	{  2,  5,  8,  8 },
	{  4,  5,  8,  8 },
	{  4,  5,  8, 11 },
	{  4,  8, 10, 11 },
	{  4,  9, 12, 16 },
	{  4,  9, 16, 16 },
	{  6, 10, 12, 18 },
	{  6, 10, 17, 16 },
	{  6, 11, 16, 19 },
	{  6, 13, 18, 21 },
	{  7, 14, 21, 25 },
	{  8, 16, 20, 25 },
	{  8, 17, 23, 25 },
	{  9, 17, 23, 34 },
	{  9, 18, 25, 30 },
	{ 10, 20, 27, 32 },
	{ 12, 21, 29, 35 },
	{ 12, 23, 34, 37 },
	{ 12, 25, 34, 40 },
	{ 13, 26, 35, 42 },
	{ 14, 28, 38, 45 },
	{ 15, 29, 40, 48 },
	{ 16, 31, 43, 51 },
	{ 17, 33, 45, 54 },
	{ 18, 35, 48, 57 },
	{ 19, 37, 51, 60 },
	{ 19, 38, 53, 63 },
	{ 20, 40, 56, 66 },
	{ 21, 43, 59, 70 },
	{ 22, 45, 62, 74 },
	{ 24, 47, 65, 77 },
	{ 25, 49, 68, 81 }
};

