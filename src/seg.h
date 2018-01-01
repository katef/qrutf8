
#ifndef SEG_H
#define SEG_H

size_t
seg_len(struct qr_segment * const a[], size_t n);

int
count_char_bits(enum qr_mode mode, unsigned ver);

int
count_total_bits(struct qr_segment * const segs[], size_t n, unsigned ver);

void
seg_free(struct qr_segment *seg);

void
seg_print(FILE *f, size_t n, struct qr_segment * const a[]);

size_t
qr_calcSegmentBufferSize(enum qr_mode mode, size_t len);

#endif

