#ifndef FKL_ZMALLOC_H
#define FKL_ZMALLOC_H

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FKL_ZMALLOC_ALIGN_PADDING

#ifdef _WIN32
#define ALIGN_PADDING (16)
#else
#define ALIGN_PADDING alignof(max_align_t)
#endif

#endif

static inline size_t fklZmallocSize(void *ptr) {
    return ptr == NULL ? 0 : *((size_t *)(((char *)ptr) - ALIGN_PADDING));
}

static inline void *fklZmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void *ptr = malloc(ALIGN_PADDING + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + ALIGN_PADDING);
}

static inline void *fklZcalloc(size_t ele_num, size_t size) {
    size *= ele_num;
    if (size == 0)
        return NULL;
    void *ptr = calloc(1, ALIGN_PADDING + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + ALIGN_PADDING);
}

static inline void fklZfree(void *ptr) {
    if (ptr == NULL)
        return;
    free(((char *)ptr) - ALIGN_PADDING);
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
    } else if (*((size_t *)(((char *)ptr) - ALIGN_PADDING)) == new_size)
        return ptr;

    void *new_ptr =
            realloc((((char *)ptr) - ALIGN_PADDING), ALIGN_PADDING + new_size);
    if (new_ptr == NULL)
        return NULL;
    *((size_t *)new_ptr) = new_size;
    return (void *)(((char *)new_ptr) + ALIGN_PADDING);
}

#define malloc (?"calling malloc when including zmalloc.h is not allowed"?)
#define calloc (?"calling calloc when including zmalloc.h is not allowed"?)
#define free (?"calling free when including zmalloc.h is not allowed"?)
#define realloc (?"calling realloc when including zmalloc.h is not allowed"?)
#define strdup (?"calling strdup when including zmalloc.h is not allowed"?)

#ifdef __cplusplus
}
#endif

#endif
