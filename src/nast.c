#include <fakeLisp/common.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/utils.h>

FklNastNode *fklCreateNastNode(FklNastType type, uint64_t line) {
    FklNastNode *r = (FklNastNode *)malloc(sizeof(FklNastNode));
    FKL_ASSERT(r);
    r->curline = line;
    r->type = type;
    r->str = NULL;
    r->refcount = 0;
    return r;
}

typedef struct {
    const FklNastNode *node;
    FklNastNode **slot;
} NodeSlotPair;

// NastNodeSlotVector
#define FKL_VECTOR_TYPE_PREFIX Nast
#define FKL_VECTOR_METHOD_PREFIX nast
#define FKL_VECTOR_ELM_TYPE NodeSlotPair
#define FKL_VECTOR_ELM_TYPE_NAME NodeSlotPair
#include <fakeLisp/vector.h>

FklNastNode *fklCopyNastNode(const FklNastNode *orig) {
    NastNodeSlotPairVector s;
    nastNodeSlotPairVectorInit(&s, 8);
    FklNastNode *retval = NULL;
    nastNodeSlotPairVectorPushBack2(
        &s, (NodeSlotPair){.node = orig, .slot = &retval});
    while (!nastNodeSlotPairVectorIsEmpty(&s)) {
        const NodeSlotPair *p = nastNodeSlotPairVectorPopBack(&s);
        const FklNastNode *top = p->node;
        FklNastNode **retval = p->slot;

        FklNastNode *node = fklCreateNastNode(top->type, top->curline);
        *retval = node;
        switch (top->type) {
        case FKL_NAST_NIL:
            break;
        case FKL_NAST_CHR:
            node->chr = top->chr;
            break;
        case FKL_NAST_FIX:
            node->fix = top->fix;
            break;
        case FKL_NAST_F64:
            node->f64 = top->f64;
            break;
        case FKL_NAST_SLOT:
        case FKL_NAST_SYM:
        case FKL_NAST_RC_SYM:
            node->sym = top->sym;
            break;
        case FKL_NAST_STR:
            node->str = fklCopyString(top->str);
            break;
        case FKL_NAST_BYTEVECTOR:
            node->bvec = fklCopyBytevector(top->bvec);
            break;
        case FKL_NAST_BIG_INT:
            node->bigInt = fklCopyBigInt(top->bigInt);
            break;

        case FKL_NAST_BOX:
            nastNodeSlotPairVectorPushBack2(
                &s, (NodeSlotPair){.node = top->box, .slot = &node->box});
            break;
        case FKL_NAST_PAIR:
            node->pair = fklCreateNastPair();
            nastNodeSlotPairVectorPushBack2(
                &s, (NodeSlotPair){.node = top->pair->car,
                                   .slot = &node->pair->car});
            nastNodeSlotPairVectorPushBack2(
                &s, (NodeSlotPair){.node = top->pair->cdr,
                                   .slot = &node->pair->cdr});
            break;
        case FKL_NAST_VECTOR:
            node->vec = fklCreateNastVector(top->vec->size);
            for (size_t i = 0; i < top->vec->size; i++) {
                nastNodeSlotPairVectorPushBack2(
                    &s, (NodeSlotPair){.node = top->vec->base[i],
                                       .slot = &node->vec->base[i]});
            }
            break;
        case FKL_NAST_HASHTABLE:
            node->hash = fklCreateNastHash(top->hash->type, top->hash->num);
            for (size_t i = 0; i < top->hash->num; i++) {
                nastNodeSlotPairVectorPushBack2(
                    &s, (NodeSlotPair){.node = top->hash->items[i].car,
                                       .slot = &node->hash->items[i].car});
                nastNodeSlotPairVectorPushBack2(
                    &s, (NodeSlotPair){.node = top->hash->items[i].cdr,
                                       .slot = &node->hash->items[i].cdr});
            }
            break;
        }
    }
    nastNodeSlotPairVectorUninit(&s);
    return retval;
}

FklNastPair *fklCreateNastPair(void) {
    FklNastPair *pair = (FklNastPair *)malloc(sizeof(FklNastPair));
    FKL_ASSERT(pair);
    pair->car = NULL;
    pair->cdr = NULL;
    return pair;
}

FklNastVector *fklCreateNastVector(size_t size) {
    FklNastVector *vec = (FklNastVector *)malloc(sizeof(FklNastVector));
    FKL_ASSERT(vec);
    vec->size = size;
    vec->base = (FklNastNode **)calloc(size, sizeof(FklNastNode *));
    FKL_ASSERT(vec->base || !size);
    return vec;
}

FklNastHashTable *fklCreateNastHash(FklHashTableEqType type, size_t num) {
    FklNastHashTable *r = (FklNastHashTable *)malloc(sizeof(FklNastHashTable));
    FKL_ASSERT(r);
    r->items = (FklNastPair *)calloc(num, sizeof(FklNastPair));
    FKL_ASSERT(r->items || !num);
    r->num = num;
    r->type = type;
    return r;
}

size_t fklNastListLength(const FklNastNode *list) {
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        ;
    return i;
}

FklNastNode *fklMakeNastNodeRef(FklNastNode *n) {
    n->refcount += 1;
    return n;
}

static void destroyNastPair(FklNastPair *pair) { free(pair); }

static void destroyNastVector(FklNastVector *vec) {
    free(vec->base);
    free(vec);
}

static void destroyNastHash(FklNastHashTable *hash) {
    free(hash->items);
    free(hash);
}

void fklDestroyNastNode(FklNastNode *node) {
    FklNastNodeVector stack;
    fklNastNodeVectorInit(&stack, 32);
    fklNastNodeVectorPushBack2(&stack, node);
    while (!fklNastNodeVectorIsEmpty(&stack)) {
        FklNastNode *cur = *fklNastNodeVectorPopBack(&stack);
        if (cur) {
            if (!cur->refcount) {
                switch (cur->type) {
                case FKL_NAST_FIX:
                case FKL_NAST_SYM:
                case FKL_NAST_SLOT:
                case FKL_NAST_NIL:
                case FKL_NAST_CHR:
                case FKL_NAST_F64:
                case FKL_NAST_RC_SYM:
                    break;
                case FKL_NAST_STR:
                    free(cur->str);
                    break;
                case FKL_NAST_BYTEVECTOR:
                    free(cur->bvec);
                    break;
                case FKL_NAST_BIG_INT:
                    fklDestroyBigInt(cur->bigInt);
                    break;
                case FKL_NAST_PAIR:
                    fklNastNodeVectorPushBack2(&stack, cur->pair->car);
                    fklNastNodeVectorPushBack2(&stack, cur->pair->cdr);
                    destroyNastPair(cur->pair);
                    break;
                case FKL_NAST_BOX:
                    fklNastNodeVectorPushBack2(&stack, cur->box);
                    break;
                case FKL_NAST_VECTOR:
                    for (size_t i = 0; i < cur->vec->size; i++)
                        fklNastNodeVectorPushBack2(&stack, cur->vec->base[i]);
                    destroyNastVector(cur->vec);
                    break;
                case FKL_NAST_HASHTABLE:
                    for (size_t i = 0; i < cur->hash->num; i++) {
                        fklNastNodeVectorPushBack2(&stack,
                                                   cur->hash->items[i].car);
                        fklNastNodeVectorPushBack2(&stack,
                                                   cur->hash->items[i].cdr);
                    }
                    destroyNastHash(cur->hash);
                    break;
                default:
                    abort();
                    break;
                }
                free(cur);
            } else
                cur->refcount--;
        }
    }
    fklNastNodeVectorUninit(&stack);
}

typedef enum {
    NAST_CAR,
    NAST_CDR,
} NastPlace;

typedef enum {
    PRINT_METHOD_FIRST = 0,
    PRINT_METHOD_CONT,
} PrintState;

#define PRINT_CTX_COMMON_HEADER                                                \
    const FklNastNode *(*method)(struct PrintCtx *, FILE * fp);                \
    PrintState state;                                                          \
    NastPlace place

typedef struct PrintCtx {
    PRINT_CTX_COMMON_HEADER;
    intptr_t data[2];
} PrintCtx;

typedef struct PrintPairCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklNastNode *cur;
} PrintPairCtx;

static_assert(sizeof(PrintPairCtx) <= sizeof(PrintCtx),
              "print pair ctx too large");

typedef struct PrintVectorCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklNastNode *vec;
    uintptr_t index;
} PrintVectorCtx;

static_assert(sizeof(PrintVectorCtx) <= sizeof(PrintCtx),
              "print vector ctx too large");

typedef struct PrintHashCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklNastNode *hash;
    uintptr_t index;
} PrintHashCtx;

static_assert(sizeof(PrintHashCtx) <= sizeof(PrintCtx),
              "print hash ctx too large");

// NastPrintCtxVector
#define FKL_VECTOR_TYPE_PREFIX Nast
#define FKL_VECTOR_METHOD_PREFIX nast
#define FKL_VECTOR_ELM_TYPE PrintCtx
#define FKL_VECTOR_ELM_TYPE_NAME PrintCtx
#include <fakeLisp/vector.h>

static void print_pair_ctx_init(PrintCtx *ctx, const FklNastNode *pair) {
    PrintPairCtx *pair_ctx = FKL_TYPE_CAST(PrintPairCtx *, ctx);
    pair_ctx->cur = pair;
}

static void print_vector_ctx_init(PrintCtx *ctx, const FklNastNode *vec) {
    PrintVectorCtx *vector_ctx = FKL_TYPE_CAST(PrintVectorCtx *, ctx);
    vector_ctx->vec = vec;
    vector_ctx->index = 0;
}

static void print_hash_ctx_init(PrintCtx *ctx, const FklNastNode *hash) {
    PrintHashCtx *hash_ctx = FKL_TYPE_CAST(PrintHashCtx *, ctx);
    hash_ctx->hash = hash;
    hash_ctx->index = 0;
}

static const FklNastNode *print_pair_method(PrintCtx *ctx, FILE *fp) {
    PrintPairCtx *pair_ctx = FKL_TYPE_CAST(PrintPairCtx *, ctx);
    if (pair_ctx->cur == NULL) {
        fputc(')', fp);
        return NULL;
    }
    const FklNastNode *r = NULL;
    switch (pair_ctx->state) {
    case PRINT_METHOD_FIRST:
        pair_ctx->state = PRINT_METHOD_CONT;
        fputc('(', fp);
        r = pair_ctx->cur->pair->car;
        pair_ctx->cur = pair_ctx->cur->pair->cdr;
        break;
    case PRINT_METHOD_CONT:
        if (pair_ctx->cur->type == FKL_NAST_PAIR) {
            fputc(' ', fp);
            r = pair_ctx->cur->pair->car;
            pair_ctx->cur = pair_ctx->cur->pair->cdr;
        } else if (pair_ctx->cur->type == FKL_NAST_NIL) {
            fputc(')', fp);
            r = NULL;
            pair_ctx->cur = NULL;
        } else {
            fputc(',', fp);
            r = pair_ctx->cur;
            pair_ctx->cur = NULL;
        }
        break;
    }
    return r;
}

static const FklNastNode *print_vector_method(PrintCtx *ctx, FILE *fp) {
    PrintVectorCtx *vector_ctx = FKL_TYPE_CAST(PrintVectorCtx *, ctx);
    if (vector_ctx->vec->vec->size == 0) {
        fputs("#()", fp);
        return NULL;
    } else if (vector_ctx->index >= vector_ctx->vec->vec->size) {
        fputc(')', fp);
        return NULL;
    }
    const FklNastNode *r = NULL;
    switch (vector_ctx->state) {
    case PRINT_METHOD_FIRST:
        vector_ctx->state = PRINT_METHOD_CONT;
        fputs("#(", fp);
        r = vector_ctx->vec->vec->base[vector_ctx->index++];
        break;
    case PRINT_METHOD_CONT:
        fputc(' ', fp);
        r = vector_ctx->vec->vec->base[vector_ctx->index++];
        break;
    }
    return r;
}

static const FklNastNode *print_hash_method(PrintCtx *ctx, FILE *fp) {
    static const char *tmp[] = {
        "#hash(",
        "#hasheqv(",
        "#hashequal(",
    };
    PrintHashCtx *hash_ctx = FKL_TYPE_CAST(PrintHashCtx *, ctx);
    if (hash_ctx->hash->hash->num == 0) {
        fputs(tmp[hash_ctx->hash->hash->type], fp);
        fputc(')', fp);
        return NULL;
    } else if (hash_ctx->index >= hash_ctx->hash->hash->num) {
        fputs("))", fp);
        return NULL;
    }
    const FklNastNode *r = NULL;
    switch (hash_ctx->state) {
    case PRINT_METHOD_FIRST:
        hash_ctx->state = PRINT_METHOD_CONT;
        fputs(tmp[hash_ctx->hash->hash->type], fp);
        fputc('(', fp);
        r = hash_ctx->hash->hash->items[hash_ctx->index].car;
        break;
    case PRINT_METHOD_CONT:
        switch (hash_ctx->place) {
        case NAST_CAR:
            hash_ctx->place = NAST_CDR;
            fputc(',', fp);
            r = hash_ctx->hash->hash->items[hash_ctx->index].cdr;
            break;
        case NAST_CDR:
            hash_ctx->place = NAST_CAR;
            ++hash_ctx->index;
            if (hash_ctx->index >= hash_ctx->hash->hash->num) {
                fputs("))", fp);
                return NULL;
            }
            fputs(") (", fp);
            r = hash_ctx->hash->hash->items[hash_ctx->index].car;
            break;
        }
        break;
    }
    return r;
}

typedef void (*PrintCtxInitFunc)(PrintCtx *ctx, const FklNastNode *);
typedef const FklNastNode *(*PrintMethod)(PrintCtx *ctx, FILE *fp);

void fklPrintNastNode(const FklNastNode *exp, FILE *fp,
                      const FklSymbolTable *table) {
    static const PrintCtxInitFunc ctx_init_method_table[FKL_NAST_RC_SYM + 1] = {
        [FKL_NAST_PAIR] = print_pair_ctx_init,
        [FKL_NAST_VECTOR] = print_vector_ctx_init,
        [FKL_NAST_HASHTABLE] = print_hash_ctx_init,
    };

    static const PrintMethod print_method_table[FKL_NAST_RC_SYM + 1] = {
        [FKL_NAST_PAIR] = print_pair_method,
        [FKL_NAST_VECTOR] = print_vector_method,
        [FKL_NAST_HASHTABLE] = print_hash_method,
    };

    NastPrintCtxVector print_contexts;
    nastPrintCtxVectorInit(&print_contexts, 1);
    for (; exp;) {
        switch (exp->type) {
        case FKL_NAST_NIL:
            fputs("()", fp);
            break;
        case FKL_NAST_FIX:
            fprintf(fp, "%" FKL_PRT64D "", exp->fix);
            break;
        case FKL_NAST_RC_SYM:
        case FKL_NAST_SYM:
            fklPrintRawSymbol(fklGetSymbolWithId(exp->sym, table)->symbol, fp);
            break;
        case FKL_NAST_CHR:
            fklPrintRawChar(exp->chr, fp);
            break;
        case FKL_NAST_F64: {
            char buf[64] = {0};
            fklWriteDoubleToBuf(buf, 64, exp->f64);
            fputs(buf, fp);
        } break;
        case FKL_NAST_BIG_INT:
            fklPrintBigInt(exp->bigInt, fp);
            break;
        case FKL_NAST_STR:
            fklPrintRawString(exp->str, fp);
            break;
        case FKL_NAST_BOX:
            fputs("#&", fp);
            exp = exp->box;
            continue;
            break;
        case FKL_NAST_BYTEVECTOR:
            fklPrintRawBytevector(exp->bvec, fp);
            break;
        case FKL_NAST_SLOT:
            fputs("[ERROR]\tslot cannot be print in nast\n", stderr);
            abort();
            break;
        default: {
            PrintCtx *ctx = nastPrintCtxVectorPushBack(&print_contexts, NULL);
            ctx->method = print_method_table[exp->type];
            ctx->state = PRINT_METHOD_FIRST;
            ctx->place = NAST_CAR;
            ctx_init_method_table[exp->type](ctx, exp);
            break;
        }
        }

        if (nastPrintCtxVectorIsEmpty(&print_contexts))
            break;
        while (!nastPrintCtxVectorIsEmpty(&print_contexts)) {
            PrintCtx *ctx = nastPrintCtxVectorBack(&print_contexts);
            exp = ctx->method(ctx, fp);
            if (exp)
                break;
            else
                nastPrintCtxVectorPopBack(&print_contexts);
        }
    }
    nastPrintCtxVectorUninit(&print_contexts);
}

int fklNastNodeEqual(const FklNastNode *n0, const FklNastNode *n1) {
    FklNastImmPairVector s;
    fklNastImmPairVectorInit(&s, 8);
    fklNastImmPairVectorPushBack2(&s, (FklNastImmPair){.car = n0, .cdr = n1});
    int r = 1;
    while (!fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *p = fklNastImmPairVectorPopBack(&s);
        const FklNastNode *car = p->car;
        const FklNastNode *cdr = p->cdr;
        if (car->type != cdr->type)
            r = 0;
        else {
            switch (car->type) {
            case FKL_NAST_SYM:
            case FKL_NAST_RC_SYM:
                r = car->sym == cdr->sym;
                break;
            case FKL_NAST_FIX:
                r = car->fix == cdr->fix;
                break;
            case FKL_NAST_F64:
                r = car->f64 == cdr->f64;
                break;
            case FKL_NAST_NIL:
                break;
            case FKL_NAST_STR:
                r = !fklStringCmp(car->str, cdr->str);
                break;
            case FKL_NAST_BYTEVECTOR:
                r = !fklBytevectorCmp(car->bvec, cdr->bvec);
                break;
            case FKL_NAST_CHR:
                r = car->chr == cdr->chr;
                break;
            case FKL_NAST_BIG_INT:
                r = fklBigIntCmp(car->bigInt, cdr->bigInt);
                break;
            case FKL_NAST_BOX:
                fklNastImmPairVectorPushBack2(
                    &s, (FklNastImmPair){.car = car->box, .cdr = cdr->box});
                break;
            case FKL_NAST_VECTOR:
                r = car->vec->size == cdr->vec->size;
                if (r) {
                    for (size_t i = 0; i < car->vec->size; i++)
                        fklNastImmPairVectorPushBack2(
                            &s, (FklNastImmPair){.car = car->vec->base[i],
                                                 .cdr = cdr->vec->base[i]});
                }
                break;
            case FKL_NAST_HASHTABLE:
                r = car->hash->type == cdr->hash->type
                 && car->hash->num == cdr->hash->num;
                if (r) {
                    for (size_t i = 0; i < car->hash->num; i++) {
                        fklNastImmPairVectorPushBack2(
                            &s,
                            (FklNastImmPair){.car = car->hash->items[i].car,
                                             .cdr = cdr->hash->items[i].car});
                        fklNastImmPairVectorPushBack2(
                            &s,
                            (FklNastImmPair){.car = car->hash->items[i].cdr,
                                             .cdr = cdr->hash->items[i].cdr});
                    }
                }
                break;
            case FKL_NAST_PAIR:
                fklNastImmPairVectorPushBack2(
                    &s, (FklNastImmPair){.car = car->pair->car,
                                         .cdr = cdr->pair->car});
                fklNastImmPairVectorPushBack2(
                    &s, (FklNastImmPair){.car = car->pair->cdr,
                                         .cdr = cdr->pair->cdr});
                break;
            case FKL_NAST_SLOT:
                r = 1;
                break;
            }
        }
        if (!r)
            break;
    }
    fklNastImmPairVectorUninit(&s);
    return r;
}

int fklIsNastNodeList(const FklNastNode *list) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        ;
    return list->type == FKL_NAST_NIL;
}

int fklIsNastNodeListAndHasSameType(const FklNastNode *list, FklNastType type) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        if (list->pair->car->type != type)
            return 0;
    return list->type == FKL_NAST_NIL;
}

FklNastNode *fklNastConsWithSym(FklSid_t sym, FklNastNode *cdr, uint64_t l1,
                                uint64_t l2) {
    FklNastNode *r = fklCreateNastNode(FKL_NAST_PAIR, l1);
    FklNastNode *h = fklCreateNastNode(FKL_NAST_SYM, l2);
    h->sym = sym;
    r->pair = fklCreateNastPair();
    r->pair->car = h;
    r->pair->cdr = cdr;
    return r;
}

FklNastNode *fklNastCons(FklNastNode *car, FklNastNode *cdr, uint64_t l1) {
    FklNastNode *r = fklCreateNastNode(FKL_NAST_PAIR, l1);
    r->pair = fklCreateNastPair();
    r->pair->car = car;
    r->pair->cdr = cdr;
    return r;
}
