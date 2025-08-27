#ifndef FKL_READLINE_H
#define FKL_READLINE_H

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
char *fklReadline(const char *prompt);
char *fklReadline2(const char *prompt, const char *init);
char *fklReadline3(const char *prompt,
        const char *init,
        FklReadlineEndPredicateCb cb,
        void *args);

int fklReadlineHistoryAdd(const char *s);

#ifdef __cplusplus
}
#endif

#endif
