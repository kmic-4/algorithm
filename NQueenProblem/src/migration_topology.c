#include "operators.h"

/*
 * Migration topologies (spec §10.2).
 * [暫定] migrationCount is the total number of copies one source island sends
 * per event; Fully Connected distributes those copies over the other islands.
 */

int topology_ring(const GAConfig *cfg, int *receiveCapacity, MigrationRoute *routes,
                  int *islandScratch, Rng *rng)
{
    (void)islandScratch;
    (void)rng;
    const int islands = cfg->islandCount;
    int count = 0;
    for (int s = 0; s < islands; ++s) {
        const int r = (s + 1) % islands;
        for (int k = 0; k < cfg->migrationCount; ++k) {
            if (r == s || receiveCapacity[r] <= 0) {
                continue; /* dropped */
            }
            receiveCapacity[r]--;
            routes[count].source = s;
            routes[count].receiver = r;
            ++count;
        }
    }
    return count;
}

/*
 * Copy k of source s goes to island (s + 1 + k mod (I-1)) mod I.
 * Every receiver therefore gets exactly migrationCount copies in total.
 */
int topology_fully_connected(const GAConfig *cfg, int *receiveCapacity,
                             MigrationRoute *routes, int *islandScratch, Rng *rng)
{
    (void)islandScratch;
    (void)rng;
    const int islands = cfg->islandCount;
    int count = 0;
    if (islands < 2) {
        return 0;
    }
    for (int s = 0; s < islands; ++s) {
        for (int k = 0; k < cfg->migrationCount; ++k) {
            const int r = (s + 1 + k % (islands - 1)) % islands;
            if (receiveCapacity[r] <= 0) {
                continue;
            }
            receiveCapacity[r]--;
            routes[count].source = s;
            routes[count].receiver = r;
            ++count;
        }
    }
    return count;
}

/*
 * Each copy picks a uniformly random receiver among the other islands that
 * still have free (non-protected) capacity; if none remain the copy is dropped.
 */
int topology_random(const GAConfig *cfg, int *receiveCapacity, MigrationRoute *routes,
                    int *islandScratch, Rng *rng)
{
    const int islands = cfg->islandCount;
    int count = 0;
    for (int s = 0; s < islands; ++s) {
        for (int k = 0; k < cfg->migrationCount; ++k) {
            int available = 0;
            for (int r = 0; r < islands; ++r) {
                if (r != s && receiveCapacity[r] > 0) {
                    islandScratch[available++] = r;
                }
            }
            if (available == 0) {
                continue;
            }
            const int r = islandScratch[rng_uniform_int(rng, available)];
            receiveCapacity[r]--;
            routes[count].source = s;
            routes[count].receiver = r;
            ++count;
        }
    }
    return count;
}

