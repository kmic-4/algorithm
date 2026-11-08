#include "ga.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fitness.h"
#include "migration.h"
#include "population.h"
#include "stats.h"
#include "util.h"

void ga_prepare_island(Island *island, const GAConfig *cfg, const GAOperators *ops)
{
    SelectionCache *cache = &island->ws.selection;
    population_sort_order(island->individuals, cfg->islandPopulation, cache->pairs,
                          cache->sortedIndices);
    ops->prepareParentSelection(island, cfg, cache);
}

static int select_second_parent(Island *island, const GAConfig *cfg, const GAOperators *ops,
                                int first)
{
    int second = ops->selectParent(island, cfg, &island->ws.selection, &island->rng);
    if (!cfg->allowSameParentPair) {
        /* [暫定] re-draw with the same method; accept after a bounded number of tries. */
        for (int t = 0; t < NQ_SAME_PARENT_RETRY_LIMIT && second == first; ++t) {
            second = ops->selectParent(island, cfg, &island->ws.selection, &island->rng);
        }
    }
    return second;
}

/*
 * Final evaluation after crossover. If the child's genes equal those of
 * `origin` (diff == 0), the parent's evaluation is reused (no re-evaluation).
 */
static void finalize_crossover_child(Individual *child, const Individual *origin, int diff,
                                     int n, FitnessWorkspace *fws)
{
    if (diff == 0) {
        memcpy(child->queenConflicts, origin->queenConflicts, (size_t)n * sizeof(int));
        child->fitnessScore = origin->fitnessScore;
        child->evaluationValid = true;
    } else {
        fitness_evaluate(child, n, fws);
    }
}

/* Mutation step and statistics for an adopted child. */
static void mutate_child(Island *island, Individual *child, int diff, const GAConfig *cfg,
                         const GAOperators *ops)
{
    const bool mutated =
        ops->mutate(child, cfg, &island->ws.fitness, island->ws.rowBuffer, &island->rng);
    if (mutated) {
        island->counters.mutationCount++;
    }
    if (diff > 0 || mutated) {
        island->counters.changedChildren++;
    }
}

void ga_step_island(Island *island, const GAConfig *cfg, const GAOperators *ops)
{
    const int p = cfg->islandPopulation;
    const int e = cfg->eliteCount;
    const int n = cfg->boardSize;
    IslandWorkspace *ws = &island->ws;
    const Individual *cur = island->individuals;
    Individual *next = island->nextIndividuals;

    /* Elites: unchanged deep copies (never touched by crossover / mutation). */
    for (int k = 0; k < e; ++k) {
        individual_copy(&next[k], &cur[ws->selection.sortedIndices[k]], n);
    }

    int filled = e;
    while (filled < p) {
        const int ia = ops->selectParent(island, cfg, &ws->selection, &island->rng);
        const int ib = select_second_parent(island, cfg, ops, ia);
        const Individual *pa = &cur[ia];
        const Individual *pb = &cur[ib];
        const bool keepSecond = (filled + 1 < p); /* P - E odd: last second child discarded */
        Individual *c1 = &next[filled];
        Individual *c2 = keepSecond ? &next[filled + 1] : &ws->spareChild;

        int diff = 0;
        if (rng_bernoulli(&island->rng, cfg->crossoverRate)) {
            island->counters.crossoverCount++;
            diff = ops->crossover(pa, pb, c1, c2, cfg, &ws->crossover, &island->rng,
                                  &island->counters);
            finalize_crossover_child(c1, pa, diff, n, &ws->fitness);
            if (keepSecond) {
                finalize_crossover_child(c2, pb, diff, n, &ws->fitness);
            }
        } else {
            individual_copy(c1, pa, n);
            if (keepSecond) {
                individual_copy(c2, pb, n);
            }
        }

        mutate_child(island, c1, diff, cfg, ops);
        if (keepSecond) {
            mutate_child(island, c2, diff, cfg, ops);
            filled += 2;
        } else {
            filled += 1;
        }
    }

    island_swap_buffers(island);
}

static double wall_clock_seconds(void)
{
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0) {
        return (double)clock() / (double)CLOCKS_PER_SEC;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void accumulate_counters(GACounters *dst, const GACounters *src)
{
    dst->fitnessEvaluations += src->fitnessEvaluations;
    dst->crossoverCount += src->crossoverCount;
    dst->changedChildren += src->changedChildren;
    dst->mutationCount += src->mutationCount;
    dst->xorForcedSwaps += src->xorForcedSwaps;
    dst->migrationEvents += src->migrationEvents;
    dst->migrantsSent += src->migrantsSent;
    dst->migrantsDropped += src->migrantsDropped;
}

/* Returns the island holding the globally best individual (lowest index on ties). */
static int best_island(const Generation *gen)
{
    int best = 0;
    for (int i = 1; i < gen->islandCount; ++i) {
        if (gen->islands[i].bestFitness < gen->islands[best].bestFitness) {
            best = i;
        }
    }
    return best;
}

static void update_all_stats(Generation *gen, const GAConfig *cfg)
{
    for (int i = 0; i < gen->islandCount; ++i) {
        island_update_stats(&gen->islands[i], cfg);
    }
}

void ga_result_free(GAResult *result)
{
    free(result->bestGenes);
    result->bestGenes = NULL;
}

NQStatus ga_run(const GAConfig *cfg, FILE *generationLog, int logInterval, GAResult *result)
{
    memset(result, 0, sizeof(*result));
    result->solutionGeneration = -1;
    result->solutionIsland = -1;
    result->boardSize = cfg->boardSize;

    GAOperators ops;
    if (operators_resolve(cfg, &ops) != NQ_OK) {
        return NQ_ERR_INVALID_CONFIG;
    }

    result->bestGenes = nq_calloc_array((size_t)cfg->boardSize, sizeof(int));
    if (result->bestGenes == NULL) {
        return NQ_ERR_ALLOC;
    }

    Generation gen;
    MigrationWorkspace mws;
    memset(&mws, 0, sizeof(mws));
    if (generation_create(&gen, cfg) != NQ_OK) {
        ga_result_free(result);
        return NQ_ERR_ALLOC;
    }
    if (migration_workspace_init(&mws, cfg) != NQ_OK) {
        generation_destroy(&gen);
        ga_result_free(result);
        return NQ_ERR_ALLOC;
    }

    Rng migrationRng;
    rng_seed(&migrationRng, cfg->randomSeed, 0u);
    GACounters migrationCounters;
    memset(&migrationCounters, 0, sizeof(migrationCounters));

    const double start = wall_clock_seconds();
    const int interval = (logInterval > 0) ? logInterval : 1;

    /* Initial population (spec §5). */
    for (int i = 0; i < gen.islandCount; ++i) {
        island_randomize(&gen.islands[i], cfg);
    }
    update_all_stats(&gen, cfg);
    if (generationLog != NULL) {
        stats_log_generation(generationLog, &gen);
    }

    int bi = best_island(&gen);
    bool solved = gen.islands[bi].bestFitness == 0;

    while (!solved && gen.generationIndex < cfg->maxGenerations) {
        for (int i = 0; i < gen.islandCount; ++i) {
            Island *island = &gen.islands[i];
            ga_prepare_island(island, cfg, &ops);
            ga_step_island(island, cfg, &ops);
        }
        gen.generationIndex++;

        if (migration_is_due(cfg, gen.generationIndex)) {
            migration_run(&gen, cfg, &ops, &mws, &migrationRng, &migrationCounters);
        }
        update_all_stats(&gen, cfg);

        bi = best_island(&gen);
        solved = gen.islands[bi].bestFitness == 0;

        if (generationLog != NULL &&
            (solved || gen.generationIndex % interval == 0 ||
             gen.generationIndex == cfg->maxGenerations)) {
            stats_log_generation(generationLog, &gen);
        }
    }

    result->elapsedSeconds = wall_clock_seconds() - start;
    result->solved = solved;
    result->generationsRun = gen.generationIndex;
    if (solved) {
        result->solutionGeneration = gen.generationIndex;
        result->solutionIsland = bi;
    }
    const Individual *best = &gen.islands[bi].individuals[gen.islands[bi].bestIndex];
    result->bestFitness = best->fitnessScore;
    memcpy(result->bestGenes, best->genes, (size_t)cfg->boardSize * sizeof(int));

    for (int i = 0; i < gen.islandCount; ++i) {
        gen.islands[i].counters.fitnessEvaluations = gen.islands[i].ws.fitness.evaluationCount;
        accumulate_counters(&result->counters, &gen.islands[i].counters);
    }
    accumulate_counters(&result->counters, &migrationCounters);

    migration_workspace_free(&mws);
    generation_destroy(&gen);
    return NQ_OK;
}
