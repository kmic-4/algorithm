/*
 * N-Queens solver using an island model genetic algorithm.
 * Specification: NQueens_IslandGA_Implementation_Spec.md
 *
 * Exit codes: 0 = solution found, 1 = not found within maxGenerations,
 *             2 = invalid configuration, 3 = allocation / I/O error.
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "ga.h"
#include "stats.h"

enum { EXIT_SOLVED = 0, EXIT_NOT_FOUND = 1, EXIT_BAD_CONFIG = 2, EXIT_RUNTIME_ERROR = 3 };

static void current_timestamp(char *buf, size_t size)
{
    const time_t now = time(NULL);
    struct tm tmValue;
    const struct tm *tmPtr = gmtime(&now); /* portable C11 (not thread-safe; single thread) */
    if (tmPtr == NULL) {
        snprintf(buf, size, "unknown");
        return;
    }
    tmValue = *tmPtr;
    strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &tmValue);
}

int main(int argc, char **argv)
{
    GAConfig cfg;
    RunOptions opts;
    char message[256];
    config_set_defaults(&cfg);
    run_options_set_defaults(&opts);

    if (config_parse_args(argc, argv, &cfg, &opts, message, sizeof(message)) != NQ_OK) {
        fprintf(stderr, "error: %s\n", message);
        config_print_usage(stderr, argv[0]);
        return EXIT_BAD_CONFIG;
    }
    if (opts.showHelp) {
        config_print_usage(stdout, argv[0]);
        return EXIT_SOLVED;
    }
    if (config_validate(&cfg, message, sizeof(message)) != NQ_OK) {
        fprintf(stderr, "error: invalid configuration: %s\n", message);
        return EXIT_BAD_CONFIG;
    }

    char timestamp[64];
    current_timestamp(timestamp, sizeof(timestamp));
    char generatedId[96];
    if (opts.experimentId == NULL) {
        snprintf(generatedId, sizeof(generatedId), "nq%d-seed%" PRIu64 "-%s", cfg.boardSize,
                 cfg.randomSeed, timestamp);
        opts.experimentId = generatedId;
    }

    FILE *genLog = NULL;
    if (opts.generationLogPath != NULL) {
        genLog = fopen(opts.generationLogPath, "w");
        if (genLog == NULL) {
            fprintf(stderr, "error: cannot open %s\n", opts.generationLogPath);
            return EXIT_RUNTIME_ERROR;
        }
        stats_log_generation_header(genLog);
    }

    stats_print_config(stdout, &cfg, opts.experimentId, timestamp);

    GAResult result;
    const NQStatus status = ga_run(&cfg, genLog, opts.logInterval, &result);
    if (genLog != NULL) {
        fclose(genLog);
    }
    if (status != NQ_OK) {
        fprintf(stderr, "error: GA run failed (status %d)\n", (int)status);
        return (status == NQ_ERR_INVALID_CONFIG) ? EXIT_BAD_CONFIG : EXIT_RUNTIME_ERROR;
    }

    stats_print_result(stdout, &result);

    int exitCode = result.solved ? EXIT_SOLVED : EXIT_NOT_FOUND;
    if (opts.summaryCsvPath != NULL) {
        /* Write the header only when the file is missing or empty. */
        bool needHeader = true;
        FILE *probe = fopen(opts.summaryCsvPath, "r");
        if (probe != NULL) {
            needHeader = (fgetc(probe) == EOF);
            fclose(probe);
        }
        FILE *summary = fopen(opts.summaryCsvPath, "a");
        if (summary == NULL) {
            fprintf(stderr, "error: cannot open %s\n", opts.summaryCsvPath);
            exitCode = EXIT_RUNTIME_ERROR;
        } else {
            if (needHeader) {
                stats_summary_csv_header(summary);
            }
            stats_summary_csv_row(summary, &cfg, &result, opts.experimentId, timestamp);
            fclose(summary);
        }
    }

    ga_result_free(&result);
    return exitCode;
}
