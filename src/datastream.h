
#ifndef DATASTREAM_H
#define DATASTREAM_H

/* TODO: should point to a struct qr here */
struct datastream {
	uint8_t		raw[QR_PAYLOAD_MAX];
	int		data_bits;
	int		ptr;

	uint8_t         data[QR_PAYLOAD_MAX];
};

void
append_bits(unsigned v, size_t n, void *buf, size_t *count);

void
read_data(const struct qr *q,
	enum qr_mask mask,
	struct datastream *ds);

int
bits_remaining(const struct datastream *ds);

int
take_bits(struct datastream *ds, int len);

#endif
