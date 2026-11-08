#ifndef NQ_UTIL_H
#define NQ_UTIL_H

#include <stddef.h>

/* calloc with multiplication overflow check. Returns NULL on failure or overflow. */
void *nq_calloc_array(size_t count, size_t size);

/* Safe size multiplication. Returns 0 on overflow (sets *ok = 0). */
size_t nq_mul_size(size_t a, size_t b, int *ok);

#endif /* NQ_UTIL_H */

