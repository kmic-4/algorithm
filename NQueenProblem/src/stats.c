#include "stats.h"

#include <inttypes.h>

#include "config.h"

void stats_log_generation_header(FILE *out)
{
    fprintf(out, "generation,island,best_fitness,mean_fitness\n");
}

void stats_log_generation(FILE *out, const Generation *gen)
{
    for (int i = 0; i < gen->islandCount; ++i) {
        const Island *island = &gen->islands[i];
        fprintf(out, "%d,%d,%d,%.4f\n", gen->generationIndex, i, island->bestFitness,
                island->meanFitness);
    }
}

void stats_print_config(FILE *out, const GAConfig *cfg, const char *experimentId,
                        const char *timestamp)
{
    fprintf(out, "experiment_id=%s\n", experimentId);
    fprintf(out, "timestamp=%s\n", timestamp);
    fprintf(out, "random_seed=%" PRIu64 "\n", cfg->randomSeed);
    fprintf(out, "board_size=%d\n", cfg->boardSize);
    fprintf(out, "island_count=%d\n", cfg->islandCount);
    fprintf(out, "island_population=%d\n", cfg->islandPopulation);
    fprintf(out, "max_generations=%d\n", cfg->maxGenerations);
    fprintf(out, "elite_count=%d\n", cfg->eliteCount);
    fprintf(out, "parent_selection=%s\n", parent_selection_name(cfg->parentSelectionMethod));
    fprintf(out, "parent_tournament_size=%d\n", cfg->parentTournamentSize);
    fprintf(out, "rank_selection_pressure=%g\n", cfg->rankSelectionPressure);
    fprintf(out, "roulette_scaling=%g\n", cfg->rouletteScaling);
    fprintf(out, "allow_same_parent_pair=%d\n", cfg->allowSameParentPair ? 1 : 0);
    fprintf(out, "crossover=%s\n", crossover_name(cfg->crossoverMethod));
    fprintf(out, "crossover_rate=%g\n", cfg->crossoverRate);
    fprintf(out, "crossover_points=%d\n", cfg->crossoverPoints);
    fprintf(out, "xor_probability=%s\n", xor_probability_name(cfg->xorSwapProbabilityMethod));
    fprintf(out, "xor_candidate=%s\n", xor_candidate_name(cfg->xorCandidateRule));
    fprintf(out, "xor_swap_rate=%g\n", cfg->xorSwapRate);
    fprintf(out, "xor_force_swap=%d\n", cfg->xorForceSwap ? 1 : 0);
    if (cfg->xorForceSwap && cfg->crossoverMethod == CROSSOVER_XOR) {
        fprintf(out, "# note: xor_swap_rate is a base probability; forced swaps make the "
                     "effective swap rate differ (see xor_forced_swaps)\n");
    }
    fprintf(out, "mutation=conflict_guided_random_resetting\n");
    fprintf(out, "mutation_rate=%g\n", cfg->mutationRate);
    fprintf(out, "generation_replacement=%s\n",
            generation_replacement_name(cfg->generationReplacementMethod));
    fprintf(out, "migration_interval=%d\n", cfg->migrationInterval);
    fprintf(out, "migration_count=%d\n", cfg->migrationCount);
    fprintf(out, "migration_topology=%s\n", topology_name(cfg->migrationTopology));
    fprintf(out, "migrant_selection=%s\n", migrant_selection_name(cfg->migrantSelectionMethod));
    fprintf(out, "migration_tournament_size=%d\n", cfg->migrationTournamentSize);
    fprintf(out, "migration_replacement=%s\n",
            migration_replacement_name(cfg->migrationReplacementMethod));
    fprintf(out, "replacement_tournament_size=%d\n", cfg->replacementTournamentSize);
}

static void print_genes(FILE *out, const GAResult *result)
{
    fputc('[', out);
    for (int i = 0; i < result->boardSize; ++i) {
        fprintf(out, "%s%d", (i == 0) ? "" : " ", result->bestGenes[i]);
    }
    fputc(']', out);
}

void stats_print_result(FILE *out, const GAResult *result)
{
    const GACounters *c = &result->counters;
    fprintf(out, "solved=%d\n", result->solved ? 1 : 0);
    fprintf(out, "solution_generation=%d\n", result->solutionGeneration);
    fprintf(out, "solution_island=%d\n", result->solutionIsland);
    fprintf(out, "generations_run=%d\n", result->generationsRun);
    fprintf(out, "best_fitness=%d\n", result->bestFitness);
    fprintf(out, "elapsed_seconds=%.6f\n", result->elapsedSeconds);
    fprintf(out, "fitness_evaluations=%" PRIu64 "\n", c->fitnessEvaluations);
    fprintf(out, "crossovers=%" PRIu64 "\n", c->crossoverCount);
    fprintf(out, "changed_children=%" PRIu64 "\n", c->changedChildren);
    fprintf(out, "mutations=%" PRIu64 "\n", c->mutationCount);
    fprintf(out, "xor_forced_swaps=%" PRIu64 "\n", c->xorForcedSwaps);
    fprintf(out, "migration_events=%" PRIu64 "\n", c->migrationEvents);
    fprintf(out, "migrants_sent=%" PRIu64 "\n", c->migrantsSent);
    fprintf(out, "migrants_dropped=%" PRIu64 "\n", c->migrantsDropped);
    fprintf(out, "best_genes=");
    print_genes(out, result);
    fputc('\n', out);
}

void stats_summary_csv_header(FILE *out)
{
    fprintf(out,
            "experiment_id,timestamp,seed,board_size,island_count,island_population,"
            "max_generations,elite_count,parent_selection,crossover,crossover_rate,"
            "crossover_points,xor_probability,xor_candidate,xor_swap_rate,xor_force_swap,"
            "mutation_rate,migration_interval,migration_count,topology,migrant_selection,"
            "migration_replacement,solved,solution_generation,generations_run,best_fitness,"
            "elapsed_seconds,fitness_evaluations,crossovers,changed_children,mutations,"
            "xor_forced_swaps,migration_events,migrants_sent,migrants_dropped,best_genes\n");
}

void stats_summary_csv_row(FILE *out, const GAConfig *cfg, const GAResult *result,
                           const char *experimentId, const char *timestamp)
{
    const GACounters *c = &result->counters;
    fprintf(out, "%s,%s,%" PRIu64 ",%d,%d,%d,%d,%d,%s,%s,%g,%d,%s,%s,%g,%d,%g,%d,%d,%s,%s,%s,",
            experimentId, timestamp, cfg->randomSeed, cfg->boardSize, cfg->islandCount,
            cfg->islandPopulation, cfg->maxGenerations, cfg->eliteCount,
            parent_selection_name(cfg->parentSelectionMethod),
            crossover_name(cfg->crossoverMethod), cfg->crossoverRate, cfg->crossoverPoints,
            xor_probability_name(cfg->xorSwapProbabilityMethod),
            xor_candidate_name(cfg->xorCandidateRule), cfg->xorSwapRate,
            cfg->xorForceSwap ? 1 : 0, cfg->mutationRate, cfg->migrationInterval,
            cfg->migrationCount, topology_name(cfg->migrationTopology),
            migrant_selection_name(cfg->migrantSelectionMethod),
            migration_replacement_name(cfg->migrationReplacementMethod));
    fprintf(out,
            "%d,%d,%d,%d,%.6f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",",
            result->solved ? 1 : 0, result->solutionGeneration, result->generationsRun,
            result->bestFitness, result->elapsedSeconds, c->fitnessEvaluations,
            c->crossoverCount, c->changedChildren, c->mutationCount, c->xorForcedSwaps,
            c->migrationEvents, c->migrantsSent, c->migrantsDropped);
    print_genes(out, result);
    fputc('\n', out);
}

