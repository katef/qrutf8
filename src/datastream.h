
#ifndef DATASTREAM_H
#define DATASTREAM_H

void
append_bit(bool v, void *buf, size_t *bits);

void
append_bits(uint32_t v, size_t n, void *buf, size_t *count);

void
read_data(const struct qr *q,
	void *buf, size_t *bits);

int
take_bits(const void *buf, size_t bits,
	size_t len, size_t *ds_ptr);

#endif

