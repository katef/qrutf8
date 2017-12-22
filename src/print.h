
#ifndef QR_PRINT_H
#define QR_PRINT_H

void
qr_print_utf8qb(FILE *f, const struct qr *q, bool wide, bool invert);

void
qr_print_xpm(FILE *f, const struct qr *q, bool invert);

void
qr_print_pbm1(FILE *f, const struct qr *q, bool invert);

#endif

