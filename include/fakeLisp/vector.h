// steal from pocketpy: https://github.com/pocketpy/pocketpy/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
#define FKL_VECTOR_ELM_TYPE void *
#endif

#ifndef FKL_VECTOR_ELM_TYPE_NAME
#define FKL_VECTOR_ELM_TYPE_NAME Ptr
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define NAME                                                                   \
    CONCAT(FKL_VECTOR_TYPE_PREFIX, CONCAT(FKL_VECTOR_ELM_TYPE_NAME, Vector))

typedef struct NAME {
    FKL_VECTOR_ELM_TYPE *base;
    size_t top;
    size_t size;
} NAME;

#define METHOD(method_name)                                                    \
    CONCAT(CONCAT(CONCAT(FKL_VECTOR_METHOD_PREFIX, FKL_VECTOR_ELM_TYPE_NAME),  \
                  Vector),                                                     \
           method_name)

static inline void METHOD(Init)(NAME *r, size_t size) {
    if (!size)
        r->base = NULL;
    else {
        r->base =
            (FKL_VECTOR_ELM_TYPE *)malloc(size * sizeof(FKL_VECTOR_ELM_TYPE));
        assert(r->base);
    }
    r->size = size;
    r->top = 0;
}

static inline void METHOD(Uninit)(NAME *r) {
    free(r->base);
    r->base = NULL;
    r->size = 0;
    r->top = 0;
}

static inline NAME *METHOD(Create)(size_t size) {
    NAME *r = (NAME *)malloc(sizeof(NAME));
    assert(r);
    METHOD(Init)(r, size);
    return r;
}

static inline void METHOD(Destroy)(NAME *r) {
    METHOD(Uninit)(r);
    free(r);
}

static inline void METHOD(Reserve)(NAME *r, size_t size) {
    if (r->size >= size)
        return;
    r->size <<= 1;
    if (r->size < size)
        r->size = size;
    FKL_VECTOR_ELM_TYPE *nbase = (FKL_VECTOR_ELM_TYPE *)realloc(
        r->base, r->size * sizeof(FKL_VECTOR_ELM_TYPE));
    assert(nbase);
    r->base = nbase;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(PushBack)(NAME *r, FKL_VECTOR_ELM_TYPE const *data) {
    METHOD(Reserve)(r, r->top + 1);
    FKL_VECTOR_ELM_TYPE *p = &r->base[r->top++];
    if (data)
        *p = *data;
    return p;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(PushBack2)(NAME *r,
                                                     FKL_VECTOR_ELM_TYPE data) {
    METHOD(Reserve)(r, r->top + 1);
    FKL_VECTOR_ELM_TYPE *p = &r->base[r->top++];
    *p = data;
    return p;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(InsertFront)(NAME *r, FKL_VECTOR_ELM_TYPE const *data) {
    METHOD(Reserve)(r, r->top + 1);
    memmove(r->base + 1, r->base, r->top * sizeof(FKL_VECTOR_ELM_TYPE));
    if (data)
        r->base[0] = *data;
    ++r->top;
    return r->base;
}

static inline FKL_VECTOR_ELM_TYPE *
METHOD(InsertFront2)(NAME *r, FKL_VECTOR_ELM_TYPE data) {
    METHOD(Reserve)(r, r->top + 1);
    memmove(r->base + 1, r->base, r->top * sizeof(FKL_VECTOR_ELM_TYPE));
    r->base[0] = data;
    ++r->top;
    return r->base;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(PopBack)(NAME *r) {
    return r->top ? &r->base[--r->top] : NULL;
}

static inline FKL_VECTOR_ELM_TYPE *METHOD(Back)(const NAME *r) {
    return r->top ? &r->base[r->top - 1] : NULL;
}

static inline void METHOD(Shrink)(NAME *r) {
    if (r->top) {
        FKL_VECTOR_ELM_TYPE *nbase = (FKL_VECTOR_ELM_TYPE *)realloc(
            r->base, r->top * sizeof(FKL_VECTOR_ELM_TYPE));
        assert(nbase);
        r->base = nbase;
    } else {
        free(r->base);
        r->base = NULL;
    }
    r->size = r->top;
}

static inline int METHOD(IsEmpty)(const NAME *r) { return r->top == 0; }

#undef CONCAT
#undef CONCAT_

#undef METHOD
#undef NAME

#undef FKL_VECTOR_ELM_TYPE
#undef FKL_VECTOR_ELM_TYPE_NAME
#undef FKL_VECTOR_TYPE_PREFIX
#undef FKL_VECTOR_METHOD_PREFIX
