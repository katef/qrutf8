/* 
 * QR Code generator test suite (C)
 * 
 * When compiling this program, the library mapgen.c needs QRCODEGEN_TEST
 * to be defined. Run this command line program with no arguments.
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

/* portions adapted from: */

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

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <eci.h>
#include <qr.h>
#include <io.h>

#include "internal.h"
#include "xalloc.h"
#include "util.h"

#include "encode.c"
#include "decode.c"
#include "load.c"
#include "seg.c"

#include "../share/git/greatest/greatest.h"

#define ARRAY_LENGTH(name)  (sizeof(name) / sizeof(name[0]))

TEST
AppendBitsToBuffer(void)
{
	{
		uint8_t buf[1] = {0};
		size_t bitLen = 0;
		append_bits(0, 0, buf, &bitLen);
		ASSERT_EQ(bitLen, 0);
		ASSERT_EQ(buf[0], 0);
		append_bits(1, 1, buf, &bitLen);
		ASSERT_EQ(bitLen, 1);
		ASSERT_EQ(buf[0], 0x80);
		append_bits(0, 1, buf, &bitLen);
		ASSERT_EQ(bitLen, 2);
		ASSERT_EQ(buf[0], 0x80);
		append_bits(5, 3, buf, &bitLen);
		ASSERT_EQ(bitLen, 5);
		ASSERT_EQ(buf[0], 0xA8);
		append_bits(6, 3, buf, &bitLen);
		ASSERT_EQ(bitLen, 8);
		ASSERT_EQ(buf[0], 0xAE);
	}
	{
		uint8_t buf[6] = {0};
		size_t bitLen = 0;
		append_bits(16942, 16, buf, &bitLen);
		ASSERT_EQ(bitLen, 16);
		ASSERT_EQ(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5], 0x00);
		append_bits(10, 7, buf, &bitLen);
		ASSERT_EQ(bitLen, 23);
		ASSERT_EQ(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x14 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5], 0x00);
		append_bits(15, 4, buf, &bitLen);
		ASSERT_EQ(bitLen, 27);
		ASSERT_EQ(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xE0 && buf[4] == 0x00 && buf[5], 0x00);
		append_bits(26664, 15, buf, &bitLen);
		ASSERT_EQ(bitLen, 42);
		ASSERT_EQ(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xFA && buf[4] == 0x0A && buf[5], 0x00);
	}

	PASS();
}


// Ported from the Java version of the code.
static uint8_t *append_eclReference(const uint8_t *data, int version, enum qr_ecl ecl) {
	// Calculate parameter numbers
	int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[version][(int) ecl];
	int blockEccLen = ECL_CODEWORDS_PER_BLOCK[version][(int) ecl];
	int rawCodewords = count_data_bits(version) / 8;
	int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	int shortBlockLen = rawCodewords / numBlocks;
	
	// Split data into blocks and append ECC to each block
	uint8_t **blocks = xmalloc(numBlocks * sizeof (uint8_t*));
	uint8_t *generator = xmalloc(blockEccLen);
	reed_solomon_generator(blockEccLen, generator);
	for (int i = 0, k = 0; i < numBlocks; i++) {
		uint8_t *block = xmalloc(shortBlockLen + 1);
		int blockDataLen = shortBlockLen - blockEccLen + (i < numShortBlocks ? 0 : 1);
		memcpy(block, &data[k], blockDataLen * sizeof(uint8_t));
		reed_solomon_remainder(&data[k], blockDataLen, generator, blockEccLen, &block[shortBlockLen + 1 - blockEccLen]);
		k += blockDataLen;
		blocks[i] = block;
	}
	free(generator);
	
	// Interleave (not concatenate) the bytes from every block into a single sequence
	uint8_t *result = xmalloc(rawCodewords);
	for (int i = 0, k = 0; i < shortBlockLen + 1; i++) {
		for (int j = 0; j < numBlocks; j++) {
			// Skip the padding byte in short blocks
			if (i != shortBlockLen - blockEccLen || j >= numShortBlocks) {
				result[k] = blocks[j][i];
				k++;
			}
		}
	}
	for (int i = 0; i < numBlocks; i++)
		free(blocks[i]);
	free(blocks);
	return result;
}


TEST
AppendErrorCorrection(void)
{
	for (int ver = QR_VER_MIN; ver <= QR_VER_MAX; ver++) {
		for (int ecl = 0; ecl < 4; ecl++) {
			int dataLen = count_codewords(ver, (enum qr_ecl)ecl);
			uint8_t *pureData = xmalloc(dataLen);
			for (int i = 0; i < dataLen; i++)
				pureData[i] = rand() % 256;
			uint8_t *expectOutput = append_eclReference(pureData, ver, (enum qr_ecl)ecl);
			
			int dataAndEccLen = count_data_bits(ver) / 8;
			uint8_t *paddedData = xmalloc(dataAndEccLen);
			memcpy(paddedData, pureData, dataLen * sizeof(uint8_t));
			uint8_t *actualOutput = xmalloc(dataAndEccLen);
			append_ecl(paddedData, ver, (enum qr_ecl)ecl, actualOutput);
			
			ASSERT_EQ(memcmp(actualOutput, expectOutput, dataAndEccLen * sizeof(uint8_t)), 0);
			free(pureData);
			free(expectOutput);
			free(paddedData);
			free(actualOutput);
		}
	}

	PASS();
}


TEST
ErrorCorrectionBlockLengths(void)
{
	/* from quirc version_db.c */
	static const int data_bytes[QR_VER_MAX + 1] = {
		   0,   26,   44,   70,  100,  134,  172,  196,  242,  292,
		 346,  404,  466,  532,  581,  655,  733,  815,  901,  991,
		1085, 1156, 1258, 1364, 1474, 1588, 1706, 1828, 1921, 2051,
		2185, 2323, 2465, 2611, 2761, 2876, 3034, 3196, 3362, 3532,
		3706,
	};

	for (int ver = QR_VER_MIN; ver <= QR_VER_MAX; ver++) {
		for (int ecl = 0; ecl < 4; ecl++) {
			const int rawCodewords = count_data_bits(ver) / 8;
			const int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ver][ecl];
			const int numShortBlocks = numBlocks - rawCodewords % numBlocks;
			const int ecc_bs = rawCodewords / numBlocks;

			const int lb_count = (data_bytes[ver] - ecc_bs * numShortBlocks) / (ecc_bs + 1);

			ASSERT_EQ(lb_count, numBlocks - numShortBlocks);
		}
	}

	PASS();
}


TEST
GetNumRawDataModules(void)
{
	const unsigned cases[][2] = {
		{ 1,   208},
		{ 2,   359},
		{ 3,   567},
		{ 6,  1383},
		{ 7,  1568},
		{12,  3728},
		{15,  5243},
		{18,  7211},
		{22, 10068},
		{26, 13652},
		{32, 19723},
		{37, 25568},
		{40, 29648},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		const unsigned *tc = cases[i];
		ASSERT_EQ(count_data_bits(tc[0]), tc[1]);
	}

	PASS();
}


TEST
GetNumDataCodewords(void)
{
	const int cases[][3] = {
		{ 3, QR_ECL_MEDIUM,     44},
		{ 3, QR_ECL_QUARTILE,   34},
		{ 3, QR_ECL_HIGH,       26},
		{ 6, QR_ECL_LOW,       136},
		{ 7, QR_ECL_LOW,       156},
		{ 9, QR_ECL_LOW,       232},
		{ 9, QR_ECL_MEDIUM,    182},
		{12, QR_ECL_HIGH,      158},
		{15, QR_ECL_LOW,       523},
		{16, QR_ECL_QUARTILE,  325},
		{19, QR_ECL_HIGH,      341},
		{21, QR_ECL_LOW,       932},
		{22, QR_ECL_LOW,      1006},
		{22, QR_ECL_MEDIUM,    782},
		{22, QR_ECL_HIGH,      442},
		{24, QR_ECL_LOW,      1174},
		{24, QR_ECL_HIGH,      514},
		{28, QR_ECL_LOW,      1531},
		{30, QR_ECL_HIGH,      745},
		{32, QR_ECL_HIGH,      845},
		{33, QR_ECL_LOW,      2071},
		{33, QR_ECL_HIGH,      901},
		{35, QR_ECL_LOW,      2306},
		{35, QR_ECL_MEDIUM,   1812},
		{35, QR_ECL_QUARTILE, 1286},
		{36, QR_ECL_HIGH,     1054},
		{37, QR_ECL_HIGH,     1096},
		{39, QR_ECL_MEDIUM,   2216},
		{40, QR_ECL_MEDIUM,   2334},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		const int *tc = cases[i];
		ASSERT_EQ(count_codewords(tc[0], (enum qr_ecl)tc[1]), tc[2]);
	}

	PASS();
}


TEST
CalcReedSolomonGenerator(void)
{
	uint8_t generator[30];
	
	reed_solomon_generator(1, generator);
	ASSERT_EQ(generator[0], 0x01);
	
	reed_solomon_generator(2, generator);
	ASSERT_EQ(generator[0], 0x03);
	ASSERT_EQ(generator[1], 0x02);
	
	reed_solomon_generator(5, generator);
	ASSERT_EQ(generator[0], 0x1F);
	ASSERT_EQ(generator[1], 0xC6);
	ASSERT_EQ(generator[2], 0x3F);
	ASSERT_EQ(generator[3], 0x93);
	ASSERT_EQ(generator[4], 0x74);
	
	reed_solomon_generator(30, generator);
	ASSERT_EQ(generator[ 0], 0xD4);
	ASSERT_EQ(generator[ 1], 0xF6);
	ASSERT_EQ(generator[ 5], 0xC0);
	ASSERT_EQ(generator[12], 0x16);
	ASSERT_EQ(generator[13], 0xD9);
	ASSERT_EQ(generator[20], 0x12);
	ASSERT_EQ(generator[27], 0x6A);
	ASSERT_EQ(generator[29], 0x96);

	PASS();
}


TEST
CalcReedSolomonRemainder(void)
{
	{
		uint8_t data[1];
		uint8_t generator[3];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, 0, generator, ARRAY_LENGTH(generator), remainder);
		ASSERT_EQ(remainder[0], 0);
		ASSERT_EQ(remainder[1], 0);
		ASSERT_EQ(remainder[2], 0);
	}
	{
		uint8_t data[2] = {0, 1};
		uint8_t generator[4];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		ASSERT_EQ(remainder[0], generator[0]);
		ASSERT_EQ(remainder[1], generator[1]);
		ASSERT_EQ(remainder[2], generator[2]);
		ASSERT_EQ(remainder[3], generator[3]);
	}
	{
		uint8_t data[5] = {0x03, 0x3A, 0x60, 0x12, 0xC7};
		uint8_t generator[5];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		ASSERT_EQ(remainder[0], 0xCB);
		ASSERT_EQ(remainder[1], 0x36);
		ASSERT_EQ(remainder[2], 0x16);
		ASSERT_EQ(remainder[3], 0xFA);
		ASSERT_EQ(remainder[4], 0x9D);
	}
	{
		uint8_t data[43] = {
			0x38, 0x71, 0xDB, 0xF9, 0xD7, 0x28, 0xF6, 0x8E, 0xFE, 0x5E,
			0xE6, 0x7D, 0x7D, 0xB2, 0xA5, 0x58, 0xBC, 0x28, 0x23, 0x53,
			0x14, 0xD5, 0x61, 0xC0, 0x20, 0x6C, 0xDE, 0xDE, 0xFC, 0x79,
			0xB0, 0x8B, 0x78, 0x6B, 0x49, 0xD0, 0x1A, 0xAD, 0xF3, 0xEF,
			0x52, 0x7D, 0x9A,
		};
		uint8_t generator[30];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		ASSERT_EQ(remainder[ 0], 0xCE);
		ASSERT_EQ(remainder[ 1], 0xF0);
		ASSERT_EQ(remainder[ 2], 0x31);
		ASSERT_EQ(remainder[ 3], 0xDE);
		ASSERT_EQ(remainder[ 8], 0xE1);
		ASSERT_EQ(remainder[12], 0xCA);
		ASSERT_EQ(remainder[17], 0xE3);
		ASSERT_EQ(remainder[19], 0x85);
		ASSERT_EQ(remainder[20], 0x50);
		ASSERT_EQ(remainder[24], 0xBE);
		ASSERT_EQ(remainder[29], 0xB3);
	}

	PASS();
}


TEST
FiniteFieldMultiply(void)
{
	const uint8_t cases[][3] = {
		{0x00, 0x00, 0x00},
		{0x01, 0x01, 0x01},
		{0x02, 0x02, 0x04},
		{0x00, 0x6E, 0x00},
		{0xB2, 0xDD, 0xE6},
		{0x41, 0x11, 0x25},
		{0xB0, 0x1F, 0x11},
		{0x05, 0x75, 0xBC},
		{0x52, 0xB5, 0xAE},
		{0xA8, 0x20, 0xA4},
		{0x0E, 0x44, 0x9F},
		{0xD4, 0x13, 0xA0},
		{0x31, 0x10, 0x37},
		{0x6C, 0x58, 0xCB},
		{0xB6, 0x75, 0x3E},
		{0xFF, 0xFF, 0xE2},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		const uint8_t *tc = cases[i];
		ASSERT_EQ(finiteFieldMul(tc[0], tc[1]), tc[2]);
	}

	PASS();
}


TEST
InitializeFunctionModulesEtc(void)
{
	for (int ver = QR_VER_MIN; ver <= QR_VER_MAX; ver++) {
		uint8_t *map = xmalloc(QR_BUF_LEN(ver));
	struct qr q = { 0, map };
		ASSERT(map != NULL);
		draw_init(ver, &q);

		int size = q.size;
		if (ver == 1)
			ASSERT_EQ(size, 21);
		else if (ver == QR_VER_MAX)
			ASSERT_EQ(size, 177);
		else
			ASSERT_EQ(size, ver * 4 + 17);

		bool hasWhite = false;
		bool hasBlack = false;
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				bool color = qr_get_module(&q, x, y);
				if (color)
					hasBlack = true;
				else
					hasWhite = true;
			}
		}
		ASSERT(hasWhite && hasBlack);
		free(map);
	}

	PASS();
}


TEST
GetAlignmentPatternPositions(void)
{
	const int cases[][9] = {
		{ 1, 0,  -1,  -1,  -1,  -1,  -1,  -1,  -1},
		{ 2, 2,   6,  18,  -1,  -1,  -1,  -1,  -1},
		{ 3, 2,   6,  22,  -1,  -1,  -1,  -1,  -1},
		{ 6, 2,   6,  34,  -1,  -1,  -1,  -1,  -1},
		{ 7, 3,   6,  22,  38,  -1,  -1,  -1,  -1},
		{ 8, 3,   6,  24,  42,  -1,  -1,  -1,  -1},
		{16, 4,   6,  26,  50,  74,  -1,  -1,  -1},
		{25, 5,   6,  32,  58,  84, 110,  -1,  -1},
		{32, 6,   6,  34,  60,  86, 112, 138,  -1},
		{33, 6,   6,  30,  58,  86, 114, 142,  -1},
		{39, 7,   6,  26,  54,  82, 110, 138, 166},
		{40, 7,   6,  30,  58,  86, 114, 142, 170},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		const int *tc = cases[i];
		unsigned pos[7];
		size_t n = getAlignmentPatternPositions(tc[0], pos);
		ASSERT_EQ(n, (size_t) tc[1]);
		for (size_t j = 0; j < n; j++)
			ASSERT_EQ(pos[j], (unsigned) tc[2 + j]);
	}

	PASS();
}


TEST
GetSetModule(void)
{
	uint8_t map[QR_BUF_LEN(23)];
	struct qr q = { 0, map };
	draw_init(23, &q);
	int size = q.size;
	
	for (int y = 0; y < size; y++) {  // Clear all to white
		for (int x = 0; x < size; x++)
			qr_set_module(&q, x, y, false);
	}
	for (int y = 0; y < size; y++) {  // Check all white
		for (int x = 0; x < size; x++)
			ASSERT_EQ(qr_get_module(&q, x, y), false);
	}
	for (int y = 0; y < size; y++) {  // Set all to black
		for (int x = 0; x < size; x++)
			qr_set_module(&q, x, y, true);
	}
	for (int y = 0; y < size; y++) {  // Check all black
		for (int x = 0; x < size; x++)
			ASSERT_EQ(qr_get_module(&q, x, y), true);
	}
	
	// Set some out of bounds modules to white
	set_module_bounded(&q, -1, -1, false);
	set_module_bounded(&q, -1, 0, false);
	set_module_bounded(&q, 0, -1, false);
	set_module_bounded(&q, size, 5, false);
	set_module_bounded(&q, 72, size, false);
	set_module_bounded(&q, size, size, false);
	for (int y = 0; y < size; y++) {  // Check all black
		for (int x = 0; x < size; x++)
			ASSERT_EQ(qr_get_module(&q, x, y), true);
	}
	
	// Set some modules to white
	qr_set_module(&q, 3, 8, false);
	qr_set_module(&q, 61, 49, false);
	for (int y = 0; y < size; y++) {  // Check most black
		for (int x = 0; x < size; x++) {
			bool white = (x == 3 && y == 8) || (x == 61 && y == 49);
			ASSERT(qr_get_module(&q, x, y) != white);
		}
	}

	PASS();
}


TEST
GetSetModuleRandomly(void)
{
	uint8_t map[QR_BUF_LEN(1)];
	struct qr q = { 0, map };
	draw_init(1, &q);
	int size = q.size;
	
	bool modules[21][21];
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++)
			modules[y][x] = qr_get_module(&q, x, y);
	}
	
	long trials = 100000;
	for (long i = 0; i < trials; i++) {
		int x = rand() % (size * 2) - size / 2;
		int y = rand() % (size * 2) - size / 2;
		bool isInBounds = 0 <= x && x < size && 0 <= y && y < size;
		bool oldColor = isInBounds && modules[y][x];
		if (isInBounds)
			ASSERT_EQ(qr_get_module(&q, x, y), oldColor);
//		ASSERT_EQ(qr_get_module(&q, x, y), oldColor);
		
		bool newColor = rand() % 2 == 0;
		if (isInBounds)
			modules[y][x] = newColor;
		if (isInBounds && rand() % 2 == 0)
			qr_set_module(&q, x, y, newColor);
		else
			set_module_bounded(&q, x, y, newColor);
	}

	PASS();
}


TEST
IsAlphanumeric(void)
{
	struct TestCase {
		bool answer;
		const char *text;
	};
	const struct TestCase cases[] = {
		{true, ""},
		{true, "0"},
		{true, "A"},
		{false, "a"},
		{true, " "},
		{true, "."},
		{true, "*"},
		{false, ","},
		{false, "|"},
		{false, "@"},
		{true, "XYZ"},
		{false, "XYZ!"},
		{true, "79068"},
		{true, "+123 ABC$"},
		{false, "\x01"},
		{false, "\x7F"},
		{false, "\x80"},
		{false, "\xC0"},
		{false, "\xFF"},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		ASSERT_EQ(qr_isalnum(cases[i].text), cases[i].answer);
	}

	PASS();
}


TEST
IsNumeric(void)
{
	struct TestCase {
		bool answer;
		const char *text;
	};
	const struct TestCase cases[] = {
		{true, ""},
		{true, "0"},
		{false, "A"},
		{false, "a"},
		{false, " "},
		{false, "."},
		{false, "*"},
		{false, ","},
		{false, "|"},
		{false, "@"},
		{false, "XYZ"},
		{false, "XYZ!"},
		{true, "79068"},
		{false, "+123 ABC$"},
		{false, "\x01"},
		{false, "\x7F"},
		{false, "\x80"},
		{false, "\xC0"},
		{false, "\xFF"},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		ASSERT_EQ(qr_isnumeric(cases[i].text), cases[i].answer);
	}

	PASS();
}


TEST
CalcSegmentBufferSize(void)
{
	{
		const size_t cases[][2] = {
			{0, 0},
			{1, 1},
			{2, 1},
			{3, 2},
			{4, 2},
			{5, 3},
			{6, 3},
			{1472, 614},
			{2097, 874},
			{5326, 2220},
			{9828, 4095},
			{9829, 4096},
			{9830, 4096},
			{9831, SIZE_MAX},
			{9832, SIZE_MAX},
			{12000, SIZE_MAX},
			{28453, SIZE_MAX},
			{55555, SIZE_MAX},
			{SIZE_MAX / 6, SIZE_MAX},
			{SIZE_MAX / 4, SIZE_MAX},
			{SIZE_MAX / 2, SIZE_MAX},
			{SIZE_MAX / 1, SIZE_MAX},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(qr_calcSegmentBufferSize(QR_MODE_NUMERIC, cases[i][0]), cases[i][1]);
		}
	}
	{
		const size_t cases[][2] = {
			{0, 0},
			{1, 1},
			{2, 2},
			{3, 3},
			{4, 3},
			{5, 4},
			{6, 5},
			{1472, 1012},
			{2097, 1442},
			{5326, 3662},
			{5955, 4095},
			{5956, 4095},
			{5957, 4096},
			{5958, SIZE_MAX},
			{5959, SIZE_MAX},
			{12000, SIZE_MAX},
			{28453, SIZE_MAX},
			{55555, SIZE_MAX},
			{SIZE_MAX / 10, SIZE_MAX},
			{SIZE_MAX / 8, SIZE_MAX},
			{SIZE_MAX / 5, SIZE_MAX},
			{SIZE_MAX / 2, SIZE_MAX},
			{SIZE_MAX / 1, SIZE_MAX},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(qr_calcSegmentBufferSize(QR_MODE_ALNUM, cases[i][0]), cases[i][1]);
		}
	}
	{
		const size_t cases[][2] = {
			{0, 0},
			{1, 1},
			{2, 2},
			{3, 3},
			{1472, 1472},
			{2097, 2097},
			{4094, 4094},
			{4095, 4095},
			{4096, SIZE_MAX},
			{4097, SIZE_MAX},
			{5957, SIZE_MAX},
			{12000, SIZE_MAX},
			{28453, SIZE_MAX},
			{55555, SIZE_MAX},
			{SIZE_MAX / 16 + 1, SIZE_MAX},
			{SIZE_MAX / 14, SIZE_MAX},
			{SIZE_MAX / 9, SIZE_MAX},
			{SIZE_MAX / 7, SIZE_MAX},
			{SIZE_MAX / 4, SIZE_MAX},
			{SIZE_MAX / 3, SIZE_MAX},
			{SIZE_MAX / 2, SIZE_MAX},
			{SIZE_MAX / 1, SIZE_MAX},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(qr_calcSegmentBufferSize(QR_MODE_BYTE, cases[i][0]), cases[i][1]);
		}
	}
	{
		const size_t cases[][2] = {
			{0, 0},
			{1, 2},
			{2, 4},
			{3, 5},
			{1472, 2392},
			{2097, 3408},
			{2519, 4094},
			{2520, 4095},
			{2521, SIZE_MAX},
			{5957, SIZE_MAX},
			{2522, SIZE_MAX},
			{12000, SIZE_MAX},
			{28453, SIZE_MAX},
			{55555, SIZE_MAX},
			{SIZE_MAX / 13 + 1, SIZE_MAX},
			{SIZE_MAX / 12, SIZE_MAX},
			{SIZE_MAX / 9, SIZE_MAX},
			{SIZE_MAX / 4, SIZE_MAX},
			{SIZE_MAX / 3, SIZE_MAX},
			{SIZE_MAX / 2, SIZE_MAX},
			{SIZE_MAX / 1, SIZE_MAX},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(qr_calcSegmentBufferSize(QR_MODE_KANJI, cases[i][0]), cases[i][1]);
		}
	}
	{
		ASSERT_EQ(qr_calcSegmentBufferSize(QR_MODE_ECI, 0), 3);
	}

	PASS();
}


TEST
CalcSegmentBitLength(void)
{
	{
		const int cases[][2] = {
			{0, 0},
			{1, 4},
			{2, 7},
			{3, 10},
			{4, 14},
			{5, 17},
			{6, 20},
			{1472, 4907},
			{2097, 6990},
			{5326, 17754},
			{9828, 32760},
			{9829, 32764},
			{9830, 32767},
			{9831, -1},
			{9832, -1},
			{12000, -1},
			{28453, -1},
			{INT_MAX / 3, -1},
			{INT_MAX / 2, -1},
			{INT_MAX / 1, -1},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(count_seg_bits(QR_MODE_NUMERIC, cases[i][0]), cases[i][1]);
		}
	}
	{
		const int cases[][2] = {
			{0, 0},
			{1, 6},
			{2, 11},
			{3, 17},
			{4, 22},
			{5, 28},
			{6, 33},
			{1472, 8096},
			{2097, 11534},
			{5326, 29293},
			{5955, 32753},
			{5956, 32758},
			{5957, 32764},
			{5958, -1},
			{5959, -1},
			{12000, -1},
			{28453, -1},
			{INT_MAX / 5, -1},
			{INT_MAX / 4, -1},
			{INT_MAX / 3, -1},
			{INT_MAX / 2, -1},
			{INT_MAX / 1, -1},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(count_seg_bits(QR_MODE_ALNUM, cases[i][0]), cases[i][1]);
		}
	}
	{
		const int cases[][2] = {
			{0, 0},
			{1, 8},
			{2, 16},
			{3, 24},
			{1472, 11776},
			{2097, 16776},
			{4094, 32752},
			{4095, 32760},
			{4096, -1},
			{4097, -1},
			{5957, -1},
			{12000, -1},
			{28453, -1},
			{INT_MAX / 8 + 1, -1},
			{INT_MAX / 7, -1},
			{INT_MAX / 6, -1},
			{INT_MAX / 5, -1},
			{INT_MAX / 4, -1},
			{INT_MAX / 3, -1},
			{INT_MAX / 2, -1},
			{INT_MAX / 1, -1},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(count_seg_bits(QR_MODE_BYTE, cases[i][0]), cases[i][1]);
		}
	}
	{
		const int cases[][2] = {
			{0, 0},
			{1, 13},
			{2, 26},
			{3, 39},
			{1472, 19136},
			{2097, 27261},
			{2519, 32747},
			{2520, 32760},
			{2521, -1},
			{5957, -1},
			{2522, -1},
			{12000, -1},
			{28453, -1},
			{INT_MAX / 13 + 1, -1},
			{INT_MAX / 12, -1},
			{INT_MAX / 9, -1},
			{INT_MAX / 4, -1},
			{INT_MAX / 3, -1},
			{INT_MAX / 2, -1},
			{INT_MAX / 1, -1},
		};
		for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
			ASSERT_EQ(count_seg_bits(QR_MODE_KANJI, cases[i][0]), cases[i][1]);
		}
	}
	{
		ASSERT_EQ(count_seg_bits(QR_MODE_ECI, 0), 24);
	}

	PASS();
}


TEST
MakeBytes(void)
{
	{
		const uint8_t data[] = {0x00};
		struct qr_segment *seg = qr_make_bytes(data, 1);
		ASSERT_EQ(seg->u.m.len, 1);
		ASSERT_EQ(seg->count, 8);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x00);
		seg_free(seg);
	}
	{
		const uint8_t data[] = {0xEF, 0xBB, 0xBF};
		struct qr_segment *seg = qr_make_bytes(data, 3);
		ASSERT_EQ(seg->u.m.len, 3);
		ASSERT_EQ(seg->count, 24);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xEF);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0xBB);
		ASSERT_EQ(((const uint8_t *) seg->data)[2], 0xBF);
		seg_free(seg);
	}

	PASS();
}


TEST
MakeNumeric(void)
{
	{
		struct qr_segment *seg = qr_make_numeric("9");
		ASSERT_EQ(strlen(seg->u.s), 1);
		ASSERT_EQ(seg->count, 4);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x90);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_numeric("81");
		ASSERT_EQ(strlen(seg->u.s), 2);
		ASSERT_EQ(seg->count, 7);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xA2);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_numeric("673");
		ASSERT_EQ(strlen(seg->u.s), 3);
		ASSERT_EQ(seg->count, 10);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xA8);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0x40);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_numeric("3141592653");
		ASSERT_EQ(strlen(seg->u.s), 10);
		ASSERT_EQ(seg->count, 34);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x4E);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0x89);
		ASSERT_EQ(((const uint8_t *) seg->data)[2], 0xF4);
		ASSERT_EQ(((const uint8_t *) seg->data)[3], 0x24);
		ASSERT_EQ(((const uint8_t *) seg->data)[4], 0xC0);
		seg_free(seg);
	}

	PASS();
}


TEST
MakeAlphanumeric(void)
{
	{
		struct qr_segment *seg = qr_make_alnum("A");
		ASSERT_EQ(strlen(seg->u.s), 1);
		ASSERT_EQ(seg->count, 6);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x28);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_alnum("%:");
		ASSERT_EQ(strlen(seg->u.s), 2);
		ASSERT_EQ(seg->count, 11);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xDB);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0x40);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_alnum("Q R");
		ASSERT_EQ(strlen(seg->u.s), 3);
		ASSERT_EQ(seg->count, 17);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x96);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0xCD);
		ASSERT_EQ(((const uint8_t *) seg->data)[2], 0x80);
		seg_free(seg);
	}

	PASS();
}


TEST
MakeEci(void)
{
	{
		struct qr_segment *seg = qr_make_eci(127);
		ASSERT_EQ(seg->mode, QR_MODE_ECI);
		ASSERT_EQ(seg->count, 8);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0x7F);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_eci(10345);
		ASSERT_EQ(seg->count, 16);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xA8);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0x69);
		seg_free(seg);
	}
	{
		struct qr_segment *seg = qr_make_eci(999999);
		ASSERT_EQ(seg->count, 24);
		ASSERT_EQ(((const uint8_t *) seg->data)[0], 0xCF);
		ASSERT_EQ(((const uint8_t *) seg->data)[1], 0x42);
		ASSERT_EQ(((const uint8_t *) seg->data)[2], 0x3F);
		seg_free(seg);
	}

	PASS();
}


TEST
GetTotalBits(void)
{
	{
		ASSERT_EQ(count_total_bits(NULL, 0, QR_VER_MIN), 0);
		ASSERT_EQ(count_total_bits(NULL, 0, QR_VER_MAX), 0);
	}
	{
		struct qr_segment *segs[] = {
			& (struct qr_segment) { QR_MODE_BYTE, .u.m = { { 'x', 'y', 'z' }, 3 }, NULL, 24 },
		};
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 2), 36);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 10), 44);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 39), 44);
	}
	{
		struct qr_segment *segs[] = {
			& (struct qr_segment) { QR_MODE_ECI,     .u.eci = ECI_DEFAULT, NULL, 8 },
			& (struct qr_segment) { QR_MODE_NUMERIC, .u.s   = "abcdxyz", NULL, 24 },
			& (struct qr_segment) { QR_MODE_ALNUM,   .u.s   = "X", NULL, 6 },
			& (struct qr_segment) { QR_MODE_KANJI,   .u.s   = "xxyy", NULL, 52 },
		};
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 9), 133);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 21), 139);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 27), 145);
	}
	{
		struct qr_segment *segs[] = {
			& (struct qr_segment) { QR_MODE_BYTE, .u.m = { "...", 4093 }, NULL, 32744 },
		};
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 1), -1);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 10), 32764);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 27), 32764);
	}
	{
		char a[2047 + 1];
		memset(a, 'x', sizeof a - 1);
		a[sizeof a - 1] = '\0';

		char b[1617 + 1];
		memset(b, 'x', sizeof b - 1);
		b[sizeof b - 1] = '\0';

		struct qr_segment *segs[] = {
			& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "" }, NULL, 6824 },
			& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "" }, NULL, 6824 },
			& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "" }, NULL, 6824 },
			& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "" }, NULL, 6824 },
			& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "" }, NULL, 5390 },
		};
		strcpy(segs[0]->u.s, a);
		strcpy(segs[1]->u.s, a);
		strcpy(segs[2]->u.s, a);
		strcpy(segs[3]->u.s, a);
		strcpy(segs[4]->u.s, b);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 1), -1);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 10), 32766);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 27), -1);
	}
	{
		char a[255 + 1];
		memset(a, 'x', sizeof a - 1);
		a[sizeof a - 1] = '\0';

		char b[511 + 1];
		memset(b, 'x', sizeof b - 1);
		b[sizeof b - 1] = '\0';

		struct qr_segment *segs[] = {
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_KANJI, { .s = "" }, NULL, 3315 },
			& (struct qr_segment) { QR_MODE_ALNUM, { .s = "" }, NULL, 2811 },
		};
		strcpy(segs[0]->u.s, a);
		strcpy(segs[1]->u.s, a);
		strcpy(segs[2]->u.s, a);
		strcpy(segs[3]->u.s, a);
		strcpy(segs[4]->u.s, a);
		strcpy(segs[5]->u.s, a);
		strcpy(segs[6]->u.s, a);
		strcpy(segs[7]->u.s, a);
		strcpy(segs[8]->u.s, a);
		strcpy(segs[9]->u.s, b);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 9), 32767);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 26), -1);
		ASSERT_EQ(count_total_bits(segs, ARRAY_LENGTH(segs), 40), -1);
	}

	PASS();
}


TEST
Examples(void)
{
	struct qr_data data;
	struct qr_stats stats;
	enum qr_decode e;
	struct qr q;
	size_t i;

	uint8_t map[QR_BUF_LEN_MAX];
	q.map = map;

	const struct {
		const char *name;
		enum qr_mask mask;
		enum qr_ecl ecl;
		size_t n;
		struct qr_segment * const *a;
	} a[] = {
		{
			"../examples/fig1.pbm",
			QR_MASK_5,
			QR_ECL_MEDIUM,
			1, (struct qr_segment *[]) {
				& (struct qr_segment) { QR_MODE_BYTE, { .m = { "QR Code Symbol", 14 } }, NULL, 0 }
			}
		},
		{
			"../examples/figG2.pbm",
			QR_MASK_3,
			QR_ECL_MEDIUM,
			1, (struct qr_segment *[]) {
				& (struct qr_segment) { QR_MODE_NUMERIC, { .s = "01234567" }, NULL, 0 }
			}
		},
		{
			"../examples/a37.pbm",
			QR_MASK_6,
			QR_ECL_MEDIUM,
			1, (struct qr_segment *[]) {
				& (struct qr_segment) { QR_MODE_ALNUM, { .s =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"ABCDEFGHIJKLMNOPQRSTUV" }, NULL, 0 }
			}
		}
	};

	for (i = 0; i < ARRAY_LENGTH(a); i++) {
		FILE *f;
		uint8_t dtmp[QR_BUF_LEN_MAX];

		f = fopen(a[i].name, "rb");
		if (f == NULL) {
			perror(a[i].name);
			exit(EXIT_FAILURE);
		}

		if (!qr_load_pbm(f, &q, false)) {
			fprintf(stderr, "load %s: %s\n", a[i].name, strerror(errno));
			FAIL();
		}

		e = qr_decode(&q, &data, &stats, dtmp);
		if (e) {
			fprintf(stderr, "decode %s: %s\n", a[i].name, qr_strerror(e));
			FAIL();
		}

		if (data.ecl != a[i].ecl) {
			FAIL();
		}

		if (data.mask != a[i].mask) {
			FAIL();
		}

		if (!seg_cmp(data.a, data.n, a[i].a, a[i].n)) {
			FAIL();
		}

		fclose(f);
	}

	PASS();
}


TEST
Decode(void)
{
	struct qr_data data;
	struct qr_stats stats;
	enum qr_decode e;
	struct qr q;

	enum eci eci;
	enum qr_ecl ecl;
	unsigned min, max;
	enum qr_mask mask;
	bool boost_ecl;

	struct qr_segment **a;
	size_t n, j;
	size_t i;

	uint8_t map[QR_BUF_LEN_MAX];
	uint8_t tmp[QR_BUF_LEN_MAX];
	q.map = map;

	min  = QR_VER_MIN;
	max  = QR_VER_MAX;
	mask = QR_MASK_AUTO;
	ecl  = QR_ECL_LOW;
	eci  = ECI_DEFAULT;
	boost_ecl = true;

	n = 1;
	a = xmalloc(sizeof *a * n);

	const char *s[] = {
		"",
		"hi",
		"HI",
		"0",
		"1234",
		"01234",
		"HIhi0123"
	};

	for (i = 0; i < ARRAY_LENGTH(s); i++) {
		uint8_t dtmp[QR_BUF_LEN_MAX];

		for (j = 0; j < n; j++) {
			/* TODO: iconv from C execution character set */
			(void) eci;

			a[j] = qr_make_any(s[i]);
		}

		if (!qr_encode(a, n, ecl, min, max, mask, boost_ecl, tmp, &q)) {
			fprintf(stderr, "encode i=%zu: %s\n", i, strerror(errno));
			FAIL();
		}

		e = qr_decode(&q, &data, &stats, dtmp);
		if (e) {
			fprintf(stderr, "decode i=%zu: %s\n", i, qr_strerror(e));
			FAIL();
		}

		for (j = 0; j < n; j++) {
			seg_free(a[j]);
		}
	}

	free(a);

	PASS();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
	GREATEST_MAIN_BEGIN();

	RUN_TEST(AppendBitsToBuffer);
	RUN_TEST(AppendErrorCorrection);
	RUN_TEST(ErrorCorrectionBlockLengths);
	RUN_TEST(GetNumRawDataModules);
	RUN_TEST(GetNumDataCodewords);
	RUN_TEST(CalcReedSolomonGenerator);
	RUN_TEST(CalcReedSolomonRemainder);
	RUN_TEST(FiniteFieldMultiply);
	RUN_TEST(InitializeFunctionModulesEtc);
	RUN_TEST(GetAlignmentPatternPositions);
	RUN_TEST(GetSetModule);
	RUN_TEST(GetSetModuleRandomly);
	RUN_TEST(IsAlphanumeric);
	RUN_TEST(IsNumeric);
	RUN_TEST(CalcSegmentBufferSize);
	RUN_TEST(CalcSegmentBitLength);
	RUN_TEST(MakeBytes);
	RUN_TEST(MakeNumeric);
	RUN_TEST(MakeAlphanumeric);
	RUN_TEST(MakeEci);
	RUN_TEST(GetTotalBits);
	RUN_TEST(Examples);
	RUN_TEST(Decode);

	GREATEST_MAIN_END();
}

