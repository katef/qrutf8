
#ifndef QR_H
#define QR_H

/* minimum and maximum defined QR Code version numbers for Model 2 */
#define QR_VER_MIN 1
#define QR_VER_MAX 40

struct qr {
	/*
	 * The side length of modules in the QR-code.
	 */
	size_t size;

	/*
	 * If the module at (x, y) is black, then the following bit is set:
	 *
	 *   map[i >> 3] & (1 << (i & 7))
	 *
	 * where i = (y * size) + x.
	 *
	 * The result is in the range [21, 177]. The length of the array buffer
	 * is related to the side length; every 'struct qr.map[]' must have
	 * length at least QR_BUF_LEN(ver), which equals ceil(size^2 / 8).
	 */
	uint8_t *map;
};

#define BM_BIT(i)  ((i) & 7)
#define BM_BYTE(i) ((i) >> 3)
#define BM_GET(map, i) (((map)[BM_BYTE(i)] >> BM_BIT(i)) & 1)
#define BM_SET(map, i) do { (map)[BM_BYTE(i)] |=   1U << BM_BIT(i);  } while (0)
#define BM_CLR(map, i) do { (map)[BM_BYTE(i)] &= ~(1U << BM_BIT(i)); } while (0)

/* Number of bytes needed to store a given number of bits */
#define BM_LEN(bits) ((((size_t) bits) + 7) / 8)

#define QR_SIZE(ver) ((size_t) (ver) * 4 + 17)
#define QR_VER(size) (((unsigned) (size) - 17) / 4)

/* maximum number of alignment patterns */
#define QR_ALIGN_MAX 7

/*
 * The number of bitmap bytes needed to store any QR Code up to and including
 * the given version number.
 *
 * For example, 'uint8_t map[QR_BUF_LEN(25)];'
 * can store any single QR Code from version 1 to 25, inclusive.
 *
 * Requires QR_VER_MIN <= n <= QR_VER_MAX.
 */
#define QR_BUF_LEN(ver) BM_LEN(QR_SIZE(ver) * QR_SIZE(ver))

/*
 * The worst-case number of bytes needed to store one QR Code, up to and including
 * version 40. This value equals 3918, which is just under 4 kilobytes.
 * Use this more convenient value to avoid calculating tighter memory bounds for buffers.
 */
#define QR_BUF_LEN_MAX QR_BUF_LEN(QR_VER_MAX)

/*
 * The mode field of a segment.
 */
enum qr_mode {
	QR_MODE_NUMERIC = 0x1,
	QR_MODE_ALNUM   = 0x2,
	QR_MODE_BYTE    = 0x4,
	QR_MODE_KANJI   = 0x8,
	QR_MODE_ECI     = 0x7
};

/* TODO: renumber per encoding */
enum qr_ecl {
	QR_ECL_LOW = 0,
	QR_ECL_MEDIUM,
	QR_ECL_QUARTILE,
	QR_ECL_HIGH
};

/* XXX: define in terms of something else */
#define QR_PAYLOAD_MAX 8896

struct qr_data {
	unsigned ver;
	unsigned ecc_level; /* TODO: enum */
	unsigned mask; /* TODO: enum */

	unsigned format_corrections;
	unsigned codeword_corrections;

	/*
	 * Data payload. For the Kanji mode, payload is encoded as Shift-JIS.
	 * For all other modes, payload is text encoded per the source.
	 */
	char payload[QR_PAYLOAD_MAX];
	size_t payload_len;

	/* ECI assignment number */
	uint32_t eci;
};

/*
 * A segment of user/application data that a QR Code symbol can convey.
 */
struct qr_segment {
	enum qr_mode mode;

	/*
	 * The length of this segment's unencoded data.
	 * Always in the range [0, 32767]. The maximum bit length is 32767,
	 * because the largest QR Code (version 40) has only 31329 modules.
	 *
	 * For numeric, alphanumeric, and kanji modes, this measures in
	 * Unicode code points. For byte mode, this measures in bytes
	 * (raw binary data, text in UTF-8, or other encodings).
	 * For ECI mode, this is always zero.
	 */
	size_t len;

	/*
	 * Encoded data bits for this segment, packed in bitwise big endian.
	 * Set NULL if the bit length is zero.
	 */
	const void *data;

	/*
	 * The number of valid data bits used in the buffer. Requires
	 * 0 <= count <= 32767, and count <= (capacity of data array) * 8.
	 */
	size_t count;
};

/*
 * Returns a segment representing the given binary data encoded in byte mode.
 */
struct qr_segment
qr_make_bytes(const void *data, size_t len);

/*
 * Returns a segment representing the given string of decimal digits encoded in numeric mode.
 */
struct qr_segment
qr_make_numeric(const char *s, void *buf);

/*
 * Returns a segment representing the given text string encoded in alphanumeric mode.
 * The characters allowed are: 0 to 9, A to Z (uppercase only), space,
 * dollar, percent, asterisk, plus, hyphen, period, slash, colon.
 */
struct qr_segment
qr_make_alnum(const char *s, void *buf);

/*
 * Returns a segment representing an Extended Channel Interpretation
 * (ECI) designator with the given assignment value.
 */
struct qr_segment
qr_make_eci(long assignVal, void *buf);

/*
 * Return the color of the module (pixel) at the given coordinates, which is either
 * false for white or true for v. The top left corner has the coordinates (x=0, y=0).
 */
bool
qr_get_module(const struct qr *q, unsigned x, unsigned y);

/*
 * Set the module at the given coordinates, which must be in bounds.
 */
void
qr_set_module(struct qr *q, unsigned x, unsigned y, bool v);

/*
 * Flip n randomly-selected modules.
 * Reserved regions are avoided if skip_reserved is true.
 */
void
qr_noise(struct qr *q, size_t n, long seed, bool skip_reserved);

/*
 * Tests whether the given string can be encoded as a segment in alphanumeric mode.
 */
bool
qr_isalnum(const char *s);

/*
 * Tests whether the given string can be encoded as a segment in numeric mode.
 */
bool
qr_isnumeric(const char *s);

#endif

