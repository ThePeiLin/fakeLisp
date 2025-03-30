#include <fakeLisp/common.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/utils.h>

// FklNastNodePtrVector
#define FKL_VECTOR_ELM_TYPE FklNastNode **
#define FKL_VECTOR_ELM_TYPE_NAME NastNodeSlot
#include <fakeLisp/vector.h>

FklNastNode *fklCreateNastNode(FklNastType type, uint64_t line) {
    FklNastNode *r = (FklNastNode *)malloc(sizeof(FklNastNode));
    FKL_ASSERT(r);
    r->curline = line;
    r->type = type;
    r->str = NULL;
    r->refcount = 0;
    return r;
}

FklNastNode *fklCopyNastNode(const FklNastNode *orig) {
    FklNastNodeVector pending0;
    FklNastNodeSlotVector pending1;
    fklNastNodeVectorInit(&pending0, 16);
    fklNastNodeSlotVectorInit(&pending1, 16);
    FklNastNode *retval = NULL;
    fklNastNodeVectorPushBack2(&pending0, FKL_REMOVE_CONST(FklNastNode, orig));

    fklNastNodeSlotVectorPushBack2(&pending1, &retval);
    while (!fklNastNodeVectorIsEmpty(&pending0)) {
        const FklNastNode *top = *fklNastNodeVectorPopBack(&pending0);
        FklNastNode **retval = *fklNastNodeSlotVectorPopBack(&pending1);
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
            fklNastNodeVectorPushBack2(&pending0, top->box);
            fklNastNodeSlotVectorPushBack2(&pending1, &node->box);
            break;
        case FKL_NAST_PAIR:
            node->pair = fklCreateNastPair();
            fklNastNodeVectorPushBack2(&pending0, top->pair->car);
            fklNastNodeVectorPushBack2(&pending0, top->pair->cdr);
            fklNastNodeSlotVectorPushBack2(&pending1, &node->pair->car);
            fklNastNodeSlotVectorPushBack2(&pending1, &node->pair->cdr);
            break;
        case FKL_NAST_VECTOR:
            node->vec = fklCreateNastVector(top->vec->size);
            for (size_t i = 0; i < top->vec->size; i++) {
                fklNastNodeVectorPushBack2(&pending0, top->vec->base[i]);
                fklNastNodeSlotVectorPushBack2(&pending1, &node->vec->base[i]);
            }
            break;
        case FKL_NAST_HASHTABLE:
            node->hash = fklCreateNastHash(top->hash->type, top->hash->num);
            for (size_t i = 0; i < top->hash->num; i++) {
                fklNastNodeVectorPushBack2(&pending0, top->hash->items[i].car);
                fklNastNodeVectorPushBack2(&pending0, top->hash->items[i].cdr);
                fklNastNodeSlotVectorPushBack2(&pending1,
                                               &node->hash->items[i].car);
                fklNastNodeSlotVectorPushBack2(&pending1,
                                               &node->hash->items[i].cdr);
            }
            break;
        }
    }
    fklNastNodeVectorUninit(&pending0);
    fklNastNodeSlotVectorUninit(&pending1);
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
    NAST_BOX,
    NAST_HASH_ITEM,
} NastPlace;

typedef struct {
    NastPlace place;
    FklNastNode *node;
} NastElem;

static NastElem *createNastElem(NastPlace place, FklNastNode *node) {
    NastElem *r = (NastElem *)malloc(sizeof(NastElem));
    FKL_ASSERT(r);
    r->node = node;
    r->place = place;
    return r;
}

static void destroyNastElem(NastElem *n) { free(n); }

void fklPrintNastNode(const FklNastNode *exp, FILE *fp,
                      const FklSymbolTable *table) {
    FklPtrQueue *queue = fklCreatePtrQueue();
    FklQueueVector queueStack;
    fklQueueVectorInit(&queueStack, 32);
    fklPushPtrQueue(createNastElem(NAST_CAR, (FklNastNode *)exp), queue);
    fklQueueVectorPushBack2(&queueStack, queue);
    while (!fklQueueVectorIsEmpty(&queueStack)) {
        FklPtrQueue *cQueue = *fklQueueVectorBack(&queueStack);
        while (fklLengthPtrQueue(cQueue)) {
            NastElem *e = fklPopPtrQueue(cQueue);
            if (e->place == NAST_CDR)
                fputc(',', fp);
            else if (e->place == NAST_HASH_ITEM) {
                FklNastPair *t = (FklNastPair *)e->node;
                fputc('(', fp);
                FklPtrQueue *iQueue = fklCreatePtrQueue();
                fklPushPtrQueue(createNastElem(NAST_CAR, t->car), iQueue);
                fklPushPtrQueue(createNastElem(NAST_CDR, t->cdr), iQueue);
                fklQueueVectorPushBack2(&queueStack, iQueue);
                cQueue = iQueue;
                destroyNastElem(e);
                continue;
            }
            FklNastNode *node = e->node;
            destroyNastElem(e);
            switch (node->type) {
            case FKL_NAST_RC_SYM:
            case FKL_NAST_SYM:
                fklPrintRawSymbol(fklGetSymbolWithId(node->sym, table)->symbol,
                                  fp);
                break;
            case FKL_NAST_BYTEVECTOR:
                fklPrintRawBytevector(node->bvec, fp);
                break;
            case FKL_NAST_STR:
                fklPrintRawString(node->str, fp);
                break;
            case FKL_NAST_FIX:
                fprintf(fp, "%" FKL_PRT64D "", node->fix);
                break;
            case FKL_NAST_F64: {
                char buf[64] = {0};
                fklWriteDoubleToBuf(buf, 64, node->f64);
                fputs(buf, fp);
            } break;
            case FKL_NAST_CHR:
                fklPrintRawChar(node->chr, fp);
                break;
            case FKL_NAST_VECTOR:
                fputs("#(", fp);
                {
                    FklPtrQueue *vQueue = fklCreatePtrQueue();
                    for (size_t i = 0; i < node->vec->size; i++)
                        fklPushPtrQueue(
                            createNastElem(NAST_CAR, node->vec->base[i]),
                            vQueue);
                    fklQueueVectorPushBack2(&queueStack, vQueue);
                    cQueue = vQueue;
                    continue;
                }
                break;
            case FKL_NAST_BIG_INT:
                fklPrintBigInt(node->bigInt, fp);
                break;
            case FKL_NAST_BOX:
                fputs("#&", fp);
                {
                    FklPtrQueue *bQueue = fklCreatePtrQueue();
                    fklPushPtrQueue(createNastElem(NAST_BOX, node->box),
                                    bQueue);
                    fklQueueVectorPushBack2(&queueStack, bQueue);
                    cQueue = bQueue;
                }
                break;
            case FKL_NAST_HASHTABLE: {
                static const char *tmp[] = {
                    "#hash(",
                    "#hasheqv(",
                    "#hashequal(",
                };
                fputs(tmp[node->hash->type], fp);
                FklPtrQueue *vQueue = fklCreatePtrQueue();
                for (size_t i = 0; i < node->hash->num; i++)
                    fklPushPtrQueue(
                        createNastElem(NAST_HASH_ITEM,
                                       (FklNastNode *)&node->hash->items[i]),
                        vQueue);
                fklQueueVectorPushBack2(&queueStack, vQueue);
                cQueue = vQueue;
                continue;
            } break;
            case FKL_NAST_NIL:
                fputs("()", fp);
                break;
            case FKL_NAST_PAIR: {
                fputc('(', fp);
                FklPtrQueue *lQueue = fklCreatePtrQueue();
                FklNastNode *cur = node;
                for (; cur->type == FKL_NAST_PAIR; cur = cur->pair->cdr) {
                    FklNastNode *c = cur->pair->car;
                    NastElem *ce = createNastElem(NAST_CAR, c);
                    fklPushPtrQueue(ce, lQueue);
                }
                if (cur->type != FKL_NAST_NIL) {
                    NastElem *cdre = createNastElem(NAST_CDR, cur);
                    fklPushPtrQueue(cdre, lQueue);
                }
                fklQueueVectorPushBack2(&queueStack, lQueue);
                cQueue = lQueue;
                continue;
            } break;
            default:
                abort();
                break;
            }
            if (fklLengthPtrQueue(cQueue)
                && ((NastElem *)fklFirstPtrQueue(cQueue))->place != NAST_CDR
                && ((NastElem *)fklFirstPtrQueue(cQueue))->place != NAST_BOX)
                fputc(' ', fp);
        }
        fklQueueVectorPopBack(&queueStack);
        fklDestroyPtrQueue(cQueue);
        if (!fklQueueVectorIsEmpty(&queueStack)) {
            fputc(')', fp);
            cQueue = *fklQueueVectorBack(&queueStack);
            if (fklLengthPtrQueue(cQueue)
                && ((NastElem *)fklFirstPtrQueue(cQueue))->place != NAST_CDR)
                fputc(' ', fp);
        }
    }
    fklQueueVectorUninit(&queueStack);
}

int fklNastNodeEqual(const FklNastNode *n0, const FklNastNode *n1) {
    FklNastNodeVector s0;
    fklNastNodeVectorInit(&s0, 16);
    FklNastNodeVector s1;
    fklNastNodeVectorInit(&s1, 16);
    fklNastNodeVectorPushBack2(&s0, FKL_REMOVE_CONST(FklNastNode, n0));
    fklNastNodeVectorPushBack2(&s1, FKL_REMOVE_CONST(FklNastNode, n1));
    int r = 1;
    while (!fklNastNodeVectorIsEmpty(&s0) && !fklNastNodeVectorIsEmpty(&s1)) {
        const FklNastNode *c0 = *fklNastNodeVectorPopBack(&s0);
        const FklNastNode *c1 = *fklNastNodeVectorPopBack(&s1);
        if (c0->type != c1->type)
            r = 0;
        else {
            switch (c0->type) {
            case FKL_NAST_SYM:
            case FKL_NAST_RC_SYM:
                r = c0->sym == c1->sym;
                break;
            case FKL_NAST_FIX:
                r = c0->fix == c1->fix;
                break;
            case FKL_NAST_F64:
                r = c0->f64 == c1->f64;
                break;
            case FKL_NAST_NIL:
                break;
            case FKL_NAST_STR:
                r = !fklStringCmp(c0->str, c1->str);
                break;
            case FKL_NAST_BYTEVECTOR:
                r = !fklBytevectorCmp(c0->bvec, c1->bvec);
                break;
            case FKL_NAST_CHR:
                r = c0->chr == c1->chr;
                break;
            case FKL_NAST_BIG_INT:
                r = fklBigIntCmp(c0->bigInt, c1->bigInt);
                break;
            case FKL_NAST_BOX:
                fklNastNodeVectorPushBack2(&s0, c0->box);
                fklNastNodeVectorPushBack2(&s1, c1->box);
                break;
            case FKL_NAST_VECTOR:
                r = c0->vec->size == c1->vec->size;
                if (r) {
                    for (size_t i = 0; i < c0->vec->size; i++)
                        fklNastNodeVectorPushBack2(&s0, c0->vec->base[i]);
                    for (size_t i = 0; i < c1->vec->size; i++)
                        fklNastNodeVectorPushBack2(&s1, c1->vec->base[i]);
                }
                break;
            case FKL_NAST_HASHTABLE:
                r = c0->hash->type == c1->hash->type
                 && c0->hash->num == c1->hash->num;
                if (r) {
                    for (size_t i = 0; i < c0->hash->num; i++) {
                        fklNastNodeVectorPushBack2(&s0, c0->hash->items[i].car);
                        fklNastNodeVectorPushBack2(&s0, c0->hash->items[i].cdr);
                    }
                    for (size_t i = 0; i < c1->hash->num; i++) {
                        fklNastNodeVectorPushBack2(&s1, c1->hash->items[i].car);
                        fklNastNodeVectorPushBack2(&s1, c1->hash->items[i].cdr);
                    }
                }
                break;
            case FKL_NAST_PAIR:
                fklNastNodeVectorPushBack2(&s0, c0->pair->car);
                fklNastNodeVectorPushBack2(&s0, c0->pair->cdr);
                fklNastNodeVectorPushBack2(&s1, c1->pair->car);
                fklNastNodeVectorPushBack2(&s1, c1->pair->cdr);
                break;
            case FKL_NAST_SLOT:
                r = 1;
                break;
            }
        }
        if (!r)
            break;
    }
    fklNastNodeVectorUninit(&s0);
    fklNastNodeVectorUninit(&s1);
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
