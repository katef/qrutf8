
#ifndef INTERNAL_H
#define INTERNAL_H

typedef enum {
	QUIRC_SUCCESS = 0,
	QUIRC_ERROR_INVALID_GRID_SIZE,
	QUIRC_ERROR_INVALID_VERSION,
	QUIRC_ERROR_FORMAT_ECC,
	QUIRC_ERROR_DATA_ECC,
	QUIRC_ERROR_DATA_OVERFLOW,
	QUIRC_ERROR_DATA_UNDERFLOW
} quirc_decode_error_t;

/*
 * The mask pattern used in a QR Code symbol.
 */
enum qr_mask {
	// A special value to tell the QR Code encoder to
	// automatically select an appropriate mask pattern
	QR_MASK_AUTO = -1,

	// The eight actual mask patterns
	QR_MASK_0 = 0,
	QR_MASK_1,
	QR_MASK_2,
	QR_MASK_3,
	QR_MASK_4,
	QR_MASK_5,
	QR_MASK_6,
	QR_MASK_7
};


static const char ALNUM_CHARSET[] =
	"0123456789"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	" $%*+-./:";

unsigned
count_data_bits(unsigned ver);

void
append_bits(unsigned v, size_t n, void *buf, size_t *count);

unsigned
getAlignmentPatternPositions(unsigned ver, unsigned a[static QR_ALIGN_MAX]);
void
draw_init(unsigned ver, struct qr *q);

extern const int8_t ECL_CODEWORDS_PER_BLOCK[QR_VER_MAX + 1][4];
extern const int8_t NUM_ERROR_CORRECTION_BLOCKS[QR_VER_MAX + 1][4];

const char *quirc_strerror(quirc_decode_error_t err);

bool
qr_encode(const struct qr_segment segs[], size_t len, enum qr_ecl ecl,
	unsigned min, unsigned max, int mask, bool boost_ecl, void *tmp, struct qr *q);

quirc_decode_error_t
quirc_decode(const struct qr *q,
	struct qr_data *data);

#endif

