
#define _POSIX_C_SOURCE 2

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libqr.c"
#include "util.c"
#include "decode.c"

#include <theft.h>

enum gate {
	GATE_START,
	GATE_ENCODED,
	GATE_DECODED,
	GATE_METADATA_VERIFIED,
	GATE_PAYLOAD_VERIFIED
};

struct qr_instance {
	/* input */
	size_t n;
	struct qr_segment *a;
	enum qr_ecl ecl;
	unsigned min;
	unsigned max;
	enum qr_mask mask;
	bool boost_ecl;

	enum gate gate;

	/* encoded */
	struct qr_code q;
	uint8_t map[QR_BUF_LEN_MAX];
	int qr_errno;

	/* decoded */
	struct quirc_data data;
	quirc_decode_error_t quirc_err;

	/* verified */
	char v_err[256];
};

static size_t
seg_len(const struct qr_segment *a, size_t n)
{
	size_t len;
	size_t j;

	assert(a != NULL);

	len = 0;

	for (j = 0; j < n; j++) {
		len += a[j].len;
	}

	return len;
}

static enum theft_trial_res
prop_roundtrip(struct theft *t, void *instance)
{
	struct qr_instance *o;
	struct quirc_data data;

	assert(t != NULL);
	assert(instance != NULL);

	o = instance;

	{
		uint8_t tmp[QR_BUF_LEN_MAX];
		struct qr_code q;

		q.map = o->map;

		if (!qr_encode_segments(o->a, o->n, o->ecl, o->min, o->max, o->mask, o->boost_ecl, tmp, &q)) {
			if (errno == EMSGSIZE) {
				return THEFT_TRIAL_SKIP;
			}

			o->qr_errno = errno;
			return THEFT_TRIAL_ERROR;
		}

		o->q = q;
		o->gate = GATE_ENCODED;
	}

	{
		struct quirc_code code;
		quirc_decode_error_t e;

		code.size = o->q.size;
		memcpy(code.cell_bitmap, o->q.map, o->q.size * o->q.size / 8);

		e = quirc_decode(&code, &data);

		if (e) {
			o->quirc_err = e;
			return THEFT_TRIAL_FAIL;
		}

		o->data = data;
		o->gate = GATE_DECODED;
	}

	{
		if (o->mask != QR_MASK_AUTO && data.mask != o->mask) {
			snprintf(o->v_err, sizeof o->v_err,
				"mask mismatch: got=%d, expected=%d",
				data.mask, o->mask);
			return THEFT_TRIAL_FAIL;
		}

		enum qr_ecl ecl[] = { QR_ECL_MEDIUM, QR_ECL_LOW, QR_ECL_HIGH, QR_ECL_QUARTILE };
		if (o->boost_ecl) {
			if (ecl[data.ecc_level] < o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					ecl[data.ecc_level], o->ecl);
				return THEFT_TRIAL_FAIL;
			}
		} else {
			if (ecl[data.ecc_level] != o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					ecl[data.ecc_level], o->ecl);
				return THEFT_TRIAL_FAIL;
			}
		}

		if (data.version < (int) o->min || data.version > (int) o->max) {
			snprintf(o->v_err, sizeof o->v_err,
				"version mismatch: got=%d, expected min=%u, max=%u",
				data.version, o->min, o->max);
			return THEFT_TRIAL_FAIL;
		}

		o->gate = GATE_METADATA_VERIFIED;
	}

	{
		size_t j;
		const unsigned char *p;

		if ((size_t) data.payload_len != seg_len(o->a, o->n)) {
			snprintf(o->v_err, sizeof o->v_err,
				"payload length mismatch: got=%zu, expected=%zu",
				(size_t) data.payload_len, seg_len(o->a, o->n));
			return THEFT_TRIAL_FAIL;
		}

		p = data.payload;

		for (j = 0; j < o->n; j++) {
			assert(p - data.payload <= data.payload_len);

			/* XXX: .len's meaning depends on .mode */
			if (0 != memcmp(p, o->a[j].data, o->a[j].len)) {
				snprintf(o->v_err, sizeof o->v_err,
					"payload data mismatch for segment %zu", j);
				return THEFT_TRIAL_FAIL;
			}

			p += o->a[j].len;
		}

		o->gate = GATE_PAYLOAD_VERIFIED;
	}

	(void) t;

	return THEFT_TRIAL_PASS;
}

static enum theft_alloc_res
seg_alloc(struct theft *t, void *env, void **instance)
{
	struct qr_instance *o;
	size_t j;

	assert(t != NULL);
	assert(env == NULL);
	assert(instance != NULL);

	o = malloc(sizeof *o);
	if (o == NULL) {
		return THEFT_ALLOC_ERROR;
	}

	o->gate = GATE_START;

	o->ecl       = theft_random_choice(t, 4);
	o->min       = theft_random_choice(t, QR_VER_MAX - QR_VER_MIN + 1) + QR_VER_MIN;
	o->max       = theft_random_choice(t, QR_VER_MAX - o->min + 1) + o->min;
	o->boost_ecl = theft_random_choice(t, 2);
	o->mask      = theft_random_choice(t, 1 + 8) - 1;

	o->n = theft_random_choice(t, 1000); /* TODO: find upper limit */
	o->a = malloc(sizeof *o->a * o->n);
	if (o->a == NULL) {
		free(o);
		return THEFT_ALLOC_ERROR;
	}

	/* TODO: permutate segments */
	(void) gen_permutation_vector;

	for (j = 0; j < o->n; j++) {
		switch (theft_random_choice(t, 5)) {
		case 0: {
			size_t len, i;
			char *s;
			void *buf;

			buf = malloc(QR_BUF_LEN(o->max));
			if (buf == NULL) {
				goto error;
			}

			len = theft_random_choice(t, QR_BUF_LEN(o->max));

			if (qr_calcSegmentBufferSize(QR_MODE_NUMERIC, len) > QR_BUF_LEN(o->max)) {
				free(buf);
				goto skip;
			}

			s = malloc(len + 1);
			if (s == NULL) {
				free(buf);
				goto error;
			}

			for (i = 0; i < len; i++) {
				s[i] = '0' + theft_random_choice(t, 10);
			}
			s[i] = '\0';

			assert(qr_isnumeric(s));

			o->a[j] = qr_make_numeric(s, buf);

			free(s);
			break;
		}

		case 1: {
			size_t len, i;
			char *s;
			void *buf;

			buf = malloc(QR_BUF_LEN(o->max));
			if (buf == NULL) {
				goto error;
			}

			len = theft_random_choice(t, QR_BUF_LEN(o->max));

			if (qr_calcSegmentBufferSize(QR_MODE_ALNUM, len) > QR_BUF_LEN(o->max)) {
				free(buf);
				goto skip;
			}

			s = malloc(len + 1);
			if (s == NULL) {
				free(buf);
				goto error;
			}

			for (i = 0; i < len; i++) {
				s[i] = ALNUM_CHARSET[theft_random_choice(t, sizeof (ALNUM_CHARSET) - 1)];
			}
			s[i] = '\0';

			assert(qr_isalnum(s));

			o->a[j] = qr_make_alnum(s, buf);

			free(s);
			break;
		}

		case 2: {
			size_t len, i;
			uint8_t *a;

			len = theft_random_choice(t, 1000); /* arbitrary legnth */

			a = malloc(len);
			if (a == NULL) {
				goto error;
			}

			for (i = 0; i < len; i++) {
				a[i] = theft_random_choice(t, UINT8_MAX + 1);
			}

			o->a[j] = qr_make_bytes(a, len);
			break;
		}

		case 3:
			/* XXX: QR_MODE_KANJI not implemented */
			goto skip;

		case 4:
			/* XXX: QR_MODE_ECI not implemented */
			goto skip;

		default:
			assert(!"unreached");
		}
	}

	(void) env;

	*instance = o;

	return THEFT_ALLOC_OK;

error:

	for (size_t i = 0; i < j; i++) {
		free((void *) o->a[i].data);
	}

	free(o->a);
	free(o);

	return THEFT_ALLOC_ERROR;

skip:

	for (size_t i = 0; i < j; i++) {
		free((void *) o->a[i].data);
	}

	free(o->a);
	free(o);

	return THEFT_ALLOC_SKIP;
}

static void
seg_free(void *instance, void *env)
{
	struct qr_instance *o;
	size_t j;

	(void) env;

	o = instance;

	for (j = 0; j < o->n; j++) {
		free((void *) o->a[j].data);
	}

	free(o->a);

	free(o);
}

static void
seg_print(FILE *f, const void *instance, void *env)
{
	const struct qr_instance *o;
	size_t j;

	assert(f != NULL);
	assert(instance != NULL);
	assert(env == NULL);

	(void) env;

	o = instance;

	printf("    Segments {\n");
	for (j = 0; j < o->n; j++) {
		const char *dts;

		switch (o->a[j].mode) {
		case QR_MODE_NUMERIC: dts = "NUMERIC"; break;
		case QR_MODE_ALNUM:   dts = "ALNUM";   break;
		case QR_MODE_BYTE:    dts = "BYTE";    break;
		case QR_MODE_KANJI:   dts = "KANJI";   break;
		default: dts = "?"; break;
		}

		printf("      %zu: %d (%s)", j, o->a[j].mode, dts);
		if (qr_isalnum(o->a[j].data) || qr_isnumeric(o->a[j].data)) {
			printf(" \"%.*s\"\n",
				(int) o->a[j].len, (const char *) o->a[j].data);
		} else {
			printf(":\n");
			hexdump(stdout, o->a[j].data, o->a[j].len);
		}
	}
	printf("    }\n");
	printf("    Segments total data length: %zu\n", seg_len(o->a, o->n));

	if (o->gate < GATE_ENCODED) {
		fprintf(stderr, "\nqr_encode_segments: %s\n", strerror(o->qr_errno));
		return;
	}

	qr_print_utf8qb(stdout, &o->q, true, true);
	printf("    Size: %zu\n", o->q.size);

	if (o->gate < GATE_DECODED) {
		fprintf(stderr, "quirc_decode: %s\n", quirc_strerror(o->quirc_err));
		return;
	}

	{
		const char *dts;

		switch (o->data.data_type) {
		case QUIRC_DATA_TYPE_NUMERIC: dts = "NUMERIC"; break;
		case QUIRC_DATA_TYPE_ALPHA:   dts = "ALNUM";   break;
		case QUIRC_DATA_TYPE_BYTE:    dts = "BYTE";    break;
		case QUIRC_DATA_TYPE_KANJI:   dts = "KANJI";   break;
		default: dts = "?"; break;
		}

		printf("    Version: %d\n", o->data.version);
		printf("    ECC level: %c\n", "MLHQ"[o->data.ecc_level]);
		printf("    Mask: %d\n", o->data.mask);
		printf("    Data type: %d (%s)\n", o->data.data_type, dts);

		if (o->data.eci) {
			printf("    ECI: %d\n", o->data.eci);
		}
	}

	if (o->gate < GATE_METADATA_VERIFIED) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}

	{
		printf("    Length: %d\n", o->data.payload_len);
		printf("    Payload: %s\n", o->data.payload);
	}

	if (o->gate < GATE_PAYLOAD_VERIFIED) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}

	(void) o;
}

int
main(int argc, char *argv[])
{
	static struct theft_type_info seg_info = {
		.alloc = seg_alloc,
		.free  = seg_free,
		.print = seg_print,
		.autoshrink_config = { .enable = true }
	};

	struct theft_run_config config = {
		.name      = "roundtrip",
		.prop1     = prop_roundtrip,
		.type_info = { &seg_info },
		.trials    = 100000,
		.seed      = theft_seed_of_time()
	};

	(void) argc;
	(void) argv;

	(void) qr_print_xpm;
	(void) qr_print_utf8qb;

	return theft_run(&config) == THEFT_RUN_PASS ? EXIT_SUCCESS : EXIT_FAILURE;
}

