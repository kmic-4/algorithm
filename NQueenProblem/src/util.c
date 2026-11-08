#include "util.h"

#include <stdint.h>
#include <stdlib.h>

size_t nq_mul_size(size_t a, size_t b, int *ok)
{
    if (a != 0 && b > SIZE_MAX / a) {
        *ok = 0;
        return 0;
    }
    return a * b;
}

void *nq_calloc_array(size_t count, size_t size)
{
    int ok = 1;
    (void)nq_mul_size(count, size, &ok);
    if (!ok) {
        return NULL;
    }
    if (count == 0 || size == 0) {
        count = 1; /* always return a unique freeable pointer */
        size = 1;
    }
    return calloc(count, size);
}

