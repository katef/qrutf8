
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

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

size_t
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

struct fuzz_instance *
fuzz_alloc(void *opaque, const struct fuzz_hook *hook)
{
	struct fuzz_instance *o;
	size_t j;
	unsigned n;

	assert(hook != NULL);

	o = xmalloc(sizeof *o);

	hook->fuzz_ecl(opaque, &o->ecl, &o->boost_ecl);
	hook->fuzz_ver(opaque, &o->min, &o->max);
	hook->fuzz_mask(opaque, &o->mask);

	hook->fuzz_uint(opaque, &n, sizeof o->a / sizeof *o->a);
	o->n = n;

	/* TODO: permutate segments */

	for (j = 0; j < o->n; j++) {
		enum qr_mode mode;

		hook->fuzz_mode(opaque, &mode);

		switch (mode) {
		case QR_MODE_NUMERIC:
			hook->fuzz_uint(opaque, &n, sizeof o->a[j].s - 1);
			o->a[j].len = n;

			if (qr_calcSegmentBufferSize(mode, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_str(opaque, hook, o->a[j].s, o->a[j].len, "0123456789");
			assert(strlen(o->a[j].s) == o->a[j].len);
			assert(qr_isnumeric(o->a[j].s));
			o->a[j].seg = qr_make_numeric(o->a[j].s, o->a[j].buf);
			break;

		case QR_MODE_ALNUM:
			hook->fuzz_uint(opaque, &n, sizeof o->a[j].s - 1);
			o->a[j].len = n;

			if (qr_calcSegmentBufferSize(mode, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_str(opaque, hook, o->a[j].s, o->a[j].len, ALNUM_CHARSET);
			assert(strlen(o->a[j].s) == o->a[j].len);
			assert(qr_isalnum(o->a[j].s));
			o->a[j].seg = qr_make_alnum(o->a[j].s, o->a[j].buf);
			break;

		case QR_MODE_BYTE: {
			hook->fuzz_uint(opaque, &n, sizeof o->a[j].s);
			o->a[j].len = n;

			if (qr_calcSegmentBufferSize(mode, o->a[j].len) > QR_BUF_LEN(o->max)) {
				goto skip;
			}

			fuzz_bytes(opaque, hook, o->a[j].s, o->a[j].len);
			o->a[j].seg = qr_make_bytes(o->a[j].s, o->a[j].len);
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
// XXX:		seg_free(o->a[j].seg);
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
		seg_free(o->a[j].seg);
	}

	free(o);
}

void
fuzz_print(FILE *f, const struct fuzz_instance *o)
{
	size_t j;

	assert(f != NULL);
	assert(o != NULL);

	printf("    Segments x%zu {\n", o->n);
	for (j = 0; j < o->n; j++) {
		const char *dts;

		switch (o->a[j].seg->mode) {
		case QR_MODE_NUMERIC: dts = "NUMERIC"; break;
		case QR_MODE_ALNUM:   dts = "ALNUM";   break;
		case QR_MODE_BYTE:    dts = "BYTE";    break;
		case QR_MODE_KANJI:   dts = "KANJI";   break;
		default: dts = "?"; break;
		}

		printf("    %zu: mode=%d (%s)\n", j, o->a[j].seg->mode, dts);
		printf("      source string: len=%zu bytes\n", o->a[j].len);
		if (qr_isalnum(o->a[j].s) || qr_isnumeric(o->a[j].s)) {
			printf("      \"%s\"\n", o->a[j].s);
		} else {
			hexdump(stdout, (void *) o->a[j].s, o->a[j].len);
		}
		printf("      encoded data: count=%zu bits\n", o->a[j].seg->count);
		hexdump(stdout, o->a[j].seg->data, BM_LEN(o->a[j].seg->count));
	}
	printf("    }\n");
	printf("    Segments total data length: %zu\n", seg_len(o->a, o->n));
}

