#include "operators.h"

/*
 * Migrant selection (spec §10.3). All methods choose distinct individuals
 * (no individual is copied twice by the same source in one event) and read
 * only the pre-event state of the islands.
 */

void migrant_select_best(const MigrantSelectArgs *args, Rng *rng, int *outIndices)
{
    (void)rng;
    for (int k = 0; k < args->count; ++k) {
        outIndices[k] = args->sourceOrder[k]; /* top individuals first */
    }
}

void migrant_select_random(const MigrantSelectArgs *args, Rng *rng, int *outIndices)
{
    const int p = args->cfg->islandPopulation;
    int *cand = args->ws->candidates;
    for (int i = 0; i < p; ++i) {
        cand[i] = i;
    }
    for (int k = 0; k < args->count; ++k) {
        const int j = k + rng_uniform_int(rng, p - k);
        const int tmp = cand[k];
        cand[k] = cand[j];
        cand[j] = tmp;
        outIndices[k] = cand[k];
    }
}

/* Tournament among individuals not yet chosen (sampling with replacement). */
void migrant_select_tournament(const MigrantSelectArgs *args, Rng *rng, int *outIndices)
{
    const int p = args->cfg->islandPopulation;
    const Individual *pop = args->islands[args->sourceIndex].individuals;
    int *cand = args->ws->candidates;
    int remaining = p;
    for (int i = 0; i < p; ++i) {
        cand[i] = i;
    }
    for (int k = 0; k < args->count; ++k) {
        int bestPos = rng_uniform_int(rng, remaining);
        for (int t = 1; t < args->cfg->migrationTournamentSize; ++t) {
            const int pos = rng_uniform_int(rng, remaining);
            if (pop[cand[pos]].fitnessScore < pop[cand[bestPos]].fitnessScore) {
                bestPos = pos;
            }
        }
        outIndices[k] = cand[bestPos];
        cand[bestPos] = cand[remaining - 1]; /* remove chosen */
        --remaining;
    }
}

static int hamming_bounded(const int *x, const int *y, int n, int bound)
{
    int d = 0;
    for (int i = 0; i < n; ++i) {
        d += (x[i] != y[i]);
        if (d >= bound) {
            return d; /* cannot become a new minimum */
        }
    }
    return d;
}

/* nnDistance[j] = min Hamming distance from source individual j to the receiver population. */
static void compute_nearest_distances(const MigrantSelectArgs *args, int receiver, int *nnDistance)
{
    const int p = args->cfg->islandPopulation;
    const int n = args->cfg->boardSize;
    const Individual *src = args->islands[args->sourceIndex].individuals;
    const Individual *dst = args->islands[receiver].individuals;
    for (int j = 0; j < p; ++j) {
        int best = n + 1;
        for (int q = 0; q < p && best > 0; ++q) {
            const int d = hamming_bounded(src[j].genes, dst[q].genes, n, best);
            if (d < best) {
                best = d;
            }
        }
        nnDistance[j] = best;
    }
}

/*
 * [暫定] Diversity-based: maximize the nearest-neighbour Hamming distance to
 * the receiving island; ties -> lower fitnessScore -> random.
 * Cost: O(P^2 N) per (source, receiver) pair.
 */
void migrant_select_diversity(const MigrantSelectArgs *args, Rng *rng, int *outIndices)
{
    const int p = args->cfg->islandPopulation;
    const int islands = args->cfg->islandCount;
    const Individual *pop = args->islands[args->sourceIndex].individuals;
    MigrationWorkspace *ws = args->ws;

    for (int r = 0; r < islands; ++r) {
        ws->nnReady[r] = 0;
    }
    for (int i = 0; i < p; ++i) {
        ws->chosen[i] = 0;
    }

    for (int k = 0; k < args->count; ++k) {
        const int receiver = args->slotReceivers[k];
        int *nn = ws->nnDistance + (size_t)receiver * (size_t)p;
        if (!ws->nnReady[receiver]) {
            compute_nearest_distances(args, receiver, nn);
            ws->nnReady[receiver] = 1;
        }
        int best = -1;
        int ties = 0;
        for (int j = 0; j < p; ++j) {
            if (ws->chosen[j]) {
                continue;
            }
            if (best < 0 || nn[j] > nn[best] ||
                (nn[j] == nn[best] && pop[j].fitnessScore < pop[best].fitnessScore)) {
                best = j;
                ties = 1;
            } else if (nn[j] == nn[best] && pop[j].fitnessScore == pop[best].fitnessScore) {
                ++ties; /* reservoir sampling among full ties */
                if (rng_uniform_int(rng, ties) == 0) {
                    best = j;
                }
            }
        }
        ws->chosen[best] = 1;
        outIndices[k] = best;
    }
}

