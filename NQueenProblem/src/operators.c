#include "operators.h"

NQStatus operators_resolve(const GAConfig *cfg, GAOperators *ops)
{
    switch (cfg->parentSelectionMethod) {
    case PARENT_SELECTION_TOURNAMENT:
        ops->prepareParentSelection = parent_prepare_none;
        ops->selectParent = parent_select_tournament;
        break;
    case PARENT_SELECTION_ROULETTE:
        ops->prepareParentSelection = parent_prepare_roulette;
        ops->selectParent = parent_select_roulette;
        break;
    case PARENT_SELECTION_RANK:
        ops->prepareParentSelection = parent_prepare_rank;
        ops->selectParent = parent_select_rank;
        break;
    default:
        return NQ_ERR_INVALID_CONFIG;
    }

    switch (cfg->crossoverMethod) {
    case CROSSOVER_NPOINT:
        ops->crossover = crossover_npoint;
        break;
    case CROSSOVER_XOR:
        ops->crossover = crossover_xor;
        break;
    default:
        return NQ_ERR_INVALID_CONFIG;
    }

    ops->mutate = mutation_conflict_guided;

    switch (cfg->migrationTopology) {
    case TOPOLOGY_RING:
        ops->planMigration = topology_ring;
        break;
    case TOPOLOGY_FULLY_CONNECTED:
        ops->planMigration = topology_fully_connected;
        break;
    case TOPOLOGY_RANDOM:
        ops->planMigration = topology_random;
        break;
    default:
        return NQ_ERR_INVALID_CONFIG;
    }

    switch (cfg->migrantSelectionMethod) {
    case MIGRANT_SELECTION_BEST:
        ops->selectMigrants = migrant_select_best;
        break;
    case MIGRANT_SELECTION_RANDOM:
        ops->selectMigrants = migrant_select_random;
        break;
    case MIGRANT_SELECTION_TOURNAMENT:
        ops->selectMigrants = migrant_select_tournament;
        break;
    case MIGRANT_SELECTION_DIVERSITY:
        ops->selectMigrants = migrant_select_diversity;
        break;
    default:
        return NQ_ERR_INVALID_CONFIG;
    }

    switch (cfg->migrationReplacementMethod) {
    case MIGRATION_REPLACE_WORST:
        ops->replaceMigrants = replace_worst;
        break;
    case MIGRATION_REPLACE_RANDOM:
        ops->replaceMigrants = replace_random;
        break;
    default:
        return NQ_ERR_INVALID_CONFIG;
    }

    if (cfg->generationReplacementMethod != GENERATION_REPLACE_ELITIST) {
        return NQ_ERR_INVALID_CONFIG;
    }
    return NQ_OK;
}

