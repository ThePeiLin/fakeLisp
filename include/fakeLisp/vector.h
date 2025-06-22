// steal from pocketpy: https://github.com/pocketpy/pocketpy/

#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef FKL_VECTOR_TYPE_PREFIX
#define FKL_VECTOR_TYPE_PREFIX Fkl
#endif

#ifndef FKL_VECTOR_METHOD_PREFIX
#define FKL_VECTOR_METHOD_PREFIX fkl
#endif

#ifndef FKL_VECTOR_INIT
#define FKL_VECTOR_INIT {NULL, 0, 0, 0}
#endif

#ifndef FKL_VECTOR_ELM_TYPE
#error "FKL_VECTOR_ELM_TYPE is undefined"
#define FKL_VECTOR_ELM_TYPE void *
#endif

#ifndef FKL_VECTOR_ELM_TYPE_NAME
#error "FKL_VECTOR_ELM_TYPE_NAME is undefined"
#define FKL_VECTOR_ELM_TYPE_NAME Ptr
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define NAME                                                                   \
    CONCAT(FKL_VECTOR_TYPE_PREFIX, CONCAT(FKL_VECTOR_ELM_TYPE_NAME, Vector))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NAME {
    FKL_VECTOR_ELM_TYPE *base;
    size_t size;
    size_t capacity;
} NAME;

#define METHOD(method_name)                                                    \
    CONCAT(CONCAT(CONCAT(FKL_VECTOR_METHOD_PREFIX, FKL_VECTOR_ELM_TYPE_NAME),  \
                  Vector),                                                     \
           method_name)

static inline void METHOD(Init)(NAME *r, size_t size) {
    if (!size)
        r->base = NULL;
    else {
        r->base = (FKL_VECTOR_ELM_TYPE *)FKL_CONTAINER_MALLOC(
            size * sizeof(FKL_VECTOR_ELM_TYPE));
        assert(r->base);
    }
    r->capacity = size;
    r->size = 0;
}

static inline void METHOD(Uninit)(NAME *r) {
    FKL_CONTAINER_FREE(r->base);
    r->base = NULL;
    r->capacity = 0;
    r->size = 0;
}

static inline NAME *METHOD(Create)(size_t size) {
    NAME *r = (NAME *)FKL_CONTAINER_MALLOC(sizeof(NAME));
    assert(r);
    METHOD(Init)(r, size);
    return r;
}

static inline void METHOD(Destroy)(NAME *r) {
    METHOD(Uninit)(r);
    FKL_CONTAINER_FREE(r);
}

static inline void METHOD(Reserve)(NAME *r, size_t size) {
    if (r->capacity >= size)
        return;
    r->capacity <<= 1;
    if (r->capacity < size)
        r->capacity = size;
    FKL_VECTOR_ELM_TYPE *nbase = (FKL_VECTOR_ELM_TYPE *)FKL_CONTAINER_REALLOC(
        r->base, r->capacity * sizeof(FKL_VECTOR_ELM_TYPE));
    assert(nbase);
    r->base = nbase;
}

static inline void METHOD(Resize)(NAME *r, size_t new_size,
                                  FKL_VECTOR_ELM_TYPE const *v) {
    if (new_size < r->size) {
        r->size = new_size;
    } else if (new_size == 0) {
        FKL_CONTAINER_FREE(r->base);
        r->base = NULL;
        r->capacity = 0;
        r->size = 0;
    } else if (new_size > r->size) {
        METHOD(Reserve)(r, new_size);
        FKL_VECTOR_ELM_TYPE *c = &r->base[r->size];
        FKL_VECTOR_ELM_TYPE *const end = &r->base[new_size];
        if (v) {
            for (; c < end; ++c)
                *c = *v;
        }
        r->size = new_size;
    }
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(PushBack)(NAME *r, FKL_VECTOR_ELM_TYPE const *data) {
    METHOD(Reserve)(r, r->size + 1);
    FKL_VECTOR_ELM_TYPE *p = &r->base[r->size++];
    if (data)
        *p = *data;
    return p;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(PushBack2)(NAME *r,
                                                     FKL_VECTOR_ELM_TYPE data) {
    METHOD(Reserve)(r, r->size + 1);
    FKL_VECTOR_ELM_TYPE *p = &r->base[r->size++];
    *p = data;
    return p;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(InsertFront)(NAME *r, FKL_VECTOR_ELM_TYPE const *data) {
    METHOD(Reserve)(r, r->size + 1);
    memmove(r->base + 1, r->base, r->size * sizeof(FKL_VECTOR_ELM_TYPE));
    if (data)
        r->base[0] = *data;
    ++r->size;
    return r->base;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(InsertFront2)(NAME *r, FKL_VECTOR_ELM_TYPE data) {
    METHOD(Reserve)(r, r->size + 1);
    memmove(r->base + 1, r->base, r->size * sizeof(FKL_VECTOR_ELM_TYPE));
    r->base[0] = data;
    ++r->size;
    return r->base;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(Insert)(NAME *r, unsigned long idx, FKL_VECTOR_ELM_TYPE const *data) {
    if (idx == r->size)
        return METHOD(PushBack)(r, data);
    else if (idx == 0)
        return METHOD(InsertFront)(r, data);
    else {
        METHOD(Reserve)(r, r->size + 1);
        memmove(r->base + idx + 1, r->base + idx,
                r->size * sizeof(FKL_VECTOR_ELM_TYPE));
        if (data)
            r->base[idx] = *data;
        ++r->size;
        return r->base;
    }
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(Insert2)(NAME *r, unsigned long idx,
                                                   FKL_VECTOR_ELM_TYPE data) {
    if (idx == r->size)
        return METHOD(PushBack2)(r, data);
    else if (idx == 0)
        return METHOD(InsertFront2)(r, data);
    else {
        METHOD(Reserve)(r, r->size + 1);
        memmove(r->base + idx + 1, r->base + idx,
                r->size * sizeof(FKL_VECTOR_ELM_TYPE));
        r->base[idx] = data;
        ++r->size;
        return r->base;
    }
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(PopBack)(NAME *r) {
    return r->size ? &r->base[--r->size] : NULL;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(Back)(const NAME *r) {
    return r->size ? &r->base[r->size - 1] : NULL;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(PopBackNonNull)(NAME *r) {
    assert(r->size);
    return &r->base[--r->size];
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(BackNonNull)(const NAME *r) {
    assert(r->size);
    return &r->base[r->size - 1];
}

static inline void METHOD(ShrinkToFit)(NAME *r) {
    if (r->size) {
        FKL_VECTOR_ELM_TYPE *nbase =
            (FKL_VECTOR_ELM_TYPE *)FKL_CONTAINER_REALLOC(
                r->base, r->size * sizeof(FKL_VECTOR_ELM_TYPE));
        assert(nbase);
        r->base = nbase;
    } else {
        FKL_CONTAINER_FREE(r->base);
        r->base = NULL;
    }
    r->capacity = r->size;
}

static inline void METHOD(Shrink)(NAME *r, size_t s) {
    if (s >= r->size) {
        FKL_VECTOR_ELM_TYPE *nbase =
            (FKL_VECTOR_ELM_TYPE *)FKL_CONTAINER_REALLOC(
                r->base, s * sizeof(FKL_VECTOR_ELM_TYPE));
        assert(nbase);
        r->base = nbase;
        r->capacity = s;
    }
}

static inline int METHOD(IsEmpty)(const NAME *r) { return r->size == 0; }

#ifdef __cplusplus
}
#endif

#undef CONCAT
#undef CONCAT_

#undef METHOD
#undef NAME

#undef FKL_VECTOR_ELM_TYPE
#undef FKL_VECTOR_ELM_TYPE_NAME
#undef FKL_VECTOR_TYPE_PREFIX
#undef FKL_VECTOR_METHOD_PREFIX
