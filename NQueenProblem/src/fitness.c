#include "fitness.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

NQStatus fitness_workspace_init(FitnessWorkspace *ws, int n)
{
    const size_t diag = (size_t)(2 * n - 1);
    int *block = nq_calloc_array((size_t)n + 2 * diag, sizeof(int));
    if (block == NULL) {
        memset(ws, 0, sizeof(*ws));
        return NQ_ERR_ALLOC;
    }
    ws->columnCount = block;
    ws->diagDownCount = block + n;
    ws->diagUpCount = ws->diagDownCount + diag;
    ws->evaluationCount = 0;
    return NQ_OK;
}

void fitness_workspace_free(FitnessWorkspace *ws)
{
    free(ws->columnCount); /* single block owns all three arrays */
    ws->columnCount = NULL;
    ws->diagDownCount = NULL;
    ws->diagUpCount = NULL;
}

void fitness_evaluate(Individual *ind, int n, FitnessWorkspace *ws)
{
    const size_t diag = (size_t)(2 * n - 1);
    int *col = ws->columnCount;
    int *down = ws->diagDownCount;
    int *up = ws->diagUpCount;
    const int *genes = ind->genes;
    int *conflicts = ind->queenConflicts;

    memset(col, 0, (size_t)n * sizeof(int));
    memset(down, 0, diag * sizeof(int));
    memset(up, 0, diag * sizeof(int));

    for (int i = 0; i < n; ++i) {
        const int x = genes[i];
        col[x]++;
        down[i - x + n - 1]++;
        up[i + x]++;
    }

    long long total = 0;
    for (int i = 0; i < n; ++i) {
        const int x = genes[i];
        const int c = (col[x] - 1) + (down[i - x + n - 1] - 1) + (up[i + x] - 1);
        conflicts[i] = c;
        total += c;
    }

    ind->fitnessScore = (int)(total / 2);
    ind->evaluationValid = true;
    ws->evaluationCount++;
}

int fitness_reference(const int *genes, int n, int *conflictsOut)
{
    if (conflictsOut != NULL) {
        memset(conflictsOut, 0, (size_t)n * sizeof(int));
    }
    int pairs = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const int dx = abs(genes[i] - genes[j]);
            if (genes[i] == genes[j] || dx == j - i) {
                ++pairs;
                if (conflictsOut != NULL) {
                    conflictsOut[i]++;
                    conflictsOut[j]++;
                }
            }
        }
    }
    return pairs;
}

