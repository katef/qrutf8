/*
 * Copyright 2017 Scott Vokes
 *
 * See LICENCE for the full copyright terms.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* Uncomment to dump as a C hex literal string */
//#define HEXDUMP_CSTR
void
hexdump(FILE *f, const uint8_t *buf, size_t size)
{
	size_t offset = 0;
	while (offset < size) {
		size_t curr = (size - offset > 16 ? 16 : size - offset);

#ifdef HEXDUMP_CSTR
		for (size_t i = 0; i < curr; i++) {
			fprintf(f, "0x%02x, ", buf[offset + i]);
		}

#else
		fprintf(f, "%04zx -- ", offset);
		for (size_t i = 0; i < curr; i++) {
			fprintf(f, "%02x ", buf[offset + i]);
		}

		for (size_t i = curr; i < 16; i++) {
			fprintf(f, "   ");
		}

		fprintf(f, "  ");
		for (size_t i = 0; i < curr; i++) {
			uint8_t c = buf[offset + i];
			fprintf(f, "%c", (isprint(c) ? c : '.'));
		}
#endif
		fprintf(f, "\n");
		offset += curr;
	}
}

size_t *
gen_permutation_vector(size_t length, uint32_t seed)
{
	static const unsigned long primes[] = {
		11, 101, 1009, 10007,
		100003, 1000003, 10000019, 100000007, 1000000007,
		1538461, 1865471, 17471, 2147483647 /* 2**32 - 1 */
	};

	size_t *res;
	uint32_t mod;
	uint32_t ceil;
	uint32_t state;
	uint32_t a, c;
	size_t offset;
	uint32_t mask;

	res = calloc(length, sizeof *res);
	if (res == NULL) {
		return NULL;
	}

	/*
	 * This is a linear congruential random number generator,
	 * which isn't actually that great as a random number generator,
	 * but has the handy property that it will walk through all
	 * the values in range exactly once before repeating any
	 * (provided the input variables are right).
	 * See Knuth Volume 2, section 3.2.1 for more details.
	 */

	mod  = 1;
	ceil = length;
	while (mod < ceil) {
		mod <<= 1;
	}

	state = seed & ((1LLU << 29) - 1);
	a = (4 * state) + 1;
	c = primes[(state * 16451) % sizeof primes / sizeof *primes];

	offset = 0;
	mask = mod - 1;
	while (offset < length) {
		do {
			state = ((a * state) + c) & mask;
		} while (state >= ceil);

		res[offset++] = state;
	}

	return res;
}

