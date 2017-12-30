
#ifndef QR_FUZZ_H
#define QR_FUZZ_H

struct fuzz_hook {
	void (*fuzz_ecl )(void *opaque, enum qr_ecl *ecl, bool *boost_ecl);
	void (*fuzz_ver )(void *opaque, unsigned *min, unsigned *max);
	void (*fuzz_mask)(void *opaque, signed *mask);
	void (*fuzz_mode)(void *opaque, enum qr_mode *mode);
	void (*fuzz_uint)(void *opaque, unsigned *n, unsigned max);
};

struct fuzz_segment {
	struct qr_segment seg;
	char buf[BM_LEN(32767) + 1]; /* XXX: centralise maths */

	/* source data */
	/* TODO: move to qr_segment */
	char s[QR_PAYLOAD_MAX];
	size_t len;
};

struct fuzz_instance {
	size_t n;
	struct fuzz_segment a[1000]; /* XXX: find max number of segments */
	enum qr_ecl ecl;
	unsigned min;
	unsigned max;
	signed mask; /* TODO: enum */
	bool boost_ecl;
};

size_t
seg_len(const struct fuzz_segment *a, size_t n);

struct fuzz_instance *
fuzz_alloc(void *opaque, const struct fuzz_hook *hook);

void
fuzz_free(struct fuzz_instance *o);

void
fuzz_print(FILE *f, const struct fuzz_instance *o);

#endif

