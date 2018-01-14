
#ifndef DATASTREAM_H
#define DATASTREAM_H

void
append_bits(unsigned v, size_t n, void *buf, size_t *count);

void
read_data(const struct qr *q,
	struct qr_bytes *ds);

int
take_bits(struct qr_bytes *ds, size_t len, size_t *ds_ptr);

#endif

