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

	min  = QR_VER_MIN;
	max  = QR_VER_MAX;
	mask = QR_MASK_AUTO;
	ecl  = QR_ECL_LOW;
	boost_ecl = true;
	invert = true;
	wide = false;
	noise = 0;

	{
		int c;

		while (c = getopt(argc, argv, "rbm:n:e:v:w"), c != -1) {
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

	qr_print_utf8qb(stdout, &q, wide, invert);
/*
	qr_print_xpm(stdout, &q, invert);
*/
	(void) qr_print_utf8qb;
	(void) qr_print_xpm;
	(void) wide;
	(void) invert;


	{
		struct quirc_data data;
		quirc_decode_error_t e;

		e = quirc_decode(&q, &data);

		if (e) {
			printf("  Decoding FAILED: %s\n", quirc_strerror(e));
		} else {
			printf("  Decoding successful:\n");

			printf("    Version: %d\n", data.version);
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

