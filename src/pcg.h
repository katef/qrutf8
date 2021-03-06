/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

#ifndef PCG_H
#define PCG_H

typedef struct {    // Internals are *Private*.
	uint64_t state;             // RNG state.  All values are possible.
	uint64_t inc;               // Controls which RNG sequence (stream) is
	// selected. Must *always* be odd.
} pcg32_random_t;

/*
 * Seed the rng.  Specified in two parts, state initializer and a
 * sequence selection constant (a.k.a. stream id)
 */
void pcg32_srandom_r(pcg32_random_t* rng, uint64_t initstate,
                     uint64_t initseq);
/*
 * pcg32_random_r(rng)
 *     Generate a uniformly distributed 32-bit random number
 */
uint32_t pcg32_random_r(pcg32_random_t* rng);

/*
 * Generate a uniformly distributed number, r, where 0 <= r < bound
 */
uint32_t pcg32_boundedrand_r(pcg32_random_t* rng, uint32_t bound);

#endif

