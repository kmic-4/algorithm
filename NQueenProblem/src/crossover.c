#include <assert.h>

#include "operators.h"

/*
 * Crossover operators (spec §7). Column duplicates are allowed, so no
 * permutation repair is needed. Both operators are O(N) per parent pair.
 */

int crossover_npoint(const Individual *parentA, const Individual *parentB,
                     Individual *childA, Individual *childB, const GAConfig *cfg,
                     CrossoverWorkspace *ws, Rng *rng, GACounters *counters)
{
    (void)counters;
    const int n = cfg->boardSize;
    const int *a = parentA->genes;
    const int *b = parentB->genes;
    int *ca = childA->genes;
    int *cb = childB->genes;
    int diff = 0;

    childA->evaluationValid = false;
    childB->evaluationValid = false;

    const int cutSlots = n - 1; /* cut k lies between genes k-1 and k (k = 1..N-1) */
    int points = cfg->crossoverPoints;
    if (points > cutSlots) {
        points = cutSlots;
    }
    if (points < 1) {
        for (int i = 0; i < n; ++i) {
            ca[i] = a[i];
            cb[i] = b[i];
        }
        return 0;
    }

    /* Choose `points` distinct cuts with a partial Fisher-Yates shuffle. */
    int *cand = ws->cutCandidates;
    unsigned char *marks = ws->cutMarks;
    for (int k = 0; k < cutSlots; ++k) {
        cand[k] = k + 1;
    }
    for (int k = 0; k < points; ++k) {
        const int j = k + rng_uniform_int(rng, cutSlots - k);
        const int tmp = cand[k];
        cand[k] = cand[j];
        cand[j] = tmp;
        marks[cand[k]] = 1;
    }

    /* Alternate inheritance segment by segment; clear marks while sweeping. */
    bool swapped = false;
    for (int i = 0; i < n; ++i) {
        if (marks[i]) {
            swapped = !swapped;
            marks[i] = 0;
        }
        const int x = a[i];
        const int y = b[i];
        if (swapped) {
            ca[i] = y;
            cb[i] = x;
            diff += (x != y);
        } else {
            ca[i] = x;
            cb[i] = y;
        }
    }
    return diff;
}

/*
 * Conflict-information based "XOR" crossover (spec §7.2; not a bitwise XOR).
 * Candidates: a[i] != b[i] and the conflict rule (OR by default) holds.
 * Selected candidates are swapped between the two children.
 */
int crossover_xor(const Individual *parentA, const Individual *parentB,
                  Individual *childA, Individual *childB, const GAConfig *cfg,
                  CrossoverWorkspace *ws, Rng *rng, GACounters *counters)
{
    assert(parentA->evaluationValid && parentB->evaluationValid);
    const int n = cfg->boardSize;
    const int *a = parentA->genes;
    const int *b = parentB->genes;
    const int *qa = parentA->queenConflicts;
    const int *qb = parentB->queenConflicts;
    int *ca = childA->genes;
    int *cb = childB->genes;
    int *cand = ws->swapCandidates;
    int count = 0;
    int maxScore = 0;
    const bool andRule = (cfg->xorCandidateRule == XOR_CANDIDATE_AND);

    childA->evaluationValid = false;
    childB->evaluationValid = false;

    for (int i = 0; i < n; ++i) {
        ca[i] = a[i];
        cb[i] = b[i];
        if (a[i] != b[i]) {
            const bool hit = andRule ? (qa[i] > 0 && qb[i] > 0) : (qa[i] > 0 || qb[i] > 0);
            if (hit) {
                cand[count++] = i;
                const int score = qa[i] + qb[i];
                if (score > maxScore) {
                    maxScore = score;
                }
            }
        }
    }

    int swaps = 0;
    const bool proportional = (cfg->xorSwapProbabilityMethod == XOR_PROB_CONFLICT_PROPORTIONAL);
    for (int k = 0; k < count; ++k) {
        const int i = cand[k];
        double prob = cfg->xorSwapRate;
        if (proportional) {
            prob = (maxScore > 0) ? cfg->xorSwapRate * (double)(qa[i] + qb[i]) / (double)maxScore
                                  : 0.0;
        }
        if (rng_bernoulli(rng, prob)) {
            ca[i] = b[i];
            cb[i] = a[i];
            ++swaps;
        }
    }

    if (swaps == 0 && count > 0 && cfg->xorForceSwap) {
        const int i = cand[rng_uniform_int(rng, count)];
        ca[i] = b[i];
        cb[i] = a[i];
        swaps = 1;
        counters->xorForcedSwaps++;
    }
    return swaps; /* every swapped position has a[i] != b[i] */
}

