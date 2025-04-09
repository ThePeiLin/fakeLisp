#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FKL_USET_ELM_EQUAL
#define FKL_USET_ELM_EQUAL(A, B) *(A) == *(B)
#endif

#ifndef FKL_USET_ELM_INIT
#define FKL_USET_ELM_INIT(X, K) *(X) = *(K)
#endif

#ifndef FKL_USET_ELM_UNINIT
#define FKL_USET_ELM_UNINIT(X)
#endif

#ifndef FKL_USET_DEFAULT_CAPACITY
#define FKL_USET_DEFAULT_CAPACITY (3)
#endif

#ifndef FKL_USET_TYPE_PREFIX
#define FKL_USET_TYPE_PREFIX Fkl
#endif

#ifndef FKL_USET_METHOD_PREFIX
#define FKL_USET_METHOD_PREFIX fkl
#endif

#ifndef FKL_USET_ELM_TYPE
#error "FKL_USET_ELM_TYPE is undefined"
#define FKL_USET_ELM_TYPE uint32_t
#endif

#ifndef FKL_USET_ELM_TYPE_NAME
#error "FKL_USET_ELM_TYPE_NAME is undefined"
#define FKL_USET_ELM_TYPE_NAME U32
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define METHOD(method_name)                                                    \
    CONCAT(                                                                    \
        CONCAT(CONCAT(FKL_USET_METHOD_PREFIX, FKL_USET_ELM_TYPE_NAME), Uset),  \
        method_name)
#define NAME CONCAT(FKL_USET_TYPE_PREFIX, CONCAT(FKL_USET_ELM_TYPE_NAME, Uset))
#define NODE_NAME                                                              \
    CONCAT(FKL_USET_TYPE_PREFIX, CONCAT(FKL_USET_ELM_TYPE_NAME, UsetNode))

#ifndef FKL_USET_ELM_HASH
#define FKL_USET_ELM_HASH                                                      \
    {                                                                          \
        uint32_t k = *pk;                                                      \
        k = ~k + (k << 15);                                                    \
        k = k ^ (k >> 12);                                                     \
        k = k + (k << 2);                                                      \
        k = k ^ (k >> 4);                                                      \
        k = (k + (k << 3)) + (k << 11);                                        \
        k = k ^ (k >> 16);                                                     \
        return k;                                                              \
    }
#endif

static inline uintptr_t METHOD(__hashv)(FKL_USET_ELM_TYPE const *pk) {
    FKL_USET_ELM_HASH
}
#define HASHV(X) METHOD(__hashv)(X)

typedef struct NODE_NAME {
    struct NODE_NAME *next;
    struct NODE_NAME *prev;
    struct NODE_NAME *bkt_next;
    FKL_USET_ELM_TYPE const k;
} NODE_NAME;

typedef struct NAME {
    NODE_NAME **buckets;
    NODE_NAME *first;
    NODE_NAME *last;
    uint32_t count;
    uint32_t capacity;
    uint32_t rehash_threshold;
    uint32_t mask;
} NAME;

static inline void METHOD(Init)(NAME *self) {
    self->capacity = (1 << FKL_USET_DEFAULT_CAPACITY);
    self->mask = self->capacity - 1;
#ifdef FKL_USET_LOAD_FACTOR
    self->rehash_threshold = self->capacity * FKL_USET_LOAD_FACTOR;
#else
    self->rehash_threshold = (self->capacity >> 2) | (self->capacity >> 1);
#endif
    self->count = 0;
    self->first = NULL;
    self->last = NULL;
    self->buckets = (NODE_NAME **)calloc(self->capacity, sizeof(NODE_NAME *));
    assert(self->buckets);
}

static inline void METHOD(Uninit)(NAME *self) {
    self->capacity = 0;
    self->rehash_threshold = 0;
    self->count = 0;
    self->mask = 0;
    free(self->buckets);
    self->buckets = NULL;
    NODE_NAME *cur = self->first;
    while (cur) {
        FKL_USET_ELM_UNINIT(&cur->data);
        NODE_NAME *prev = cur;
        cur = prev->next;
        free(prev);
    }
    self->first = NULL;
    self->last = NULL;
}

static inline NAME *METHOD(Create)(void) {
    NAME *r = (NAME *)malloc(sizeof(NAME));
    assert(r);
    METHOD(Init)(r);
    return r;
}

static inline void METHOD(Destroy)(NAME *r) {
    METHOD(Uninit)(r);
    free(r);
}

static inline void METHOD(Clear)(NAME *self) {
    memset(self->buckets, 0, self->capacity * sizeof(NODE_NAME *));
    self->count = 0;
    NODE_NAME *cur = self->first;
    while (cur) {
        FKL_USET_ELM_UNINIT(&cur->data);
        NODE_NAME *prev = cur;
        cur = prev->next;
        free(prev);
    }
    self->first = NULL;
    self->last = NULL;
}

static inline void METHOD(Rehash)(NAME *self) {
    NODE_NAME *cur = self->first;
    memset(self->buckets, 0, self->capacity * sizeof(NODE_NAME *));
    for (; cur; cur = cur->next) {
        NODE_NAME **pp = &self->buckets[HASHV(&cur->k) & self->mask];
        cur->bkt_next = *pp;
        *pp = cur;
    }
}

static inline int METHOD(Put)(NAME *self, FKL_USET_ELM_TYPE const *k) {
    uint32_t const hashv = HASHV(k);
    NODE_NAME **pp = &self->buckets[hashv & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_USET_ELM_EQUAL(k, &pn->k))
            return 1;
    }

    // check threshold
    if (self->count >= self->rehash_threshold) {
        NODE_NAME *cur = self->first;
        self->capacity <<= 1;
        self->mask = self->capacity - 1;
#ifdef FKL_USET_LOAD_FACTOR
        self->rehash_threshold = self->capacity * FKL_USET_LOAD_FACTOR;
#else
        self->rehash_threshold = (self->capacity >> 2) | (self->capacity >> 1);
#endif
        NODE_NAME **buckets = (NODE_NAME **)realloc(
            self->buckets, self->capacity * sizeof(NODE_NAME *));
        assert(buckets);
        self->buckets = buckets;
        memset(self->buckets, 0, self->capacity * sizeof(NODE_NAME *));
        for (; cur; cur = cur->next) {
            NODE_NAME **pp = &self->buckets[HASHV(&cur->k) & self->mask];
            cur->bkt_next = *pp;
            *pp = cur;
        }
        pp = &self->buckets[hashv & self->mask];
    }

    NODE_NAME *node = (NODE_NAME *)calloc(1, sizeof(NODE_NAME));
    assert(node);
    FKL_USET_ELM_INIT((FKL_USET_ELM_TYPE *)&node->k, k);
    node->bkt_next = *pp;
    *pp = node;
    if (self->first) {
        node->prev = self->last;
        self->last->next = node;
    } else
        self->first = node;
    self->last = node;
    ++self->count;
    return 0;
}

static inline int METHOD(Put2)(NAME *self, FKL_USET_ELM_TYPE k) {
    return METHOD(Put)(self, &k);
}

static inline int METHOD(Has)(NAME *self, FKL_USET_ELM_TYPE const *k) {
    NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_USET_ELM_EQUAL(k, &pn->k))
            return 1;
    }
    return 0;
}

static inline int METHOD(Has2)(NAME *self, FKL_USET_ELM_TYPE k) {
    return METHOD(Has)(self, &k);
}

static inline int METHOD(Del)(NAME *self, FKL_USET_ELM_TYPE const *k) {
    for (NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask]; *pp;
         pp = &(*pp)->bkt_next) {
        if (FKL_USET_ELM_EQUAL(k, &(*pp)->k)) {
            NODE_NAME *pn = *pp;
            *pp = pn->bkt_next;
            if (pn->prev)
                pn->prev->next = pn->next;
            if (pn->next)
                pn->next->prev = pn->prev;
            if (self->last == pn)
                self->last = pn->prev;
            if (self->first == pn)
                self->first = pn->next;

            --self->count;
            FKL_USET_ELM_UNINIT(&pn->data);
            free(pn);
            return 1;
        }
    }
    return 0;
}

static inline int METHOD(Del2)(NAME *self, FKL_USET_ELM_TYPE k) {
    return METHOD(Del)(self, &k);
}

#undef CONCAT
#undef CONCAT_

#undef METHOD
#undef NAME
#undef NODE_NAME

#undef HASHV
#undef FKL_USET_ELM_EQUAL
#undef FKL_USET_ELM_HASH
#undef FKL_USET_ELM_TYPE
#undef FKL_USET_ELM_TYPE_NAME
#undef FKL_USET_TYPE_PREFIX
#undef FKL_USET_METHOD_PREFIX
