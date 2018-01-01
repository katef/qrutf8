
#ifndef QR_FUZZ_H
#define QR_FUZZ_H

struct fuzz_hook {
	void (*fuzz_ecl )(void *opaque, enum qr_ecl *ecl, bool *boost_ecl);
	void (*fuzz_ver )(void *opaque, unsigned *min, unsigned *max);
	void (*fuzz_mask)(void *opaque, signed *mask);
	void (*fuzz_mode)(void *opaque, enum qr_mode *mode);
	void (*fuzz_uint)(void *opaque, unsigned *n, unsigned max);
};

struct fuzz_instance {
	size_t n;
	struct qr_segment *a[1000]; /* XXX: find max number of segments */
	enum qr_ecl ecl;
	unsigned min;
	unsigned max;
	signed mask; /* TODO: enum */
	bool boost_ecl;
};

struct fuzz_instance *
fuzz_alloc(void *opaque, const struct fuzz_hook *hook);

void
fuzz_free(struct fuzz_instance *o);

#endif

