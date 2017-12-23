#define _POSIX_C_SOURCE 2

#include <unistd.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <qr.h>
#include <print.h>

#include "encode.c"
#include "decode.c"

enum img {
	IMG_UTF8QB,
	IMG_XPM,
	IMG_XBM,
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
		{ IMG_XPM,    "xpm"    },
		{ IMG_XBM,    "xbm"    },
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

int
main(int argc, char * const argv[])
{
	enum qr_mask mask;
	enum qr_ecl ecl;
	unsigned min, max;
	bool boost_ecl;
	bool invert;
	bool wide;
	unsigned noise;
	enum img img;

	min  = QR_VER_MIN;
	max  = QR_VER_MAX;
	mask = QR_MASK_AUTO;
	ecl  = QR_ECL_LOW;
	boost_ecl = true;
	invert = true;
	wide = false;
	noise = 0;
	img = IMG_UTF8QB;

	{
		int c;

		while (c = getopt(argc, argv, "rbl:m:n:e:v:w"), c != -1) {
			switch (c) {
			case 'r':
				invert = false;
				break;

			case 'b':
				boost_ecl = false;
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

		if (argc != 1) {
			exit(EXIT_FAILURE);
		}
	}

	/* TODO: iterate over argv[], guess at type for each segment */
	/* TODO: micro-QR */

	struct qr q;
	uint8_t map[QR_BUF_LEN_MAX];
	q.map = map;
	uint8_t tmp[QR_BUF_LEN_MAX];
	if (!qr_encode_str(argv[0], tmp, &q, ecl, min, max, mask, boost_ecl)) {
		fprintf(stderr, "hmm\n"); /* XXX */
		exit(EXIT_FAILURE);
	}

	qr_noise(&q, noise, 0, false);

	switch (img) {
	case IMG_UTF8QB: qr_print_utf8qb(stdout, &q, wide, invert); break;
	case IMG_XPM:    qr_print_xpm(stdout, &q, invert);          break;
	case IMG_XBM:    qr_print_xbm(stdout, &q, invert);          break;
	case IMG_PBM1:   qr_print_pbm1(stdout, &q, invert);         break;
	case IMG_PBM4:   qr_print_pbm4(stdout, &q, invert);         break;
	case IMG_SVG:    qr_print_svg(stdout, &q, invert);          break;
	}

	{
		struct qr_data data;
		quirc_decode_error_t e;

		e = quirc_decode(&q, &data);

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
			printf("    Format corrections: %u\n", data.format_corrections);
			printf("    Codeword corrections: %u\n", data.codeword_corrections);
			printf("    Length: %zu\n", data.payload_len);
			printf("    Payload: %s\n", data.payload);
		}

		printf("\n");
	}

	return 0;
}

