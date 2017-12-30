
#ifndef QR_IO_H
#define QR_IO_H

void
qr_print_utf8qb(FILE *f, const struct qr *q, bool wide, bool invert);

void
qr_print_pbm1(FILE *f, const struct qr *q, bool invert);

void
qr_print_pbm4(FILE *f, const struct qr *q, bool invert);

void
qr_print_svg(FILE *f, const struct qr *q, bool invert);

bool
qr_load_pbm(FILE *f, struct qr *q, bool invert);

void
seg_print(FILE *f, size_t n, const struct qr_segment *a);

#endif

