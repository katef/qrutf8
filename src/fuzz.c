
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

#include "theft.h"

struct qr_instance {
	size_t n;
	struct qr_segment *a;
	enum qr_ecl ecl;
	unsigned min;
	unsigned max;
	enum qr_mask mask;
	bool boost_ecl;
};

static enum theft_trial_res
prop_roundtrip(struct theft *t, void *instance)
{
	uint8_t tmp[QR_BUF_LEN_MAX];
	uint8_t map[QR_BUF_LEN_MAX];
	const struct qr_instance *o;
	struct qr_code q;

	assert(t != NULL);
	assert(instance != NULL);

	o = instance;

	q.map = map;

	if (!qr_encode_segments(o->a, o->n, o->ecl, o->min, o->max, o->mask, o->boost_ecl, tmp, &q)) {
		if (errno == EMSGSIZE) {
			return THEFT_TRIAL_SKIP;
		}

		perror("\nqr_encode_segments");
		return THEFT_TRIAL_ERROR;
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

	assert(f != NULL);
	assert(instance != NULL);
	assert(env == NULL);

	(void) env;

	o = instance;

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

