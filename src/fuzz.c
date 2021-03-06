
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <eci.h>
#include <qr.h>

#include "internal.h"
#include "fuzz.h"
#include "seg.h"
#include "xalloc.h"
#include "util.h"

/* len is as per strlen (number of bytes - 1 for the terminator) */
static void
fuzz_str(void *opaque, const struct fuzz_hook *hook,
	char *s, size_t len, const char *charset)
{
	size_t i;
	unsigned n;

	assert(s != NULL);
	assert(charset != NULL);

	n = strlen(charset);

	for (i = 0; i < len; i++) {
		hook->fuzz_uint(opaque, &n, sizeof charset);
		s[i] = charset[n];
	}
	s[i] = '\0';

	/* TODO: permutate string */
}

/* len is number of bytes */
static void
fuzz_bytes(void *opaque, const struct fuzz_hook *hook,
	void *p, size_t len)
{
	unsigned char *q;
	size_t i;
	unsigned n;

	assert(p != NULL);

	q = p;

	for (i = 0; i < len; i++) {
		hook->fuzz_uint(opaque, &n, UCHAR_MAX);
		q[i] = n;
	}

	/* TODO: permutate string */
}

struct fuzz_instance *
fuzz_alloc(void *opaque, const struct fuzz_hook *hook)
{
	struct fuzz_instance *o;
	size_t j;

	assert(hook != NULL);

	o = xmalloc(sizeof *o);

	hook->fuzz_ecl(opaque, &o->ecl, &o->boost_ecl);
	hook->fuzz_ver(opaque, &o->min, &o->max);
	hook->fuzz_mask(opaque, &o->mask);

	{
		unsigned n;

		hook->fuzz_uint(opaque, &n, sizeof o->a / sizeof *o->a);
		o->n = n;
	}

	/* TODO: permutate segments */

	for (j = 0; j < o->n; j++) {
		enum qr_mode mode;

		hook->fuzz_mode(opaque, &mode);

		switch (mode) {
		case QR_MODE_NUMERIC: {
			char payload[QR_PAYLOAD_MAX];
			unsigned n;

			hook->fuzz_uint(opaque, &n, sizeof payload - 1);

			if (qr_calcSegmentBufferSize(mode, n) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_str(opaque, hook, payload, n, "0123456789");
			assert(strlen(payload) == n);
			assert(qr_isnumeric(payload));
			o->a[j] = qr_make_numeric(payload);
			break;
		}

		case QR_MODE_ALNUM: {
			char payload[QR_PAYLOAD_MAX];
			unsigned n;

			hook->fuzz_uint(opaque, &n, sizeof payload - 1);

			if (qr_calcSegmentBufferSize(mode, n) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_str(opaque, hook, payload, n, ALNUM_CHARSET);
			assert(strlen(payload) == n);
			assert(qr_isalnum(payload));
			o->a[j] = qr_make_alnum(payload);
			break;
		}

		case QR_MODE_BYTE: {
			char payload[QR_PAYLOAD_MAX];
			unsigned n;

			hook->fuzz_uint(opaque, &n, sizeof payload);

			if (qr_calcSegmentBufferSize(mode, n) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_bytes(opaque, hook, payload, n);
			o->a[j] = qr_make_bytes(payload, n);
			break;
		}

		case QR_MODE_KANJI:
			/* XXX: not implemented */
			goto skip;

		case QR_MODE_ECI:
			/* XXX: not implemented */
			goto skip;

		default:
			assert(!"unreached");
		}
	}

	return o;

skip:

	while (j) {
// XXX:		seg_free(o->a[j]);
		j--;
	}

	free(o);

	return NULL;
}

void
fuzz_free(struct fuzz_instance *o)
{
	size_t j;

	assert(o != NULL);

	for (j = 0; j < o->n; j++) {
		seg_free(o->a[j]);
	}

	free(o);
}

