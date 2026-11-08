#include "population.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "fitness.h"
#include "util.h"

void individual_bind(Individual *ind, int *base, int n)
{
    ind->genes = base;
    ind->queenConflicts = base + n;
    ind->fitnessScore = 0;
    ind->evaluationValid = false;
}

void individual_copy(Individual *dst, const Individual *src, int n)
{
    assert(dst->genes != src->genes);
    memcpy(dst->genes, src->genes, (size_t)n * sizeof(int));
    memcpy(dst->queenConflicts, src->queenConflicts, (size_t)n * sizeof(int));
    dst->fitnessScore = src->fitnessScore;
    dst->evaluationValid = src->evaluationValid;
}

/* Allocates P individuals backed by one contiguous storage block. */
static NQStatus alloc_population(Individual **individuals, int **storage, int p, int n)
{
    int ok = 1;
    const size_t perInd = nq_mul_size(2, (size_t)n, &ok);
    const size_t total = nq_mul_size(perInd, (size_t)p, &ok);
    if (!ok) {
        return NQ_ERR_ALLOC;
    }
    *individuals = nq_calloc_array((size_t)p, sizeof(Individual));
    *storage = nq_calloc_array(total, sizeof(int));
    if (*individuals == NULL || *storage == NULL) {
        return NQ_ERR_ALLOC;
    }
    for (int i = 0; i < p; ++i) {
        individual_bind(&(*individuals)[i], *storage + (size_t)i * perInd, n);
    }
    return NQ_OK;
}

static NQStatus island_workspace_init(IslandWorkspace *ws, int p, int n)
{
    if (fitness_workspace_init(&ws->fitness, n) != NQ_OK) {
        return NQ_ERR_ALLOC;
    }
    ws->selection.sortedIndices = nq_calloc_array((size_t)p, sizeof(int));
    ws->selection.pairs = nq_calloc_array((size_t)p, sizeof(FitnessIndexPair));
    ws->selection.cumulativeWeights = nq_calloc_array((size_t)p, sizeof(double));
    ws->selection.totalWeight = 0.0;
    ws->crossover.cutCandidates = nq_calloc_array((size_t)n, sizeof(int));
    ws->crossover.cutMarks = nq_calloc_array((size_t)n, sizeof(unsigned char));
    ws->crossover.swapCandidates = nq_calloc_array((size_t)n, sizeof(int));
    ws->rowBuffer = nq_calloc_array((size_t)n, sizeof(int));
    ws->spareStorage = nq_calloc_array(2 * (size_t)n, sizeof(int));
    if (ws->selection.sortedIndices == NULL || ws->selection.pairs == NULL ||
        ws->selection.cumulativeWeights == NULL || ws->crossover.cutCandidates == NULL ||
        ws->crossover.cutMarks == NULL || ws->crossover.swapCandidates == NULL ||
        ws->rowBuffer == NULL || ws->spareStorage == NULL) {
        return NQ_ERR_ALLOC;
    }
    individual_bind(&ws->spareChild, ws->spareStorage, n);
    return NQ_OK;
}

static void island_workspace_free(IslandWorkspace *ws)
{
    fitness_workspace_free(&ws->fitness);
    free(ws->selection.sortedIndices);
    free(ws->selection.pairs);
    free(ws->selection.cumulativeWeights);
    free(ws->crossover.cutCandidates);
    free(ws->crossover.cutMarks);
    free(ws->crossover.swapCandidates);
    free(ws->rowBuffer);
    free(ws->spareStorage);
    memset(ws, 0, sizeof(*ws));
}

NQStatus generation_create(Generation *gen, const GAConfig *cfg)
{
    memset(gen, 0, sizeof(*gen));
    const int islandCount = cfg->islandCount;
    const int p = cfg->islandPopulation;
    const int n = cfg->boardSize;

    gen->islands = nq_calloc_array((size_t)islandCount, sizeof(Island));
    if (gen->islands == NULL) {
        return NQ_ERR_ALLOC;
    }
    gen->islandCount = islandCount;
    gen->generationIndex = 0;

    for (int k = 0; k < islandCount; ++k) {
        Island *island = &gen->islands[k];
        if (alloc_population(&island->individuals, &island->storage, p, n) != NQ_OK ||
            alloc_population(&island->nextIndividuals, &island->nextStorage, p, n) != NQ_OK ||
            island_workspace_init(&island->ws, p, n) != NQ_OK) {
            generation_destroy(gen);
            return NQ_ERR_ALLOC;
        }
        /* Stream 0 is reserved for the migration controller. */
        rng_seed(&island->rng, cfg->randomSeed, (uint64_t)k + 1u);
        island->bestIndex = 0;
        island->bestFitness = 0;
        island->meanFitness = 0.0;
    }
    return NQ_OK;
}

void generation_destroy(Generation *gen)
{
    if (gen->islands != NULL) {
        for (int k = 0; k < gen->islandCount; ++k) {
            Island *island = &gen->islands[k];
            free(island->individuals);
            free(island->storage);
            free(island->nextIndividuals);
            free(island->nextStorage);
            island_workspace_free(&island->ws);
        }
        free(gen->islands);
    }
    memset(gen, 0, sizeof(*gen));
}

void island_randomize(Island *island, const GAConfig *cfg)
{
    const int n = cfg->boardSize;
    for (int i = 0; i < cfg->islandPopulation; ++i) {
        Individual *ind = &island->individuals[i];
        for (int r = 0; r < n; ++r) {
            ind->genes[r] = rng_uniform_int(&island->rng, n);
        }
        fitness_evaluate(ind, n, &island->ws.fitness);
    }
}

static int compare_pairs(const void *a, const void *b)
{
    const FitnessIndexPair *x = a;
    const FitnessIndexPair *y = b;
    if (x->fitness != y->fitness) {
        return (x->fitness < y->fitness) ? -1 : 1;
    }
    return (x->index < y->index) ? -1 : (x->index > y->index);
}

void population_sort_order(const Individual *individuals, int count,
                           FitnessIndexPair *pairs, int *order)
{
    for (int i = 0; i < count; ++i) {
        assert(individuals[i].evaluationValid);
        pairs[i].fitness = individuals[i].fitnessScore;
        pairs[i].index = i;
    }
    qsort(pairs, (size_t)count, sizeof(FitnessIndexPair), compare_pairs);
    for (int i = 0; i < count; ++i) {
        order[i] = pairs[i].index;
    }
}

void island_swap_buffers(Island *island)
{
    Individual *tmpInd = island->individuals;
    island->individuals = island->nextIndividuals;
    island->nextIndividuals = tmpInd;

    int *tmpStorage = island->storage;
    island->storage = island->nextStorage;
    island->nextStorage = tmpStorage;
}

void island_update_stats(Island *island, const GAConfig *cfg)
{
    int best = 0;
    long long sum = 0;
    for (int i = 0; i < cfg->islandPopulation; ++i) {
        const int f = island->individuals[i].fitnessScore;
        sum += f;
        if (f < island->individuals[best].fitnessScore) {
            best = i;
        }
    }
    island->bestIndex = best;
    island->bestFitness = island->individuals[best].fitnessScore;
    island->meanFitness = (double)sum / (double)cfg->islandPopulation;
}

