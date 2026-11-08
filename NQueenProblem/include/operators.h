#ifndef NQ_OPERATORS_H
#define NQ_OPERATORS_H

/*
 * Function pointer interfaces for interchangeable GA strategies (spec §4.3).
 * Every operator receives the configuration, its RNG and its workspace
 * explicitly; no operator touches global state.
 */

#include "definition.h"

/* ---------------- Parent selection (spec §6) ---------------- */

/*
 * Builds the per-generation selection cache. Called once per island per
 * generation, after cache->sortedIndices has been filled (best -> worst).
 */
typedef void (*ParentPrepareFn)(const Island *island, const GAConfig *cfg,
                                SelectionCache *cache);

/* Returns the index of the selected parent in island->individuals. */
typedef int (*ParentSelectFn)(const Island *island, const GAConfig *cfg,
                              const SelectionCache *cache, Rng *rng);

/* ---------------- Crossover (spec §7) ---------------- */

/*
 * Writes the genes of two children; children become evaluationValid = false.
 * Parents must be evaluated. Returns the number of positions where childA
 * differs from parentA (equal to childB vs parentB).
 */
typedef int (*CrossoverFn)(const Individual *parentA, const Individual *parentB,
                           Individual *childA, Individual *childB,
                           const GAConfig *cfg, CrossoverWorkspace *ws, Rng *rng,
                           GACounters *counters);

/* ---------------- Mutation (spec §8) ---------------- */

/*
 * Conflict-guided mutation. Evaluates the child first if needed.
 * Leaves the child evaluated. Returns true if a gene was changed.
 */
typedef bool (*MutationFn)(Individual *child, const GAConfig *cfg,
                           FitnessWorkspace *fws, int *rowBuffer, Rng *rng);

/* ---------------- Migration (spec §10) ---------------- */

/*
 * Plans routes for one migration event. For every source island s (ascending)
 * emits up to cfg->migrationCount routes (s -> receiver != s), decrementing
 * receiveCapacity[receiver]. Routes are grouped by source in ascending order.
 * Returns the number of routes written (copies that cannot be placed are dropped).
 */
typedef int (*MigrationRouteFn)(const GAConfig *cfg, int *receiveCapacity,
                                MigrationRoute *routes, int *islandScratch, Rng *rng);

typedef struct {
    const Island *islands;       /* all islands (pre-event state) */
    int sourceIndex;
    const int *sourceOrder;      /* source individuals sorted best -> worst */
    const int *orders;           /* I * P sort orders of all islands */
    const int *slotReceivers;    /* receiver for each migrant slot */
    int count;                   /* number of migrants to choose */
    const GAConfig *cfg;
    MigrationWorkspace *ws;
} MigrantSelectArgs;

/* Writes args->count distinct source indices into outIndices (slot order). */
typedef void (*MigrantSelectFn)(const MigrantSelectArgs *args, Rng *rng, int *outIndices);

/*
 * Chooses count distinct slots of the receiving island to overwrite.
 * receiverOrder is the receiver sorted best -> worst; the first
 * protectedCount entries (protected elites) are never chosen.
 * Requires count <= populationSize - protectedCount.
 */
typedef void (*ReplaceFn)(const int *receiverOrder, int populationSize, int protectedCount,
                          int count, Rng *rng, int *scratch, int *outSlots);

typedef struct {
    ParentPrepareFn prepareParentSelection;
    ParentSelectFn selectParent;
    CrossoverFn crossover;
    MutationFn mutate;
    MigrantSelectFn selectMigrants;
    MigrationRouteFn planMigration;
    ReplaceFn replaceMigrants;
} GAOperators;

/* Resolves the function pointers for the methods chosen in cfg. */
NQStatus operators_resolve(const GAConfig *cfg, GAOperators *ops);

/* ---- Concrete strategies ---- */
void parent_prepare_none(const Island *island, const GAConfig *cfg, SelectionCache *cache);
void parent_prepare_roulette(const Island *island, const GAConfig *cfg, SelectionCache *cache);
void parent_prepare_rank(const Island *island, const GAConfig *cfg, SelectionCache *cache);
int parent_select_tournament(const Island *island, const GAConfig *cfg,
                             const SelectionCache *cache, Rng *rng);
int parent_select_roulette(const Island *island, const GAConfig *cfg,
                           const SelectionCache *cache, Rng *rng);
int parent_select_rank(const Island *island, const GAConfig *cfg,
                       const SelectionCache *cache, Rng *rng);

int crossover_npoint(const Individual *parentA, const Individual *parentB,
                     Individual *childA, Individual *childB, const GAConfig *cfg,
                     CrossoverWorkspace *ws, Rng *rng, GACounters *counters);
int crossover_xor(const Individual *parentA, const Individual *parentB,
                  Individual *childA, Individual *childB, const GAConfig *cfg,
                  CrossoverWorkspace *ws, Rng *rng, GACounters *counters);

bool mutation_conflict_guided(Individual *child, const GAConfig *cfg,
                              FitnessWorkspace *fws, int *rowBuffer, Rng *rng);

int topology_ring(const GAConfig *cfg, int *receiveCapacity, MigrationRoute *routes,
                  int *islandScratch, Rng *rng);
int topology_fully_connected(const GAConfig *cfg, int *receiveCapacity,
                             MigrationRoute *routes, int *islandScratch, Rng *rng);
int topology_random(const GAConfig *cfg, int *receiveCapacity, MigrationRoute *routes,
                    int *islandScratch, Rng *rng);

void migrant_select_best(const MigrantSelectArgs *args, Rng *rng, int *outIndices);
void migrant_select_random(const MigrantSelectArgs *args, Rng *rng, int *outIndices);
void migrant_select_tournament(const MigrantSelectArgs *args, Rng *rng, int *outIndices);
void migrant_select_diversity(const MigrantSelectArgs *args, Rng *rng, int *outIndices);

void replace_worst(const int *receiverOrder, int populationSize, int protectedCount,
                   int count, Rng *rng, int *scratch, int *outSlots);
void replace_random(const int *receiverOrder, int populationSize, int protectedCount,
                    int count, Rng *rng, int *scratch, int *outSlots);

#endif /* NQ_OPERATORS_H */

