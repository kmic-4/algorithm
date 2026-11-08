#ifndef NQ_CONFIG_H
#define NQ_CONFIG_H

#include <stdio.h>

#include "definition.h"

/* Options that control output only (they do not affect the algorithm). */
typedef struct {
    const char *generationLogPath;  /* CSV per generation / island, NULL = none */
    const char *summaryCsvPath;     /* one summary row appended per run, NULL = none */
    const char *experimentId;       /* NULL = generated */
    int logInterval;                /* generation log interval */
    bool showHelp;
} RunOptions;

void config_set_defaults(GAConfig *cfg);
void run_options_set_defaults(RunOptions *opts);

/* Validates cfg (spec §4.2). On failure writes a message and returns NQ_ERR_INVALID_CONFIG. */
NQStatus config_validate(const GAConfig *cfg, char *message, size_t messageSize);

/* Parses --key=value / --key value arguments into cfg and opts. */
NQStatus config_parse_args(int argc, char **argv, GAConfig *cfg, RunOptions *opts,
                           char *message, size_t messageSize);

void config_print_usage(FILE *out, const char *program);

const char *parent_selection_name(ParentSelectionMethod m);
const char *crossover_name(CrossoverMethod m);
const char *xor_probability_name(XorSwapProbabilityMethod m);
const char *xor_candidate_name(XorCandidateRule m);
const char *topology_name(MigrationTopology m);
const char *migrant_selection_name(MigrantSelectionMethod m);
const char *migration_replacement_name(MigrationReplacementMethod m);
const char *generation_replacement_name(GenerationReplacementMethod m);

#endif /* NQ_CONFIG_H */

