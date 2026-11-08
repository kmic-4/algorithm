/*
 * Focused correctness tests (spec §15).
 * Build & run: make test   (or make asan-test for ASan/UBSan)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fitness.h"
#include "ga.h"
#include "migration.h"
#include "operators.h"
#include "population.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        ++g_checks;                                                                            \
        if (!(cond)) {                                                                         \
            ++g_failures;                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                    \
        }                                                                                      \
    } while (0)

typedef struct {
    Individual ind;
    int *storage;
} TestIndividual;

static TestIndividual make_individual(int n)
{
    TestIndividual t;
    t.storage = calloc(2 * (size_t)n, sizeof(int));
    if (t.storage == NULL) {
        fprintf(stderr, "allocation failed\n");
        exit(3);
    }
    individual_bind(&t.ind, t.storage, n);
    return t;
}

static void free_individual(TestIndividual *t)
{
    free(t->storage);
    t->storage = NULL;
}

static bool genes_in_range(const Individual *ind, int n)
{
    for (int i = 0; i < n; ++i) {
        if (ind->genes[i] < 0 || ind->genes[i] >= n) {
            return false;
        }
    }
    return true;
}

/* Evaluation is valid, matches the reference evaluator and sum(conflicts) == 2F. */
static bool evaluation_consistent(const Individual *ind, int n)
{
    if (!ind->evaluationValid || !genes_in_range(ind, n)) {
        return false;
    }
    int *ref = calloc((size_t)n, sizeof(int));
    if (ref == NULL) {
        exit(3);
    }
    const int f = fitness_reference(ind->genes, n, ref);
    bool ok = (f == ind->fitnessScore);
    long sum = 0;
    for (int i = 0; i < n; ++i) {
        ok = ok && (ref[i] == ind->queenConflicts[i]);
        sum += ind->queenConflicts[i];
    }
    ok = ok && (sum == 2L * ind->fitnessScore);
    free(ref);
    return ok;
}

static GAConfig small_config(void)
{
    GAConfig cfg;
    config_set_defaults(&cfg);
    cfg.boardSize = 8;
    cfg.islandCount = 4;
    cfg.islandPopulation = 20;
    cfg.maxGenerations = 2000;
    cfg.eliteCount = 2;
    cfg.migrationInterval = 10;
    cfg.migrationCount = 3;
    cfg.randomSeed = 42;
    return cfg;
}

/* ---------------- 1. small boards ---------------- */
static int count_solutions_bruteforce(int n, FitnessWorkspace *ws)
{
    TestIndividual t = make_individual(n);
    long total = 1;
    for (int i = 0; i < n; ++i) {
        total *= n;
    }
    int solutions = 0;
    for (long code = 0; code < total; ++code) {
        long c = code;
        for (int i = 0; i < n; ++i) {
            t.ind.genes[i] = (int)(c % n);
            c /= n;
        }
        fitness_evaluate(&t.ind, n, ws);
        CHECK(evaluation_consistent(&t.ind, n));
        solutions += (t.ind.fitnessScore == 0);
    }
    free_individual(&t);
    return solutions;
}

static void test_small_boards(void)
{
    const int expected[] = {0, 1, 0, 0, 2, 10, 4};
    for (int n = 1; n <= 6; ++n) {
        FitnessWorkspace ws;
        CHECK(fitness_workspace_init(&ws, n) == NQ_OK);
        CHECK(count_solutions_bruteforce(n, &ws) == expected[n]);
        fitness_workspace_free(&ws);
    }

    /* Known N=4 solutions and a duplicate-column example from spec §2.1. */
    FitnessWorkspace ws;
    CHECK(fitness_workspace_init(&ws, 4) == NQ_OK);
    TestIndividual t = make_individual(4);
    const int sol1[4] = {1, 3, 0, 2};
    const int sol2[4] = {2, 0, 3, 1};
    const int bad[4] = {0, 2, 2, 3};
    memcpy(t.ind.genes, sol1, sizeof(sol1));
    fitness_evaluate(&t.ind, 4, &ws);
    CHECK(t.ind.fitnessScore == 0);
    memcpy(t.ind.genes, sol2, sizeof(sol2));
    fitness_evaluate(&t.ind, 4, &ws);
    CHECK(t.ind.fitnessScore == 0);
    memcpy(t.ind.genes, bad, sizeof(bad));
    fitness_evaluate(&t.ind, 4, &ws);
    CHECK(t.ind.fitnessScore > 0);
    CHECK(evaluation_consistent(&t.ind, 4));

    /* Same column k times -> C(k,2) column pairs (all-zero genes: N=4 -> 6). */
    memset(t.ind.genes, 0, 4 * sizeof(int));
    fitness_evaluate(&t.ind, 4, &ws);
    CHECK(t.ind.fitnessScore == 6);
    free_individual(&t);
    fitness_workspace_free(&ws);
}

/* ---------------- 2/3. random evaluation vs reference ---------------- */
static void test_random_evaluation(void)
{
    Rng rng;
    rng_seed(&rng, 7, 1);
    for (int n = 1; n <= 64; ++n) {
        FitnessWorkspace ws;
        CHECK(fitness_workspace_init(&ws, n) == NQ_OK);
        TestIndividual t = make_individual(n);
        for (int rep = 0; rep < 50; ++rep) {
            for (int i = 0; i < n; ++i) {
                t.ind.genes[i] = rng_uniform_int(&rng, n);
            }
            fitness_evaluate(&t.ind, n, &ws);
            CHECK(evaluation_consistent(&t.ind, n));
        }
        free_individual(&t);
        fitness_workspace_free(&ws);
    }
}

/* ---------------- 4/5. crossover ---------------- */
static void random_genes(Individual *ind, int n, Rng *rng)
{
    for (int i = 0; i < n; ++i) {
        ind->genes[i] = rng_uniform_int(rng, n);
    }
}

/* childA[i] in {a[i], b[i]}, childB mirrors it, and diff counts the changes. */
static bool crossover_structure_ok(const Individual *a, const Individual *b, const Individual *ca,
                                   const Individual *cb, int n, int diff)
{
    int counted = 0;
    for (int i = 0; i < n; ++i) {
        const bool fromA = (ca->genes[i] == a->genes[i] && cb->genes[i] == b->genes[i]);
        const bool fromB = (ca->genes[i] == b->genes[i] && cb->genes[i] == a->genes[i]);
        if (!fromA && !fromB) {
            return false;
        }
        counted += (ca->genes[i] != a->genes[i]);
    }
    return counted == diff && genes_in_range(ca, n) && genes_in_range(cb, n);
}

static void test_crossover(void)
{
    const int n = 12;
    GAConfig cfg = small_config();
    cfg.boardSize = n;
    Rng rng;
    rng_seed(&rng, 99, 3);
    FitnessWorkspace fws;
    CHECK(fitness_workspace_init(&fws, n) == NQ_OK);
    CrossoverWorkspace cws;
    cws.cutCandidates = calloc((size_t)n, sizeof(int));
    cws.cutMarks = calloc((size_t)n, 1);
    cws.swapCandidates = calloc((size_t)n, sizeof(int));
    GACounters counters;
    memset(&counters, 0, sizeof(counters));
    TestIndividual a = make_individual(n), b = make_individual(n);
    TestIndividual ca = make_individual(n), cb = make_individual(n);

    for (int rep = 0; rep < 2000; ++rep) {
        random_genes(&a.ind, n, &rng);
        random_genes(&b.ind, n, &rng);
        fitness_evaluate(&a.ind, n, &fws);
        fitness_evaluate(&b.ind, n, &fws);

        cfg.crossoverPoints = 1 + rep % (n - 1);
        int diff = crossover_npoint(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
        CHECK(crossover_structure_ok(&a.ind, &b.ind, &ca.ind, &cb.ind, n, diff));
        CHECK(!ca.ind.evaluationValid && !cb.ind.evaluationValid);

        cfg.xorSwapProbabilityMethod = (rep % 2) ? XOR_PROB_CONFLICT_PROPORTIONAL : XOR_PROB_FIXED;
        cfg.xorCandidateRule = (rep % 3 == 0) ? XOR_CANDIDATE_AND : XOR_CANDIDATE_OR;
        cfg.xorSwapRate = (double)(rep % 5) / 4.0;
        diff = crossover_xor(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
        CHECK(crossover_structure_ok(&a.ind, &b.ind, &ca.ind, &cb.ind, n, diff));
        /* swapped positions must be candidates */
        for (int i = 0; i < n; ++i) {
            if (ca.ind.genes[i] != a.ind.genes[i]) {
                const int qa = a.ind.queenConflicts[i], qb = b.ind.queenConflicts[i];
                CHECK(cfg.xorCandidateRule == XOR_CANDIDATE_AND ? (qa > 0 && qb > 0)
                                                                : (qa > 0 || qb > 0));
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        CHECK(cws.cutMarks[i] == 0); /* workspace left clean */
    }

    /* n-point with N-1 cuts alternates every gene. */
    random_genes(&a.ind, n, &rng);
    for (int i = 0; i < n; ++i) {
        b.ind.genes[i] = (a.ind.genes[i] + 1) % n;
    }
    cfg.crossoverPoints = n - 1;
    int diff = crossover_npoint(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
    CHECK(diff == n / 2);
    for (int i = 0; i < n; ++i) {
        CHECK(ca.ind.genes[i] == ((i % 2 == 0) ? a.ind.genes[i] : b.ind.genes[i]));
    }

    /* XOR forced swap: rate 0 + force -> exactly one swap; without force -> none. */
    for (int i = 0; i < n; ++i) {
        a.ind.genes[i] = 0;  /* every queen conflicts */
        b.ind.genes[i] = i;  /* main diagonal: every queen conflicts */
    }
    fitness_evaluate(&a.ind, n, &fws);
    fitness_evaluate(&b.ind, n, &fws);
    cfg.xorSwapRate = 0.0;
    cfg.xorSwapProbabilityMethod = XOR_PROB_FIXED;
    cfg.xorCandidateRule = XOR_CANDIDATE_OR;
    cfg.xorForceSwap = true;
    const uint64_t forcedBefore = counters.xorForcedSwaps;
    diff = crossover_xor(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
    CHECK(diff == 1);
    CHECK(counters.xorForcedSwaps == forcedBefore + 1);
    CHECK(crossover_structure_ok(&a.ind, &b.ind, &ca.ind, &cb.ind, n, diff));
    cfg.xorForceSwap = false;
    diff = crossover_xor(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
    CHECK(diff == 0);

    /* Rate 1 swaps all candidates (positions 1..n-1 differ). */
    cfg.xorSwapRate = 1.0;
    diff = crossover_xor(&a.ind, &b.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
    CHECK(diff == n - 1);

    /* Identical parents: no candidates -> unchanged even with force. */
    cfg.xorForceSwap = true;
    cfg.xorSwapRate = 0.0;
    diff = crossover_xor(&a.ind, &a.ind, &ca.ind, &cb.ind, &cfg, &cws, &rng, &counters);
    CHECK(diff == 0);
    CHECK(memcmp(ca.ind.genes, a.ind.genes, (size_t)n * sizeof(int)) == 0);

    free_individual(&a);
    free_individual(&b);
    free_individual(&ca);
    free_individual(&cb);
    free(cws.cutCandidates);
    free(cws.cutMarks);
    free(cws.swapCandidates);
    fitness_workspace_free(&fws);
}

/* ---------------- mutation ---------------- */
static void test_mutation(void)
{
    const int n = 10;
    GAConfig cfg = small_config();
    cfg.boardSize = n;
    cfg.mutationRate = 1.0;
    Rng rng;
    rng_seed(&rng, 5, 5);
    FitnessWorkspace fws;
    CHECK(fitness_workspace_init(&fws, n) == NQ_OK);
    int rows[10];
    int before[10];
    int conflictsBefore[10];
    TestIndividual t = make_individual(n);

    for (int rep = 0; rep < 2000; ++rep) {
        random_genes(&t.ind, n, &rng);
        t.ind.evaluationValid = false; /* mutation must evaluate first */
        fitness_evaluate(&t.ind, n, &fws);
        memcpy(before, t.ind.genes, sizeof(before));
        memcpy(conflictsBefore, t.ind.queenConflicts, sizeof(conflictsBefore));
        const int fBefore = t.ind.fitnessScore;
        const bool mutated = mutation_conflict_guided(&t.ind, &cfg, &fws, rows, &rng);
        CHECK(mutated == (fBefore > 0));
        int changed = 0;
        for (int i = 0; i < n; ++i) {
            if (before[i] != t.ind.genes[i]) {
                ++changed;
                CHECK(conflictsBefore[i] > 0);
            }
        }
        CHECK(changed == (mutated ? 1 : 0));
        CHECK(evaluation_consistent(&t.ind, n));
    }

    /* Unevaluated child is evaluated before mutation. */
    random_genes(&t.ind, n, &rng);
    t.ind.evaluationValid = false;
    cfg.mutationRate = 0.0;
    CHECK(!mutation_conflict_guided(&t.ind, &cfg, &fws, rows, &rng));
    CHECK(evaluation_consistent(&t.ind, n));
    free_individual(&t);
    fitness_workspace_free(&fws);

    /* Solutions and N = 1 are never mutated. */
    FitnessWorkspace ws4;
    CHECK(fitness_workspace_init(&ws4, 4) == NQ_OK);
    TestIndividual s = make_individual(4);
    const int sol[4] = {1, 3, 0, 2};
    memcpy(s.ind.genes, sol, sizeof(sol));
    fitness_evaluate(&s.ind, 4, &ws4);
    cfg.boardSize = 4;
    cfg.mutationRate = 1.0;
    CHECK(!mutation_conflict_guided(&s.ind, &cfg, &ws4, rows, &rng));
    CHECK(memcmp(s.ind.genes, sol, sizeof(sol)) == 0);
    free_individual(&s);
    fitness_workspace_free(&ws4);
}

/* ---------------- parent selection ---------------- */
static void test_parent_selection(void)
{
    GAConfig cfg = small_config();
    Generation gen;
    CHECK(generation_create(&gen, &cfg) == NQ_OK);
    Island *island = &gen.islands[0];
    island_randomize(island, &cfg);
    GAOperators ops;

    const ParentSelectionMethod methods[] = {PARENT_SELECTION_TOURNAMENT, PARENT_SELECTION_ROULETTE,
                                             PARENT_SELECTION_RANK};
    for (int m = 0; m < 3; ++m) {
        cfg.parentSelectionMethod = methods[m];
        cfg.rankSelectionPressure = 2.0; /* worst rank gets weight 0 */
        CHECK(operators_resolve(&cfg, &ops) == NQ_OK);
        ga_prepare_island(island, &cfg, &ops);
        const int worst = island->ws.selection.sortedIndices[cfg.islandPopulation - 1];
        for (int rep = 0; rep < 5000; ++rep) {
            const int idx = ops.selectParent(island, &cfg, &island->ws.selection, &island->rng);
            CHECK(idx >= 0 && idx < cfg.islandPopulation);
            if (methods[m] == PARENT_SELECTION_RANK) {
                CHECK(idx != worst);
            }
        }
    }
    /* sortedIndices is sorted best -> worst */
    for (int i = 1; i < cfg.islandPopulation; ++i) {
        const int *o = island->ws.selection.sortedIndices;
        CHECK(island->individuals[o[i - 1]].fitnessScore <= island->individuals[o[i]].fitnessScore);
    }
    generation_destroy(&gen);
}

/* ---------------- 6. elites / generation step ---------------- */
static void test_generation_step(void)
{
    const CrossoverMethod crossovers[] = {CROSSOVER_NPOINT, CROSSOVER_XOR};
    for (int c = 0; c < 2; ++c) {
        for (int odd = 0; odd < 2; ++odd) {
            GAConfig cfg = small_config();
            cfg.crossoverMethod = crossovers[c];
            cfg.eliteCount = 3 + odd; /* P - E odd and even */
            cfg.allowSameParentPair = (odd == 0);
            const int n = cfg.boardSize, p = cfg.islandPopulation, e = cfg.eliteCount;
            GAOperators ops;
            CHECK(operators_resolve(&cfg, &ops) == NQ_OK);
            Generation gen;
            CHECK(generation_create(&gen, &cfg) == NQ_OK);
            Island *island = &gen.islands[0];
            island_randomize(island, &cfg);

            for (int step = 0; step < 20; ++step) {
                ga_prepare_island(island, &cfg, &ops);
                int *eliteGenes = calloc((size_t)(e * n), sizeof(int));
                int *eliteFit = calloc((size_t)e, sizeof(int));
                int *allGenes = calloc((size_t)(p * n), sizeof(int));
                for (int k = 0; k < e; ++k) {
                    const Individual *src = &island->individuals[island->ws.selection.sortedIndices[k]];
                    memcpy(eliteGenes + k * n, src->genes, (size_t)n * sizeof(int));
                    eliteFit[k] = src->fitnessScore;
                }
                for (int i = 0; i < p; ++i) {
                    memcpy(allGenes + i * n, island->individuals[i].genes, (size_t)n * sizeof(int));
                }
                const Individual *oldPop = island->individuals;

                ga_step_island(island, &cfg, &ops);

                CHECK(island->nextIndividuals == oldPop); /* buffers swapped */
                for (int i = 0; i < p; ++i) { /* old generation not modified */
                    CHECK(memcmp(allGenes + i * n, oldPop[i].genes, (size_t)n * sizeof(int)) == 0);
                }
                for (int k = 0; k < e; ++k) {
                    CHECK(memcmp(island->individuals[k].genes, eliteGenes + k * n,
                                 (size_t)n * sizeof(int)) == 0);
                    CHECK(island->individuals[k].fitnessScore == eliteFit[k]);
                }
                for (int i = 0; i < p; ++i) {
                    CHECK(evaluation_consistent(&island->individuals[i], n));
                }
                free(eliteGenes);
                free(eliteFit);
                free(allGenes);
            }
            generation_destroy(&gen);
        }
    }
}

/* ---------------- 7. migration ---------------- */
static bool storage_owned(const Island *island, const Individual *ind, int p, int n)
{
    const int *begin = island->storage;
    const int *end = island->storage + (size_t)p * 2 * (size_t)n;
    return ind->genes >= begin && ind->genes + n <= end && ind->queenConflicts >= begin &&
           ind->queenConflicts + n <= end;
}

static void test_migration_case(MigrationTopology topo, MigrantSelectionMethod sel,
                                MigrationReplacementMethod rep)
{
    GAConfig cfg = small_config();
    cfg.islandPopulation = 10;
    cfg.eliteCount = 2;
    cfg.migrationCount = 5;
    cfg.migrationTopology = topo;
    cfg.migrantSelectionMethod = sel;
    cfg.migrationReplacementMethod = rep;
    const int islands = cfg.islandCount, p = cfg.islandPopulation, n = cfg.boardSize;
    const int e = cfg.eliteCount;
    GAOperators ops;
    CHECK(config_validate(&cfg, NULL, 0) == NQ_OK);
    CHECK(operators_resolve(&cfg, &ops) == NQ_OK);
    Generation gen;
    CHECK(generation_create(&gen, &cfg) == NQ_OK);
    MigrationWorkspace mws;
    CHECK(migration_workspace_init(&mws, &cfg) == NQ_OK);
    for (int i = 0; i < islands; ++i) {
        island_randomize(&gen.islands[i], &cfg);
    }

    /* Pre-event copies and orders. */
    int *pre = calloc((size_t)(islands * p * n), sizeof(int));
    int *preOrder = calloc((size_t)(islands * p), sizeof(int));
    FitnessIndexPair *pairs = calloc((size_t)p, sizeof(FitnessIndexPair));
    for (int i = 0; i < islands; ++i) {
        population_sort_order(gen.islands[i].individuals, p, pairs, preOrder + i * p);
        for (int j = 0; j < p; ++j) {
            memcpy(pre + (i * p + j) * n, gen.islands[i].individuals[j].genes,
                   (size_t)n * sizeof(int));
        }
    }

    Rng rng;
    rng_seed(&rng, 11, 0);
    GACounters counters;
    memset(&counters, 0, sizeof(counters));
    migration_run(&gen, &cfg, &ops, &mws, &rng, &counters);
    CHECK(counters.migrationEvents == 1);
    CHECK(counters.migrantsSent + counters.migrantsDropped == (uint64_t)(islands * cfg.migrationCount));
    if (topo != TOPOLOGY_RANDOM) {
        CHECK(counters.migrantsDropped == 0);
    }
    const int routeCount = (int)counters.migrantsSent;

    for (int r = 0; r < islands; ++r) {
        const Island *island = &gen.islands[r];
        int incoming = 0;
        for (int k = 0; k < routeCount; ++k) {
            CHECK(mws.routes[k].source != mws.routes[k].receiver);
            incoming += (mws.routes[k].receiver == r);
        }
        CHECK(incoming <= p - e);
        /* protected elites unchanged */
        for (int k = 0; k < e; ++k) {
            const int slot = preOrder[r * p + k];
            CHECK(memcmp(island->individuals[slot].genes, pre + (r * p + slot) * n,
                         (size_t)n * sizeof(int)) == 0);
        }
        int changed = 0;
        for (int j = 0; j < p; ++j) {
            const Individual *ind = &island->individuals[j];
            CHECK(evaluation_consistent(ind, n));
            CHECK(storage_owned(island, ind, p, n));
            if (memcmp(ind->genes, pre + (r * p + j) * n, (size_t)n * sizeof(int)) != 0) {
                ++changed;
                /* must be a copy of a pre-event individual from another island */
                bool found = false;
                for (int s = 0; s < islands && !found; ++s) {
                    for (int q = 0; q < p && !found; ++q) {
                        found = (s != r) && memcmp(ind->genes, pre + (s * p + q) * n,
                                                   (size_t)n * sizeof(int)) == 0;
                    }
                }
                CHECK(found);
            }
            if (rep == MIGRATION_REPLACE_WORST) {
                /* only the worst `incoming` slots may change */
                bool inWorst = false;
                for (int k = 0; k < incoming; ++k) {
                    inWorst = inWorst || (preOrder[r * p + p - 1 - k] == j);
                }
                if (!inWorst) {
                    CHECK(memcmp(ind->genes, pre + (r * p + j) * n, (size_t)n * sizeof(int)) == 0);
                }
            }
        }
        CHECK(changed <= incoming);
    }

    /* Ring + Best + Worst: island r+1's worst slots receive island r's top individuals. */
    if (topo == TOPOLOGY_RING && sel == MIGRANT_SELECTION_BEST && rep == MIGRATION_REPLACE_WORST) {
        for (int s = 0; s < islands; ++s) {
            const int r = (s + 1) % islands;
            for (int k = 0; k < cfg.migrationCount; ++k) {
                const int slot = preOrder[r * p + p - 1 - k];
                const int srcIdx = preOrder[s * p + k];
                CHECK(memcmp(gen.islands[r].individuals[slot].genes, pre + (s * p + srcIdx) * n,
                             (size_t)n * sizeof(int)) == 0);
            }
        }
    }

    /* No aliasing between any two individuals. */
    for (int i = 0; i < islands; ++i) {
        for (int j = 0; j < p; ++j) {
            for (int k = j + 1; k < p; ++k) {
                CHECK(gen.islands[i].individuals[j].genes != gen.islands[i].individuals[k].genes);
            }
        }
    }

    free(pre);
    free(preOrder);
    free(pairs);
    migration_workspace_free(&mws);
    generation_destroy(&gen);
}

static void test_migration(void)
{
    for (int t = 0; t < TOPOLOGY_COUNT_; ++t) {
        for (int s = 0; s < MIGRANT_SELECTION_COUNT_; ++s) {
            for (int r = 0; r < MIGRATION_REPLACE_COUNT_; ++r) {
                test_migration_case((MigrationTopology)t, (MigrantSelectionMethod)s,
                                    (MigrationReplacementMethod)r);
            }
        }
    }

    /* Fully connected distributes migrationCount evenly: each receiver gets exactly m. */
    GAConfig cfg = small_config();
    cfg.islandCount = 5;
    cfg.migrationCount = 7;
    int capacity[5];
    MigrationRoute routes[35];
    int scratch[5];
    Rng rng;
    rng_seed(&rng, 1, 0);
    for (int i = 0; i < 5; ++i) {
        capacity[i] = 100;
    }
    const int count = topology_fully_connected(&cfg, capacity, routes, scratch, &rng);
    CHECK(count == 35);
    for (int i = 0; i < 5; ++i) {
        CHECK(capacity[i] == 100 - 7);
    }
    /* Random topology never exceeds capacity. */
    for (int i = 0; i < 5; ++i) {
        capacity[i] = 3;
    }
    const int rc = topology_random(&cfg, capacity, routes, scratch, &rng);
    CHECK(rc <= 15);
    for (int i = 0; i < 5; ++i) {
        CHECK(capacity[i] >= 0);
    }
}

/* ---------------- 8. reproducibility & end-to-end ---------------- */
static bool same_result(const GAResult *a, const GAResult *b)
{
    return a->solved == b->solved && a->solutionGeneration == b->solutionGeneration &&
           a->generationsRun == b->generationsRun && a->bestFitness == b->bestFitness &&
           memcmp(a->bestGenes, b->bestGenes, (size_t)a->boardSize * sizeof(int)) == 0 &&
           a->counters.fitnessEvaluations == b->counters.fitnessEvaluations &&
           a->counters.crossoverCount == b->counters.crossoverCount &&
           a->counters.mutationCount == b->counters.mutationCount &&
           a->counters.migrantsSent == b->counters.migrantsSent;
}

static void test_end_to_end(void)
{
    /* Every strategy combination solves N=8 and the solution is verified independently. */
    for (int ps = 0; ps < PARENT_SELECTION_COUNT_; ++ps) {
        for (int cx = 0; cx < CROSSOVER_COUNT_; ++cx) {
            for (int topo = 0; topo < TOPOLOGY_COUNT_; ++topo) {
                for (int ms = 0; ms < MIGRANT_SELECTION_COUNT_; ++ms) {
                    GAConfig cfg = small_config();
                    cfg.parentSelectionMethod = (ParentSelectionMethod)ps;
                    cfg.crossoverMethod = (CrossoverMethod)cx;
                    cfg.migrationTopology = (MigrationTopology)topo;
                    cfg.migrantSelectionMethod = (MigrantSelectionMethod)ms;
                    cfg.migrationReplacementMethod = (MigrationReplacementMethod)(ms % 2);
                    cfg.xorSwapProbabilityMethod = (XorSwapProbabilityMethod)(topo % 2);
                    cfg.migrationInterval = 5;
                    CHECK(config_validate(&cfg, NULL, 0) == NQ_OK);
                    GAResult res1, res2;
                    CHECK(ga_run(&cfg, NULL, 1, &res1) == NQ_OK);
                    CHECK(ga_run(&cfg, NULL, 1, &res2) == NQ_OK);
                    CHECK(res1.solved);
                    CHECK(fitness_reference(res1.bestGenes, cfg.boardSize, NULL) == res1.bestFitness);
                    CHECK(same_result(&res1, &res2)); /* deterministic replay */
                    ga_result_free(&res1);
                    ga_result_free(&res2);
                }
            }
        }
    }

    /* N=1 is solved at generation 0; N=2,3 stop at maxGenerations. */
    for (int n = 1; n <= 3; ++n) {
        GAConfig cfg = small_config();
        cfg.boardSize = n;
        cfg.crossoverPoints = 1;
        cfg.maxGenerations = 30;
        cfg.migrationInterval = 3;
        CHECK(config_validate(&cfg, NULL, 0) == NQ_OK);
        GAResult res;
        CHECK(ga_run(&cfg, NULL, 1, &res) == NQ_OK);
        if (n == 1) {
            CHECK(res.solved && res.solutionGeneration == 0);
        } else {
            CHECK(!res.solved && res.generationsRun == 30 && res.bestFitness > 0);
        }
        ga_result_free(&res);
    }

    /* Different seeds should (very likely) give different trajectories. */
    GAConfig cfg = small_config();
    cfg.boardSize = 16;
    cfg.crossoverPoints = 2;
    GAResult a, b;
    CHECK(ga_run(&cfg, NULL, 1, &a) == NQ_OK);
    cfg.randomSeed = 43;
    CHECK(ga_run(&cfg, NULL, 1, &b) == NQ_OK);
    CHECK(!same_result(&a, &b));
    ga_result_free(&a);
    ga_result_free(&b);
}

/* ---------------- configuration validation ---------------- */
static void test_config_validation(void)
{
    GAConfig cfg;
    config_set_defaults(&cfg);
    CHECK(config_validate(&cfg, NULL, 0) == NQ_OK);

    GAConfig bad = cfg;
    bad.boardSize = 0;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.islandPopulation = 1;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.eliteCount = bad.islandPopulation;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.crossoverRate = 1.5;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.crossoverPoints = bad.boardSize;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.migrationCount = bad.islandPopulation - bad.eliteCount + 1;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);
    bad = cfg;
    bad.rankSelectionPressure = 2.5;
    CHECK(config_validate(&bad, NULL, 0) != NQ_OK);

    char msg[128];
    RunOptions opts;
    run_options_set_defaults(&opts);
    char *argv1[] = {"prog", "--n=20", "--crossover", "xor", "--topology=full", "--seed=7"};
    CHECK(config_parse_args(6, argv1, &cfg, &opts, msg, sizeof(msg)) == NQ_OK);
    CHECK(cfg.boardSize == 20 && cfg.crossoverMethod == CROSSOVER_XOR &&
          cfg.migrationTopology == TOPOLOGY_FULLY_CONNECTED && cfg.randomSeed == 7);
    char *argv2[] = {"prog", "--crossover=bogus"};
    CHECK(config_parse_args(2, argv2, &cfg, &opts, msg, sizeof(msg)) != NQ_OK);
    char *argv3[] = {"prog", "--unknown=1"};
    CHECK(config_parse_args(2, argv3, &cfg, &opts, msg, sizeof(msg)) != NQ_OK);
}

int main(void)
{
    test_small_boards();
    test_random_evaluation();
    test_crossover();
    test_mutation();
    test_parent_selection();
    test_generation_step();
    test_migration();
    test_end_to_end();
    test_config_validation();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

