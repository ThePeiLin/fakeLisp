#include <fakeLisp/zmalloc.h>

#include <stdlib.h>

#if defined(__linux__) // linux

#include <malloc.h>

#define __zmalloc_size malloc_usable_size
#define __zmalloc malloc
#define __zrealloc realloc
#define __zcalloc calloc
#define __zfree free

#elif defined(_WIN32) // win32

#include <malloc.h>

#define __zmalloc_size _msize
#define __zmalloc malloc
#define __zrealloc realloc
#define __zcalloc calloc
#define __zfree free

#elif __APPLE__

#include <malloc/malloc.h>

#define __zmalloc_size malloc_size
#define __zmalloc malloc
#define __zrealloc realloc
#define __zcalloc calloc
#define __zfree free

#elif defined(__FreeBSD__)

#include <malloc_np.h>

#define __zmalloc_size malloc_usable_size
#define __zmalloc malloc
#define __zrealloc realloc
#define __zcalloc calloc
#define __zfree free

#else // fallback

#ifndef FKL_ZMALLOC_ALIGN_PADDING

#define ALIGN_PADDING (sizeof(size_t))

#endif

#include <stdalign.h>

static FKL_ALWAYS_INLINE size_t __zmalloc_size(void *ptr) {
    return ptr == NULL ? 0 : *((size_t *)(((char *)ptr) - ALIGN_PADDING));
}

static FKL_ALWAYS_INLINE void *__zmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void *ptr = malloc(ALIGN_PADDING + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + ALIGN_PADDING);
}

static FKL_ALWAYS_INLINE void *__zcalloc(size_t ele_num, size_t size) {
    size *= ele_num;
    if (size == 0)
        return NULL;
    void *ptr = calloc(1, ALIGN_PADDING + size);
    if (ptr == NULL)
        return NULL;
    *((size_t *)ptr) = size;
    return (void *)(((char *)ptr) + ALIGN_PADDING);
}

static FKL_ALWAYS_INLINE void __zfree(void *ptr) {
    if (ptr == NULL)
        return;
    free(((char *)ptr) - ALIGN_PADDING);
}

static FKL_ALWAYS_INLINE void *__zrealloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        if (ptr)
            _zfree(ptr);
        return NULL;
    } else if (ptr == NULL) {
        return __zmalloc(new_size);
    } else if (*((size_t *)(((char *)ptr) - ALIGN_PADDING)) == new_size)
        return ptr;

    void *old_ptr = (((char *)ptr) - ALIGN_PADDING);
    void *new_ptr = realloc(old_ptr, ALIGN_PADDING + new_size);
    if (new_ptr == NULL)
        return NULL;
    *((size_t *)new_ptr) = new_size;
    return (void *)(((char *)new_ptr) + ALIGN_PADDING);
}

#endif

size_t fklZmallocSize(void *ptr) { return __zmalloc_size(ptr); }

void *fklZmalloc(size_t size) { return __zmalloc(size); }

void *fklZcalloc(size_t ele_num, size_t size) {
    return __zcalloc(ele_num, size);
}

void fklZfree(void *ptr) { __zfree(ptr); }

void *fklZrealloc(void *ptr, size_t new_size) {
    return __zrealloc(ptr, new_size);
}
