#ifndef NQ_MIGRATION_H
#define NQ_MIGRATION_H

#include "definition.h"
#include "operators.h"

/* Allocates every buffer needed by one migration event (reused for all events). */
NQStatus migration_workspace_init(MigrationWorkspace *ws, const GAConfig *cfg);
void migration_workspace_free(MigrationWorkspace *ws);

/* True if a migration event should run after generation `generationIndex`. */
bool migration_is_due(const GAConfig *cfg, int generationIndex);

/*
 * Runs one migration event (spec §10, §11 step 5.4):
 *  1. snapshot the sort order of every island (pre-event state);
 *  2. plan routes with the topology (receiver capacity = P - eliteCount);
 *  3. select migrants per source and deep-copy them into a snapshot buffer;
 *  4. overwrite the chosen receiver slots with deep copies (genes + evaluation).
 * Sources are never modified by their own sending; protected elites of the
 * receiver (its pre-event top eliteCount) are never replaced.
 */
void migration_run(Generation *gen, const GAConfig *cfg, const GAOperators *ops,
                   MigrationWorkspace *ws, Rng *rng, GACounters *counters);

#endif /* NQ_MIGRATION_H */

