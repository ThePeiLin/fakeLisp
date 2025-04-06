// steal from pocketpy: https://github.com/pocketpy/pocketpy/

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef FKL_QUEUE_TYPE_PREFIX
#define FKL_QUEUE_TYPE_PREFIX Fkl
#endif

#ifndef FKL_QUEUE_METHOD_PREFIX
#define FKL_QUEUE_METHOD_PREFIX fkl
#endif

#ifndef FKL_QUEUE_INIT
#define FKL_QUEUE_INIT ({NULL, NULL})
#endif

#ifndef FKL_QUEUE_ELM_TYPE
#error "FKL_QUEUE_ELM_TYPE is undefined"
#define FKL_QUEUE_ELM_TYPE void *
#endif

#ifndef FKL_QUEUE_ELM_TYPE_NAME
#error "FKL_QUEUE_ELM_TYPE_NAME is undefined"
#define FKL_QUEUE_ELM_TYPE_NAME Ptr
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define NAME                                                                   \
    CONCAT(FKL_QUEUE_TYPE_PREFIX, CONCAT(FKL_QUEUE_ELM_TYPE_NAME, Queue))

#define NODE_NAME                                                              \
    CONCAT(FKL_QUEUE_TYPE_PREFIX, CONCAT(FKL_QUEUE_ELM_TYPE_NAME, QueueNode))

typedef struct NODE_NAME {
    struct NODE_NAME *next;
    FKL_QUEUE_ELM_TYPE data;
} NODE_NAME;

typedef struct NAME {
    NODE_NAME *head;
    NODE_NAME *tail;
    NODE_NAME *cache;
} NAME;

#define METHOD(method_name)                                                    \
    CONCAT(CONCAT(CONCAT(FKL_QUEUE_METHOD_PREFIX, FKL_QUEUE_ELM_TYPE_NAME),    \
                  Queue),                                                      \
           method_name)

static inline void METHOD(Init)(NAME *r) {
    r->head = NULL;
    r->tail = NULL;
    r->cache = NULL;
}

static inline void METHOD(Uninit)(NAME *r) {
    NODE_NAME *cur = r->head;
    while (cur) {
        NODE_NAME *prev = cur;
        cur = cur->next;
        free(prev);
    }
    cur = r->cache;
    while (cur) {
        NODE_NAME *prev = cur;
        cur = cur->next;
        free(prev);
    }
    r->head = NULL;
    r->tail = NULL;
    r->cache = NULL;
}

static inline NAME *METHOD(Create)(size_t size) {
    NAME *r = (NAME *)malloc(sizeof(NAME));
    assert(r);
    METHOD(Init)(r);
    return r;
}

static inline void METHOD(Destroy)(NAME *r) {
    METHOD(Uninit)(r);
    free(r);
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(Push)(NAME *r,
                                               FKL_QUEUE_ELM_TYPE const *data) {
    NODE_NAME *node;
    if (r->cache) {
        node = r->cache;
        r->cache = node->next;
    } else {
        node = (NODE_NAME *)malloc(sizeof(NODE_NAME));
        assert(node);
    }
    node->next = NULL;
    if (!r->head)
        r->head = node;
    if (r->tail) {
        r->tail->next = node;
        r->tail = node;
    } else
        r->tail = node;
    if (data)
        node->data = *data;
    return &node->data;
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(Push2)(NAME *r,
                                                FKL_QUEUE_ELM_TYPE data) {
    NODE_NAME *node;
    if (r->cache) {
        node = r->cache;
        r->cache = node->next;
    } else {
        node = (NODE_NAME *)malloc(sizeof(NODE_NAME));
        assert(node);
    }
    node->next = NULL;
    if (!r->head)
        r->head = node;
    if (r->tail) {
        r->tail->next = node;
        r->tail = node;
    } else
        r->tail = node;
    node->data = data;
    return &node->data;
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(PushNode)(NAME *r, NODE_NAME *node) {
    node->next = NULL;
    if (!r->head)
        r->head = node;
    if (r->tail) {
        r->tail->next = node;
        r->tail = node;
    } else
        r->tail = node;
    return &node->data;
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(Pop)(NAME *r) {
    NODE_NAME *n = r->head;
    if (!n)
        return NULL;
    else {
        r->head = n->next;
        n->next = r->cache;
        r->cache = n;
        if (!r->head)
            r->tail = NULL;
        return &n->data;
    }
}

static inline NODE_NAME *METHOD(PopNode)(NAME *r) {
    NODE_NAME *n = r->head;
    if (!n)
        return NULL;
    else {
        r->head = n->next;
        n->next = NULL;
        if (!r->head)
            r->tail = NULL;
        return n;
    }
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(Front)(const NAME *r) {
    return r->head ? &r->head->data : NULL;
}

static inline FKL_QUEUE_ELM_TYPE *METHOD(Back)(const NAME *r) {
    return r->tail ? &r->tail->data : NULL;
}

static inline void METHOD(Shrink)(NAME *r) {
    NODE_NAME *cur = r->cache;
    while (cur) {
        NODE_NAME *prev = cur;
        cur = cur->next;
        free(prev);
    }
    r->cache = NULL;
}

static inline int METHOD(IsEmpty)(const NAME *r) { return r->head == NULL; }

static inline size_t METHOD(Length)(const NAME *r) {
    size_t l = 0;
    for (const NODE_NAME *n = r->head; n; n = n->next, ++l)
        ;
    return l;
}

#undef CONCAT
#undef CONCAT_

#undef METHOD
#undef NAME

#undef FKL_QUEUE_ELM_TYPE
#undef FKL_QUEUE_ELM_TYPE_NAME
#undef FKL_QUEUE_TYPE_PREFIX
#undef FKL_QUEUE_METHOD_PREFIX
