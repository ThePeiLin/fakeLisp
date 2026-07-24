#ifndef FKL_ZMALLOC_H
#define FKL_ZMALLOC_H

#include <assert.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t fklZmallocSize(void *ptr);
void *fklZmalloc(size_t size);
void *fklZcalloc(size_t ele_num, size_t size);
void fklZfree(void *ptr);
void *fklZrealloc(void *ptr, size_t new_size);

static inline char *fklZstrdup(const char *str) {
    if (str == NULL)
        return NULL;
    size_t len = strlen(str) + 1;
    char *new_str = (char *)fklZmalloc(len * sizeof(char));
    assert(new_str);
    memcpy(new_str, str, len);
    return new_str;
}

#ifdef __cplusplus
}
#endif

#endif
