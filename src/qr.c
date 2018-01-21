#define _POSIX_C_SOURCE 2

#include <unistd.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <eci.h>
#include <qr.h>
#include <io.h>

#include "internal.h"
#include "fuzz.h"
#include "ssim.h"
#include "pcg.h"
#include "seg.h"
#include "util.h"
#include "xalloc.h"

enum img {
	IMG_UTF8QB,
	IMG_PBM1,
	IMG_PBM4,
	IMG_SVG
};

static enum img
imgname(const char *s)
{
	size_t i;

	static const struct {
		enum img img;
		const char *s;
	} a[] = {
		{ IMG_UTF8QB, "utf8qb" },
		{ IMG_PBM1,   "pbm1"   },
		{ IMG_PBM4,   "pbm4"   },
		{ IMG_SVG,    "svg"    }
	};

	for (i = 0; i < sizeof a / sizeof *a; i++) {
		if (0 == strcmp(a[i].s, s)) {
			return a[i].img;
		}
	}

	fprintf(stderr, "unrecognised image format '%s'; valid formats are: ", s);

	for (i = 0; i < sizeof a / sizeof *a; i++) {
		fprintf(stderr, "%s", a[i].s);
		if (i < sizeof a / sizeof *a) {
			fprintf(stderr, ", ");
		}
	}

	exit(EXIT_FAILURE);
}

static void
yv12(const struct qr *q, YV12_BUFFER_CONFIG *img)
{
	size_t x, y;

	img->y_width   = q->size;
	img->y_height  = q->size;
	img->y_stride  = q->size;
	img->uv_width  = q->size;
	img->uv_height = q->size;
	img->uv_stride = q->size;

	img->y_buffer  = xmalloc(q->size * q->size);
	img->u_buffer  = img->y_buffer;
	img->v_buffer  = img->y_buffer;

	for (y = 0; y < q->size; y++) {
		for (x = 0; x < q->size; x++) {
			img->y_buffer[y * q->size + x] = qr_get_module(q, x, y) ? 255 : 0;
		}
	}
}

static void
fuzz_ecl(void *opaque, enum qr_ecl *ecl, bool *boost_ecl)
{
	pcg32_random_t *pcg = opaque;

	assert(pcg != NULL);
	assert(ecl != NULL);
	assert(boost_ecl != NULL);

	*ecl       = pcg32_boundedrand_r(pcg, 4);
	*boost_ecl = pcg32_boundedrand_r(pcg, 2);
}

static void
fuzz_ver(void *opaque, unsigned *min, unsigned *max)
{
	pcg32_random_t *pcg = opaque;

	assert(pcg != NULL);
	assert(min != NULL);
	assert(max != NULL);

	*min = pcg32_boundedrand_r(pcg, QR_VER_MAX - QR_VER_MIN + 1) + QR_VER_MIN;
	*max = pcg32_boundedrand_r(pcg, QR_VER_MAX - *min + 1) + *min;
}

static void
fuzz_mask(void *opaque, signed *mask)
{
	pcg32_random_t *pcg = opaque;

	assert(pcg != NULL);
	assert(mask != NULL);

	*mask = pcg32_boundedrand_r(pcg, 1 + 8) - 1;
}

static void
fuzz_mode(void *opaque, enum qr_mode *mode)
{
	pcg32_random_t *pcg = opaque;

	const enum qr_mode m[] = {
		QR_MODE_NUMERIC,
		QR_MODE_ALNUM,
		QR_MODE_BYTE,
		QR_MODE_KANJI,
		QR_MODE_ECI
	};

	assert(pcg != NULL);
	assert(mode != NULL);

	*mode = m[pcg32_boundedrand_r(pcg, sizeof m / sizeof *m)];
}

static void
fuzz_uint(void *opaque, unsigned *n, unsigned max)
{
	pcg32_random_t *pcg = opaque;

	assert(pcg != NULL);
	assert(n != NULL);

	*n = pcg32_boundedrand_r(pcg, max + 1);
}

static void
encode_fuzz(struct qr *q, uint64_t seed,
	enum eci eci,
	enum qr_ecl ecl,
	unsigned min, unsigned max,
	enum qr_mask mask,
	bool boost_ecl)
{
	struct fuzz_instance *o;
	pcg32_random_t pcg;

	const struct fuzz_hook hook = {
		.fuzz_ecl  = fuzz_ecl,
		.fuzz_ver  = fuzz_ver,
		.fuzz_mask = fuzz_mask,
		.fuzz_mode = fuzz_mode,
		.fuzz_uint = fuzz_uint
	};

	pcg32_srandom_r(&pcg, seed, 1);

	do {
		o = fuzz_alloc(&pcg, &hook);
	} while (o == NULL);

	/* TODO: */
	(void) eci;

	if (!boost_ecl) {
		o->ecl = ecl;
	}

	if (o->min < min) {
		o->min = min;
	}

	if (o->max > max) {
		o->max = max;
	}

	if (o->min > o->max) {
		o->min = o->max;
	}

	if (mask != QR_MASK_AUTO) {
		o->mask = mask;
	}

	uint8_t tmp[QR_BUF_LEN_MAX];

	if (!qr_encode(o->a, o->n, o->ecl, o->min, o->max, o->mask, o->boost_ecl, tmp, q)) {
		/* TODO: */
		exit(EXIT_FAILURE);
	}
}

static void
encode_file(struct qr *q, const char *filename)
{
	FILE *f;

	assert(q != NULL);
	assert(filename != NULL);

	f = fopen(filename, "rb");
	if (f == NULL) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	/* TODO: separate mechanism to invert from file */
	if (!qr_load_pbm(f, q, false)) {
		/* TODO: */
		exit(EXIT_FAILURE);
	}

	fclose(f);
}

static void
encode_argv(struct qr *q, int argc, char * const argv[],
	enum eci eci,
	enum qr_ecl ecl,
	unsigned min, unsigned max,
	enum qr_mask mask,
	bool boost_ecl)
{
	struct qr_segment **a;
	size_t i, n;

	assert(q != NULL);
	assert(argc >= 0);
	assert(argv != NULL);

	n = argc;
	a = xmalloc(sizeof *a * n);

	/* TODO: add ECI_AUTO option, to find whatever best suits this segment */
	/* TODO: override eci by getopt */
	(void) eci;

	for (i = 0; i < n; i++) {
		/* TODO: iconv from terminal encoding to eci, per segment */

		a[i] = qr_make_any(argv[i]);
	}

	uint8_t tmp[QR_BUF_LEN_MAX];
	if (!qr_encode(a, n, ecl, min, max, mask, boost_ecl, tmp, q)) {
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		seg_free(a[i]);
	}

	free(a);
}

int
main(int argc, char * const argv[])
{
	enum qr_mask mask;
	enum qr_ecl ecl;
	enum eci eci;
	unsigned min, max;
	bool boost_ecl;
	bool fuzz;
	bool decode;
	bool invert;
	enum qr_utf8 uwidth;
	unsigned noise;
	enum img img;
	uint64_t seed;
	const char *filename = NULL;
	const char *target   = NULL;

	min  = QR_VER_MIN;
	max  = QR_VER_MAX;
	mask = QR_MASK_AUTO;
	ecl  = QR_ECL_LOW;
	eci  = ECI_DEFAULT;
	boost_ecl = true;
	fuzz = false;
	decode = false;
	invert = true;
	uwidth = QR_UTF8_DOUBLE;
	noise = 0;
	seed = 0;
	img = IMG_UTF8QB;

	{
		int c;

		while (c = getopt(argc, argv, "drbf:t:l:m:n:e:v:y:swz"), c != -1) {
			switch (c) {
			case 'd':
				decode = true;
				break;

			case 'r':
				invert = false;
				break;

			case 'b':
				boost_ecl = false;
				break;

			case 'z':
				fuzz = true;
				break;

			case 'f':
				filename = optarg;
				break;

			case 't':
				target = optarg;
				break;

			case 'w':
				uwidth = QR_UTF8_WIDE;
				break;

			case 's':
				uwidth = QR_UTF8_SINGLE;
				break;

			case 'n':
				noise = atoi(optarg); /* XXX */
				break;

			case 'l':
				img = imgname(optarg);
				break;

			case 'y':
				seed = atoi(optarg); /* XXX */
				break;

			case 'm':
				if (0 == strcmp(optarg, "auto")) {
					mask = QR_MASK_AUTO;
					break;
				}

				mask = atoi(optarg);
				if (mask < 0 || mask > 7) {
					fprintf(stderr, "invalid mask\n");
					exit(EXIT_FAILURE);
				}
				break;

			case 'v':
				min = atoi(optarg);
				max = min;
				if (min < QR_VER_MIN || max > QR_VER_MAX) {
					fprintf(stderr, "version out of range\n");
					exit(EXIT_FAILURE);
				}
				break;

			case 'e':
				if (0 == strcmp(optarg, "low"))      { ecl = QR_ECL_LOW;      break; }
				if (0 == strcmp(optarg, "medium"))   { ecl = QR_ECL_MEDIUM;   break; }
				if (0 == strcmp(optarg, "quartile")) { ecl = QR_ECL_QUARTILE; break; }
				if (0 == strcmp(optarg, "high"))     { ecl = QR_ECL_HIGH;     break; }

				fprintf(stderr, "invalid ecl\n");
				exit(EXIT_FAILURE);

			case '?':
			default:
				exit(EXIT_FAILURE);
				break;
			}
		}

		argc -= optind;
		argv += optind;
	}

	/* TODO: micro-QR */

	struct qr q;
	uint8_t map[QR_BUF_LEN_MAX];
	q.map = map;

	if (filename != NULL) {
		if (argc != 0) {
			exit(EXIT_FAILURE);
		}

		encode_file(&q, filename);
	} else if (fuzz) {
		encode_fuzz(&q, seed,
			eci,
			ecl, min, max, mask, boost_ecl);
	} else {
		encode_argv(&q, argc, argv,
			eci,
			ecl, min, max, mask, boost_ecl);
	}

	qr_noise(&q, noise, seed, false);

	switch (img) {
	case IMG_UTF8QB: qr_print_utf8qb(stdout, &q, uwidth, invert); break;
	case IMG_PBM1:   qr_print_pbm1(stdout, &q, invert);           break;
	case IMG_PBM4:   qr_print_pbm4(stdout, &q, invert);           break;
	case IMG_SVG:    qr_print_svg(stdout, &q, invert);            break;
	}

	if (decode) {
		struct qr_data data;
		struct qr_stats stats;
		enum qr_decode e;

		uint8_t tmp[QR_BUF_LEN_MAX];

		e = qr_decode(&q, &data, &stats, tmp);

		if (e) {
			printf("  Decoding FAILED: %s\n", qr_strerror(e));
		} else {
			printf("  Decoding successful:\n");

			printf("    Version: %d\n", data.ver);
			printf("    ECC level: %c\n", "LMQH"[(int) data.ecl]);
			printf("    Mask: %d\n", data.mask);

			struct qr mq;
			uint8_t mtmp[QR_BUF_LEN_MAX];
			mq.size = q.size;
			mq.map = mtmp;
			memcpy(mq.map, q.map, QR_BUF_LEN(data.ver));
			qr_apply_mask(&mq, data.mask);
			qr_print_utf8qb(stdout, &mq, uwidth, invert);

			printf("    Noise: %u\n", noise);
			printf("    Raw bitstream: %zu bits\n", stats.raw.bits);
			hexdump(stdout, stats.raw.data, BM_LEN(stats.raw.bits));
			printf("    ECC bitstream: %zu bits\n", stats.ecc.bits);
			hexdump(stdout, stats.ecc.data, BM_LEN(stats.ecc.bits));
			printf("    Corrected bitstream: %zu bits\n", stats.corrected.bits);
			hexdump(stdout, stats.corrected.data, BM_LEN(stats.corrected.bits));
			printf("    Padding: %zu bits\n", stats.padding.bits);
			hexdump(stdout, stats.padding.data, BM_LEN(stats.padding.bits));
			printf("    Raw formats:\n");
			hexdump(stdout, (void *) &stats.format_raw[0], sizeof stats.format_raw[0]);
			hexdump(stdout, (void *) &stats.format_raw[1], sizeof stats.format_raw[1]);
			printf("    Corrected formats:\n");
			hexdump(stdout, (void *) &stats.format_corrected[0], sizeof stats.format_corrected[0]);
			hexdump(stdout, (void *) &stats.format_corrected[1], sizeof stats.format_corrected[1]);
			printf("    Format corrections: %u\n", stats.format_corrections);
			printf("    Codeword corrections: %u\n", stats.codeword_corrections);
			seg_print(stdout, data.n, data.a);
		}

		printf("\n");
	}

	if (target != NULL) {
		FILE *f;
		struct qr t;
		double p;

		uint8_t tmp[QR_BUF_LEN_MAX];
		t.map = tmp;

		/* Open input file. */
		f = fopen(target, "rb");
		if (f == NULL) {
			exit(EXIT_FAILURE);
		}

		if (!qr_load_pbm(f, &t, invert)) {
			exit(EXIT_FAILURE);
		}

		fclose(f);

		YV12_BUFFER_CONFIG a, b;

		yv12(&q, &a);
		yv12(&t, &b);

		p = vp8_calc_ssimg(&a, &b);
		printf("ssimg: %f\n", 1 / (1 - p));

		p = vp8_calc_ssim(&a, &b);
		printf("ssim: %f\n", 1 / (1 - p));

		free(a.y_buffer);
		free(b.y_buffer);
	}

	return 0;
}

