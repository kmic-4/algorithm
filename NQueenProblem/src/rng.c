#include "rng.h"

static uint64_t splitmix64(uint64_t *state)
{
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static uint64_t rotl(uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

void rng_seed(Rng *rng, uint64_t seed, uint64_t stream)
{
    uint64_t streamState = stream;
    uint64_t sm = seed ^ splitmix64(&streamState);
    for (int i = 0; i < 4; ++i) {
        rng->s[i] = splitmix64(&sm);
    }
    if ((rng->s[0] | rng->s[1] | rng->s[2] | rng->s[3]) == 0) {
        rng->s[0] = UINT64_C(1); /* all-zero state is invalid for xoshiro */
    }
}

uint64_t rng_next_u64(Rng *rng)
{
    uint64_t *s = rng->s;
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

int rng_uniform_int(Rng *rng, int bound)
{
    const uint64_t b = (uint64_t)bound;
    /* (2^64 - b) mod b : values below this threshold would introduce bias */
    const uint64_t threshold = (UINT64_C(0) - b) % b;
    for (;;) {
        const uint64_t r = rng_next_u64(rng);
        if (r >= threshold) {
            return (int)(r % b);
        }
    }
}

double rng_uniform_double(Rng *rng)
{
    return (double)(rng_next_u64(rng) >> 11) * (1.0 / 9007199254740992.0);
}

bool rng_bernoulli(Rng *rng, double p)
{
    if (p <= 0.0) {
        return false;
    }
    if (p >= 1.0) {
        return true;
    }
    return rng_uniform_double(rng) < p;
}

