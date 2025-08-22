#include <fakeLisp/zmalloc.h>

#ifndef FKL_READLINE_MALLOC
#define FKL_READLINE_MALLOC fklZmalloc
#endif

#ifndef FKL_READLINE_FREE
#define FKL_READLINE_FREE fklZfree
#endif

#ifndef FKL_READLINE_REALLOC
#define FKL_READLINE_REALLOC fklZrealloc
#endif

#ifndef FKL_READLINE_STRDUP
#define FKL_READLINE_STRDUP fklZstrdup
#endif

