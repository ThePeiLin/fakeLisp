#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef FKL_TABLE_KEY_EQUAL
#define FKL_TABLE_KEY_EQUAL(A, B) *(A) == *(B)
#endif

#ifndef FKL_TABLE_KEY_INIT
#define FKL_TABLE_KEY_INIT(X, K) *(X) = *(K)
#endif

#ifndef FKL_TABLE_KEY_UNINIT
#define FKL_TABLE_KEY_UNINIT(X)
#endif

#ifndef FKL_TABLE_DEFAULT_CAPACITY_SHIFT
#define FKL_TABLE_DEFAULT_CAPACITY_SHIFT (3)
#endif

#if FKL_TABLE_DEFAULT_CAPACITY_SHIFT < 1
#error "FKL_TABLE_DEFAULT_CAPACITY_SHIFT should greater than 1"
#endif

#ifndef FKL_TABLE_TYPE_PREFIX
#define FKL_TABLE_TYPE_PREFIX Fkl
#endif

#ifndef FKL_TABLE_METHOD_PREFIX
#define FKL_TABLE_METHOD_PREFIX fkl
#endif

#ifndef FKL_TABLE_KEY_TYPE
#error "FKL_TABLE_KEY_TYPE is undefined"
#define FKL_TABLE_KEY_TYPE uint32_t
#endif

#ifndef FKL_TABLE_ELM_NAME
#error "FKL_TABLE_ELM_NAME is undefined"
#define FKL_TABLE_ELM_NAME U32
#endif

#ifndef FKL_TABLE_VAL_INIT
#ifdef FKL_TABLE_VAL_TYPE
#define FKL_TABLE_VAL_INIT(X, V) *(X) = *(V)
#else
#define FKL_TABLE_VAL_INIT(X, V)
#endif
#endif

#ifndef FKL_TABLE_VAL_UNINIT
#define FKL_TABLE_VAL_UNINIT(X)
#endif

#ifndef FKL_TABLE_VAL_TYPE
#define POST_PREFIX HashSet
#else
#define POST_PREFIX HashMap
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define METHOD(method_name)                                                    \
    CONCAT(CONCAT(CONCAT(FKL_TABLE_METHOD_PREFIX, FKL_TABLE_ELM_NAME),         \
                  POST_PREFIX),                                                \
           method_name)
#define NAME                                                                   \
    CONCAT(FKL_TABLE_TYPE_PREFIX, CONCAT(FKL_TABLE_ELM_NAME, POST_PREFIX))
#define NODE_NAME                                                              \
    CONCAT(FKL_TABLE_TYPE_PREFIX,                                              \
           CONCAT(FKL_TABLE_ELM_NAME, CONCAT(POST_PREFIX, Node)))

#ifndef FKL_TABLE_KEY_HASH
#define FKL_TABLE_KEY_HASH                                                     \
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

static inline uintptr_t METHOD(__hashv)(FKL_TABLE_KEY_TYPE const *pk) {
    FKL_TABLE_KEY_HASH;
}

#define HASHV(X) METHOD(__hashv)(X)

#ifdef FKL_TABLE_VAL_TYPE
#define ELM_NAME                                                               \
    CONCAT(FKL_TABLE_TYPE_PREFIX,                                              \
           CONCAT(FKL_TABLE_ELM_NAME, CONCAT(POST_PREFIX, Elm)))

#define MUTABLE_ELM_NAME                                                       \
    CONCAT(FKL_TABLE_TYPE_PREFIX,                                              \
           CONCAT(FKL_TABLE_ELM_NAME, CONCAT(POST_PREFIX, MutElm)))

typedef struct ELM_NAME {
    FKL_TABLE_KEY_TYPE const k;
    FKL_TABLE_VAL_TYPE v;
} ELM_NAME;

typedef struct MUTABLE_ELM_NAME {
    FKL_TABLE_KEY_TYPE k;
    FKL_TABLE_VAL_TYPE v;
} MUTABLE_ELM_NAME;

typedef struct NODE_NAME {
    struct NODE_NAME *prev;
    struct NODE_NAME *next;
    struct NODE_NAME *bkt_next;
    uintptr_t const hashv;
    union {
        struct {
            FKL_TABLE_KEY_TYPE const k;
            FKL_TABLE_VAL_TYPE v;
        };
        ELM_NAME elm;
    };
} NODE_NAME;

#else
typedef struct NODE_NAME {
    struct NODE_NAME *prev;
    struct NODE_NAME *next;
    struct NODE_NAME *bkt_next;
    uintptr_t const hashv;
    FKL_TABLE_KEY_TYPE const k;
} NODE_NAME;
#endif

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
    self->capacity = (1 << FKL_TABLE_DEFAULT_CAPACITY_SHIFT);
    self->mask = self->capacity - 1;
#ifdef FKL_TABLE_LOAD_FACTOR
    self->rehash_threshold = self->capacity * FKL_TABLE_LOAD_FACTOR;
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
        FKL_TABLE_KEY_UNINIT((FKL_TABLE_KEY_TYPE *)&cur->k);
        FKL_TABLE_VAL_UNINIT(&cur->v);
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
        FKL_TABLE_KEY_UNINIT((FKL_TABLE_KEY_TYPE *)&cur->k);
        FKL_TABLE_VAL_UNINIT(&cur->v);
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
        NODE_NAME **pp = &self->buckets[cur->hashv & self->mask];
        cur->bkt_next = *pp;
        *pp = cur;
    }
}

static inline void METHOD(Grow)(NAME *self) {
    NODE_NAME *cur = self->first;
    self->capacity <<= 1;
    self->mask = self->capacity - 1;
#ifdef FKL_TABLE_LOAD_FACTOR
    self->rehash_threshold = self->capacity * FKL_TABLE_LOAD_FACTOR;
#else
    self->rehash_threshold = (self->capacity >> 2) | (self->capacity >> 1);
#endif
    NODE_NAME **buckets = (NODE_NAME **)realloc(
        self->buckets, self->capacity * sizeof(NODE_NAME *));
    assert(buckets);
    self->buckets = buckets;
    memset(self->buckets, 0, self->capacity * sizeof(NODE_NAME *));
    for (; cur; cur = cur->next) {
        NODE_NAME **pp = &self->buckets[cur->hashv & self->mask];
        cur->bkt_next = *pp;
        *pp = cur;
    }
}

static inline NODE_NAME *const *METHOD(Bucket)(NAME *self, uintptr_t hashv) {
    return &self->buckets[hashv & self->mask];
}

static inline NODE_NAME *METHOD(CreateNode)(uintptr_t hashv,
                                            FKL_TABLE_KEY_TYPE const *k) {
    NODE_NAME *node = (NODE_NAME *)calloc(1, sizeof(NODE_NAME));
    assert(node);
    *((uintptr_t *)&node->hashv) = hashv;
    FKL_TABLE_KEY_INIT((FKL_TABLE_KEY_TYPE *)&node->k, k);
    return node;
}

static inline NODE_NAME *METHOD(CreateNode2)(uintptr_t hashv,
                                             FKL_TABLE_KEY_TYPE k) {
    NODE_NAME *node = (NODE_NAME *)calloc(1, sizeof(NODE_NAME));
    assert(node);
    *((uintptr_t *)&node->hashv) = hashv;
    FKL_TABLE_KEY_INIT((FKL_TABLE_KEY_TYPE *)&node->k, &k);
    return node;
}

static inline NODE_NAME **METHOD(InsertNode)(NAME *self, uintptr_t hashv,
                                             NODE_NAME *node) {
    // check threshold and grow capacity
    if (self->count >= self->rehash_threshold)
        METHOD(Grow)(self);
    NODE_NAME **pp = &self->buckets[hashv & self->mask];

    node->bkt_next = *pp;
    *pp = node;
    if (self->first) {
        node->prev = self->last;
        self->last->next = node;
    } else
        self->first = node;
    self->last = node;
    ++self->count;
    return pp;
}

#ifdef FKL_TABLE_VAL_TYPE
static inline FKL_TABLE_VAL_TYPE *METHOD(Put)(NAME *self,
                                              FKL_TABLE_KEY_TYPE const *k,
                                              FKL_TABLE_VAL_TYPE const *v) {
#else
static inline int METHOD(Put)(NAME *self, FKL_TABLE_KEY_TYPE const *k) {
#endif
    uintptr_t const hashv = HASHV(k);
    NODE_NAME **pp = &self->buckets[hashv & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k))
#ifdef FKL_TABLE_VAL_TYPE
            return &pn->v;
#else
            return 1;
#endif
    }

    NODE_NAME *node = METHOD(CreateNode)(hashv, k);
#ifdef FKL_TABLE_VAL_TYPE
    if (v) {
        FKL_TABLE_VAL_INIT(&node->v, v);
    }
#endif

    pp = METHOD(InsertNode)(self, hashv, node);
#ifdef FKL_TABLE_VAL_TYPE
    return NULL;
#else
    return 0;
#endif
}

#ifdef FKL_TABLE_VAL_TYPE
static inline FKL_TABLE_VAL_TYPE *METHOD(Put2)(NAME *self, FKL_TABLE_KEY_TYPE k,
                                               FKL_TABLE_VAL_TYPE v) {
    return METHOD(Put)(self, &k, &v);
}
#else
static inline int METHOD(Put2)(NAME *self, FKL_TABLE_KEY_TYPE k) {
    return METHOD(Put)(self, &k);
}
#endif

#ifdef FKL_TABLE_VAL_TYPE
static inline FKL_TABLE_VAL_TYPE *METHOD(Get)(NAME const *self,
                                              FKL_TABLE_KEY_TYPE const *k) {
    NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k))
            return &pn->v;
    }
    return NULL;
}

static inline FKL_TABLE_VAL_TYPE *METHOD(Get2)(NAME const *self,
                                               FKL_TABLE_KEY_TYPE k) {
    return METHOD(Get)(self, &k);
}

static inline ELM_NAME *METHOD(At)(NAME const *self,
                                   FKL_TABLE_KEY_TYPE const *k) {
    NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k))
            return &pn->elm;
    }
    return NULL;
}

static inline ELM_NAME *METHOD(At2)(NAME const *self, FKL_TABLE_KEY_TYPE k) {
    return METHOD(At)(self, &k);
}

static inline FKL_TABLE_VAL_TYPE *METHOD(Add)(NAME *self,
                                              FKL_TABLE_KEY_TYPE const *k,
                                              FKL_TABLE_VAL_TYPE const *v) {
    uintptr_t const hashv = HASHV(k);
    NODE_NAME **pp = &self->buckets[hashv & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k)) {
            if (v) {
                FKL_TABLE_VAL_INIT(&pn->v, v);
            }
            return &pn->v;
        }
    }

    NODE_NAME *node = METHOD(CreateNode)(hashv, k);
    if (v) {
        FKL_TABLE_VAL_INIT(&node->v, v);
    }

    pp = METHOD(InsertNode)(self, hashv, node);
    return &node->v;
}

static inline FKL_TABLE_VAL_TYPE *METHOD(Add1)(NAME *self,
                                               FKL_TABLE_KEY_TYPE k) {
    return METHOD(Add)(self, &k, NULL);
}

static inline FKL_TABLE_VAL_TYPE *METHOD(Add2)(NAME *self, FKL_TABLE_KEY_TYPE k,
                                               FKL_TABLE_VAL_TYPE v) {
    return METHOD(Add)(self, &k, &v);
}

static inline ELM_NAME *METHOD(Insert)(NAME *self, FKL_TABLE_KEY_TYPE const *k,
                                       FKL_TABLE_VAL_TYPE const *v) {
    uintptr_t const hashv = HASHV(k);
    NODE_NAME **pp = &self->buckets[hashv & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k)) {
            return &pn->elm;
        }
    }

    NODE_NAME *node = METHOD(CreateNode)(hashv, k);
    if (v) {
        FKL_TABLE_VAL_INIT(&node->v, v);
    }

    pp = METHOD(InsertNode)(self, hashv, node);
    return &node->elm;
}

static inline ELM_NAME *METHOD(Insert2)(NAME *self, FKL_TABLE_KEY_TYPE k,
                                        FKL_TABLE_VAL_TYPE v) {
    return METHOD(Insert)(self, &k, &v);
}

static inline int METHOD(Earase)(NAME *self, FKL_TABLE_KEY_TYPE const *k,
                                 FKL_TABLE_VAL_TYPE *pv,
                                 FKL_TABLE_KEY_TYPE *pk) {
    for (NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask]; *pp;
         pp = &(*pp)->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &(*pp)->k)) {
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
            if (pk) {
                FKL_TABLE_KEY_INIT(pk, &pn->k);
            } else {
                FKL_TABLE_KEY_UNINIT((FKL_TABLE_KEY_TYPE *)&pn->k);
            }
            if (pv) {
                FKL_TABLE_VAL_INIT(pv, &pn->v);
            } else {
                FKL_TABLE_VAL_UNINIT(&pn->v);
            }
            free(pn);
            return 1;
        }
    }
    return 0;
}

static inline int METHOD(Earase2)(NAME *self, FKL_TABLE_KEY_TYPE k,
                                  FKL_TABLE_VAL_TYPE *pv,
                                  FKL_TABLE_KEY_TYPE *pk) {
    return METHOD(Earase)(self, &k, pv, pk);
}
#else
static inline int METHOD(Has)(NAME const *self, FKL_TABLE_KEY_TYPE const *k) {
    NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask];
    for (NODE_NAME *pn = *pp; pn; pn = pn->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &pn->k))
            return 1;
    }
    return 0;
}

static inline int METHOD(Has2)(NAME const *self, FKL_TABLE_KEY_TYPE k) {
    return METHOD(Has)(self, &k);
}
#endif

static inline int METHOD(DelNode)(NAME *self, NODE_NAME **pp) {
    NODE_NAME *pn = *pp;
    if (pn) {
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
        FKL_TABLE_KEY_UNINIT((FKL_TABLE_KEY_TYPE *)&pn->k);
        FKL_TABLE_VAL_UNINIT(&pn->v);
        free(pn);
        return 1;
    }
    return 0;
}

static inline int METHOD(Del)(NAME *self, FKL_TABLE_KEY_TYPE const *k) {
    for (NODE_NAME **pp = &self->buckets[HASHV(k) & self->mask]; *pp;
         pp = &(*pp)->bkt_next) {
        if (FKL_TABLE_KEY_EQUAL(k, &(*pp)->k)) {
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
            FKL_TABLE_KEY_UNINIT((FKL_TABLE_KEY_TYPE *)&pn->k);
            FKL_TABLE_VAL_UNINIT(&pn->v);
            free(pn);
            return 1;
        }
    }
    return 0;
}

static inline int METHOD(Del2)(NAME *self, FKL_TABLE_KEY_TYPE k) {
    return METHOD(Del)(self, &k);
}

#undef CONCAT
#undef CONCAT_

#undef METHOD
#undef NAME
#undef NODE_NAME
#undef ELM_NAME
#undef MUTABLE_ELM_NAME

#undef HASHV
#undef RETURN_TYPE
#undef POST_PREFIX
#undef FKL_TABLE_DEFAULT_CAPACITY_SHIFT
#undef FKL_TABLE_KEY_EQUAL
#undef FKL_TABLE_KEY_HASH
#undef FKL_TABLE_KEY_TYPE
#undef FKL_TABLE_VAL_TYPE
#undef FKL_TABLE_ELM_NAME
#undef FKL_TABLE_TYPE_PREFIX
#undef FKL_TABLE_METHOD_PREFIX
#undef FKL_TABLE_KEY_INIT
#undef FKL_TABLE_KEY_UNINIT
#undef FKL_TABLE_VAL_INIT
#undef FKL_TABLE_VAL_UNINIT
