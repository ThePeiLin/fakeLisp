#ifndef FKL_ZMALLOC_H
#define FKL_ZMALLOC_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t fklZmallocSize(void *ptr) {
    return ptr == NULL ? 0 : *((size_t *)(((char *)ptr) - sizeof(size_t)));
}

static inline void *fklZmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void *ptr = malloc(sizeof(size_t) + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + sizeof(size_t));
}

static inline void *fklZcalloc(size_t ele_num, size_t size) {
    size *= ele_num;
    if (size == 0)
        return NULL;
    void *ptr = calloc(1, sizeof(size_t) + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + sizeof(size_t));
}

static inline void fklZfree(void *ptr) {
    if (ptr == NULL)
        return;
    free(((char *)ptr) - sizeof(size_t));
}

static inline char *fklZstrdup(const char *str) {
    if (str == NULL)
        return NULL;
    size_t len = strlen(str) + 1;
    char *new_str = (char *)fklZmalloc(len * sizeof(char));
    assert(new_str);
    memcpy(new_str, str, len);
    return new_str;
}

static inline void *fklZrealloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        if (ptr)
            fklZfree(ptr);
        return NULL;
    } else if (ptr == NULL) {
        return fklZmalloc(new_size);
    } else if (*((size_t *)(((char *)ptr) - sizeof(size_t))) == new_size)
        return ptr;

    void *new_ptr =
        realloc((((char *)ptr) - sizeof(size_t)), sizeof(size_t) + new_size);
    if (new_ptr == NULL)
        return NULL;
    *((size_t *)new_ptr) = new_size;
    return (void *)(((char *)new_ptr) + sizeof(size_t));
}

#ifdef __cplusplus
}
#endif

#endif
