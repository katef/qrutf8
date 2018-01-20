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

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"
#include "datastream.h"

#define MAX_POLY       64

/************************************************************************
 * Galois fields
 */

struct galois_field {
	int p;
	const uint8_t *log;
	const uint8_t *exp;
};

static const uint8_t gf16_exp[16] = {
	0x01, 0x02, 0x04, 0x08, 0x03, 0x06, 0x0c, 0x0b,
	0x05, 0x0a, 0x07, 0x0e, 0x0f, 0x0d, 0x09, 0x01
};

static const uint8_t gf16_log[16] = {
	0x00, 0x0f, 0x01, 0x04, 0x02, 0x08, 0x05, 0x0a,
	0x03, 0x0e, 0x09, 0x07, 0x06, 0x0d, 0x0b, 0x0c
};

static const struct galois_field gf16 = {
	.p = 15,
	.log = gf16_log,
	.exp = gf16_exp
};

static const uint8_t gf256_exp[256] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26,
	0x4c, 0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9,
	0x8f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
	0x9d, 0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35,
	0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23,
	0x46, 0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0,
	0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1,
	0x5f, 0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc,
	0x65, 0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0,
	0xfd, 0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f,
	0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2,
	0xd9, 0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
	0x0d, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce,
	0x81, 0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93,
	0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc,
	0x85, 0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9,
	0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54,
	0xa8, 0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa,
	0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73,
	0xe6, 0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e,
	0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff,
	0xe3, 0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4,
	0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41,
	0x82, 0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e,
	0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6,
	0x51, 0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef,
	0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09,
	0x12, 0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5,
	0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16,
	0x2c, 0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83,
	0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01
};

static const uint8_t gf256_log[256] = {
	0x00, 0xff, 0x01, 0x19, 0x02, 0x32, 0x1a, 0xc6,
	0x03, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b,
	0x04, 0x64, 0xe0, 0x0e, 0x34, 0x8d, 0xef, 0x81,
	0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x08, 0x4c, 0x71,
	0x05, 0x8a, 0x65, 0x2f, 0xe1, 0x24, 0x0f, 0x21,
	0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45,
	0x1d, 0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9,
	0xc9, 0x9a, 0x09, 0x78, 0x4d, 0xe4, 0x72, 0xa6,
	0x06, 0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd,
	0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88,
	0x36, 0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd,
	0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40,
	0x1e, 0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e,
	0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d,
	0xca, 0x5e, 0x9b, 0x9f, 0x0a, 0x15, 0x79, 0x2b,
	0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57,
	0x07, 0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0x0d,
	0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18,
	0xe3, 0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c,
	0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e,
	0x37, 0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd,
	0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61,
	0xf2, 0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e,
	0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2,
	0x1f, 0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76,
	0xc4, 0x17, 0x49, 0xec, 0x7f, 0x0c, 0x6f, 0xf6,
	0x6c, 0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa,
	0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a,
	0xcb, 0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51,
	0x0b, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7,
	0x4f, 0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8,
	0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf
};

static const struct galois_field gf256 = {
	.p = 255,
	.log = gf256_log,
	.exp = gf256_exp
};

const char *
qr_strerror(enum qr_decode err)
{
	switch (err) {
	case QR_SUCCESS:                 return "Success";
	case QR_ERROR_INVALID_MODE:      return "Invalid mode";
	case QR_ERROR_INVALID_GRID_SIZE: return "Invalid grid size";
	case QR_ERROR_INVALID_VERSION:   return "Invalid version";
	case QR_ERROR_FORMAT_ECC:        return "Format data ECC failure";
	case QR_ERROR_DATA_ECC:          return "ECC failure";
	case QR_ERROR_DATA_OVERFLOW:     return "Data overflow";
	case QR_ERROR_DATA_UNDERFLOW:    return "Data underflow";

	default:
		return "Unknown error";
	}
}

/************************************************************************
 * Polynomial operations
 */

static void
poly_add(uint8_t *dst, const uint8_t *src, uint8_t c,
	int shift, const struct galois_field *gf)
{
	int i;
	int log_c = gf->log[c];

	if (!c)
		return;

	for (i = 0; i < MAX_POLY; i++) {
		int p = i + shift;
		uint8_t v = src[i];

		if (p < 0 || p >= MAX_POLY)
			continue;
		if (!v)
			continue;

		dst[p] ^= gf->exp[(gf->log[v] + log_c) % gf->p];
	}
}

static uint8_t
poly_eval(const uint8_t *s, uint8_t x,
	const struct galois_field *gf)
{
	int i;
	uint8_t sum = 0;
	uint8_t log_x = gf->log[x];

	if (!x)
		return s[0];

	for (i = 0; i < MAX_POLY; i++) {
		uint8_t c = s[i];

		if (!c)
			continue;

		sum ^= gf->exp[(gf->log[c] + log_x * i) % gf->p];
	}

	return sum;
}

/************************************************************************
 * Berlekamp-Massey algorithm for finding error locator polynomials.
 */

static void
berlekamp_massey(const uint8_t *s, int N,
	const struct galois_field *gf,
	uint8_t *sigma)
{
	uint8_t C[MAX_POLY];
	uint8_t B[MAX_POLY];
	int L = 0;
	int m = 1;
	uint8_t b = 1;
	int n;

	memset(B, 0, sizeof(B));
	memset(C, 0, sizeof(C));
	B[0] = 1;
	C[0] = 1;

	for (n = 0; n < N; n++) {
		uint8_t d = s[n];
		uint8_t mult;
		int i;

		for (i = 1; i <= L; i++) {
			if (!(C[i] && s[n - i]))
				continue;

			d ^= gf->exp[(gf->log[C[i]] +
				      gf->log[s[n - i]]) %
				     gf->p];
		}

		mult = gf->exp[(gf->p - gf->log[b] + gf->log[d]) % gf->p];

		if (!d) {
			m++;
		} else if (L * 2 <= n) {
			uint8_t T[MAX_POLY];

			memcpy(T, C, sizeof(T));
			poly_add(C, B, mult, m, gf);
			memcpy(B, T, sizeof(B));
			L = n + 1 - L;
			b = d;
			m = 1;
		} else {
			poly_add(C, B, mult, m, gf);
			m++;
		}
	}

	memcpy(sigma, C, MAX_POLY);
}

/************************************************************************
 * Code stream error correction
 *
 * Generator polynomial for GF(2^8) is x^8 + x^4 + x^3 + x^2 + 1
 */

static int
block_syndromes(const uint8_t *data, int bs, int npar, uint8_t *s)
{
	int nonzero = 0;
	int i;

	memset(s, 0, MAX_POLY);

	for (i = 0; i < npar; i++) {
		int j;

		for (j = 0; j < bs; j++) {
			uint8_t c = data[bs - j - 1];

			if (!c)
				continue;

			s[i] ^= gf256_exp[((int) gf256_log[c] + i * j) % 255];
		}

		if (s[i])
			nonzero = 1;
	}

	return nonzero;
}

static void
eloc_poly(uint8_t *omega,
	const uint8_t *s, const uint8_t *sigma,
	int npar)
{
	int i;

	memset(omega, 0, MAX_POLY);

	for (i = 0; i < npar; i++) {
		const uint8_t a = sigma[i];
		const uint8_t log_a = gf256_log[a];
		int j;

		if (!a)
			continue;

		for (j = 0; j + 1 < MAX_POLY; j++) {
			const uint8_t b = s[j + 1];

			if (i + j >= npar)
				break;

			if (!b)
				continue;

			omega[i + j] ^=
			    gf256_exp[(log_a + gf256_log[b]) % 255];
		}
	}
}

static enum qr_decode
correct_block(uint8_t *data, int ecc_bs, int ecc_dw, unsigned *corrections)
{
	int npar = ecc_bs - ecc_dw;
	uint8_t s[MAX_POLY];
	uint8_t sigma[MAX_POLY];
	uint8_t sigma_deriv[MAX_POLY];
	uint8_t omega[MAX_POLY];
	int i;

	*corrections = 0;

	/* Compute syndrome vector */
	if (!block_syndromes(data, ecc_bs, npar, s))
		return QR_SUCCESS;

	berlekamp_massey(s, npar, &gf256, sigma);

	/* Compute derivative of sigma */
	memset(sigma_deriv, 0, MAX_POLY);
	for (i = 0; i + 1 < MAX_POLY; i += 2)
		sigma_deriv[i] = sigma[i + 1];

	/* Compute error evaluator polynomial */
	eloc_poly(omega, s, sigma, npar - 1);

	/* Find error locations and magnitudes */
	for (i = 0; i < ecc_bs; i++) {
		uint8_t xinv = gf256_exp[255 - i];

		if (!poly_eval(sigma, xinv, &gf256)) {
			uint8_t sd_x = poly_eval(sigma_deriv, xinv, &gf256);
			uint8_t omega_x = poly_eval(omega, xinv, &gf256);
			uint8_t error = gf256_exp[(255 - gf256_log[sd_x] +
						   gf256_log[omega_x]) % 255];

			(*corrections)++;
			data[ecc_bs - i - 1] ^= error;
		}
	}

	if (block_syndromes(data, ecc_bs, npar, s))
		return QR_ERROR_DATA_ECC;

	return QR_SUCCESS;
}

/************************************************************************
 * Format value error correction
 *
 * Generator polynomial for GF(2^4) is x^4 + x + 1
 */

#define FORMAT_MAX_ERROR        3
#define FORMAT_SYNDROMES        (FORMAT_MAX_ERROR * 2)
#define FORMAT_BITS             15

static int
format_syndromes(uint16_t u, uint8_t *s)
{
	int i;
	int nonzero = 0;

	memset(s, 0, MAX_POLY);

	for (i = 0; i < FORMAT_SYNDROMES; i++) {
		int j;

		s[i] = 0;
		for (j = 0; j < FORMAT_BITS; j++)
			if (u & (1 << j))
				s[i] ^= gf16_exp[((i + 1) * j) % 15];

		if (s[i])
			nonzero = 1;
	}

	return nonzero;
}

static enum qr_decode
correct_format(uint16_t *f_ret, unsigned *corrections)
{
	uint16_t u = *f_ret;
	int i;
	uint8_t s[MAX_POLY];
	uint8_t sigma[MAX_POLY];

	*corrections = 0;

	/* Evaluate U (received codeword) at each of alpha_1 .. alpha_6
	 * to get S_1 .. S_6 (but we index them from 0).
	 */
	if (!format_syndromes(u, s))
		return QR_SUCCESS;

	berlekamp_massey(s, FORMAT_SYNDROMES, &gf16, sigma);

	/* Now, find the roots of the polynomial */
	for (i = 0; i < 15; i++) {
		if (!poly_eval(sigma, gf16_exp[15 - i], &gf16)) {
			(*corrections)++;
			u ^= (1 << i);
		}
	}

	if (format_syndromes(u, s))
		return QR_ERROR_FORMAT_ECC;

	*f_ret = u;
	return QR_SUCCESS;
}

/************************************************************************
 * Decoder algorithm
 */

static enum qr_ecl
ecl_decode(int e)
{
	switch (e) {
	case 0x1: return QR_ECL_LOW;
	case 0x0: return QR_ECL_MEDIUM;
	case 0x3: return QR_ECL_QUARTILE;
	case 0x2: return QR_ECL_HIGH;

	default:
		assert(!"unreached");
		abort();
	}
}

static enum qr_decode
read_format(const struct qr *q,
	struct qr_data *data, struct qr_stats *stats, int which)
{
	int i;
	uint16_t format = 0;
	uint16_t fdata;
	enum qr_decode err;

	if (which) {
		for (i = 0; i < 7; i++)
			format = (format << 1) |
				qr_get_module(q, 8, q->size - 1 - i);
		for (i = 0; i < 8; i++)
			format = (format << 1) |
				qr_get_module(q, q->size - 8 + i, 8);
	} else {
		static const int xs[15] = {
			8, 8, 8, 8, 8, 8, 8, 8, 7, 5, 4, 3, 2, 1, 0
		};
		static const int ys[15] = {
			0, 1, 2, 3, 4, 5, 7, 8, 8, 8, 8, 8, 8, 8, 8
		};

		for (i = 14; i >= 0; i--)
			format = (format << 1) | qr_get_module(q, xs[i], ys[i]);
	}

	format ^= 0x5412;

	err = correct_format(&format, &stats->format_corrections);
	if (err)
		return err;

	fdata = format >> 10;
	data->ecl = ecl_decode(fdata >> 3);
	data->mask = fdata & 7;

	return QR_SUCCESS;
}

static enum qr_decode
codestream_ecc(struct qr_data *data, struct qr_stats *stats,
	struct qr_bytes *raw, struct qr_bytes *corrected)
{
	const int blockEccLen = ECL_CODEWORDS_PER_BLOCK[data->ver][data->ecl];
	const int rawCodewords = count_data_bits(data->ver) / 8;
	const int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[data->ver][data->ecl];
	const int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	const int ecc_bs = rawCodewords / numBlocks;
	const int shortBlockDataLen = ecc_bs - blockEccLen;

	const int lb_count = numBlocks - numShortBlocks;
	const int bc = lb_count + numShortBlocks;
	const int ecc_offset = shortBlockDataLen * bc + lb_count;
	int dst_offset = 0;
	int i;

	struct qr_rs_params {
		int bs; /* Small block size */
		int dw; /* Small data words */
	};

	struct qr_rs_params sb_ecc;
	sb_ecc.bs = ecc_bs;
	sb_ecc.dw = shortBlockDataLen;

	struct qr_rs_params lb_ecc;
	lb_ecc.bs = ecc_bs + 1;
	lb_ecc.dw = shortBlockDataLen + 1;

	for (i = 0; i < bc; i++) {
		uint8_t *dst = corrected->data + dst_offset;
		const struct qr_rs_params *ecc = (i < numShortBlocks) ? &sb_ecc : &lb_ecc;
		const int num_ec = ecc_bs - ecc->dw;
		enum qr_decode err;
		int j;

		for (j = 0; j < ecc->dw; j++)
			dst[j] = raw->data[j * bc + i];
		for (j = 0; j < num_ec; j++)
			dst[ecc->dw + j] = raw->data[ecc_offset + j * bc + i];

		err = correct_block(dst, ecc->bs, ecc->dw, &stats->codeword_corrections);
		if (err)
			return err;

		dst_offset += ecc->dw;
	}

	corrected->bits = dst_offset * 8;

	return QR_SUCCESS;
}

static int
tuple(char *s,
	struct qr_bytes *ds, size_t *ds_ptr,
	size_t bits, int digits,
	const char *charset)
{
	int tuple;
	int i;
	size_t n;

	assert(s != NULL);
	assert(charset != NULL);

	n = strlen(charset);

	if (ds->bits - *ds_ptr < bits)
		return -1;

	tuple = take_bits(ds, bits, ds_ptr);

	for (i = 0; i < digits; i++) {
		s[digits - i - 1] = charset[tuple % n];
		tuple /= n;
	}

	return 0;
}

static enum qr_decode
decode_numeric(unsigned ver, struct qr_segment *seg,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	static const char *numeric_map =
		"0123456789";

	size_t bits = 14;
	size_t count;
	size_t len;

	if (ver < 10)
		bits = 10;
	else if (ver < 27)
		bits = 12;

	count = take_bits(ds, bits, ds_ptr);
	if ((size_t) count > sizeof seg->u.s - 1)
		return QR_ERROR_DATA_OVERFLOW;

	len = 0;

	while (count >= 3) {
		if (tuple(seg->u.s + len, ds, ds_ptr, 10, 3, numeric_map) < 0)
			return QR_ERROR_DATA_UNDERFLOW;
		len += 3;
		count -= 3;
	}

	if (count >= 2) {
		if (tuple(seg->u.s + len, ds, ds_ptr, 7, 2, numeric_map) < 0)
			return QR_ERROR_DATA_UNDERFLOW;
		len += 2;
		count -= 2;
	}

	if (count) {
		if (tuple(seg->u.s + len, ds, ds_ptr, 4, 1, numeric_map) < 0)
			return QR_ERROR_DATA_UNDERFLOW;
		len += 1;
		count--;
	}

	seg->u.s[len] = '\0';

	return QR_SUCCESS;
}

static enum qr_decode
decode_alnum(unsigned ver, struct qr_segment *seg,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	static const char *alpha_map =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		" $%*+-./:";

	size_t bits = 13;
	size_t count;
	size_t len;

	if (ver < 10)
		bits = 9;
	else if (ver < 27)
		bits = 11;

	count = take_bits(ds, bits, ds_ptr);
	if ((size_t) count > sizeof seg->u.s - 1)
		return QR_ERROR_DATA_OVERFLOW;

	len = 0;

	while (count >= 2) {
		if (tuple(seg->u.s + len, ds, ds_ptr, 11, 2, alpha_map) < 0)
			return QR_ERROR_DATA_UNDERFLOW;
		len += 2;
		count -= 2;
	}

	if (count) {
		if (tuple(seg->u.s + len, ds, ds_ptr, 6, 1, alpha_map) < 0)
			return QR_ERROR_DATA_UNDERFLOW;
		len += 1;
		count--;
	}

	seg->u.s[len] = '\0';

	return QR_SUCCESS;
}

static enum qr_decode
decode_byte(unsigned ver, struct qr_segment *seg,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	size_t bits = 16;
	size_t count, i;
	size_t len;

	if (ver < 10)
		bits = 8;

	count = take_bits(ds, bits, ds_ptr);
	if ((size_t) count > sizeof seg->u.m.data)
		return QR_ERROR_DATA_OVERFLOW;
	if (ds->bits - *ds_ptr < count * 8)
		return QR_ERROR_DATA_UNDERFLOW;

	len = 0;

	for (i = 0; i < count; i++)
		seg->u.m.data[len++] = take_bits(ds, 8, ds_ptr);

	seg->u.m.bits = len * 8;

	return QR_SUCCESS;
}

static enum qr_decode
decode_kanji(unsigned ver, struct qr_segment *seg,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	size_t bits = 12;
	size_t count, i;
	size_t len;

	if (ver < 10)
		bits = 8;
	else if (ver < 27)
		bits = 10;

	count = take_bits(ds, bits, ds_ptr);
	if ((size_t) count * 2 > sizeof seg->u.s - 1)
		return QR_ERROR_DATA_OVERFLOW;
	if (ds->bits - *ds_ptr < count * 13)
		return QR_ERROR_DATA_UNDERFLOW;

	len = 0;

	for (i = 0; i < count; i++) {
		int d = take_bits(ds, 13, ds_ptr);
		int msB = d / 0xc0;
		int lsB = d % 0xc0;
		int intermediate = (msB << 8) | lsB;
		uint16_t sjw;

		if (intermediate + 0x8140 <= 0x9ffc) {
			/* bytes are in the range 0x8140 to 0x9FFC */
			sjw = intermediate + 0x8140;
		} else {
			/* bytes are in the range 0xE040 to 0xEBBF */
			sjw = intermediate + 0xc140;
		}

		seg->u.s[len++] = sjw >> 8;
		seg->u.s[len++] = sjw & 0xff;
	}

	seg->u.s[len] = '\0';

	return QR_SUCCESS;
}

static enum qr_decode
decode_eci(struct qr_segment *seg,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	unsigned eci;

	if (ds->bits - *ds_ptr < 8)
		return QR_ERROR_DATA_UNDERFLOW;

	eci = take_bits(ds, 8, ds_ptr);

	if ((eci & 0xc0) == 0x80) {
		if (ds->bits - *ds_ptr < 8)
			return QR_ERROR_DATA_UNDERFLOW;

		eci = (eci << 8) | take_bits(ds, 8, ds_ptr);
	} else if ((eci & 0xe0) == 0xc0) {
		if (ds->bits - *ds_ptr < 16)
			return QR_ERROR_DATA_UNDERFLOW;

		eci = (eci << 16) | take_bits(ds, 16, ds_ptr);
	}

	seg->u.eci = eci;

	return QR_SUCCESS;
}

static enum qr_decode
decode_payload(struct qr_data *data,
	struct qr_bytes *ds, size_t *ds_ptr)
{
	data->n = 0;
	data->a = NULL;

	while (ds->bits - *ds_ptr >= 4) {
		void *tmp;

		enum qr_decode err = QR_SUCCESS;
		enum qr_mode mode = take_bits(ds, 4, ds_ptr);

		/* XXX */
		tmp = realloc(data->a, sizeof *data->a * (data->n + 1));
		if (tmp == NULL) {
			free(data->a);
			return QR_ERROR_DATA_OVERFLOW; // XXX
		}
		data->a = tmp;

		size_t i = data->n;

		data->a[i] = malloc(sizeof *data->a[i]);
		if (data->a[i] == NULL) {
			free(data->a);
			return QR_ERROR_DATA_OVERFLOW; // XXX
		}

		data->a[i]->mode   = mode;
		(void) data->a[i]->m.data; // TODO: populate from ds
		data->a[i]->m.bits = 0;    // TODO: populate from ds

		if (mode == 0x0) {
			goto done;
		}

		switch (mode) {
		case QR_MODE_NUMERIC: err = decode_numeric(data->ver, data->a[i], ds, ds_ptr); break;
		case QR_MODE_ALNUM:   err = decode_alnum  (data->ver, data->a[i], ds, ds_ptr); break;
		case QR_MODE_BYTE:    err = decode_byte   (data->ver, data->a[i], ds, ds_ptr); break;
		case QR_MODE_KANJI:   err = decode_kanji  (data->ver, data->a[i], ds, ds_ptr); break;
		case QR_MODE_ECI:     err = decode_eci    (           data->a[i], ds, ds_ptr); break;

		default:
			free(data->a);
			return QR_ERROR_INVALID_MODE; // XXX
		}

		if (err)
			return err;

		data->n++;
	}

done:

	return QR_SUCCESS;
}

enum qr_decode
qr_decode(const struct qr *q,
	struct qr_data *data, struct qr_stats *stats,
	void *tmp)
{
	enum qr_decode err;
	struct qr_bytes raw, corrected;

	if ((q->size - 17) % 4)
		return QR_ERROR_INVALID_GRID_SIZE;

	memset(data, 0, sizeof(*data));
	memset(&raw, 0, sizeof(raw));
	memset(&corrected, 0, sizeof(corrected));

	data->ver = QR_VER(q->size);

	if (data->ver < QR_VER_MIN || data->ver > QR_VER_MAX)
		return QR_ERROR_INVALID_VERSION;

	/* Read format information -- try both locations */
	err = read_format(q, data, stats, 0);
	if (err)
		err = read_format(q, data, stats, 1);
	if (err)
		return err;

	/* Remove mask */
	struct qr qtmp;
	qtmp.map  = tmp;
	qtmp.size = q->size;
	memcpy(tmp, q->map, QR_BUF_LEN(data->ver));
	qr_apply_mask(&qtmp, data->mask); // Undoes the mask due to XOR

	read_data(&qtmp, &raw);
	err = codestream_ecc(data, stats, &raw, &corrected);
	if (err)
		return err;

	size_t ds_ptr = 0;
	err = decode_payload(data, &corrected, &ds_ptr);
	if (err)
		return err;

	return QR_SUCCESS;
}

