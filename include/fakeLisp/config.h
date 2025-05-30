#ifndef FKL_CONFIG_H
#define FKL_CONFIG_H

#include "zmalloc.h"
#define FKL_CONTAINER_MALLOC fklZmalloc
#define FKL_CONTAINER_CALLOC fklZcalloc
#define FKL_CONTAINER_REALLOC fklZrealloc
#define FKL_CONTAINER_FREE fklZfree

#ifndef FKL_CONTAINER_MALLOC
#define FKL_CONTAINER_MALLOC malloc
#endif

#ifndef FKL_CONTAINER_CALLOC
#define FKL_CONTAINER_CALLOC calloc
#endif

#ifndef FKL_CONTAINER_REALLOC
#define FKL_CONTAINER_REALLOC realloc
#endif

#ifndef FKL_CONTAINER_FREE
#define FKL_CONTAINER_FREE free
#endif

#endif
