#ifndef NQ_RNG_H
#define NQ_RNG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Reproducible pseudo random number generator (xoshiro256**).
 * Every island and the migration controller own an independent Rng state,
 * so no global random state (e.g. rand()) is shared.
 */
typedef struct {
    uint64_t s[4];
} Rng;

/* Initializes a state from (seed, stream). Different streams yield independent sequences. */
void rng_seed(Rng *rng, uint64_t seed, uint64_t stream);

uint64_t rng_next_u64(Rng *rng);

/* Uniform integer in [0, bound). bound must be > 0. Unbiased (rejection sampling). */
int rng_uniform_int(Rng *rng, int bound);

/* Uniform double in [0, 1). */
double rng_uniform_double(Rng *rng);

/* Returns true with probability p (p <= 0: never, p >= 1: always). */
bool rng_bernoulli(Rng *rng, double p);

#endif /* NQ_RNG_H */

