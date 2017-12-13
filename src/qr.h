
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

#define QR_SIZE(ver) ((size_t) (ver) * 4 + 17)

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
#define QR_BUF_LEN(ver) \
	((QR_SIZE(ver) * QR_SIZE(ver) + 7) / 8)

/*
 * The worst-case number of bytes needed to store one QR Code, up to and including
 * version 40. This value equals 3918, which is just under 4 kilobytes.
 * Use this more convenient value to avoid calculating tighter memory bounds for buffers.
 */
#define QR_BUF_LEN_MAX QR_BUF_LEN(QR_VER_MAX)

#endif

