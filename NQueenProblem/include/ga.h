#ifndef NQ_GA_H
#define NQ_GA_H

#include <stdio.h>

#include "definition.h"
#include "operators.h"

/*
 * Sorts the current generation of the island and rebuilds the parent
 * selection cache. Must be called whenever fitness values changed.
 */
void ga_prepare_island(Island *island, const GAConfig *cfg, const GAOperators *ops);

/*
 * Elitist generational step (spec §9.1) for one island. Requires
 * ga_prepare_island() to have been called on the current generation.
 * Copies the top eliteCount individuals unchanged, fills the remaining
 * slots with children, then swaps the generation buffers.
 */
void ga_step_island(Island *island, const GAConfig *cfg, const GAOperators *ops);

/*
 * Runs the island model GA (spec §11). cfg must already be validated.
 * generationLog (may be NULL) receives CSV rows every logInterval
 * generations (and at the last generation). result->bestGenes is
 * allocated and must be released with ga_result_free().
 */
NQStatus ga_run(const GAConfig *cfg, FILE *generationLog, int logInterval, GAResult *result);

void ga_result_free(GAResult *result);

#endif /* NQ_GA_H */

