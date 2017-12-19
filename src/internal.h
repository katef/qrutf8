
#ifndef INTERNAL_H
#define INTERNAL_H

unsigned
getAlignmentPatternPositions(unsigned ver, unsigned a[static QR_ALIGN_MAX]);
void
draw_init(unsigned ver, struct qr *q);

extern const int8_t ECL_CODEWORDS_PER_BLOCK[QR_VER_MAX + 1][4];
extern const int8_t NUM_ERROR_CORRECTION_BLOCKS[QR_VER_MAX + 1][4];

#endif

