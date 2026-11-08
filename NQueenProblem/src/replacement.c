#include "operators.h"

/*
 * Migration replacement (spec §10.4). Protected elites (the first
 * protectedCount entries of receiverOrder) are never replaced and every
 * slot is chosen at most once per event.
 */

void replace_worst(const int *receiverOrder, int populationSize, int protectedCount,
                   int count, Rng *rng, int *scratch, int *outSlots)
{
    (void)protectedCount;
    (void)rng;
    (void)scratch;
    for (int k = 0; k < count; ++k) {
        outSlots[k] = receiverOrder[populationSize - 1 - k];
    }
}

void replace_random(const int *receiverOrder, int populationSize, int protectedCount,
                    int count, Rng *rng, int *scratch, int *outSlots)
{
    const int available = populationSize - protectedCount;
    for (int i = 0; i < available; ++i) {
        scratch[i] = receiverOrder[protectedCount + i];
    }
    for (int k = 0; k < count; ++k) {
        const int j = k + rng_uniform_int(rng, available - k);
        const int tmp = scratch[k];
        scratch[k] = scratch[j];
        scratch[j] = tmp;
        outSlots[k] = scratch[k];
    }
}

