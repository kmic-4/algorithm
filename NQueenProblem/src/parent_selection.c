#include <math.h>

#include "operators.h"

/*
 * Parent selection (spec §6). The same individual may be selected many
 * times and elites are eligible. Weights / ranks are cached once per
 * generation in SelectionCache.
 */

void parent_prepare_none(const Island *island, const GAConfig *cfg, SelectionCache *cache)
{
    (void)island;
    (void)cfg;
    cache->totalWeight = 0.0;
}

/* [暫定] spec §6.1: w_i = (1 / (1 + F_i))^rouletteScaling (scaling 1.0 = spec formula). */
void parent_prepare_roulette(const Island *island, const GAConfig *cfg, SelectionCache *cache)
{
    double acc = 0.0;
    const bool plain = (cfg->rouletteScaling == 1.0);
    for (int i = 0; i < cfg->islandPopulation; ++i) {
        double w = 1.0 / (1.0 + (double)island->individuals[i].fitnessScore);
        if (!plain) {
            w = pow(w, cfg->rouletteScaling);
        }
        acc += w;
        cache->cumulativeWeights[i] = acc;
    }
    cache->totalWeight = acc;
}

/*
 * Linear ranking with selection pressure s in [1, 2]:
 *   w(r) = (2 - s) + 2 (s - 1) (P - 1 - r) / (P - 1),  r = 0 is the best.
 * Ties receive consecutive ranks ordered by index (deterministic).
 * Cumulative weights are stored in rank order.
 */
void parent_prepare_rank(const Island *island, const GAConfig *cfg, SelectionCache *cache)
{
    (void)island;
    const int p = cfg->islandPopulation;
    const double s = cfg->rankSelectionPressure;
    double acc = 0.0;
    for (int r = 0; r < p; ++r) {
        const double w = (2.0 - s) + 2.0 * (s - 1.0) * (double)(p - 1 - r) / (double)(p - 1);
        acc += w;
        cache->cumulativeWeights[r] = acc;
    }
    cache->totalWeight = acc;
}

int parent_select_tournament(const Island *island, const GAConfig *cfg,
                             const SelectionCache *cache, Rng *rng)
{
    (void)cache;
    const int p = cfg->islandPopulation;
    int best = rng_uniform_int(rng, p);
    for (int t = 1; t < cfg->parentTournamentSize; ++t) {
        const int c = rng_uniform_int(rng, p);
        if (island->individuals[c].fitnessScore < island->individuals[best].fitnessScore) {
            best = c;
        }
    }
    return best;
}

/* First position whose cumulative weight exceeds r. */
static int search_cumulative(const double *cum, int count, double r)
{
    int lo = 0;
    int hi = count - 1;
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if (cum[mid] > r) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

static int select_weighted(const SelectionCache *cache, int p, Rng *rng)
{
    if (!(cache->totalWeight > 0.0)) {
        return rng_uniform_int(rng, p); /* degenerate weights: uniform */
    }
    const double r = rng_uniform_double(rng) * cache->totalWeight;
    return search_cumulative(cache->cumulativeWeights, p, r);
}

int parent_select_roulette(const Island *island, const GAConfig *cfg,
                           const SelectionCache *cache, Rng *rng)
{
    (void)island;
    return select_weighted(cache, cfg->islandPopulation, rng);
}

int parent_select_rank(const Island *island, const GAConfig *cfg,
                       const SelectionCache *cache, Rng *rng)
{
    (void)island;
    const int rank = select_weighted(cache, cfg->islandPopulation, rng);
    return cache->sortedIndices[rank];
}

