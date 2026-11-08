#include "fitness.h"
#include "operators.h"

/*
 * Conflict-guided Random Resetting (spec §8): pick a conflicting row
 * uniformly and assign it a different random column. One gene per mutation.
 */
bool mutation_conflict_guided(Individual *child, const GAConfig *cfg,
                              FitnessWorkspace *fws, int *rowBuffer, Rng *rng)
{
    const int n = cfg->boardSize;
    if (!child->evaluationValid) {
        fitness_evaluate(child, n, fws);
    }
    if (child->fitnessScore == 0 || n < 2) {
        return false;
    }
    if (!rng_bernoulli(rng, cfg->mutationRate)) {
        return false;
    }

    int count = 0;
    for (int i = 0; i < n; ++i) {
        if (child->queenConflicts[i] > 0) {
            rowBuffer[count++] = i;
        }
    }
    if (count == 0) {
        return false; /* unreachable when fitnessScore > 0 */
    }

    const int row = rowBuffer[rng_uniform_int(rng, count)];
    const int current = child->genes[row];
    int column = rng_uniform_int(rng, n - 1);
    if (column >= current) {
        ++column; /* uniform over columns != current */
    }
    child->genes[row] = column;
    child->evaluationValid = false;
    fitness_evaluate(child, n, fws);
    return true;
}

