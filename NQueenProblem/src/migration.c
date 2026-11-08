#include "migration.h"

#include <stdlib.h>
#include <string.h>

#include "population.h"
#include "util.h"

NQStatus migration_workspace_init(MigrationWorkspace *ws, const GAConfig *cfg)
{
    memset(ws, 0, sizeof(*ws));
    const size_t islands = (size_t)cfg->islandCount;
    const size_t p = (size_t)cfg->islandPopulation;
    const size_t m = (size_t)cfg->migrationCount;
    const size_t n = (size_t)cfg->boardSize;
    int ok = 1;
    const size_t routes = nq_mul_size(islands, m, &ok);
    const size_t orderLen = nq_mul_size(islands, p, &ok);
    const size_t snapInts = nq_mul_size(routes, 2 * n, &ok);
    if (!ok || routes > (size_t)INT32_MAX) {
        return NQ_ERR_ALLOC;
    }
    ws->capacityRoutes = (int)routes;

    ws->orders = nq_calloc_array(orderLen, sizeof(int));
    ws->pairs = nq_calloc_array(p, sizeof(FitnessIndexPair));
    ws->receiveCapacity = nq_calloc_array(islands, sizeof(int));
    ws->islandScratch = nq_calloc_array(islands, sizeof(int));
    ws->routes = nq_calloc_array(routes, sizeof(MigrationRoute));
    ws->slotReceivers = nq_calloc_array(m, sizeof(int));
    ws->selected = nq_calloc_array(m, sizeof(int));
    ws->candidates = nq_calloc_array(p, sizeof(int));
    ws->chosen = nq_calloc_array(p, sizeof(unsigned char));
    ws->nnDistance = nq_calloc_array(orderLen, sizeof(int));
    ws->nnReady = nq_calloc_array(islands, sizeof(unsigned char));
    ws->routeIndices = nq_calloc_array(routes, sizeof(int));
    ws->replaceSlots = nq_calloc_array(p, sizeof(int));
    ws->replaceScratch = nq_calloc_array(p, sizeof(int));
    ws->snapshot = nq_calloc_array(routes, sizeof(Individual));
    ws->snapshotStorage = nq_calloc_array(snapInts, sizeof(int));
    if (ws->orders == NULL || ws->pairs == NULL || ws->receiveCapacity == NULL ||
        ws->islandScratch == NULL || ws->routes == NULL || ws->slotReceivers == NULL ||
        ws->selected == NULL || ws->candidates == NULL || ws->chosen == NULL ||
        ws->nnDistance == NULL || ws->nnReady == NULL || ws->routeIndices == NULL ||
        ws->replaceSlots == NULL || ws->replaceScratch == NULL || ws->snapshot == NULL ||
        ws->snapshotStorage == NULL) {
        migration_workspace_free(ws);
        return NQ_ERR_ALLOC;
    }
    for (size_t k = 0; k < routes; ++k) {
        individual_bind(&ws->snapshot[k], ws->snapshotStorage + k * 2 * n, (int)n);
    }
    return NQ_OK;
}

void migration_workspace_free(MigrationWorkspace *ws)
{
    free(ws->orders);
    free(ws->pairs);
    free(ws->receiveCapacity);
    free(ws->islandScratch);
    free(ws->routes);
    free(ws->slotReceivers);
    free(ws->selected);
    free(ws->candidates);
    free(ws->chosen);
    free(ws->nnDistance);
    free(ws->nnReady);
    free(ws->routeIndices);
    free(ws->replaceSlots);
    free(ws->replaceScratch);
    free(ws->snapshot);
    free(ws->snapshotStorage);
    memset(ws, 0, sizeof(*ws));
}

bool migration_is_due(const GAConfig *cfg, int generationIndex)
{
    return cfg->migrationInterval > 0 && cfg->migrationCount > 0 && cfg->islandCount > 1 &&
           generationIndex > 0 && generationIndex % cfg->migrationInterval == 0;
}

void migration_run(Generation *gen, const GAConfig *cfg, const GAOperators *ops,
                   MigrationWorkspace *ws, Rng *rng, GACounters *counters)
{
    const int islands = cfg->islandCount;
    const int p = cfg->islandPopulation;
    const int n = cfg->boardSize;
    const int protectedCount = cfg->eliteCount;

    /* 1. Pre-event sort orders (used for Best selection and Worst replacement). */
    for (int i = 0; i < islands; ++i) {
        population_sort_order(gen->islands[i].individuals, p, ws->pairs,
                              ws->orders + (size_t)i * (size_t)p);
        ws->receiveCapacity[i] = p - protectedCount;
    }

    /* 2. Routes, grouped by source in ascending order. */
    const int routeCount =
        ops->planMigration(cfg, ws->receiveCapacity, ws->routes, ws->islandScratch, rng);
    const int requested = islands * cfg->migrationCount;

    /* 3. Select migrants and snapshot deep copies before any replacement. */
    int r0 = 0;
    while (r0 < routeCount) {
        const int source = ws->routes[r0].source;
        int r1 = r0;
        while (r1 < routeCount && ws->routes[r1].source == source) {
            ws->slotReceivers[r1 - r0] = ws->routes[r1].receiver;
            ++r1;
        }
        MigrantSelectArgs args;
        args.islands = gen->islands;
        args.sourceIndex = source;
        args.sourceOrder = ws->orders + (size_t)source * (size_t)p;
        args.orders = ws->orders;
        args.slotReceivers = ws->slotReceivers;
        args.count = r1 - r0;
        args.cfg = cfg;
        args.ws = ws;
        ops->selectMigrants(&args, rng, ws->selected);
        for (int k = 0; k < args.count; ++k) {
            individual_copy(&ws->snapshot[r0 + k],
                            &gen->islands[source].individuals[ws->selected[k]], n);
        }
        r0 = r1;
    }

    /* 4. Replace receiver slots (distinct, non-protected) with the snapshots. */
    for (int r = 0; r < islands; ++r) {
        int incoming = 0;
        for (int k = 0; k < routeCount; ++k) {
            if (ws->routes[k].receiver == r) {
                ws->routeIndices[incoming++] = k;
            }
        }
        if (incoming == 0) {
            continue;
        }
        ops->replaceMigrants(ws->orders + (size_t)r * (size_t)p, p, protectedCount, incoming,
                             rng, ws->replaceScratch, ws->replaceSlots);
        Island *island = &gen->islands[r];
        for (int k = 0; k < incoming; ++k) {
            individual_copy(&island->individuals[ws->replaceSlots[k]],
                            &ws->snapshot[ws->routeIndices[k]], n);
        }
    }

    counters->migrationEvents++;
    counters->migrantsSent += (uint64_t)routeCount;
    counters->migrantsDropped += (uint64_t)(requested - routeCount);
}

