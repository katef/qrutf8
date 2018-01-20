
#ifndef QR_IO_H
#define QR_IO_H

enum qr_utf8 {
	QR_UTF8_SINGLE, /* single width (typically rectangular) */
	QR_UTF8_WIDE,   /* terminal wide mode */
	QR_UTF8_DOUBLE  /* double-up glyphs for width */
};

void
qr_print_utf8qb(FILE *f, const struct qr *q, enum qr_utf8 uwidth, bool invert);

void
qr_print_pbm1(FILE *f, const struct qr *q, bool invert);

void
qr_print_pbm4(FILE *f, const struct qr *q, bool invert);

void
qr_print_svg(FILE *f, const struct qr *q, bool invert);

bool
qr_load_pbm(FILE *f, struct qr *q, bool invert);

#endif

