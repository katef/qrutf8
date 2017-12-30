
#ifndef SEG_H
#define SEG_H

size_t
xseg_len(const struct qr_segment *a, size_t n);

int
count_char_bits(enum qr_mode mode, unsigned ver);

int
count_total_bits(const struct qr_segment segs[], size_t n, unsigned ver);

void
seg_print(FILE *f, size_t n, const struct qr_segment *a);

size_t
qr_calcSegmentBufferSize(enum qr_mode mode, size_t len);

#endif

