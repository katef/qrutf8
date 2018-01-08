
#ifndef INTERNAL_H
#define INTERNAL_H

enum qr_decode {
	QR_SUCCESS = 0,
	QR_ERROR_INVALID_GRID_SIZE,
	QR_ERROR_INVALID_VERSION,
	QR_ERROR_FORMAT_ECC,
	QR_ERROR_DATA_ECC,
	QR_ERROR_DATA_OVERFLOW,
	QR_ERROR_DATA_UNDERFLOW
};

static const char ALNUM_CHARSET[] =
	"0123456789"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	" $%*+-./:";

unsigned
count_data_bits(unsigned ver);

void
append_bits(unsigned v, size_t n, void *buf, size_t *count);

// Sets the module at the given coordinates, doing nothing if out of bounds.
void
set_module_bounded(struct qr *q, unsigned x, unsigned y, bool v);

// Sets every pixel in the range [left : left + width] * [top : top + height] to v.
void
fill(unsigned left, unsigned top, unsigned width, unsigned height, struct qr *q);

unsigned
getAlignmentPatternPositions(unsigned ver, unsigned a[static QR_ALIGN_MAX]);
void
draw_init(unsigned ver, struct qr *q);

extern const int8_t ECL_CODEWORDS_PER_BLOCK[QR_VER_MAX + 1][4];
extern const int8_t NUM_ERROR_CORRECTION_BLOCKS[QR_VER_MAX + 1][4];

const char *qr_strerror(enum qr_decode err);

bool
reserved_module(const struct qr *q, unsigned x, unsigned y);

bool
mask_bit(enum qr_mask mask, unsigned x, unsigned y);

void
apply_mask(struct qr *q, enum qr_mask mask);

bool
qr_encode(struct qr_segment * const segs[], size_t len, enum qr_ecl ecl,
	unsigned min, unsigned max, int mask, bool boost_ecl, void *tmp, struct qr *q);

enum qr_decode
qr_decode(const struct qr *q,
	struct qr_data *data, struct qr_stats *stats,
	void *tmp);

#endif

