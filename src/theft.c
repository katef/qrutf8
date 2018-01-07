
#define _POSIX_C_SOURCE 2

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <eci.h>
#include <qr.h>
#include <io.h>

#include "internal.h"
#include "fuzz.h"
#include "seg.h"
#include "xalloc.h"
#include "util.h"

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

struct type_instance {
	struct fuzz_instance *o;

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
	struct qr_stats stats;
	quirc_decode_error_t quirc_err;

	/* verified */
	char v_err[256];
};

static enum theft_trial_res
prop_gated(struct theft *t, void *instance)
{
	struct type_instance *o = instance;
	struct qr_data data;
	struct qr_stats stats;

	assert(t != NULL);
	assert(instance != NULL);

	const enum gate g = * (const enum gate *) theft_hook_get_env(t);

	assert(g == GATE_PASS || g == GATE_NOISE);

	o->gate = 0;

	{
		uint8_t tmp[QR_BUF_LEN_MAX];
		struct qr q;

		q.map = o->map;

		if (!qr_encode(o->o->a, o->o->n, o->o->ecl, o->o->min, o->o->max, o->o->mask, o->o->boost_ecl, tmp, &q)) {
			if (errno == EMSGSIZE) {
				return THEFT_TRIAL_SKIP;
			}

			o->qr_errno = errno;
			o->gate = GATE_ENCODE;
			return THEFT_TRIAL_ERROR;
		}

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

		e = quirc_decode(&o->q, &data, &stats);

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

		o->data  = data;
		o->stats = stats;
	}

	/* because we skip reserved areas, a flipped bit should always
	 * result in an ecc correction */
	if ((g & GATE_NOISE) != 0) {
		if (o->codeword_noise > 0 && seg_len(o->o->a, o->o->n) > 0) {
			if (o->stats.codeword_corrections == 0) {
// XXX: not format_corrections && o->stats.format_corrections == 0) {
				snprintf(o->v_err, sizeof o->v_err,
					"no corrections: noise=%d",
					o->codeword_noise);
				o->gate = GATE_NOISE;
				return THEFT_TRIAL_FAIL;
			}
		}
	}

	{
		if (o->o->mask != QR_MASK_AUTO && (enum qr_mask) data.mask != o->o->mask) {
			snprintf(o->v_err, sizeof o->v_err,
				"mask mismatch: got=%d, expected=%d",
				data.mask, o->o->mask);
			o->gate = GATE_METADATA;
			return THEFT_TRIAL_FAIL;
		}

		if (o->o->boost_ecl) {
			if (data.ecl < o->o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					data.ecl, o->o->ecl);
				o->gate = GATE_METADATA;
				return THEFT_TRIAL_FAIL;
			}
		} else {
			if (data.ecl != o->o->ecl) {
				snprintf(o->v_err, sizeof o->v_err,
					"ecl mismatch: got=%d, expected=%d",
					data.ecl, o->o->ecl);
				o->gate = GATE_METADATA;
				return THEFT_TRIAL_FAIL;
			}
		}

		if (data.ver < o->o->min || data.ver > o->o->max) {
			snprintf(o->v_err, sizeof o->v_err,
				"version mismatch: got=%u, expected min=%u, max=%u",
				data.ver, o->o->min, o->o->max);
			o->gate = GATE_METADATA;
			return THEFT_TRIAL_FAIL;
		}
	}

	if (!seg_cmp(data.a, data.n, o->o->a, o->o->n)) {
		snprintf(o->v_err, sizeof o->v_err,
			"segment mismatch");
		o->gate = GATE_PAYLOAD;
		return THEFT_TRIAL_FAIL;
	}

	(void) t;

	return THEFT_TRIAL_PASS;
}

static void
fuzz_ecl(void *opaque, enum qr_ecl *ecl, bool *boost_ecl)
{
	struct theft *t = opaque;

	assert(t != NULL);
	assert(ecl != NULL);
	assert(boost_ecl != NULL);

	*ecl       = theft_random_choice(t, 4);
	*boost_ecl = theft_random_choice(t, 2);
}

static void
fuzz_ver(void *opaque, unsigned *min, unsigned *max)
{
	struct theft *t = opaque;

	assert(t != NULL);
	assert(min != NULL);
	assert(max != NULL);

	*min = theft_random_choice(t, QR_VER_MAX - QR_VER_MIN + 1) + QR_VER_MIN;
	*max = theft_random_choice(t, QR_VER_MAX - *min + 1) + *min;
}

static void
fuzz_mask(void *opaque, signed *mask)
{
	struct theft *t = opaque;

	assert(t != NULL);
	assert(mask != NULL);

	*mask = theft_random_choice(t, 1 + 8) - 1;
}

static void
fuzz_mode(void *opaque, enum qr_mode *mode)
{
	struct theft *t = opaque;

	const enum qr_mode m[] = {
		QR_MODE_NUMERIC,
		QR_MODE_ALNUM,
		QR_MODE_BYTE,
		QR_MODE_KANJI,
		QR_MODE_ECI
	};

	assert(t != NULL);
	assert(mode != NULL);

	*mode = m[theft_random_choice(t, sizeof m / sizeof *m)];
}

static void
fuzz_uint(void *opaque, unsigned *n, unsigned max)
{
	struct theft *t = opaque;

	assert(t != NULL);
	assert(n != NULL);

	*n = theft_random_choice(t, max + 1);
}

static enum theft_alloc_res
type_alloc(struct theft *t, void *env, void **instance)
{
	struct type_instance *o;

	const struct fuzz_hook hook = {
		.fuzz_ecl  = fuzz_ecl,
		.fuzz_ver  = fuzz_ver,
		.fuzz_mask = fuzz_mask,
		.fuzz_mode = fuzz_mode,
		.fuzz_uint = fuzz_uint
	};

	assert(t != NULL);
	assert(env == NULL);
	assert(instance != NULL);

	o = xmalloc(sizeof *o);

	o->o = fuzz_alloc(t, &hook);
	if (o->o == NULL) {
		goto skip;
	}

	o->gate = GATE_PASS;

	(void) env;

	*instance = o;

	return THEFT_ALLOC_OK;

skip:

	free(o);

	return THEFT_ALLOC_SKIP;
}

static void
type_free(void *instance, void *env)
{
	struct type_instance *o = instance;

	(void) env;

	if (o->gate > GATE_DECODE) {
		free(o->data.a);
	}

	fuzz_free(o->o);

	free(o);
}

static void
type_print(FILE *f, const void *instance, void *env)
{
	const struct type_instance *o = instance;

	assert(f != NULL);
	assert(instance != NULL);
	assert(env == NULL);

	(void) env;

	fprintf(stderr, "\n    gate: %d\n", o->gate);

	seg_print(f, o->o->n, o->o->a);

	if (o->gate == GATE_ENCODE) {
		fprintf(stderr, "qr_encode: %s\n", strerror(o->qr_errno));
		return;
	}

	qr_print_utf8qb(stdout, &o->q, true, true);
	printf("	Size: %zu\n", o->q.size);

	if (o->gate == GATE_DECODE) {
		fprintf(stderr, "quirc_decode: %s\n", quirc_strerror(o->quirc_err));
		return;
	}

	{
		printf("	Version: %u\n", o->data.ver);
		printf("	ECC level: %c\n", "MLHQ"[o->data.ecl]);
		printf("	Mask: %d\n", o->data.mask);
	}

	{
		printf("	Noise: %u\n", o->codeword_noise);
		printf("	Format corrections: %u\n", o->stats.format_corrections);
		printf("	Codeword corrections: %u\n", o->stats.codeword_corrections);
	}

	if (o->gate == GATE_NOISE) {
		fprintf(stderr, "qr_noise: %s\n", o->v_err);
		return;
	}

	if (o->gate == GATE_METADATA) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}

	seg_print(stdout, o->data.n, o->data.a);

	if (o->gate == GATE_PAYLOAD) {
		printf("metadata verification failed: %s\n", o->v_err);
		return;
	}
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
		.seed      = theft_seed_of_time()
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
	static struct theft_type_info info = {
		.alloc = type_alloc,
		.free  = type_free,
		.print = type_print,
		.autoshrink_config = { .enable = true }
	};

	GREATEST_MAIN_BEGIN();

	RUN_TEST1(roundtrip, &info);
	RUN_TEST1(noise, &info);

	GREATEST_MAIN_END();
}

