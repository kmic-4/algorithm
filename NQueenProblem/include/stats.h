#ifndef NQ_STATS_H
#define NQ_STATS_H

#include <stdio.h>

#include "definition.h"

/* Generation log (CSV): generation,island,best_fitness,mean_fitness */
void stats_log_generation_header(FILE *out);
void stats_log_generation(FILE *out, const Generation *gen);

/* Human readable key=value dump of every effective parameter (spec §14.2). */
void stats_print_config(FILE *out, const GAConfig *cfg, const char *experimentId,
                        const char *timestamp);

/* key=value dump of the run result, counters and best individual. */
void stats_print_result(FILE *out, const GAResult *result);

/* One-line CSV summary for multi-seed experiments. */
void stats_summary_csv_header(FILE *out);
void stats_summary_csv_row(FILE *out, const GAConfig *cfg, const GAResult *result,
                           const char *experimentId, const char *timestamp);

#endif /* NQ_STATS_H */

