#ifndef FKL_READLINE_H
#define FKL_READLINE_H

#include "common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*FklReadlineEndPredicateCb)(const char *buf,
        int len,
        const uint32_t *u32_buf,
        int u32_len,
        int pos,
        void *args);

FKL_API char *fklReadline(const char *prompt);
FKL_API char *fklReadline2(const char *prompt, const char *init);

FKL_API
char *fklReadline3(const char *prompt,
        const char *init,
        FklReadlineEndPredicateCb cb,
        void *args);

FKL_API int fklReadlineHistoryAdd(const char *s);

#ifdef __cplusplus
}
#endif

#endif
