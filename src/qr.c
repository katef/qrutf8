#define _POSIX_C_SOURCE 2

#include <unistd.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <qr.h>
#include <io.h>

#include "internal.h"
#include "ssim.h"
#include "seg.h"
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

	for (i = 0; i < n; i++) {
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
	unsigned min, max;
	bool boost_ecl;
	bool decode;
	bool invert;
	bool wide;
	unsigned noise;
	enum img img;
	const char *filename = NULL;
	const char *target   = NULL;

	min  = QR_VER_MIN;
	max  = QR_VER_MAX;
	mask = QR_MASK_AUTO;
	ecl  = QR_ECL_LOW;
	boost_ecl = true;
	decode = false;
	invert = true;
	wide = false;
	noise = 0;
	img = IMG_UTF8QB;

	{
		int c;

		while (c = getopt(argc, argv, "drbf:t:l:m:n:e:v:w"), c != -1) {
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

			case 'f':
				filename = optarg;
				break;

			case 't':
				target = optarg;
				break;

			case 'w':
				wide = true;
				break;

			case 'n':
				noise = atoi(optarg); /* XXX */
				break;

			case 'l':
				img = imgname(optarg);
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
	} else {
		encode_argv(&q, argc, argv,
			ecl, min, max, mask, boost_ecl);
	}

	qr_noise(&q, noise, 0, false);

	switch (img) {
	case IMG_UTF8QB: qr_print_utf8qb(stdout, &q, wide, invert); break;
	case IMG_PBM1:   qr_print_pbm1(stdout, &q, invert);         break;
	case IMG_PBM4:   qr_print_pbm4(stdout, &q, invert);         break;
	case IMG_SVG:    qr_print_svg(stdout, &q, invert);          break;
	}

	if (decode) {
		struct qr_data data;
		struct qr_stats stats;
		quirc_decode_error_t e;

		e = quirc_decode(&q, &data, &stats);

		if (e) {
			printf("  Decoding FAILED: %s\n", quirc_strerror(e));
		} else {
			printf("  Decoding successful:\n");

			printf("    Version: %d\n", data.ver);
			printf("    ECC level: %c\n", "MLHQ"[data.ecc_level]);
			printf("    Mask: %d\n", data.mask);

			if (data.eci) {
				printf("    ECI: %d\n", data.eci);
			}

			printf("    Noise: %u\n", noise);
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

		uint8_t tmap[QR_BUF_LEN_MAX];
		t.map = tmap;

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

