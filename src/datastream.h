
#ifndef DATASTREAM_H
#define DATASTREAM_H

/* TODO: should point to a struct qr here */
struct datastream_raw {
	uint8_t		raw[QR_PAYLOAD_MAX];
	int		bits;
};

struct datastream_data {
	uint8_t         data[QR_PAYLOAD_MAX];
	int		bits;
};

void
append_bits(unsigned v, size_t n, void *buf, size_t *count);

void
read_data(const struct qr *q,
	struct datastream_raw *ds);

int
take_bits(struct datastream_data *ds, int len, int *ds_ptr);

#endif

