#ifndef NQ_DEFINITION_H
#define NQ_DEFINITION_H

/*
 * Central definitions: types, tunable parameters and their defaults.
 * Algorithm specification: NQueens_IslandGA_Implementation_Spec.md
 *
 * All tunable values live in GAConfig. Defaults below are experimental
 * placeholders (spec §4.2), not tuned optimal values.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rng.h"

/* ------------------------------------------------------------------ */
/* Default parameter values                                            */
/* ------------------------------------------------------------------ */
#define DEFAULT_BOARD_SIZE                 32
#define DEFAULT_ISLAND_COUNT               4
#define DEFAULT_ISLAND_POPULATION          100
#define DEFAULT_MAX_GENERATIONS            10000
#define DEFAULT_CROSSOVER_RATE             0.9
#define DEFAULT_MUTATION_RATE              0.3
#define DEFAULT_ELITE_COUNT                2
#define DEFAULT_MIGRATION_INTERVAL         50
#define DEFAULT_MIGRATION_COUNT            2
#define DEFAULT_PARENT_TOURNAMENT_SIZE     3
#define DEFAULT_MIGRATION_TOURNAMENT_SIZE  3
#define DEFAULT_REPLACEMENT_TOURNAMENT_SIZE 3 /* reserved for optional §9.2 modules */
#define DEFAULT_RANK_SELECTION_PRESSURE    1.5
#define DEFAULT_ROULETTE_SCALING           1.0
#define DEFAULT_CROSSOVER_POINTS           2
#define DEFAULT_XOR_SWAP_RATE              0.5
#define DEFAULT_XOR_FORCE_SWAP             true
#define DEFAULT_RANDOM_SEED                1u
#define DEFAULT_ALLOW_SAME_PARENT_PAIR     true /* [暫定] spec §6 */

/* Fitness is at most N(N-1)/2 and must fit into int. */
#define NQ_MAX_BOARD_SIZE                  46340

/* Re-draw limit for the "avoid identical parents" option (spec §6). */
#define NQ_SAME_PARENT_RETRY_LIMIT         64

/* ------------------------------------------------------------------ */
/* Status codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    NQ_OK = 0,
    NQ_ERR_INVALID_CONFIG = 1,
    NQ_ERR_ALLOC = 2,
    NQ_ERR_IO = 3
} NQStatus;

/* ------------------------------------------------------------------ */
/* Strategy switches                                                   */
/* ------------------------------------------------------------------ */
typedef enum {
    PARENT_SELECTION_TOURNAMENT = 0,
    PARENT_SELECTION_ROULETTE,
    PARENT_SELECTION_RANK,
    PARENT_SELECTION_COUNT_
} ParentSelectionMethod;

typedef enum {
    CROSSOVER_NPOINT = 0,
    CROSSOVER_XOR,
    CROSSOVER_COUNT_
} CrossoverMethod;

typedef enum {
    XOR_PROB_FIXED = 0,
    XOR_PROB_CONFLICT_PROPORTIONAL,
    XOR_PROB_COUNT_
} XorSwapProbabilityMethod;

/* [暫定] spec §7.2: which positions are XOR swap candidates (genes must differ). */
typedef enum {
    XOR_CANDIDATE_OR = 0,  /* conflictsA[i] > 0 || conflictsB[i] > 0 (default) */
    XOR_CANDIDATE_AND,     /* conflictsA[i] > 0 && conflictsB[i] > 0 */
    XOR_CANDIDATE_COUNT_
} XorCandidateRule;

typedef enum {
    TOPOLOGY_RING = 0,
    TOPOLOGY_FULLY_CONNECTED,
    TOPOLOGY_RANDOM,
    TOPOLOGY_COUNT_
} MigrationTopology;

typedef enum {
    MIGRANT_SELECTION_BEST = 0,
    MIGRANT_SELECTION_RANDOM,
    MIGRANT_SELECTION_TOURNAMENT,
    MIGRANT_SELECTION_DIVERSITY,
    MIGRANT_SELECTION_COUNT_
} MigrantSelectionMethod;

typedef enum {
    MIGRATION_REPLACE_WORST = 0,
    MIGRATION_REPLACE_RANDOM,
    MIGRATION_REPLACE_COUNT_
} MigrationReplacementMethod;

/* Only the elitist generational GA (spec §9.1) is required in the first version. */
typedef enum {
    GENERATION_REPLACE_ELITIST = 0,
    GENERATION_REPLACE_COUNT_
} GenerationReplacementMethod;

/* ------------------------------------------------------------------ */
/* Configuration (treated as constant during a run)                    */
/* ------------------------------------------------------------------ */
typedef struct {
    int boardSize;                  /* N */
    int islandCount;
    int islandPopulation;           /* individuals per island */
    int maxGenerations;
    double crossoverRate;           /* probability per parent pair */
    double mutationRate;            /* probability per child (fitness > 0) */
    int eliteCount;                 /* per island */
    int migrationInterval;          /* 0 disables migration */
    int migrationCount;             /* total copies sent by one island per event */
    int parentTournamentSize;
    int migrationTournamentSize;
    int replacementTournamentSize;  /* reserved (spec §9.2 optional modules) */
    double rankSelectionPressure;   /* linear ranking, in [1, 2] */
    double rouletteScaling;         /* w = (1/(1+F))^rouletteScaling */
    int crossoverPoints;            /* n for n-point crossover */
    double xorSwapRate;             /* base swap probability */
    bool xorForceSwap;              /* guarantee at least one swap */
    uint64_t randomSeed;
    bool allowSameParentPair;
    ParentSelectionMethod parentSelectionMethod;
    CrossoverMethod crossoverMethod;
    XorSwapProbabilityMethod xorSwapProbabilityMethod;
    XorCandidateRule xorCandidateRule;
    MigrationTopology migrationTopology;
    MigrantSelectionMethod migrantSelectionMethod;
    MigrationReplacementMethod migrationReplacementMethod;
    GenerationReplacementMethod generationReplacementMethod;
} GAConfig;

/* ------------------------------------------------------------------ */
/* Individuals and populations                                         */
/* ------------------------------------------------------------------ */

/*
 * genes / queenConflicts point into storage owned by the Island
 * (or by a workspace); an Individual never owns its arrays.
 * Copy individuals with individual_copy() (deep copy), never by
 * struct assignment, which would alias the arrays.
 */
typedef struct {
    int *genes;              /* length N: genes[row] = column */
    int *queenConflicts;     /* length N */
    int fitnessScore;        /* number of conflicting pairs (0 = solution) */
    bool evaluationValid;    /* evaluation matches the current genes */
} Individual;

typedef struct {
    int fitness;
    int index;
} FitnessIndexPair;

typedef struct {
    uint64_t fitnessEvaluations;
    uint64_t crossoverCount;      /* crossovers executed */
    uint64_t changedChildren;     /* adopted children whose genes differ from their parent */
    uint64_t mutationCount;
    uint64_t xorForcedSwaps;      /* XOR crossovers where the forced swap was applied */
    uint64_t migrationEvents;
    uint64_t migrantsSent;
    uint64_t migrantsDropped;     /* copies that could not be placed (capacity) */
} GACounters;

/* Occupancy arrays for the O(N) evaluation; reused per island. */
typedef struct {
    int *columnCount;    /* N */
    int *diagDownCount;  /* 2N-1, index i - x + N - 1 */
    int *diagUpCount;    /* 2N-1, index i + x */
    uint64_t evaluationCount;
} FitnessWorkspace;

/* Per-generation cache for parent selection (rebuilt when fitness values change). */
typedef struct {
    int *sortedIndices;          /* P, best -> worst (ties by index) */
    FitnessIndexPair *pairs;     /* P, sort scratch */
    double *cumulativeWeights;   /* P, roulette / rank */
    double totalWeight;
} SelectionCache;

typedef struct {
    int *cutCandidates;          /* N-1 */
    unsigned char *cutMarks;     /* N, all zero between calls */
    int *swapCandidates;         /* N */
} CrossoverWorkspace;

typedef struct {
    FitnessWorkspace fitness;
    SelectionCache selection;
    CrossoverWorkspace crossover;
    int *rowBuffer;              /* N, mutation candidates */
    Individual spareChild;       /* discarded second child when P-E is odd */
    int *spareStorage;
} IslandWorkspace;

typedef struct {
    Individual *individuals;     /* current generation, P entries */
    Individual *nextIndividuals; /* next generation buffer, P entries */
    int *storage;                /* backs individuals (P * 2N ints) */
    int *nextStorage;            /* backs nextIndividuals */
    Rng rng;
    IslandWorkspace ws;
    GACounters counters;
    int bestIndex;
    int bestFitness;
    double meanFitness;
} Island;

typedef struct {
    Island *islands;
    int islandCount;
    int generationIndex;
} Generation;

/* ------------------------------------------------------------------ */
/* Migration                                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    int source;
    int receiver;
} MigrationRoute;

typedef struct {
    int *orders;                 /* I * P, pre-event sort order of each island */
    FitnessIndexPair *pairs;     /* P */
    int *receiveCapacity;        /* I */
    int *islandScratch;          /* I */
    MigrationRoute *routes;      /* I * m */
    int *slotReceivers;          /* m */
    int *selected;               /* m */
    int *candidates;             /* P */
    unsigned char *chosen;       /* P */
    int *nnDistance;             /* I * P, diversity cache */
    unsigned char *nnReady;      /* I */
    int *routeIndices;           /* I * m */
    int *replaceSlots;           /* P */
    int *replaceScratch;         /* P */
    Individual *snapshot;        /* I * m migrant copies */
    int *snapshotStorage;
    int capacityRoutes;          /* I * m */
} MigrationWorkspace;

/* ------------------------------------------------------------------ */
/* Run result                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    bool solved;
    int solutionGeneration;      /* -1 if not solved */
    int solutionIsland;          /* -1 if not solved */
    int generationsRun;
    int bestFitness;
    int boardSize;
    int *bestGenes;              /* owned, free with ga_result_free() */
    GACounters counters;
    double elapsedSeconds;       /* wall clock */
} GAResult;

#endif /* NQ_DEFINITION_H */

