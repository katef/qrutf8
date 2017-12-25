
#ifndef UTIL_H
#define UTIL_H

void
hexdump(FILE *f, const uint8_t *buf, size_t size);

size_t *
gen_permutation_vector(size_t length, uint32_t seed);

#endif

