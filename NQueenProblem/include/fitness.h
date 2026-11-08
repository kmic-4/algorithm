#ifndef NQ_FITNESS_H
#define NQ_FITNESS_H

#include "definition.h"

/* Allocates the occupancy arrays for board size n. Returns NQ_OK or NQ_ERR_ALLOC. */
NQStatus fitness_workspace_init(FitnessWorkspace *ws, int n);
void fitness_workspace_free(FitnessWorkspace *ws);

/*
 * O(N) evaluation using column / diagonal occupancy counts (spec §3.1).
 * Fills queenConflicts[], fitnessScore and sets evaluationValid.
 */
void fitness_evaluate(Individual *ind, int n, FitnessWorkspace *ws);

/*
 * O(N^2) brute force reference evaluator (for tests).
 * conflictsOut may be NULL. Returns the number of conflicting pairs.
 */
int fitness_reference(const int *genes, int n, int *conflictsOut);

#endif /* NQ_FITNESS_H */

