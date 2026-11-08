#include "config.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *const PARENT_NAMES[] = {"tournament", "roulette", "rank"};
static const char *const CROSSOVER_NAMES[] = {"npoint", "xor"};
static const char *const XOR_PROB_NAMES[] = {"fixed", "conflict"};
static const char *const XOR_CAND_NAMES[] = {"or", "and"};
static const char *const TOPOLOGY_NAMES[] = {"ring", "full", "random"};
static const char *const MIGRANT_NAMES[] = {"best", "random", "tournament", "diversity"};
static const char *const REPLACE_NAMES[] = {"worst", "random"};
static const char *const GENERATION_NAMES[] = {"elitist"};

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *name_of(const char *const *names, int count, int value)
{
    return (value >= 0 && value < count) ? names[value] : "invalid";
}

const char *parent_selection_name(ParentSelectionMethod m)
{
    return name_of(PARENT_NAMES, COUNT_OF(PARENT_NAMES), (int)m);
}
const char *crossover_name(CrossoverMethod m)
{
    return name_of(CROSSOVER_NAMES, COUNT_OF(CROSSOVER_NAMES), (int)m);
}
const char *xor_probability_name(XorSwapProbabilityMethod m)
{
    return name_of(XOR_PROB_NAMES, COUNT_OF(XOR_PROB_NAMES), (int)m);
}
const char *xor_candidate_name(XorCandidateRule m)
{
    return name_of(XOR_CAND_NAMES, COUNT_OF(XOR_CAND_NAMES), (int)m);
}
const char *topology_name(MigrationTopology m)
{
    return name_of(TOPOLOGY_NAMES, COUNT_OF(TOPOLOGY_NAMES), (int)m);
}
const char *migrant_selection_name(MigrantSelectionMethod m)
{
    return name_of(MIGRANT_NAMES, COUNT_OF(MIGRANT_NAMES), (int)m);
}
const char *migration_replacement_name(MigrationReplacementMethod m)
{
    return name_of(REPLACE_NAMES, COUNT_OF(REPLACE_NAMES), (int)m);
}
const char *generation_replacement_name(GenerationReplacementMethod m)
{
    return name_of(GENERATION_NAMES, COUNT_OF(GENERATION_NAMES), (int)m);
}

void config_set_defaults(GAConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->boardSize = DEFAULT_BOARD_SIZE;
    cfg->islandCount = DEFAULT_ISLAND_COUNT;
    cfg->islandPopulation = DEFAULT_ISLAND_POPULATION;
    cfg->maxGenerations = DEFAULT_MAX_GENERATIONS;
    cfg->crossoverRate = DEFAULT_CROSSOVER_RATE;
    cfg->mutationRate = DEFAULT_MUTATION_RATE;
    cfg->eliteCount = DEFAULT_ELITE_COUNT;
    cfg->migrationInterval = DEFAULT_MIGRATION_INTERVAL;
    cfg->migrationCount = DEFAULT_MIGRATION_COUNT;
    cfg->parentTournamentSize = DEFAULT_PARENT_TOURNAMENT_SIZE;
    cfg->migrationTournamentSize = DEFAULT_MIGRATION_TOURNAMENT_SIZE;
    cfg->replacementTournamentSize = DEFAULT_REPLACEMENT_TOURNAMENT_SIZE;
    cfg->rankSelectionPressure = DEFAULT_RANK_SELECTION_PRESSURE;
    cfg->rouletteScaling = DEFAULT_ROULETTE_SCALING;
    cfg->crossoverPoints = DEFAULT_CROSSOVER_POINTS;
    cfg->xorSwapRate = DEFAULT_XOR_SWAP_RATE;
    cfg->xorForceSwap = DEFAULT_XOR_FORCE_SWAP;
    cfg->randomSeed = DEFAULT_RANDOM_SEED;
    cfg->allowSameParentPair = DEFAULT_ALLOW_SAME_PARENT_PAIR;
    cfg->parentSelectionMethod = PARENT_SELECTION_TOURNAMENT;
    cfg->crossoverMethod = CROSSOVER_NPOINT;
    cfg->xorSwapProbabilityMethod = XOR_PROB_FIXED;
    cfg->xorCandidateRule = XOR_CANDIDATE_OR;
    cfg->migrationTopology = TOPOLOGY_RING;
    cfg->migrantSelectionMethod = MIGRANT_SELECTION_BEST;
    cfg->migrationReplacementMethod = MIGRATION_REPLACE_WORST;
    cfg->generationReplacementMethod = GENERATION_REPLACE_ELITIST;
}

void run_options_set_defaults(RunOptions *opts)
{
    opts->generationLogPath = NULL;
    opts->summaryCsvPath = NULL;
    opts->experimentId = NULL;
    opts->logInterval = 1;
    opts->showHelp = false;
}

static bool is_probability(double p)
{
    return isfinite(p) && p >= 0.0 && p <= 1.0;
}

#define FAIL(...)                                                                              \
    do {                                                                                       \
        if (message != NULL && messageSize > 0) {                                              \
            snprintf(message, messageSize, __VA_ARGS__);                                       \
        }                                                                                      \
        return NQ_ERR_INVALID_CONFIG;                                                          \
    } while (0)

NQStatus config_validate(const GAConfig *cfg, char *message, size_t messageSize)
{
    if (cfg->boardSize < 1 || cfg->boardSize > NQ_MAX_BOARD_SIZE) {
        FAIL("board size must be in [1, %d]", NQ_MAX_BOARD_SIZE);
    }
    if (cfg->islandCount < 1) {
        FAIL("island count must be >= 1");
    }
    if (cfg->islandPopulation < 2) {
        FAIL("island population must be >= 2");
    }
    if (cfg->maxGenerations < 0) {
        FAIL("max generations must be >= 0");
    }
    if (!is_probability(cfg->crossoverRate)) {
        FAIL("crossover rate must be in [0, 1]");
    }
    if (!is_probability(cfg->mutationRate)) {
        FAIL("mutation rate must be in [0, 1]");
    }
    if (cfg->eliteCount < 0 || cfg->eliteCount >= cfg->islandPopulation) {
        FAIL("elite count must satisfy 0 <= eliteCount < islandPopulation");
    }
    if (cfg->migrationInterval < 0) {
        FAIL("migration interval must be >= 0 (0 disables migration)");
    }
    if (cfg->migrationCount < 0) {
        FAIL("migration count must be >= 0");
    }
    if (cfg->migrationCount > cfg->islandPopulation - cfg->eliteCount) {
        FAIL("migration count (%d) exceeds receive capacity islandPopulation - eliteCount (%d)",
             cfg->migrationCount, cfg->islandPopulation - cfg->eliteCount);
    }
    if (cfg->parentTournamentSize < 1 || cfg->migrationTournamentSize < 1 ||
        cfg->replacementTournamentSize < 1) {
        FAIL("tournament sizes must be >= 1");
    }
    if (!isfinite(cfg->rankSelectionPressure) || cfg->rankSelectionPressure < 1.0 ||
        cfg->rankSelectionPressure > 2.0) {
        FAIL("rank selection pressure must be in [1, 2]");
    }
    if (!isfinite(cfg->rouletteScaling) || cfg->rouletteScaling < 0.0) {
        FAIL("roulette scaling must be a finite value >= 0");
    }
    if (cfg->boardSize >= 2 &&
        (cfg->crossoverPoints < 1 || cfg->crossoverPoints > cfg->boardSize - 1)) {
        FAIL("crossover points must be in [1, N-1]");
    }
    if (!is_probability(cfg->xorSwapRate)) {
        FAIL("xor swap rate must be in [0, 1]");
    }
    if ((int)cfg->parentSelectionMethod < 0 ||
        cfg->parentSelectionMethod >= PARENT_SELECTION_COUNT_ ||
        (int)cfg->crossoverMethod < 0 || cfg->crossoverMethod >= CROSSOVER_COUNT_ ||
        (int)cfg->xorSwapProbabilityMethod < 0 ||
        cfg->xorSwapProbabilityMethod >= XOR_PROB_COUNT_ ||
        (int)cfg->xorCandidateRule < 0 || cfg->xorCandidateRule >= XOR_CANDIDATE_COUNT_ ||
        (int)cfg->migrationTopology < 0 || cfg->migrationTopology >= TOPOLOGY_COUNT_ ||
        (int)cfg->migrantSelectionMethod < 0 ||
        cfg->migrantSelectionMethod >= MIGRANT_SELECTION_COUNT_ ||
        (int)cfg->migrationReplacementMethod < 0 ||
        cfg->migrationReplacementMethod >= MIGRATION_REPLACE_COUNT_ ||
        (int)cfg->generationReplacementMethod < 0 ||
        cfg->generationReplacementMethod >= GENERATION_REPLACE_COUNT_) {
        FAIL("invalid strategy selection");
    }
    return NQ_OK;
}

static bool parse_int(const char *s, int *out)
{
    char *end = NULL;
    errno = 0;
    const long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) {
        return false;
    }
    *out = (int)v;
    return true;
}

static bool parse_double(const char *s, double *out)
{
    char *end = NULL;
    errno = 0;
    const double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_u64(const char *s, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    if (s[0] == '-') {
        return false;
    }
    const unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return false;
    }
    *out = (uint64_t)v;
    return true;
}

static bool parse_bool(const char *s, bool *out)
{
    if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 || strcmp(s, "no") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool parse_enum(const char *s, const char *const *names, int count, int *out)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(s, names[i]) == 0) {
            *out = i;
            return true;
        }
    }
    return false;
}

typedef enum { KIND_INT, KIND_DOUBLE, KIND_BOOL, KIND_U64, KIND_ENUM, KIND_STRING } OptionKind;

typedef struct {
    const char *name;
    OptionKind kind;
    void *target;
    const char *const *names; /* KIND_ENUM */
    int nameCount;
} OptionSpec;

NQStatus config_parse_args(int argc, char **argv, GAConfig *cfg, RunOptions *opts,
                           char *message, size_t messageSize)
{
    int parent = (int)cfg->parentSelectionMethod;
    int crossover = (int)cfg->crossoverMethod;
    int xorProb = (int)cfg->xorSwapProbabilityMethod;
    int xorCand = (int)cfg->xorCandidateRule;
    int topology = (int)cfg->migrationTopology;
    int migrant = (int)cfg->migrantSelectionMethod;
    int replace = (int)cfg->migrationReplacementMethod;
    int generation = (int)cfg->generationReplacementMethod;

    const OptionSpec specs[] = {
        {"board-size", KIND_INT, &cfg->boardSize, NULL, 0},
        {"n", KIND_INT, &cfg->boardSize, NULL, 0},
        {"islands", KIND_INT, &cfg->islandCount, NULL, 0},
        {"population", KIND_INT, &cfg->islandPopulation, NULL, 0},
        {"generations", KIND_INT, &cfg->maxGenerations, NULL, 0},
        {"crossover-rate", KIND_DOUBLE, &cfg->crossoverRate, NULL, 0},
        {"mutation-rate", KIND_DOUBLE, &cfg->mutationRate, NULL, 0},
        {"elite", KIND_INT, &cfg->eliteCount, NULL, 0},
        {"migration-interval", KIND_INT, &cfg->migrationInterval, NULL, 0},
        {"migration-count", KIND_INT, &cfg->migrationCount, NULL, 0},
        {"parent-tournament", KIND_INT, &cfg->parentTournamentSize, NULL, 0},
        {"migration-tournament", KIND_INT, &cfg->migrationTournamentSize, NULL, 0},
        {"replacement-tournament", KIND_INT, &cfg->replacementTournamentSize, NULL, 0},
        {"rank-pressure", KIND_DOUBLE, &cfg->rankSelectionPressure, NULL, 0},
        {"roulette-scaling", KIND_DOUBLE, &cfg->rouletteScaling, NULL, 0},
        {"crossover-points", KIND_INT, &cfg->crossoverPoints, NULL, 0},
        {"xor-swap-rate", KIND_DOUBLE, &cfg->xorSwapRate, NULL, 0},
        {"xor-force-swap", KIND_BOOL, &cfg->xorForceSwap, NULL, 0},
        {"seed", KIND_U64, &cfg->randomSeed, NULL, 0},
        {"allow-same-parent", KIND_BOOL, &cfg->allowSameParentPair, NULL, 0},
        {"parent-selection", KIND_ENUM, &parent, PARENT_NAMES, COUNT_OF(PARENT_NAMES)},
        {"crossover", KIND_ENUM, &crossover, CROSSOVER_NAMES, COUNT_OF(CROSSOVER_NAMES)},
        {"xor-probability", KIND_ENUM, &xorProb, XOR_PROB_NAMES, COUNT_OF(XOR_PROB_NAMES)},
        {"xor-candidate", KIND_ENUM, &xorCand, XOR_CAND_NAMES, COUNT_OF(XOR_CAND_NAMES)},
        {"topology", KIND_ENUM, &topology, TOPOLOGY_NAMES, COUNT_OF(TOPOLOGY_NAMES)},
        {"migrant-selection", KIND_ENUM, &migrant, MIGRANT_NAMES, COUNT_OF(MIGRANT_NAMES)},
        {"migration-replacement", KIND_ENUM, &replace, REPLACE_NAMES, COUNT_OF(REPLACE_NAMES)},
        {"generation-replacement", KIND_ENUM, &generation, GENERATION_NAMES,
         COUNT_OF(GENERATION_NAMES)},
        {"generation-log", KIND_STRING, &opts->generationLogPath, NULL, 0},
        {"summary-csv", KIND_STRING, &opts->summaryCsvPath, NULL, 0},
        {"experiment-id", KIND_STRING, &opts->experimentId, NULL, 0},
        {"log-interval", KIND_INT, &opts->logInterval, NULL, 0},
    };
    const int specCount = COUNT_OF(specs);

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            opts->showHelp = true;
            continue;
        }
        if (strncmp(arg, "--", 2) != 0) {
            FAIL("unexpected argument: %s", arg);
        }
        const char *key = arg + 2;
        const char *eq = strchr(key, '=');
        size_t keyLen = (eq != NULL) ? (size_t)(eq - key) : strlen(key);
        const char *value = NULL;
        if (eq != NULL) {
            value = eq + 1;
        } else if (i + 1 < argc) {
            value = argv[++i];
        } else {
            FAIL("missing value for --%s", key);
        }

        const OptionSpec *spec = NULL;
        for (int s = 0; s < specCount; ++s) {
            if (strlen(specs[s].name) == keyLen && strncmp(specs[s].name, key, keyLen) == 0) {
                spec = &specs[s];
                break;
            }
        }
        if (spec == NULL) {
            FAIL("unknown option: %.*s", (int)keyLen, key);
        }

        bool ok = false;
        switch (spec->kind) {
        case KIND_INT:
            ok = parse_int(value, (int *)spec->target);
            break;
        case KIND_DOUBLE:
            ok = parse_double(value, (double *)spec->target);
            break;
        case KIND_BOOL:
            ok = parse_bool(value, (bool *)spec->target);
            break;
        case KIND_U64:
            ok = parse_u64(value, (uint64_t *)spec->target);
            break;
        case KIND_ENUM:
            ok = parse_enum(value, spec->names, spec->nameCount, (int *)spec->target);
            break;
        case KIND_STRING:
            *(const char **)spec->target = value;
            ok = true;
            break;
        }
        if (!ok) {
            FAIL("invalid value for --%s: %s", spec->name, value);
        }
    }

    cfg->parentSelectionMethod = (ParentSelectionMethod)parent;
    cfg->crossoverMethod = (CrossoverMethod)crossover;
    cfg->xorSwapProbabilityMethod = (XorSwapProbabilityMethod)xorProb;
    cfg->xorCandidateRule = (XorCandidateRule)xorCand;
    cfg->migrationTopology = (MigrationTopology)topology;
    cfg->migrantSelectionMethod = (MigrantSelectionMethod)migrant;
    cfg->migrationReplacementMethod = (MigrationReplacementMethod)replace;
    cfg->generationReplacementMethod = (GenerationReplacementMethod)generation;
    if (opts->logInterval < 1) {
        FAIL("log interval must be >= 1");
    }
    return NQ_OK;
}

void config_print_usage(FILE *out, const char *program)
{
    fprintf(out,
            "Usage: %s [--option=value ...]\n"
            "\n"
            "Problem / population:\n"
            "  --n, --board-size N        board size (default %d)\n"
            "  --islands I                island count (default %d)\n"
            "  --population P             individuals per island (default %d)\n"
            "  --generations G            max generations (default %d)\n"
            "  --elite E                  elites per island (default %d)\n"
            "  --seed S                   random seed (default %u)\n"
            "\n"
            "Operators:\n"
            "  --parent-selection M       tournament | roulette | rank\n"
            "  --parent-tournament K      parent tournament size (default %d)\n"
            "  --rank-pressure S          linear ranking pressure in [1,2] (default %.2f)\n"
            "  --roulette-scaling X       w = (1/(1+F))^X (default %.2f)\n"
            "  --allow-same-parent B      allow identical parents in a pair (default 1)\n"
            "  --crossover M              npoint | xor\n"
            "  --crossover-rate R         per parent pair (default %.2f)\n"
            "  --crossover-points K       n for n-point crossover (default %d)\n"
            "  --xor-probability M        fixed | conflict\n"
            "  --xor-candidate M          or | and\n"
            "  --xor-swap-rate R          base swap probability (default %.2f)\n"
            "  --xor-force-swap B         force >= 1 swap (default 1)\n"
            "  --mutation-rate R          per child (default %.2f)\n"
            "\n"
            "Migration:\n"
            "  --migration-interval G     generations between events, 0 = off (default %d)\n"
            "  --migration-count M        copies sent per island per event (default %d)\n"
            "  --topology M               ring | full | random\n"
            "  --migrant-selection M      best | random | tournament | diversity\n"
            "  --migration-tournament K   migrant tournament size (default %d)\n"
            "  --migration-replacement M  worst | random\n"
            "  --generation-replacement M elitist\n"
            "  --replacement-tournament K reserved for optional replacement modules\n"
            "\n"
            "Output:\n"
            "  --generation-log FILE      CSV: generation,island,best_fitness,mean_fitness\n"
            "  --log-interval K           log every K generations (default 1)\n"
            "  --summary-csv FILE         append one summary row per run\n"
            "  --experiment-id ID         experiment identifier\n",
            program, DEFAULT_BOARD_SIZE, DEFAULT_ISLAND_COUNT, DEFAULT_ISLAND_POPULATION,
            DEFAULT_MAX_GENERATIONS, DEFAULT_ELITE_COUNT, (unsigned)DEFAULT_RANDOM_SEED,
            DEFAULT_PARENT_TOURNAMENT_SIZE, DEFAULT_RANK_SELECTION_PRESSURE,
            DEFAULT_ROULETTE_SCALING, DEFAULT_CROSSOVER_RATE, DEFAULT_CROSSOVER_POINTS,
            DEFAULT_XOR_SWAP_RATE, DEFAULT_MUTATION_RATE, DEFAULT_MIGRATION_INTERVAL,
            DEFAULT_MIGRATION_COUNT, DEFAULT_MIGRATION_TOURNAMENT_SIZE);
}

