#ifndef NQ_POPULATION_H
#define NQ_POPULATION_H

#include "definition.h"

/*
 * Ownership:
 *  - generation_create() allocates every island, both generation buffers,
 *    their contiguous gene/conflict storage and all per-island workspaces.
 *  - generation_destroy() frees everything; safe on partially created objects.
 */
NQStatus generation_create(Generation *gen, const GAConfig *cfg);
void generation_destroy(Generation *gen);

/* Points ind->genes / ind->queenConflicts into base (2N ints). */
void individual_bind(Individual *ind, int *base, int n);

/* Deep copy of genes, conflicts and evaluation state. dst and src must not alias. */
void individual_copy(Individual *dst, const Individual *src, int n);

/* Fills every individual with uniformly random genes (spec §5) and evaluates it. */
void island_randomize(Island *island, const GAConfig *cfg);

/*
 * Writes indices of individuals sorted best -> worst (ties by index) into order.
 * pairs is scratch of length count. All individuals must be evaluated.
 */
void population_sort_order(const Individual *individuals, int count,
                           FitnessIndexPair *pairs, int *order);

/* Swaps current and next generation buffers (no deep copy). */
void island_swap_buffers(Island *island);

/* Recomputes bestIndex, bestFitness and meanFitness of the current generation. */
void island_update_stats(Island *island, const GAConfig *cfg);

#endif /* NQ_POPULATION_H */

