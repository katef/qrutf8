/* 
 * QR Code generator test suite (C)
 * 
 * When compiling this program, the library qrcodegen.c needs QRCODEGEN_TEST
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

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST
#include "qr-utf8.c"

#define ARRAY_LENGTH(name)  (sizeof(name) / sizeof(name[0]))

#ifndef __cplusplus
	#define MALLOC(num, type)  malloc((num) * sizeof(type))
#else
	#define MALLOC(num, type)  static_cast<type*>(malloc((num) * sizeof(type)))
#endif


// Global variables
static int numTestCases = 0;


/*---- Test cases ----*/

static void testAppendBitsToBuffer(void) {
	{
		uint8_t buf[1] = {0};
		size_t bitLen = 0;
		append_bits(0, 0, buf, &bitLen);
		assert(bitLen == 0);
		assert(buf[0] == 0);
		append_bits(1, 1, buf, &bitLen);
		assert(bitLen == 1);
		assert(buf[0] == 0x80);
		append_bits(0, 1, buf, &bitLen);
		assert(bitLen == 2);
		assert(buf[0] == 0x80);
		append_bits(5, 3, buf, &bitLen);
		assert(bitLen == 5);
		assert(buf[0] == 0xA8);
		append_bits(6, 3, buf, &bitLen);
		assert(bitLen == 8);
		assert(buf[0] == 0xAE);
		numTestCases++;
	}
	{
		uint8_t buf[6] = {0};
		size_t bitLen = 0;
		append_bits(16942, 16, buf, &bitLen);
		assert(bitLen == 16);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x00 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5] == 0x00);
		append_bits(10, 7, buf, &bitLen);
		assert(bitLen == 23);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x14 && buf[3] == 0x00 && buf[4] == 0x00 && buf[5] == 0x00);
		append_bits(15, 4, buf, &bitLen);
		assert(bitLen == 27);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xE0 && buf[4] == 0x00 && buf[5] == 0x00);
		append_bits(26664, 15, buf, &bitLen);
		assert(bitLen == 42);
		assert(buf[0] == 0x42 && buf[1] == 0x2E && buf[2] == 0x15 && buf[3] == 0xFA && buf[4] == 0x0A && buf[5] == 0x00);
		numTestCases++;
	}
}


// Ported from the Java version of the code.
static uint8_t *append_eclReference(const uint8_t *data, int version, enum qr_ecl ecl) {
	// Calculate parameter numbers
	int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[(int)ecl][version];
	int blockEccLen = ECL_CODEWORDS_PER_BLOCK[(int)ecl][version];
	int rawCodewords = count_data_bits(version) / 8;
	int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	int shortBlockLen = rawCodewords / numBlocks;
	
	// Split data into blocks and append ECC to each block
	uint8_t **blocks = MALLOC(numBlocks, uint8_t*);
	uint8_t *generator = MALLOC(blockEccLen, uint8_t);
	reed_solomon_generator(blockEccLen, generator);
	for (int i = 0, k = 0; i < numBlocks; i++) {
		uint8_t *block = MALLOC(shortBlockLen + 1, uint8_t);
		int blockDataLen = shortBlockLen - blockEccLen + (i < numShortBlocks ? 0 : 1);
		memcpy(block, &data[k], blockDataLen * sizeof(uint8_t));
		reed_solomon_remainder(&data[k], blockDataLen, generator, blockEccLen, &block[shortBlockLen + 1 - blockEccLen]);
		k += blockDataLen;
		blocks[i] = block;
	}
	free(generator);
	
	// Interleave (not concatenate) the bytes from every block into a single sequence
	uint8_t *result = MALLOC(rawCodewords, uint8_t);
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


static void testAppendErrorCorrection(void) {
	for (int version = 1; version <= 40; version++) {
		for (int ecl = 0; ecl < 4; ecl++) {
			int dataLen = count_codewords(version, (enum qr_ecl)ecl);
			uint8_t *pureData = MALLOC(dataLen, uint8_t);
			for (int i = 0; i < dataLen; i++)
				pureData[i] = rand() % 256;
			uint8_t *expectOutput = append_eclReference(pureData, version, (enum qr_ecl)ecl);
			
			int dataAndEccLen = count_data_bits(version) / 8;
			uint8_t *paddedData = MALLOC(dataAndEccLen, uint8_t);
			memcpy(paddedData, pureData, dataLen * sizeof(uint8_t));
			uint8_t *actualOutput = MALLOC(dataAndEccLen, uint8_t);
			append_ecl(paddedData, version, (enum qr_ecl)ecl, actualOutput);
			
			assert(memcmp(actualOutput, expectOutput, dataAndEccLen * sizeof(uint8_t)) == 0);
			free(pureData);
			free(expectOutput);
			free(paddedData);
			free(actualOutput);
			numTestCases++;
		}
	}
}


static void testGetNumRawDataModules(void) {
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
		assert(count_data_bits(tc[0]) == tc[1]);
		numTestCases++;
	}
}


static void testGetNumDataCodewords(void) {
	const int cases[][3] = {
		{ 3, 1,   44},
		{ 3, 2,   34},
		{ 3, 3,   26},
		{ 6, 0,  136},
		{ 7, 0,  156},
		{ 9, 0,  232},
		{ 9, 1,  182},
		{12, 3,  158},
		{15, 0,  523},
		{16, 2,  325},
		{19, 3,  341},
		{21, 0,  932},
		{22, 0, 1006},
		{22, 1,  782},
		{22, 3,  442},
		{24, 0, 1174},
		{24, 3,  514},
		{28, 0, 1531},
		{30, 3,  745},
		{32, 3,  845},
		{33, 0, 2071},
		{33, 3,  901},
		{35, 0, 2306},
		{35, 1, 1812},
		{35, 2, 1286},
		{36, 3, 1054},
		{37, 3, 1096},
		{39, 1, 2216},
		{40, 1, 2334},
	};
	for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
		const int *tc = cases[i];
		assert(count_codewords(tc[0], (enum qr_ecl)tc[1]) == tc[2]);
		numTestCases++;
	}
}


static void testCalcReedSolomonGenerator(void) {
	uint8_t generator[30];
	
	reed_solomon_generator(1, generator);
	assert(generator[0] == 0x01);
	numTestCases++;
	
	reed_solomon_generator(2, generator);
	assert(generator[0] == 0x03);
	assert(generator[1] == 0x02);
	numTestCases++;
	
	reed_solomon_generator(5, generator);
	assert(generator[0] == 0x1F);
	assert(generator[1] == 0xC6);
	assert(generator[2] == 0x3F);
	assert(generator[3] == 0x93);
	assert(generator[4] == 0x74);
	numTestCases++;
	
	reed_solomon_generator(30, generator);
	assert(generator[ 0] == 0xD4);
	assert(generator[ 1] == 0xF6);
	assert(generator[ 5] == 0xC0);
	assert(generator[12] == 0x16);
	assert(generator[13] == 0xD9);
	assert(generator[20] == 0x12);
	assert(generator[27] == 0x6A);
	assert(generator[29] == 0x96);
	numTestCases++;
}


static void testCalcReedSolomonRemainder(void) {
	{
		uint8_t data[1];
		uint8_t generator[3];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, 0, generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == 0);
		assert(remainder[1] == 0);
		assert(remainder[2] == 0);
		numTestCases++;
	}
	{
		uint8_t data[2] = {0, 1};
		uint8_t generator[4];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == generator[0]);
		assert(remainder[1] == generator[1]);
		assert(remainder[2] == generator[2]);
		assert(remainder[3] == generator[3]);
		numTestCases++;
	}
	{
		uint8_t data[5] = {0x03, 0x3A, 0x60, 0x12, 0xC7};
		uint8_t generator[5];
		uint8_t remainder[ARRAY_LENGTH(generator)];
		reed_solomon_generator(ARRAY_LENGTH(generator), generator);
		reed_solomon_remainder(data, ARRAY_LENGTH(data), generator, ARRAY_LENGTH(generator), remainder);
		assert(remainder[0] == 0xCB);
		assert(remainder[1] == 0x36);
		assert(remainder[2] == 0x16);
		assert(remainder[3] == 0xFA);
		assert(remainder[4] == 0x9D);
		numTestCases++;
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
		assert(remainder[ 0] == 0xCE);
		assert(remainder[ 1] == 0xF0);
		assert(remainder[ 2] == 0x31);
		assert(remainder[ 3] == 0xDE);
		assert(remainder[ 8] == 0xE1);
		assert(remainder[12] == 0xCA);
		assert(remainder[17] == 0xE3);
		assert(remainder[19] == 0x85);
		assert(remainder[20] == 0x50);
		assert(remainder[24] == 0xBE);
		assert(remainder[29] == 0xB3);
		numTestCases++;
	}
}


static void testFiniteFieldMultiply(void) {
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
		assert(finiteFieldMul(tc[0], tc[1]) == tc[2]);
		numTestCases++;
	}
}


static void testInitializeFunctionModulesEtc(void) {
	for (int ver = 1; ver <= 40; ver++) {
		uint8_t *qrcode = MALLOC(QR_BUF_LEN(ver), uint8_t);
struct qr_code q = { 0, qrcode };
		assert(qrcode != NULL);
		draw_init(ver, &q);

		int size = q.size;
		if (ver == 1)
			assert(size == 21);
		else if (ver == 40)
			assert(size == 177);
		else
			assert(size == ver * 4 + 17);

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
		assert(hasWhite && hasBlack);
		free(qrcode);
		numTestCases++;
	}
}


static void testGetAlignmentPatternPositions(void) {
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
		uint8_t pos[7];
		int num = getAlignmentPatternPositions(tc[0], pos);
		assert(num == tc[1]);
		for (int j = 0; j < num; j++)
			assert(pos[j] == tc[2 + j]);
		numTestCases++;
	}
}


static void testGetSetModule(void) {
	uint8_t qrcode[QR_BUF_LEN(23)];
struct qr_code q = { 0, qrcode };
	draw_init(23, &q);
	int size = q.size;
	
	for (int y = 0; y < size; y++) {  // Clear all to white
		for (int x = 0; x < size; x++)
			set_module(&q, x, y, false);
	}
	for (int y = 0; y < size; y++) {  // Check all white
		for (int x = 0; x < size; x++)
			assert(qr_get_module(&q, x, y) == false);
	}
	for (int y = 0; y < size; y++) {  // Set all to black
		for (int x = 0; x < size; x++)
			set_module(&q, x, y, true);
	}
	for (int y = 0; y < size; y++) {  // Check all black
		for (int x = 0; x < size; x++)
			assert(qr_get_module(&q, x, y) == true);
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
			assert(qr_get_module(&q, x, y) == true);
	}
	
	// Set some modules to white
	set_module(&q, 3, 8, false);
	set_module(&q, 61, 49, false);
	for (int y = 0; y < size; y++) {  // Check most black
		for (int x = 0; x < size; x++) {
			bool white = (x == 3 && y == 8) || (x == 61 && y == 49);
			assert(qr_get_module(&q, x, y) != white);
		}
	}
	numTestCases++;
}


static void testGetSetModuleRandomly(void) {
	uint8_t qrcode[QR_BUF_LEN(1)];
struct qr_code q = { 0, qrcode };
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
			assert(qr_get_module(&q, x, y) == oldColor);
//		assert(qr_get_module(&q, x, y) == oldColor);
		
		bool newColor = rand() % 2 == 0;
		if (isInBounds)
			modules[y][x] = newColor;
		if (isInBounds && rand() % 2 == 0)
			set_module(&q, x, y, newColor);
		else
			set_module_bounded(&q, x, y, newColor);
	}
	numTestCases++;
}


static void testIsAlphanumeric(void) {
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
		assert(qr_isalnum(cases[i].text) == cases[i].answer);
		numTestCases++;
	}
}


static void testIsNumeric(void) {
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
		assert(qr_isnumeric(cases[i].text) == cases[i].answer);
		numTestCases++;
	}
}


static void testCalcSegmentBufferSize(void) {
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
			assert(qr_calcSegmentBufferSize(QR_MODE_NUMERIC, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(qr_calcSegmentBufferSize(QR_MODE_ALNUM, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(qr_calcSegmentBufferSize(QR_MODE_BYTE, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(qr_calcSegmentBufferSize(QR_MODE_KANJI, cases[i][0]) == cases[i][1]);
			numTestCases++;
		}
	}
	{
		assert(qr_calcSegmentBufferSize(QR_MODE_ECI, 0) == 3);
		numTestCases++;
	}
}


static void testCalcSegmentBitLength(void) {
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
			assert(count_seg_bits(QR_MODE_NUMERIC, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(count_seg_bits(QR_MODE_ALNUM, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(count_seg_bits(QR_MODE_BYTE, cases[i][0]) == cases[i][1]);
			numTestCases++;
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
			assert(count_seg_bits(QR_MODE_KANJI, cases[i][0]) == cases[i][1]);
			numTestCases++;
		}
	}
	{
		assert(count_seg_bits(QR_MODE_ECI, 0) == 24);
		numTestCases++;
	}
}


static void testMakeBytes(void) {
	{
		struct qr_segment seg = qr_make_bytes(NULL, 0);
		assert(seg.mode == QR_MODE_BYTE);
		assert(seg.len == 0);
		assert(seg.count == 0);
		numTestCases++;
	}
	{
		const uint8_t data[] = {0x00};
		struct qr_segment seg = qr_make_bytes(data, 1);
		assert(seg.len == 1);
		assert(seg.count == 8);
		assert(((const uint8_t *) seg.data)[0] == 0x00);
		numTestCases++;
	}
	{
		const uint8_t data[] = {0xEF, 0xBB, 0xBF};
		struct qr_segment seg = qr_make_bytes(data, 3);
		assert(seg.len == 3);
		assert(seg.count == 24);
		assert(((const uint8_t *) seg.data)[0] == 0xEF);
		assert(((const uint8_t *) seg.data)[1] == 0xBB);
		assert(((const uint8_t *) seg.data)[2] == 0xBF);
		numTestCases++;
	}
}


static void testMakeNumeric(void) {
	{
		struct qr_segment seg = qr_make_numeric("", NULL);
		assert(seg.mode == QR_MODE_NUMERIC);
		assert(seg.len == 0);
		assert(seg.count == 0);
		numTestCases++;
	}
	{
		uint8_t buf[1];
		struct qr_segment seg = qr_make_numeric("9", buf);
		assert(seg.len == 1);
		assert(seg.count == 4);
		assert(((const uint8_t *) seg.data)[0] == 0x90);
		numTestCases++;
	}
	{
		uint8_t buf[1];
		struct qr_segment seg = qr_make_numeric("81", buf);
		assert(seg.len == 2);
		assert(seg.count == 7);
		assert(((const uint8_t *) seg.data)[0] == 0xA2);
		numTestCases++;
	}
	{
		uint8_t buf[2];
		struct qr_segment seg = qr_make_numeric("673", buf);
		assert(seg.len == 3);
		assert(seg.count == 10);
		assert(((const uint8_t *) seg.data)[0] == 0xA8);
		assert(((const uint8_t *) seg.data)[1] == 0x40);
		numTestCases++;
	}
	{
		uint8_t buf[5];
		struct qr_segment seg = qr_make_numeric("3141592653", buf);
		assert(seg.len == 10);
		assert(seg.count == 34);
		assert(((const uint8_t *) seg.data)[0] == 0x4E);
		assert(((const uint8_t *) seg.data)[1] == 0x89);
		assert(((const uint8_t *) seg.data)[2] == 0xF4);
		assert(((const uint8_t *) seg.data)[3] == 0x24);
		assert(((const uint8_t *) seg.data)[4] == 0xC0);
		numTestCases++;
	}
}


static void testMakeAlphanumeric(void) {
	{
		struct qr_segment seg = qr_make_alnum("", NULL);
		assert(seg.mode == QR_MODE_ALNUM);
		assert(seg.len == 0);
		assert(seg.count == 0);
		numTestCases++;
	}
	{
		uint8_t buf[1];
		struct qr_segment seg = qr_make_alnum("A", buf);
		assert(seg.len == 1);
		assert(seg.count == 6);
		assert(((const uint8_t *) seg.data)[0] == 0x28);
		numTestCases++;
	}
	{
		uint8_t buf[2];
		struct qr_segment seg = qr_make_alnum("%:", buf);
		assert(seg.len == 2);
		assert(seg.count == 11);
		assert(((const uint8_t *) seg.data)[0] == 0xDB);
		assert(((const uint8_t *) seg.data)[1] == 0x40);
		numTestCases++;
	}
	{
		uint8_t buf[3];
		struct qr_segment seg = qr_make_alnum("Q R", buf);
		assert(seg.len == 3);
		assert(seg.count == 17);
		assert(((const uint8_t *) seg.data)[0] == 0x96);
		assert(((const uint8_t *) seg.data)[1] == 0xCD);
		assert(((const uint8_t *) seg.data)[2] == 0x80);
		numTestCases++;
	}
}


static void testMakeEci(void) {
	{
		uint8_t buf[1];
		struct qr_segment seg = qr_make_eci(127, buf);
		assert(seg.mode == QR_MODE_ECI);
		assert(seg.len == 0);
		assert(seg.count == 8);
		assert(((const uint8_t *) seg.data)[0] == 0x7F);
		numTestCases++;
	}
	{
		uint8_t buf[2];
		struct qr_segment seg = qr_make_eci(10345, buf);
		assert(seg.len == 0);
		assert(seg.count == 16);
		assert(((const uint8_t *) seg.data)[0] == 0xA8);
		assert(((const uint8_t *) seg.data)[1] == 0x69);
		numTestCases++;
	}
	{
		uint8_t buf[3];
		struct qr_segment seg = qr_make_eci(999999, buf);
		assert(seg.len == 0);
		assert(seg.count == 24);
		assert(((const uint8_t *) seg.data)[0] == 0xCF);
		assert(((const uint8_t *) seg.data)[1] == 0x42);
		assert(((const uint8_t *) seg.data)[2] == 0x3F);
		numTestCases++;
	}
}


static void testGetTotalBits(void) {
	{
		assert(count_total_bits(NULL, 0, 1) == 0);
		numTestCases++;
		assert(count_total_bits(NULL, 0, 40) == 0);
		numTestCases++;
	}
	{
		struct qr_segment segs[] = {
			{QR_MODE_BYTE, 3, NULL, 24},
		};
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 2) == 36);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 10) == 44);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 39) == 44);
		numTestCases++;
	}
	{
		struct qr_segment segs[] = {
			{QR_MODE_ECI, 0, NULL, 8},
			{QR_MODE_NUMERIC, 7, NULL, 24},
			{QR_MODE_ALNUM, 1, NULL, 6},
			{QR_MODE_KANJI, 4, NULL, 52},
		};
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 9) == 133);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 21) == 139);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 27) == 145);
		numTestCases++;
	}
	{
		struct qr_segment segs[] = {
			{QR_MODE_BYTE, 4093, NULL, 32744},
		};
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 1) == -1);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 10) == 32764);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 27) == 32764);
		numTestCases++;
	}
	{
		struct qr_segment segs[] = {
			{QR_MODE_NUMERIC, 2047, NULL, 6824},
			{QR_MODE_NUMERIC, 2047, NULL, 6824},
			{QR_MODE_NUMERIC, 2047, NULL, 6824},
			{QR_MODE_NUMERIC, 2047, NULL, 6824},
			{QR_MODE_NUMERIC, 1617, NULL, 5390},
		};
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 1) == -1);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 10) == 32766);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 27) == -1);
		numTestCases++;
	}
	{
		struct qr_segment segs[] = {
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_KANJI, 255, NULL, 3315},
			{QR_MODE_ALNUM, 511, NULL, 2811},
		};
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 9) == 32767);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 26) == -1);
		numTestCases++;
		assert(count_total_bits(segs, ARRAY_LENGTH(segs), 40) == -1);
		numTestCases++;
	}
}


/*---- Main runner ----*/

int main(void) {
	srand(time(NULL));
	testAppendBitsToBuffer();
	testAppendErrorCorrection();
	testGetNumRawDataModules();
	testGetNumDataCodewords();
	testCalcReedSolomonGenerator();
	testCalcReedSolomonRemainder();
	testFiniteFieldMultiply();
	testInitializeFunctionModulesEtc();
	testGetAlignmentPatternPositions();
	testGetSetModule();
	testGetSetModuleRandomly();
	testIsAlphanumeric();
	testIsNumeric();
	testCalcSegmentBufferSize();
	testCalcSegmentBitLength();
	testMakeBytes();
	testMakeNumeric();
	testMakeAlphanumeric();
	testMakeEci();
	testGetTotalBits();
	printf("All %d test cases passed\n", numTestCases);

	(void) qr_print_utf8qb;

	return EXIT_SUCCESS;
}
