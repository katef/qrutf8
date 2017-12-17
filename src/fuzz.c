
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

#include <qr.h>
#include <print.h>

#include "xalloc.h"

#include "encode.c"
#include "decode.c"
#include "util.c"

#include <theft.h>

#include "../share/git/greatest/greatest.h"

enum gate {
	GATE_PASS,
	GATE_ENCODE   = 1 << 0,
	GATE_DECODE   = 1 << 1,
	GATE_NOISE    = 1 << 2,
	GATE_METADATA = 1 << 3,
	GATE_PAYLOAD  = 1 << 4
};

struct fuzz_segment {
	struct qr_segment seg;
	char buf[(32767 + 7) / 8 + 1]; /* XXX: centralise maths */

	/* source data */
	char s[QR_PAYLOAD_MAX];
	size_t len;
};

struct fuzz_instance {
	/* input */
	size_t n;
	struct fuzz_segment a[1000]; /* XXX: find max number of segments */
	enum qr_ecl ecl;
	unsigned min;
	unsigned max;
	enum qr_mask mask;
	bool boost_ecl;

	enum gate gate;

	/* encoded */
	struct qr q;
	uint8_t map[QR_BUF_LEN_MAX];
	int qr_errno;

	/* noise */
	unsigned codeword_noise;
	/* TODO: format_noise */

	/* decoded */
	struct qr_data data;
	quirc_decode_error_t quirc_err;

	/* verified */
	char v_err[256];
};

static size_t
seg_len(const struct fuzz_segment *a, size_t n)
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
prop_gated(struct theft *t, void *instance)
{
	struct fuzz_instance *o;
	struct qr_data data;

	assert(t != NULL);
	assert(instance != NULL);

	const enum gate g = * (const enum gate *) theft_hook_get_env(t);

	assert(g == GATE_PASS || g == GATE_NOISE);

	o = instance;

	o->gate = 0;

	{
		uint8_t tmp[QR_BUF_LEN_MAX];
		struct qr_segment *a;
		struct qr q;
		size_t i;

		a = xmalloc(sizeof *a * o->n);
		for (i = 0; i < o->n; i++) {
			a[i] = o->a[i].seg;
		}

		q.map = o->map;

		if (!qr_encode_segments(a, o->n, o->ecl, o->min, o->max, o->mask, o->boost_ecl, tmp, &q)) {
free(a);
			if (errno == EMSGSIZE) {
				return THEFT_TRIAL_SKIP;
			}

			o->qr_errno = errno;
			o->gate = GATE_ENCODE;
			return THEFT_TRIAL_ERROR;
		}

free(a);

		o->q = q;
	}

	if ((g & GATE_NOISE) != 0) {
		long seed;

		/* XXX: need to add noise in *either* the data or ECC codeword bits,
		 * not both.
		 * So until we have more granular regions, we only flip one bit at most.
		 */
		o->codeword_noise = theft_random_choice(t, 2);
#if 0
		/* QR codes claim to deal with 30% errors max.
		 * Here we flip up to 50% of bits. */
		o->codeword_noise = theft_random_choice(t, (o->q.size * o->q.size) / 2);
#endif

		seed  = theft_random_choice(t, LONG_MAX); /* XXX: ignoring negative values */

		/* TODO: pass function pointer for rand? */
		qr_noise(&o->q, o->codeword_noise, seed, true);
	}

	{
		quirc_decode_error_t e;

		e = quirc_decode(&o->q, &data);

		if ((g & GATE_NOISE) != 0) {
			if (o->codeword_noise > 0 && e == QUIRC_ERROR_DATA_ECC) {
				return THEFT_TRIAL_SKIP;
			}
		}

		if (e) {
			o->quirc_err = e;
			o->gate = GATE_DECODE;
			return THEFT_TRIAL_FAIL;
		}

		o->data = data;
	}

	/* because we skip reserved areas, a flipped bit should always
	 * result in an ecc correction */
	if ((g & GATE_NOISE) != 0) {
		if (o->codeword_noise > 0 && seg_len(o->a, o->n) > 0) {
			if (o->data.codeword_corrections == 0) {
// XXX: not format_corrections && o->data.format_corrections == 0) {
				snprintf(o->v_err, sizeof o->v_err,
					"no corrections: noise=%d",
					o->codeword_noise);
				o->gate = GATE_NOISE;
				return THEFT_TRIAL_FAIL;
			}
		}
	}

	{
		if (o->mask != QR_MASK_AUTO && (enum qr_mask) data.mask != o->mask) {
			snprintf(o->v_err, sizeof o->v_err,
				"mask mismatch: got=%d, expected=%d",
				data.mask, o->mask);
			o->gate = GATE_METADATA;
			return THEFT_TRIAL_FAIL;
		}

		enum qr_ecl ecl[] = { QR_ECL_MEDIUM, QR_ECL_LOW, QR_ECL_HIGH, QR_ECL_QUARTILE };
		if (o->boost_ecl) {
			if (ecl[data.ecc_level] < o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					ecl[data.ecc_level], o->ecl);
				o->gate = GATE_METADATA;
				return THEFT_TRIAL_FAIL;
			}
		} else {
			if (ecl[data.ecc_level] != o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					ecl[data.ecc_level], o->ecl);
				o->gate = GATE_METADATA;
				return THEFT_TRIAL_FAIL;
			}
		}

		if (data.ver < o->min || data.ver > o->max) {
			snprintf(o->v_err, sizeof o->v_err,
				"version mismatch: got=%u, expected min=%u, max=%u",
				data.ver, o->min, o->max);
			o->gate = GATE_METADATA;
			return THEFT_TRIAL_FAIL;
		}
	}

	{
		size_t j;
		const char *p;

		if ((size_t) data.payload_len != seg_len(o->a, o->n)) {
			snprintf(o->v_err, sizeof o->v_err,
				"payload length mismatch: got=%zu, expected=%zu",
				(size_t) data.payload_len, seg_len(o->a, o->n));
			o->gate = GATE_PAYLOAD;
			return THEFT_TRIAL_FAIL;
		}

		p = data.payload;

		for (j = 0; j < o->n; j++) {
			assert(p - data.payload <= (ptrdiff_t) data.payload_len);

			switch (o->a[j].seg.mode) {
			case QR_MODE_NUMERIC:
			case QR_MODE_ALNUM:
			case QR_MODE_BYTE:
				break;

			case QR_MODE_KANJI:
			case QR_MODE_ECI:
				assert(!"unimplemented");
				break;
			}

			assert(o->a[j].s != NULL);

			/* XXX: .len's meaning depends on .mode */
			if (0 != memcmp(p, o->a[j].s, o->a[j].len)) {
				snprintf(o->v_err, sizeof o->v_err,
					"payload data mismatch for segment %zu", j);
				o->gate = GATE_PAYLOAD;
				return THEFT_TRIAL_FAIL;
			}

			p += o->a[j].len;
		}
	}

	(void) t;

	return THEFT_TRIAL_PASS;
}

static enum theft_alloc_res
seg_alloc(struct theft *t, void *env, void **instance)
{
	struct fuzz_instance *o;
	size_t j;

	assert(t != NULL);
	assert(env == NULL);
	assert(instance != NULL);

	o = xmalloc(sizeof *o);
	o->gate = GATE_PASS;

	o->ecl       = theft_random_choice(t, 4);
	o->min       = theft_random_choice(t, QR_VER_MAX - QR_VER_MIN + 1) + QR_VER_MIN;
	o->max       = theft_random_choice(t, QR_VER_MAX - o->min + 1) + o->min;
	o->boost_ecl = theft_random_choice(t, 2);
	o->mask      = theft_random_choice(t, 1 + 8) - 1;

	o->n = theft_random_choice(t, sizeof o->a / sizeof *o->a + 1);
//	o->a = xmalloc(sizeof *o->a * o->n);

	/* TODO: permutate segments */
	(void) gen_permutation_vector;

	for (j = 0; j < o->n; j++) {
		switch (theft_random_choice(t, 5)) {
		case 0: {
			size_t i;

			o->a[j].len = theft_random_choice(t, sizeof o->a[j].s);

			if (qr_calcSegmentBufferSize(QR_MODE_NUMERIC, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			for (i = 0; i < o->a[j].len; i++) {
				o->a[j].s[i] = '0' + theft_random_choice(t, 10);
			}
			o->a[j].s[i] = '\0';

			assert(qr_isnumeric(o->a[j].s));

			o->a[j].seg = qr_make_numeric(o->a[j].s, o->a[j].buf);

			break;
		}

		case 1: {
			size_t i;

			o->a[j].len = theft_random_choice(t, sizeof o->a[j].s);

			if (qr_calcSegmentBufferSize(QR_MODE_ALNUM, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			for (i = 0; i < o->a[j].len; i++) {
				o->a[j].s[i] = ALNUM_CHARSET[theft_random_choice(t, sizeof (ALNUM_CHARSET) - 1)];
			}
			o->a[j].s[i] = '\0';

			assert(qr_isalnum(o->a[j].s));

			o->a[j].seg = qr_make_alnum(o->a[j].s, o->a[j].buf);

			break;
		}

		case 2: {
			size_t i;

			o->a[j].len = theft_random_choice(t, sizeof o->a[j].s);

			if (qr_calcSegmentBufferSize(QR_MODE_BYTE, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			for (i = 0; i < o->a[j].len; i++) {
				o->a[j].s[i] = theft_random_choice(t, UINT8_MAX + 1);
			}

			o->a[j].seg = qr_make_bytes(o->a[j].s, o->a[j].len);
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

skip:

//	free(o->a);
	free(o);

	return THEFT_ALLOC_SKIP;
}

static void
seg_free(void *instance, void *env)
{
	struct fuzz_instance *o;
	size_t j;

	(void) env;

	o = instance;

	for (j = 0; j < o->n; j++) {
//		free((void *) o->a[j].seg.data);
	}

//	free(o->a);

	free(o);
}

static void
seg_print(FILE *f, const void *instance, void *env)
{
	const struct fuzz_instance *o;
	size_t j;

	assert(f != NULL);
	assert(instance != NULL);
	assert(env == NULL);

	(void) env;

	o = instance;

	printf("    Segments {\n");
	for (j = 0; j < o->n; j++) {
		const char *dts;

		switch (o->a[j].seg.mode) {
		case QR_MODE_NUMERIC: dts = "NUMERIC"; break;
		case QR_MODE_ALNUM:   dts = "ALNUM";   break;
		case QR_MODE_BYTE:    dts = "BYTE";    break;
		case QR_MODE_KANJI:   dts = "KANJI";   break;
		default: dts = "?"; break;
		}

		printf("      %zu: %d (%s)\n", j, o->a[j].seg.mode, dts);
		printf("      source string: len=%zu bytes\n", o->a[j].len);
		if (qr_isalnum(o->a[j].s) || qr_isnumeric(o->a[j].s)) {
			printf("      \"%s\"\n", o->a[j].s);
		} else {
			hexdump(stdout, (void *) o->a[j].s, o->a[j].len);
		}
		printf("      encoded data: count=%zu bits\n", o->a[j].seg.count);
		hexdump(stdout, o->a[j].seg.data, (o->a[j].seg.count + 7) / 8);
	}
	printf("    }\n");
	printf("    Segments total data length: %zu\n", seg_len(o->a, o->n));

	if (o->gate == GATE_ENCODE) {
		fprintf(stderr, "\nqr_encode_segments: %s\n", strerror(o->qr_errno));
		return;
	}

	qr_print_utf8qb(stdout, &o->q, true, true);
	printf("    Size: %zu\n", o->q.size);

	if (o->gate == GATE_DECODE) {
		fprintf(stderr, "quirc_decode: %s\n", quirc_strerror(o->quirc_err));
		return;
	}

	{
		printf("    Version: %u\n", o->data.ver);
		printf("    ECC level: %c\n", "MLHQ"[o->data.ecc_level]);
		printf("    Mask: %d\n", o->data.mask);

		if (o->data.eci) {
			printf("    ECI: %d\n", o->data.eci);
		}
	}

	{
		printf("    Noise: %u\n", o->codeword_noise);
		printf("    Format corrections: %u\n", o->data.format_corrections);
		printf("    Codeword corrections: %u\n", o->data.codeword_corrections);
	}

	if (o->gate == GATE_NOISE) {
		fprintf(stderr, "\nqr_noise: %s\n", o->v_err);
		return;
	}

	if (o->gate == GATE_METADATA) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}

	{
		printf("    Length: %zu\n", o->data.payload_len);
		printf("    Payload: %s\n", o->data.payload);
	}

	if (o->gate == GATE_PAYLOAD) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}

	(void) o;
}

TEST
roundtrip(void *env)
{
	struct theft_run_config config = {
		.name      = "roundtrip",
		.prop1     = prop_gated,
		.hooks     = { .env = & (enum gate) { GATE_PASS } },
		.type_info = { env },
		.trials    = 100000,
		.seed      = 0xf5eabf7097b93381 // theft_seed_of_time()
	};

	if (theft_run(&config) == THEFT_RUN_PASS) {
		PASS();
	}

	FAIL();
}

TEST
noise(void *env)
{
	struct theft_run_config config = {
		.name      = "noise",
		.prop1     = prop_gated,
		.hooks     = { .env = & (enum gate) { GATE_NOISE } },
		.type_info = { env },
		.trials    = 100000,
		.seed      = theft_seed_of_time()
	};

	if (theft_run(&config) == THEFT_RUN_PASS) {
		PASS();
	}

	FAIL();
}

GREATEST_MAIN_DEFS();

int
main(int argc, char *argv[])
{
	static struct theft_type_info seg_info = {
		.alloc = seg_alloc,
		.free  = seg_free,
		.print = seg_print,
		.autoshrink_config = { .enable = true }
	};

	GREATEST_MAIN_BEGIN();

	RUN_TEST1(roundtrip, &seg_info);
	RUN_TEST1(noise, &seg_info);

	GREATEST_MAIN_END();
}

