#include <fakeLisp/bytecode.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <math.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define fileno _fileno
#endif

typedef struct {
    const FklVMvalue *v;
    FklVMvalue **slot;
} VMvalueSlot;

// VmVMvalueSlotVector
#define FKL_VECTOR_TYPE_PREFIX Vm
#define FKL_VECTOR_METHOD_PREFIX vm
#define FKL_VECTOR_ELM_TYPE VMvalueSlot
#define FKL_VECTOR_ELM_TYPE_NAME VMvalueSlot
#include <fakeLisp/cont/vector.h>

FklVMvalue *fklCopyVMlistOrAtom(const FklVMvalue *obj, FklVM *vm) {
    VmVMvalueSlotVector s;
    vmVMvalueSlotVectorInit(&s, 32);
    FklVMvalue *tmp = FKL_VM_NIL;
    vmVMvalueSlotVectorPushBack2(&s, (VMvalueSlot){ .v = obj, .slot = &tmp });
    while (!vmVMvalueSlotVectorIsEmpty(&s)) {
        const VMvalueSlot *top = vmVMvalueSlotVectorPopBackNonNull(&s);
        const FklVMvalue *root = top->v;
        FklVMvalue **root1 = top->slot;
        FklVMptrTag tag = FKL_GET_TAG(root);
        switch (tag) {
        case FKL_TAG_NIL:
        case FKL_TAG_FIX:
        case FKL_TAG_CHR:
            *root1 = FKL_TYPE_CAST(FklVMvalue *, root);
            break;
        case FKL_TAG_PTR: {
            FklValueType type = root->type_;
            switch (type) {
            case FKL_TYPE_PAIR:
                *root1 = fklCreateVMvaluePairNil(vm);
                vmVMvalueSlotVectorPushBack2(&s,
                        (VMvalueSlot){
                            .v = FKL_VM_CAR(root),
                            .slot = &FKL_VM_CAR(*root1),
                        });
                vmVMvalueSlotVectorPushBack2(&s,
                        (VMvalueSlot){
                            .v = FKL_VM_CDR(root),
                            .slot = &FKL_VM_CDR(*root1),
                        });
                break;
            default:
                *root1 = FKL_TYPE_CAST(FklVMvalue *, root);
                break;
            }
        } break;
        default:
            return NULL;
            break;
        }
    }
    vmVMvalueSlotVectorUninit(&s);
    return tmp;
}

static FklVMvalue *__fkl_f64_copyer(const FklVMvalue *obj, FklVM *vm) {
    FklVMvalue *tmp = fklCreateVMvalueF64(vm, FKL_VM_F64(obj));
    return tmp;
}

static FklVMvalue *__fkl_bigint_copyer(const FklVMvalue *obj, FklVM *vm) {
    return fklCreateVMvalueBigIntWithOther(vm, FKL_VM_BI(obj));
}

static FklVMvalue *__fkl_vector_copyer(const FklVMvalue *obj, FklVM *vm) {
    FklVMvec *vec = FKL_VM_VEC(obj);
    return fklCreateVMvalueVecWithPtr(vm, vec->size, vec->base);
}

static FklVMvalue *__fkl_str_copyer(const FklVMvalue *obj, FklVM *vm) {
    return fklCreateVMvalueStr(vm, FKL_VM_STR(obj));
}

static FklVMvalue *__fkl_bytevector_copyer(const FklVMvalue *obj, FklVM *vm) {
    return fklCreateVMvalueBvec(vm, FKL_VM_BVEC(obj));
}

static FklVMvalue *__fkl_pair_copyer(const FklVMvalue *obj, FklVM *vm) {
    return fklCopyVMlistOrAtom(obj, vm);
}

static FklVMvalue *__fkl_box_copyer(const FklVMvalue *obj, FklVM *vm) {
    return fklCreateVMvalueBox(vm, FKL_VM_BOX(obj));
}

static FklVMvalue *__fkl_userdata_copyer(const FklVMvalue *obj, FklVM *vm) {
    // const FklVMvalueUd *ud = FKL_VM_UD(obj);
    FklVMudCopyAppendCb copy = FKL_VM_UD(obj)->mt_->__copy_append;
    return copy(vm, obj, 0, NULL);
}

static FklVMvalue *__fkl_hashtable_copyer(const FklVMvalue *obj, FklVM *vm) {
    FklVMvalueHash *ht = FKL_VM_HASH(obj);
    FklVMvalue *r = fklCreateVMvalueHashEq(vm);
    FklVMvalueHash *nht = FKL_VM_HASH(r);
    nht->eq_type = ht->eq_type;
    for (FklValueHashMapNode *list = ht->ht.first; list; list = list->next) {
        fklVMhashTableSet(nht, list->k, list->v);
    }
    return r;
}

static FklVMvalue *__fkl_sym_copyer(const FklVMvalue *obj, FklVM *vm) {
    return FKL_TYPE_CAST(FklVMvalue *, obj);
}

static FklVMvalue *(*const valueCopyers[FKL_VM_VALUE_GC_TYPE_NUM])(
        const FklVMvalue *obj,
        FklVM *vm) = {
    [FKL_TYPE_F64] = __fkl_f64_copyer,
    [FKL_TYPE_BIGINT] = __fkl_bigint_copyer,
    [FKL_TYPE_STR] = __fkl_str_copyer,
    [FKL_TYPE_VECTOR] = __fkl_vector_copyer,
    [FKL_TYPE_PAIR] = __fkl_pair_copyer,
    [FKL_TYPE_BOX] = __fkl_box_copyer,
    [FKL_TYPE_BYTEVECTOR] = __fkl_bytevector_copyer,
    [FKL_TYPE_USERDATA] = __fkl_userdata_copyer,
    [FKL_TYPE_HASHTABLE] = __fkl_hashtable_copyer,
    [FKL_TYPE_SYM] = __fkl_sym_copyer,
};

FklVMvalue *fklCopyVMvalue(const FklVMvalue *obj, FklVM *vm) {
    FklVMvalue *tmp = FKL_VM_NIL;
    FklVMptrTag tag = FKL_GET_TAG(obj);
    switch (tag) {
    case FKL_TAG_NIL:
    case FKL_TAG_FIX:
    case FKL_TAG_CHR:
        tmp = FKL_TYPE_CAST(FklVMvalue *, obj);
        break;
    case FKL_TAG_PTR: {
        FklValueType type = obj->type_;
        FKL_ASSERT(type <= FKL_TYPE_HASHTABLE);
        FklVMvalue *(*valueCopyer)(const FklVMvalue *obj, FklVM *vm) =
                valueCopyers[type];
        if (!valueCopyer)
            return NULL;
        else
            return valueCopyer(obj, vm);
    } break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return tmp;
}

FklVMvalue *fklCreateTrueValue() { return FKL_MAKE_VM_FIX(1); }

FklVMvalue *fklCreateNilValue() { return FKL_VM_NIL; }

int fklVMvalueEqual(const FklVMvalue *fir, const FklVMvalue *sec) {
    FklVMpairVector s;

    if (FKL_IS_PTR(fir) && FKL_IS_PTR(sec)) {
        if (fir->type_ != sec->type_)
            return 0;
        switch (fir->type_) {
        case FKL_TYPE_PROC:
        case FKL_TYPE_CPROC:
        case FKL_TYPE_SYM:
            return fir == sec;
            break;
        case FKL_TYPE_F64:
            return FKL_VM_F64(fir) == FKL_VM_F64(sec);
            break;
        case FKL_TYPE_STR:
            return fklStringEqual(FKL_VM_STR(fir), FKL_VM_STR(sec));
            break;
        case FKL_TYPE_BYTEVECTOR:
            return fklBytevectorEqual(FKL_VM_BVEC(fir), FKL_VM_BVEC(sec));
            break;
        case FKL_TYPE_BIGINT:
            return fklVMbigIntEqual(FKL_VM_BI(fir), FKL_VM_BI(sec));
            break;
        case FKL_TYPE_USERDATA: {
            FklVMvalueUd *ud1 = FKL_VM_UD(fir);
            FklVMvalueUd *ud2 = FKL_VM_UD(sec);
            if (ud1->mt_ != ud2->mt_ || !ud1->mt_->__equal)
                return 0;
            else
                return ud1->mt_->__equal(fir, sec);
        } break;
        case FKL_TYPE_PAIR:
        case FKL_TYPE_BOX:
        case FKL_TYPE_VECTOR:
        case FKL_TYPE_HASHTABLE:
            goto nested_equal;
            break;
        case FKL_TYPE_VAR_REF:
            FKL_UNREACHABLE();
            break;
        default:
            fprintf(stderr,
                    "[%s: %d] %s: unknown value type!\n",
                    __FILE__,
                    __LINE__,
                    __FUNCTION__);
            abort();
            break;
        }
    } else
        return fir == sec;

nested_equal:
    fklVMpairVectorInit(&s, 8);
    fklVMpairVectorPushBack2(&s,
            (FklVMpair){ .car = FKL_TYPE_CAST(FklVMvalue *, fir),
                .cdr = FKL_TYPE_CAST(FklVMvalue *, sec) });
    int r = 1;
    while (!fklVMpairVectorIsEmpty(&s)) {
        const FklVMpair *top = fklVMpairVectorPopBackNonNull(&s);
        FklVMvalue *root1 = top->car;
        FklVMvalue *root2 = top->cdr;
        if (FKL_GET_TAG(root1) != FKL_GET_TAG(root2))
            r = 0;
        else if (!FKL_IS_PTR(root1) && !FKL_IS_PTR(root2) && root1 != root2)
            r = 0;
        else if (FKL_IS_PTR(root1) && FKL_IS_PTR(root2)) {
            if (root1->type_ != root2->type_)
                r = 0;
            else
                switch (root1->type_) {
                case FKL_TYPE_SYM:
                case FKL_TYPE_PROC:
                case FKL_TYPE_CPROC:
                    r = root1 == root2;
                    break;
                case FKL_TYPE_F64:
                    r = FKL_VM_F64(root1) == FKL_VM_F64(root2);
                    break;
                case FKL_TYPE_STR:
                    r = fklStringEqual(FKL_VM_STR(root1), FKL_VM_STR(root2));
                    break;
                case FKL_TYPE_BYTEVECTOR:
                    r = fklBytevectorEqual(FKL_VM_BVEC(root1),
                            FKL_VM_BVEC(root2));
                    break;
                case FKL_TYPE_PAIR:
                    r = 1;
                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = FKL_VM_CDR(root1),
                                .cdr = FKL_VM_CDR(root2),
                            });
                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = FKL_VM_CAR(root1),
                                .cdr = FKL_VM_CAR(root2),
                            });
                    break;
                case FKL_TYPE_BOX:
                    r = 1;
                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = FKL_VM_BOX(root1),
                                .cdr = FKL_VM_BOX(root2),
                            });
                    break;
                case FKL_TYPE_VECTOR: {
                    FklVMvec *vec1 = FKL_VM_VEC(root1);
                    FklVMvec *vec2 = FKL_VM_VEC(root2);
                    if (vec1->size != vec2->size)
                        r = 0;
                    else {
                        r = 1;
                        for (size_t i = vec1->size; i > 0; --i) {
                            fklVMpairVectorPushBack2(&s,
                                    (FklVMpair){
                                        .car = vec1->base[i - 1],
                                        .cdr = vec2->base[i - 1],
                                    });
                        }
                    }
                } break;
                case FKL_TYPE_BIGINT:
                    r = fklVMbigIntEqual(FKL_VM_BI(root1), FKL_VM_BI(root2));
                    break;
                case FKL_TYPE_USERDATA: {
                    FklVMvalueUd *ud1 = FKL_VM_UD(root1);
                    FklVMvalueUd *ud2 = FKL_VM_UD(root2);
                    if (ud1->mt_ != ud2->mt_ || !ud1->mt_->__equal)
                        r = 0;
                    else
                        r = ud1->mt_->__equal(root1, root2);
                } break;
                case FKL_TYPE_HASHTABLE: {
                    FklVMvalueHash *h1 = FKL_VM_HASH(root1);
                    FklVMvalueHash *h2 = FKL_VM_HASH(root2);
                    if (h1->eq_type != h2->eq_type
                            || h1->ht.count != h2->ht.count)
                        r = 0;
                    else {
                        FklValueHashMapNode *i1 = h1->ht.last;
                        FklValueHashMapNode *i2 = h2->ht.last;
                        for (; i1; i1 = i1->prev, i2 = i2->prev) {
                            fklVMpairVectorPushBack2(&s,
                                    (FklVMpair){ .car = i1->v, .cdr = i2->v });
                            fklVMpairVectorPushBack2(&s,
                                    (FklVMpair){ .car = i1->k, .cdr = i2->k });
                        }
                    }
                } break;
                case FKL_TYPE_VAR_REF:
                    FKL_UNREACHABLE();
                    break;
                default:
                    fprintf(stderr,
                            "[%s: %d] %s: unknown value type!\n",
                            __FILE__,
                            __LINE__,
                            __FUNCTION__);
                    abort();
                    break;
                }
        }
        if (!r)
            break;
    }
    fklVMpairVectorUninit(&s);
    return r;
}

static inline int
cmp_vm_ud(const FklVMvalue *a, const FklVMvalue *b, int *err) {
    return FKL_VM_UD(a)->mt_->__cmp(a, b, err);
}

static inline int is_cmpable_ud(const FklVMvalueUd *u) {
    return u->mt_->__cmp != NULL;
}

int fklVMvalueCmp(FklVMvalue *a, FklVMvalue *b, int *err) {
    int r = 0;
    *err = 0;
    if ((FKL_IS_F64(a) && fklIsVMnumber(b))
            || (FKL_IS_F64(b) && fklIsVMnumber(a))) {
        double af = fklVMgetDouble(a);
        double bf = fklVMgetDouble(b);
        r = isgreater(af, bf) ? 1 : (isless(af, bf) ? -1 : 0);
    } else if (FKL_IS_FIX(a) && FKL_IS_FIX(b)) {
        int64_t rr = FKL_GET_FIX(a) - FKL_GET_FIX(b);
        r = rr > 0 ? 1 : rr < 0 ? -1 : 0;
    } else if (FKL_IS_BIGINT(a) && FKL_IS_BIGINT(b))
        r = fklVMbigIntCmp(FKL_VM_BI(a), FKL_VM_BI(b));
    else if (FKL_IS_BIGINT(a) && FKL_IS_FIX(b))
        r = fklVMbigIntCmpI(FKL_VM_BI(a), FKL_GET_FIX(b));
    else if (FKL_IS_FIX(a) && FKL_IS_BIGINT(b))
        r = -1 * (fklVMbigIntCmpI(FKL_VM_BI(b), FKL_GET_FIX(a)));
    else if (FKL_IS_STR(a) && FKL_IS_STR(b))
        r = fklStringCmp(FKL_VM_STR(a), FKL_VM_STR(b));
    else if (FKL_IS_BYTEVECTOR(a) && FKL_IS_BYTEVECTOR(b))
        r = fklBytevectorCmp(FKL_VM_BVEC(a), FKL_VM_BVEC(b));
    else if (FKL_IS_CHR(a) && FKL_IS_CHR(b))
        r = FKL_GET_CHR(a) - FKL_GET_CHR(b);
    else if (FKL_IS_USERDATA(a) && is_cmpable_ud(FKL_VM_UD(a)))
        r = cmp_vm_ud(a, b, err);
    else if (FKL_IS_USERDATA(b) && is_cmpable_ud(FKL_VM_UD(b)))
        r = -cmp_vm_ud(b, a, err);
    else
        *err = 1;
    return r;
}

FklVMfpRW fklGetVMfpRwFromCstr(const char *mode) {
    int hasPlus = 0;
    int hasW = 0;
    switch (*(mode++)) {
    case 'w':
    case 'a':
        hasW = 1;
        break;
    case 'r':
        break;
    default:
        break;
    }

    while (*mode)
        switch (*(mode++)) {
        case '+':
            hasPlus = 1;
            goto ret;
            break;
        case 't':
        case 'b':
        case 'c':
        case 'n':
            break;
        default:
            goto ret;
            break;
        }
ret:
    if (hasPlus)
        return FKL_VM_FP_RW;
    else if (hasW)
        return FKL_VM_FP_W;
    return FKL_VM_FP_R;
}

int fklVMfpEof(FklVMvalueFp *vfp) { return feof(vfp->fp); }

int fklVMfpRewind(FklVMvalueFp *vfp, FklStringBuffer *b, size_t j) {
    return fklRewindStream(vfp->fp, b->buf + j, b->index - j);
}

int fklVMfpFileno(FklVMvalueFp *vfp) { return fileno(vfp->fp); }

int fklVMfpClose(FklVMvalueFp *vfp) {
    int r = 0;
    FILE *fp = vfp->fp;
    if (fp == NULL || fclose(fp) == EOF)
        r = 1;
    vfp->fp = NULL;
    return r;
}

void fklInitVMdll(FklVMvalue *rel, FklVM *exe) {
    FklVMvalueDll *dll = FKL_VM_DLL(rel);
    FklDllInitFunc init = (FklDllInitFunc)fklGetAddress("_fklInit", &dll->dll);
    if (init)
        init(dll, exe);
}

static inline void chanl_push_recv(FklVMvalueChanl *ch, FklVMchanlRecv *recv) {
    *(ch->recvq.tail) = recv;
    ch->recvq.tail = &recv->next;
}

static inline FklVMchanlRecv *chanl_pop_recv(FklVMvalueChanl *ch) {
    FklVMchanlRecv *r = ch->recvq.head;
    if (r) {
        ch->recvq.head = r->next;
        if (r->next == NULL)
            ch->recvq.tail = &ch->recvq.head;
    }
    return r;
}

static inline void chanl_push_send(FklVMvalueChanl *ch, FklVMchanlSend *send) {
    *(ch->sendq.tail) = send;
    ch->sendq.tail = &send->next;
}

static inline FklVMchanlSend *chanl_pop_send(FklVMvalueChanl *ch) {
    FklVMchanlSend *s = ch->sendq.head;
    if (s) {
        ch->sendq.head = s->next;
        if (s->next == NULL)
            ch->sendq.tail = &ch->sendq.head;
    }
    return s;
}

static inline void chanl_push_msg(FklVMvalueChanl *c, FklVMvalue *msg) {
    c->buf[c->sendx] = msg;
    c->sendx = (c->sendx + 1) % c->qsize;
    c->count++;
}

static inline FklVMvalue *chanl_pop_msg(FklVMvalueChanl *c) {
    FklVMvalue *r = c->buf[c->recvx];
    c->recvx = (c->recvx + 1) % c->qsize;
    c->count--;
    return r;
}

int fklChanlRecvOk(FklVMvalueChanl *ch, FklVMvalue **slot) {
    uv_mutex_lock(&ch->lock);
    FklVMchanlSend *send = chanl_pop_send(ch);
    if (send) {
        if (ch->count) {
            *slot = chanl_pop_msg(ch);
            chanl_push_msg(ch, send->msg);
        } else
            *slot = send->msg;
        uv_cond_signal(&send->cond);
        uv_mutex_unlock(&ch->lock);
        return 1;
    } else if (ch->count) {
        *slot = chanl_pop_msg(ch);
        uv_mutex_unlock(&ch->lock);
        return 1;
    } else {
        uv_mutex_unlock(&ch->lock);
        return 0;
    }
}

void fklChanlRecv(FklVMvalueChanl *ch, uint32_t slot, FklVM *exe) {
    uv_mutex_lock(&ch->lock);
    FklVMchanlSend *send = chanl_pop_send(ch);
    if (send) {
        if (ch->count) {
            exe->base[slot] = chanl_pop_msg(ch);
            chanl_push_msg(ch, send->msg);
        } else
            exe->base[slot] = send->msg;
        uv_cond_signal(&send->cond);
        uv_mutex_unlock(&ch->lock);
        return;
    } else if (ch->count) {
        exe->base[slot] = chanl_pop_msg(ch);
        uv_mutex_unlock(&ch->lock);
        return;
    } else {
        FklVMchanlRecv r = {
            .exe = exe,
            .slot = slot,
        };
        if (uv_cond_init(&r.cond)) {
            FKL_UNREACHABLE();
        }
        chanl_push_recv(ch, &r);
        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            uv_cond_wait(&r.cond, &ch->lock);
            uv_mutex_unlock(&ch->lock);
        }
        uv_cond_destroy(&r.cond);
        return;
    }
}

void fklChanlSend(FklVMvalueChanl *ch, FklVMvalue *msg, FklVM *exe) {
    uv_mutex_lock(&ch->lock);
    FklVMchanlRecv *recv = chanl_pop_recv(ch);
    if (recv) {
        recv->exe->base[recv->slot] = msg;
        uv_cond_signal(&recv->cond);
        uv_mutex_unlock(&ch->lock);
        return;
    } else if (ch->count < ch->qsize) {
        chanl_push_msg(ch, msg);
        uv_mutex_unlock(&ch->lock);
        return;
    } else {
        FklVMchanlSend s = {
            .msg = msg,
        };
        if (uv_cond_init(&s.cond)) {
            FKL_UNREACHABLE();
        }
        chanl_push_send(ch, &s);
        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            uv_cond_wait(&s.cond, &ch->lock);
            uv_mutex_unlock(&ch->lock);
        }
        uv_cond_destroy(&s.cond);
        return;
    }
}

uint64_t fklVMchanlRecvqLen(FklVMvalueChanl *ch) {
    uint64_t l = 0;
    uv_mutex_lock(&ch->lock);
    for (FklVMchanlRecv *q = ch->recvq.head; q; q = q->next)
        ++l;
    uv_mutex_unlock(&ch->lock);
    return l;
}

uint64_t fklVMchanlSendqLen(FklVMvalueChanl *ch) {
    uint64_t l = 0;
    uv_mutex_lock(&ch->lock);
    for (FklVMchanlSend *q = ch->sendq.head; q; q = q->next)
        ++l;
    uv_mutex_unlock(&ch->lock);
    return l;
}

uint64_t fklVMchanlMessageNum(FklVMvalueChanl *ch) {
    uint64_t r = 0;
    uv_mutex_lock(&ch->lock);
    r = ch->count;
    uv_mutex_unlock(&ch->lock);
    return r;
}

int fklVMchanlFull(FklVMvalueChanl *ch) {
    int r = 0;
    uv_mutex_lock(&ch->lock);
    r = ch->count >= ch->qsize;
    uv_mutex_unlock(&ch->lock);
    return r;
}

int fklVMchanlEmpty(FklVMvalueChanl *ch) {
    int r = 0;
    uv_mutex_lock(&ch->lock);
    r = ch->count == 0;
    uv_mutex_unlock(&ch->lock);
    return r;
}

static uintptr_t _f64_hashFunc(const FklVMvalue *v) {
    union {
        double f;
        uint64_t i;
    } t = {
        .f = FKL_VM_F64(v),
    };
    return t.i;
}

static uintptr_t _str_hashFunc(const FklVMvalue *v) {
    return fklStringHash(FKL_VM_STR(v));
}

static uintptr_t _bytevector_hashFunc(const FklVMvalue *v) {
    return fklBytevectorHash(FKL_VM_BVEC(v));
}

static uintptr_t _vector_hashFunc(const FklVMvalue *v) {
    const FklVMvec *vec = FKL_VM_VEC(v);
    uintptr_t seed = vec->size;
    for (size_t i = 0; i < vec->size; ++i)
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(vec->base[i]));
    return seed;
}

static uintptr_t _pair_hashFunc(const FklVMvalue *v) {
    uintptr_t seed = 2;
    seed = fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_CAR(v)));
    seed = fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_CDR(v)));
    return seed;
}

static uintptr_t _box_hashFunc(const FklVMvalue *v) {
    uintptr_t seed = 1;
    return fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_BOX(v)));
}

static size_t _userdata_hashFunc(const FklVMvalue *v) {
    size_t (*hashv)(const FklVMvalue *) = FKL_VM_UD(v)->mt_->__hash;
    if (hashv)
        return hashv(v);
    else {
        uintptr_t t = FKL_TYPE_CAST(uintptr_t, v) >> FKL_UNUSEDBITNUM;
        return fklHash64Shift(t);
    }
}

static size_t _hashTable_hashFunc(const FklVMvalue *v) {
    FklVMvalueHash *hash = FKL_VM_HASH(v);
    uintptr_t seed = hash->ht.count + hash->eq_type;
    for (FklValueHashMapNode *list = hash->ht.first; list; list = list->next) {
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(list->k));
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(list->v));
    }
    return seed;
}

static uintptr_t (*const valueHashFuncTable[FKL_VM_VALUE_GC_TYPE_NUM])(
        const FklVMvalue *) = {
    [FKL_TYPE_F64] = _f64_hashFunc,
    [FKL_TYPE_STR] = _str_hashFunc,
    [FKL_TYPE_VECTOR] = _vector_hashFunc,
    [FKL_TYPE_PAIR] = _pair_hashFunc,
    [FKL_TYPE_BOX] = _box_hashFunc,
    [FKL_TYPE_BYTEVECTOR] = _bytevector_hashFunc,
    [FKL_TYPE_USERDATA] = _userdata_hashFunc,
    [FKL_TYPE_HASHTABLE] = _hashTable_hashFunc,
};

static uintptr_t VMvalueHashFunc(const FklVMvalue *root) {
    size_t sum = 0;
    size_t (*valueHashFunc)(const FklVMvalue *) = NULL;
    if (fklIsVMint(root))
        sum += fklVMintegerHashv(root);
    else if (FKL_IS_PTR(root)
             && (valueHashFunc = valueHashFuncTable[root->type_]))
        sum += valueHashFunc(root);
    else
        sum += ((uintptr_t)root >> FKL_UNUSEDBITNUM);
    return sum;
}

uintptr_t fklVMvalueEqualHashv(const FklVMvalue *v) {
    if (fklIsVMint(v))
        return fklVMintegerHashv(v);
    else
        return VMvalueHashFunc(v);
}

void fklAtomicVMhashTable(FklVMvalue *pht, FklVMgc *gc) {
    FklVMvalueHash *table = FKL_VM_HASH(pht);
    for (FklValueHashMapNode *list = table->ht.first; list; list = list->next) {
        fklVMgcToGray(list->k, gc);
        fklVMgcToGray(list->v, gc);
    }
}

const char *fklGetVMhashTablePrefix(const FklVMvalueHash *ht) {
    static const char *prefix[] = {
        "#hash(",
        "#hasheqv(",
        "#hashequal(",
    };
    return prefix[ht->eq_type];
}

static uintptr_t (*const VMhashFunc[])(const FklVMvalue *k) = {
    [FKL_HASH_EQ] = fklVMvalueEqHashv,
    [FKL_HASH_EQV] = fklVMvalueEqvHashv,
    [FKL_HASH_EQUAL] = fklVMvalueEqualHashv,
};

static int (*const VMhashEqFunc[])(const FklVMvalue *a, const FklVMvalue *b) = {
    [FKL_HASH_EQ] = fklVMvalueEq,
    [FKL_HASH_EQV] = fklVMvalueEqv,
    [FKL_HASH_EQUAL] = fklVMvalueEqual,
};

static inline FklValueHashMapElm *
vmhash_find_node(FklVMvalueHash *ht, FklVMvalue *key, uintptr_t *hashv) {
    *hashv = VMhashFunc[ht->eq_type](key);
    int (*const eq_func)(const FklVMvalue *, const FklVMvalue *) =
            VMhashEqFunc[ht->eq_type];

    FklValueHashMapNode *const *pp = fklValueHashMapBucket(&ht->ht, *hashv);

    for (; *pp; pp = &(*pp)->bkt_next) {
        if (eq_func(key, (*pp)->k)) {
            return &(*pp)->elm;
        }
    }
    return NULL;
}

FklValueHashMapElm *fklVMhashTableGet(FklVMvalueHash *ht, FklVMvalue *key) {
    uintptr_t hashv;
    return vmhash_find_node(ht, key, &hashv);
}

FklValueHashMapElm *
fklVMhashTableRef1(FklVMvalueHash *ht, FklVMvalue *key, FklVMvalue *v) {
    uintptr_t hashv;
    FklValueHashMapElm *r = vmhash_find_node(ht, key, &hashv);
    if (r)
        return r;
    else {
        FklValueHashMapNode *node = fklValueHashMapCreateNode2(hashv, key);
        node->v = v;
        fklValueHashMapInsertNode(&ht->ht, node);
        return &node->elm;
    }
}

FklValueHashMapElm *
fklVMhashTableSet(FklVMvalueHash *ht, FklVMvalue *key, FklVMvalue *v) {
    uintptr_t hashv;
    FklValueHashMapElm *r = vmhash_find_node(ht, key, &hashv);
    if (r) {
        r->v = v;
        return r;
    } else {
        FklValueHashMapNode *node = fklValueHashMapCreateNode2(hashv, key);
        node->v = v;
        fklValueHashMapInsertNode(&ht->ht, node);
        return &node->elm;
    }
}

int fklVMhashTableDel(FklVMvalueHash *ht,
        FklVMvalue *key,
        FklVMvalue **pv,
        FklVMvalue **pk) {
    uintptr_t hashv = VMhashFunc[ht->eq_type](key);
    int (*const eq_func)(const FklVMvalue *, const FklVMvalue *) =
            VMhashEqFunc[ht->eq_type];

    FklValueHashMapNode *const *pp = fklValueHashMapBucket(&ht->ht, hashv);

    for (; *pp; pp = &(*pp)->bkt_next) {
        if (eq_func(key, (*pp)->k)) {
            if (pk)
                *pk = (*pp)->k;
            if (pv)
                *pv = (*pp)->v;
            fklValueHashMapDelNode(&ht->ht,
                    FKL_TYPE_CAST(FklValueHashMapNode **, pp));
            return 1;
        }
    }
    return 0;
}

#define NEW_OBJ(TYPE) (FklVMvalue *)fklZcalloc(1, sizeof(TYPE));

FklVMvalue *fklCreateVMvaluePair(FklVM *exe, FklVMvalue *car, FklVMvalue *cdr) {
    FklVMvalue *r = NEW_OBJ(FklVMvaluePair);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_PAIR;
    FKL_VM_CAR(r) = car;
    FKL_VM_CDR(r) = cdr;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvaluePairWithCar(FklVM *exe, FklVMvalue *car) {
    FklVMvalue *r = NEW_OBJ(FklVMvaluePair);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_PAIR;
    FKL_VM_CAR(r) = car;
    FKL_VM_CDR(r) = FKL_VM_NIL;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvaluePairNil(FklVM *exe) {
    FklVMvalue *r = NEW_OBJ(FklVMvaluePair);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_PAIR;
    FKL_VM_CAR(r) = FKL_VM_NIL;
    FKL_VM_CDR(r) = FKL_VM_NIL;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueVec(FklVM *exe, size_t size) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueVec) + size * sizeof(FklVMvalue *));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    v->size = size;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *
fklCreateVMvalueVecWithPtr(FklVM *exe, size_t size, FklVMvalue *const *ptr) {
    size_t ss = size * sizeof(FklVMvalue *);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, sizeof(FklVMvalueVec) + ss);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    memcpy(v->base, ptr, ss);
    v->size = size;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *
fklCreateVMvalueVec3(FklVM *exe, FklVMvalue *a, FklVMvalue *b, FklVMvalue *c) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueVec) + 3 * sizeof(FklVMvalue *));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    v->base[0] = a;
    v->base[1] = b;
    v->base[2] = c;
    v->size = 3;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueVec4(FklVM *exe,
        FklVMvalue *a,
        FklVMvalue *b,
        FklVMvalue *c,
        FklVMvalue *d) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueVec) + 4 * sizeof(FklVMvalue *));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    v->base[0] = a;
    v->base[1] = b;
    v->base[2] = c;
    v->base[3] = d;
    v->size = 4;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueVec5(FklVM *exe,
        FklVMvalue *a,
        FklVMvalue *b,
        FklVMvalue *c,
        FklVMvalue *d,
        FklVMvalue *f) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueVec) + 5 * sizeof(FklVMvalue *));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    v->base[0] = a;
    v->base[1] = b;
    v->base[2] = c;
    v->base[3] = d;
    v->base[4] = f;
    v->size = 5;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueVec6(FklVM *exe,
        FklVMvalue *a,
        FklVMvalue *b,
        FklVMvalue *c,
        FklVMvalue *d,
        FklVMvalue *f,
        FklVMvalue *e) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueVec) + 6 * sizeof(FklVMvalue *));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvec *v = FKL_VM_VEC(r);
    v->base[0] = a;
    v->base[1] = b;
    v->base[2] = c;
    v->base[3] = d;
    v->base[4] = f;
    v->base[5] = e;
    v->size = 6;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBox(FklVM *exe, FklVMvalue *b) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueBox);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BOX;
    FKL_VM_BOX(r) = b;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBoxNil(FklVM *exe) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueBox);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BOX;
    FKL_VM_BOX(r) = FKL_VM_NIL;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueF64(FklVM *exe, double d) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueF64);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_F64;
    FKL_VM_F64(r) = d;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueStr(FklVM *exe, const FklString *s) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueStr) + s->size * sizeof(s->str[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_STR;
    FklString *rs = FKL_VM_STR(r);
    rs->size = s->size;
    memcpy(rs->str, s->str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueStr2(FklVM *exe, size_t size, const char *str) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueStr) + size * sizeof(str[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_STR;
    FklString *rs = FKL_VM_STR(r);
    rs->size = size;
    if (str)
        memcpy(rs->str, str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueSym(FklVM *exe, const FklString *s) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueSym) + s->size * sizeof(s->str[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_SYM;
    FklString *rs = FKL_VM_SYM(r);
    rs->size = s->size;
    memcpy(rs->str, s->str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueSym2(FklVM *exe, size_t size, const char *str) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueSym) + size * sizeof(str[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_SYM;
    FklString *rs = FKL_VM_SYM(r);
    rs->size = size;
    if (str)
        memcpy(rs->str, str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBvec(FklVM *exe, const FklBytevector *b) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueBvec) + b->size * sizeof(b->ptr[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BYTEVECTOR;
    FklBytevector *bvec = FKL_VM_BVEC(r);
    bvec->size = b->size;
    memcpy(bvec->ptr, b->ptr, bvec->size * sizeof(bvec->ptr[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBvec2(FklVM *exe, size_t size, const uint8_t *ptr) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueBvec) + size * sizeof(ptr[0]));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BYTEVECTOR;
    FklBytevector *bvec = FKL_VM_BVEC(r);
    bvec->size = size;
    if (ptr)
        memcpy(bvec->ptr, ptr, bvec->size * sizeof(bvec->ptr[0]));
    fklAddToGC(r, exe);
    return r;
}

// static FKL_ALWAYS_INLINE FklVMvalueError *as_err(const FklVMvalue *v) {
//     FKL_ASSERT(fklIsVMvalueError(v));
//     return FKL_TYPE_CAST(FklVMvalueError *, v);
// }

static void _error_userdata_as_princ(const FklVMvalue *ud,
        FklCodeBuilder *build,
        FklVM *exe) {
    // FKL_DECL_UD_DATA(err, FklVMerror, ud);
    FklVMvalueError *err = FKL_VM_ERR(ud);
    fklPrintString2(FKL_VM_STR(err->message), build);
}

static void _error_userdata_as_prin1(const FklVMvalue *ud,
        FklCodeBuilder *build,
        FklVM *exe) {
    // FKL_DECL_UD_DATA(err, FklVMerror, ud);
    FklVMvalueError *err = FKL_VM_ERR(ud);
    fklVMformat(exe,
            build,
            "#<err t: %S, message: %S>",
            NULL,
            2,
            (FklVMvalue *[]){ err->type, err->message });
}

static void _error_userdata_atomic(const FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(err, FklVMerror, v);
    FklVMvalueError *err = FKL_VM_ERR(v);
    fklVMgcToGray(err->message, gc);
    fklVMgcToGray(err->type, gc);
}

static FklVMudMetaTable const ErrorUserDataMetaTable = {
    // .size = sizeof(FklVMerror),
    .size = sizeof(FklVMvalueError),
    .__as_princ = _error_userdata_as_princ,
    .__as_prin1 = _error_userdata_as_prin1,
    .__atomic = _error_userdata_atomic,
};

FklVMvalue *
fklCreateVMvalueError(FklVM *exe, FklVMvalue *type, FklVMvalue *message) {
    FKL_ASSERT(FKL_IS_SYM(type) && FKL_IS_STR(message));
    FklVMvalue *r = fklCreateVMvalueUd(exe, &ErrorUserDataMetaTable, NULL);
    FklVMvalueError *err = FKL_VM_ERR(r);
    err->type = type;
    err->message = message;
    return r;
}

int fklIsVMvalueError(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &ErrorUserDataMetaTable;
}

static inline void init_chanl_sendq(struct FklVMchanlSendq *q) {
    q->head = NULL;
    q->tail = &q->head;
}

static inline void init_chanl_recvq(struct FklVMchanlRecvq *q) {
    q->head = NULL;
    q->tail = &q->head;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_chanl_userdata_as_print, "chanl");

// static FKL_ALWAYS_INLINE FklVMvalueChanl *as_chanl(const FklVMvalue *v) {
//     FKL_ASSERT(fklIsVMvalueChanl(v));
//     return FKL_TYPE_CAST(FklVMvalueChanl *, v);
// }

static void _chanl_userdata_atomic(const FklVMvalue *root, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(ch, FklVMchanl, root);
    FklVMvalueChanl *ch = FKL_VM_CHANL(root);

    if (ch->recvx < ch->sendx) {

        FklVMvalue **const end = &ch->buf[ch->sendx];
        for (FklVMvalue **buf = &ch->buf[ch->recvx]; buf < end; buf++)
            fklVMgcToGray(*buf, gc);
    } else {
        FklVMvalue **end = &ch->buf[ch->qsize];
        FklVMvalue **buf = &ch->buf[ch->recvx];
        for (; buf < end; buf++)
            fklVMgcToGray(*buf, gc);

        buf = ch->buf;
        end = &ch->buf[ch->sendx];
        for (; buf < end; buf++)
            fklVMgcToGray(*buf, gc);
    }
    for (FklVMchanlSend *s = ch->sendq.head; s; s = s->next)
        fklVMgcToGray(s->msg, gc);
}

static FklVMudMetaTable const ChanlUserDataMetaTable = {
    // .size = sizeof(FklVMchanl),
    .size = sizeof(FklVMvalueChanl),
    .__as_princ = _chanl_userdata_as_print,
    .__as_prin1 = _chanl_userdata_as_print,
    .__atomic = _chanl_userdata_atomic,
};

FklVMvalue *fklCreateVMvalueChanl(FklVM *exe, uint32_t qsize) {
    FklVMvalue *r = fklCreateVMvalueUd2(exe,
            &ChanlUserDataMetaTable,
            sizeof(FklVMvalue *) * qsize,
            NULL);
    FklVMvalueChanl *ch = FKL_VM_CHANL(r);
    ch->qsize = qsize;
    uv_mutex_init(&ch->lock);
    init_chanl_sendq(&ch->sendq);
    init_chanl_recvq(&ch->recvq);
    return r;
}

int fklIsVMvalueChanl(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &ChanlUserDataMetaTable;
}

// static FKL_ALWAYS_INLINE FklVMvalueFp *as_vm_fp(const FklVMvalue *v) {
//     FKL_ASSERT(fklIsVMvalueFp(v));
//     return FKL_TYPE_CAST(FklVMvalueFp *, v);
// }

static int _fp_userdata_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(fp, FklVMfp, ud);
    // FklVMfp *fp = &as_vm_fp(ud)->fp;
    // fklUninitVMfp(fp);
    fklVMfpClose(FKL_VM_FP(ud));
    return FKL_VM_UD_FINALIZE_NOW;
}

// static void
// _fp_userdata_as_print(const FklVMvalue *ud, FklCodeBuilder *buf, FklVM *exe)
// {
//     // FKL_DECL_UD_DATA(vfp, FklVMfp, ud);
//     FklVMfp *vfp = &as_vm_fp(ud)->fp;
//     fklCodeBuilderFmt(buf, "#<fp %p>", vfp);
// }

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_fp_userdata_as_print, "fp");

static FklVMudMetaTable const FpUserDataMetaTable = {
    // .size = sizeof(FklVMfp),
    .size = sizeof(FklVMvalueFp),
    .__as_princ = _fp_userdata_as_print,
    .__as_prin1 = _fp_userdata_as_print,
    .__finalizer = _fp_userdata_finalizer,
};

#define VM_FP_STATIC_INIT(FP, RW)                                              \
    (FklVMvalueFp) {                                                           \
        .next_ = NULL, .gray_next_ = NULL, .mark_ = FKL_MARK_B,                \
        .type_ = FKL_TYPE_USERDATA, .mt_ = &FpUserDataMetaTable, .dll_ = NULL, \
        .fp = (FP), .rw = (RW),                                                \
    }

void fklInitVMvalueFp(FklVMvalueFp *vfp, FILE *fp, FklVMfpRW rw) {
    *vfp = VM_FP_STATIC_INIT(fp, rw);
}

FklVMvalue *fklCreateVMvalueFp(FklVM *exe, FILE *fp, FklVMfpRW rw) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &FpUserDataMetaTable, NULL);
    FklVMvalueFp *vfp = FKL_VM_FP(r);
    vfp->fp = fp;
    vfp->rw = rw;
    return r;
}

int fklIsVMvalueFp(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &FpUserDataMetaTable;
}

FklVMvalue *
fklCreateVMvalueBigIntWithString(FklVM *exe, const FklString *str, int base) {
    if (base == 10)
        return fklCreateVMvalueBigIntWithDecString(exe, str);
    else if (base == 16)
        return fklCreateVMvalueBigIntWithHexString(exe, str);
    else
        return fklCreateVMvalueBigIntWithOctString(exe, str);
}

typedef struct {
    FklVM *exe;
    FklVMvalue *bi;
} VMbigIntInitCtx;

static FklBigIntDigit *vmbigint_alloc_cb(void *ptr, size_t size) {
    VMbigIntInitCtx *ctx = (VMbigIntInitCtx *)ptr;
    ctx->bi = fklCreateVMvalueBigInt(ctx->exe, size);
    return FKL_VM_BI(ctx->bi)->digits;
}

static int64_t *vmbigint_num_cb(void *ptr) {
    VMbigIntInitCtx *ctx = (VMbigIntInitCtx *)ptr;
    return &FKL_VM_BI(ctx->bi)->num;
}

static const FklBigIntInitWithCharBufMethodTable
        VMbigIntInitWithCharBufMethodTable = {
            .alloc = vmbigint_alloc_cb,
            .num = vmbigint_num_cb,
        };

FklVMvalue *fklCreateVMvalueBigIntWithDecString(FklVM *exe,
        const FklString *str) {
    VMbigIntInitCtx ctx = {
        .exe = exe,
        .bi = NULL,
    };
    fklInitBigIntWithDecCharBuf2(&ctx,
            &VMbigIntInitWithCharBufMethodTable,
            str->str,
            str->size);
    return ctx.bi;
}

FklVMvalue *fklCreateVMvalueBigIntWithHexString(FklVM *exe,
        const FklString *str) {
    VMbigIntInitCtx ctx = {
        .exe = exe,
        .bi = NULL,
    };
    fklInitBigIntWithHexCharBuf2(&ctx,
            &VMbigIntInitWithCharBufMethodTable,
            str->str,
            str->size);
    return ctx.bi;
}

FklVMvalue *fklCreateVMvalueBigIntWithOctString(FklVM *exe,
        const FklString *str) {
    VMbigIntInitCtx ctx = {
        .exe = exe,
        .bi = NULL,
    };
    fklInitBigIntWithOctCharBuf2(&ctx,
            &VMbigIntInitWithCharBufMethodTable,
            str->str,
            str->size);
    return ctx.bi;
}

FklVMvalue *fklCreateVMvalueBigInt(FklVM *exe, size_t size) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
            sizeof(FklVMvalueBigInt) + size * sizeof(FklBigIntDigit));
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BIGINT;
    FklVMvalueBigInt *b = FKL_VM_BI(r);
    b->num = size + 1;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBigInt2(FklVM *exe, const FklBigInt *bi) {
    FklVMvalue *r = fklCreateVMvalueBigInt(exe, fklAbs(bi->num));
    FklVMvalueBigInt *b = FKL_VM_BI(r);
    FklBigInt t = {
        .digits = b->digits,
        .num = 0,
        .size = fklAbs(b->num),
        .const_size = 1,
    };
    fklSetBigInt(&t, bi);
    b->num = t.num;
    return r;
}

FklVMvalue *
fklCreateVMvalueBigInt3(FklVM *exe, const FklBigInt *bi, size_t size) {
    FKL_ASSERT(size >= (size_t)fklAbs(bi->num));
    FklVMvalue *r = fklCreateVMvalueBigInt(exe, fklAbs(bi->num));
    FklVMvalueBigInt *b = FKL_VM_BI(r);
    FklBigInt t = {
        .digits = b->digits,
        .num = 0,
        .size = size,
        .const_size = 1,
    };
    fklSetBigInt(&t, bi);
    b->num = t.num;
    return r;
}

FklVMvalue *fklCreateVMvalueBigIntWithI64(FklVM *exe, int64_t i) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = {
        .digits = digits,
        .num = 0,
        .size = FKL_MAX_INT64_DIGITS_COUNT,
        .const_size = 1,
    };
    fklSetBigIntI(&bi, i);
    return fklCreateVMvalueBigInt2(exe, &bi);
}

FklVMvalue *fklCreateVMvalueBigIntWithU64(FklVM *exe, uint64_t u) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = {
        .digits = digits,
        .num = 0,
        .size = FKL_MAX_INT64_DIGITS_COUNT,
        .const_size = 1,
    };
    fklSetBigIntU(&bi, u);
    return fklCreateVMvalueBigInt2(exe, &bi);
}

FklVMvalue *fklCreateVMvalueBigIntWithF64(FklVM *exe, double d) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = {
        .digits = digits,
        .num = 0,
        .size = FKL_MAX_INT64_DIGITS_COUNT,
        .const_size = 1,
    };
    fklSetBigIntD(&bi, d);
    return fklCreateVMvalueBigInt2(exe, &bi);
}

FklVMvalue *fklVMbigIntAddI(FklVM *exe, const FklVMvalueBigInt *a, int64_t b) {
    FklVMvalue *r =
            fklCreateVMvalueBigIntWithOther2(exe, a, fklAbs(a->num) + 1);
    FklVMvalueBigInt *a0 = FKL_VM_BI(r);
    FklBigInt a1 = {
        .digits = a0->digits,
        .num = 0,
        .size = fklAbs(a0->num),
        .const_size = 1,
    };
    fklSetBigIntWithVMbigInt(&a1, a);
    fklAddBigIntI(&a1, b);
    a0->num = a1.num;
    return r;
}

FklVMvalue *fklVMbigIntSubI(FklVM *exe, const FklVMvalueBigInt *a, int64_t b) {
    FklVMvalue *r =
            fklCreateVMvalueBigIntWithOther2(exe, a, fklAbs(a->num) + 1);
    FklVMvalueBigInt *a0 = FKL_VM_BI(r);
    FklBigInt a1 = {
        .digits = a0->digits,
        .num = 0,
        .size = fklAbs(a0->num),
        .const_size = 1,
    };
    fklSetBigIntWithVMbigInt(&a1, a);
    fklSubBigIntI(&a1, b);
    a0->num = a1.num;
    return r;
}

FklVMvalue *fklCreateVMvalueProc2(FklVM *exe,
        const FklInstruction *spc,
        uint64_t cpc,
        FklVMvalue *codeObj,
        uint32_t pid,
        const FklVMvalueProtos *pts) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueProc);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_PROC;
    FklVMvalueProc *proc = FKL_VM_PROC(r);

    FklFuncPrototype *pt = &pts->p.pa[pid];

    proc->pts = FKL_TYPE_CAST(FklVMvalueProtos *, pts);
    proc->spc = spc;
    proc->end = spc + cpc;
    proc->name = pt->name;
    proc->protoId = pid;
    proc->lcount = pt->lcount;
    proc->codeObj = codeObj;
    proc->konsts = pt->konsts;

    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueProc(FklVM *exe,
        FklVMvalue *codeObj,
        uint32_t pid,
        const FklVMvalueProtos *pts) {
    FklByteCode *bc = &FKL_VM_CO(codeObj)->bc;
    return fklCreateVMvalueProc2(exe, bc->code, bc->len, codeObj, pid, pts);
}

FklVMvalue *fklCreateVMvalueHash(FklVM *exe, FklHashTableEqType type) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueHash);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_HASHTABLE;
    fklValueHashMapInit(&FKL_VM_HASH(r)->ht);
    FKL_VM_HASH(r)->eq_type = type;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueHashEq(FklVM *exe) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueHash);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_HASHTABLE;
    fklValueHashMapInit(&FKL_VM_HASH(r)->ht);
    FKL_VM_HASH(r)->eq_type = FKL_HASH_EQ;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueHashEqv(FklVM *exe) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueHash);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_HASHTABLE;
    fklValueHashMapInit(&FKL_VM_HASH(r)->ht);
    FKL_VM_HASH(r)->eq_type = FKL_HASH_EQV;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueHashEqual(FklVM *exe) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueHash);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_HASHTABLE;
    fklValueHashMapInit(&FKL_VM_HASH(r)->ht);
    FKL_VM_HASH(r)->eq_type = FKL_HASH_EQUAL;
    fklAddToGC(r, exe);
    return r;
}

static FKL_ALWAYS_INLINE FklVMvalueCodeObj *as_co(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueCodeObj(v));
    return FKL_TYPE_CAST(FklVMvalueCodeObj *, v);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_code_obj_userdata_as_print, "code-obj");

static int _code_obj_userdata_finalizer(FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, FklByteCodelnt, v);
    FklByteCodelnt *t = &as_co(v)->bcl;
    fklUninitByteCodelnt(t);
    return FKL_VM_UD_FINALIZE_NOW;
}

static void __code_obj_atomic(const FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, FklByteCodelnt, v);
    FklByteCodelnt *t = &as_co(v)->bcl;
    fklVMgcMarkCodeObject(gc, t);
}

static FklVMudMetaTable const CodeObjUserDataMetaTable = {
    // .size = sizeof(FklByteCodelnt),
    .size = sizeof(FklVMvalueCodeObj),
    .__as_princ = _code_obj_userdata_as_print,
    .__as_prin1 = _code_obj_userdata_as_print,
    .__atomic = __code_obj_atomic,
    .__finalizer = _code_obj_userdata_finalizer,
};

FklVMvalue *fklCreateVMvalueCodeObj1(FklVM *exe) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &CodeObjUserDataMetaTable, NULL);
    fklInitByteCodelnt(FKL_VM_CO(r), 0);
    return r;
}

FklVMvalue *fklCreateVMvalueCodeObjMove(FklVM *exe, FklByteCodelnt *bcl) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &CodeObjUserDataMetaTable, NULL);
    fklMoveByteCodelnt(FKL_VM_CO(r), bcl);
    return r;
}

FklVMvalue *fklCreateVMvalueCodeObj(FklVM *exe, const FklByteCodelnt *bcl) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &CodeObjUserDataMetaTable, NULL);
    fklSetByteCodelnt(FKL_VM_CO(r), bcl);
    return r;
}

int fklIsVMvalueCodeObj(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &CodeObjUserDataMetaTable;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_dll_userdata_as_print, "dll");

// static FKL_ALWAYS_INLINE FklVMvalueDll *as_dll(const FklVMvalue *v) {
//     FKL_ASSERT(fklIsVMvalueDll(v));
//     return FKL_TYPE_CAST(FklVMvalueDll *, v);
// }

static void _dll_userdata_atomic(const FklVMvalue *root, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(dll, FklVMdll, root);
    FklVMvalueDll *dll = FKL_VM_DLL(root);
    fklVMgcToGray(dll->pd, gc);
}

static int _dll_userdata_finalizer(FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(dll, FklVMdll, v);
    FklVMvalueDll *dll = FKL_VM_DLL(v);
    FklDllUninitFunc uninit =
            (FklDllUninitFunc)fklGetAddress("_fklUninit", &dll->dll);
    if (uninit)
        uninit();
    uv_dlclose(&dll->dll);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const DllUserDataMetaTable = {
    // .size = sizeof(FklVMdll),
    .size = sizeof(FklVMvalueDll),
    .__as_princ = _dll_userdata_as_print,
    .__as_prin1 = _dll_userdata_as_print,
    .__atomic = _dll_userdata_atomic,
    .__finalizer = _dll_userdata_finalizer,
};

FklVMvalue *
fklCreateVMvalueDll(FklVM *exe, const char *dllName, FklVMvalue **errorStr) {
    size_t len = strlen(dllName) + strlen(FKL_DLL_FILE_TYPE) + 1;
    char *realDllName = (char *)fklZmalloc(len);
    FKL_ASSERT(realDllName);
    strcpy(realDllName, dllName);
    strcat(realDllName, FKL_DLL_FILE_TYPE);
    char *rpath = fklRealpath(realDllName);
    if (rpath) {
        fklZfree(realDllName);
        realDllName = rpath;
    }
    uv_lib_t lib;
    if (uv_dlopen(realDllName, &lib)) {
        *errorStr = fklCreateVMvalueStrFromCstr(exe, uv_dlerror(&lib));
        uv_dlclose(&lib);
        goto err;
    }
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DllUserDataMetaTable, NULL);
    FklVMvalueDll *dll = FKL_VM_DLL(r);
    dll->dll = lib;
    dll->pd = FKL_VM_NIL;
    fklZfree(realDllName);
    return r;
err:
    fklZfree(realDllName);
    return NULL;
}

int fklIsVMvalueDll(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &DllUserDataMetaTable;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_libs_userdata_as_print, "libs");

static FKL_ALWAYS_INLINE FklVMvalueLibs *as_libs(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueLibs(v));
    return FKL_TYPE_CAST(FklVMvalueLibs *, v);
}

static void _libs_userdata_atomic(const FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, struct FklVMlibs, v);
    // struct FklVMlibs *t = &as_libs(v)->_libs;
    FklVMvalueLibs *t = as_libs(v);

    for (size_t i = 1; i <= t->count; ++i) {
        FklVMlib *lib = &t->libs[i];
        fklVMgcToGray(lib->proc, gc);
        FklVMvalue **cur = lib->loc;
        FklVMvalue **const end = cur + lib->count;
        for (; cur < end; ++cur)
            fklVMgcToGray(*cur, gc);
    }
}

static int _libs_userdata_finalizer(FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, struct FklVMlibs, v);
    // struct FklVMlibs *t = &as_libs(v)->_libs;
    FklVMvalueLibs *t = as_libs(v);
    uv_mutex_destroy(&t->lock);
    for (size_t i = 1; i <= t->count; i++)
        fklUninitVMlib(&t->libs[i]);
    t->count = 0;
    fklZfree(t->libs);
    t->libs = NULL;

    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const LibsUserDataMetaTable = {
    // .size = sizeof(struct FklVMlibs),
    .size = sizeof(FklVMvalueLibs),
    .__as_princ = _libs_userdata_as_print,
    .__as_prin1 = _libs_userdata_as_print,
    .__atomic = _libs_userdata_atomic,
    .__finalizer = _libs_userdata_finalizer,
};

FklVMvalueLibs *fklCreateVMvalueLibs(FklVM *exe) {
    FklVMvalueLibs *r = (FklVMvalueLibs *)fklCreateVMvalueUd(exe,
            &LibsUserDataMetaTable,
            NULL);
    uv_mutex_init_recursive(&r->lock);
    return r;
}

int fklIsVMvalueLibs(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &LibsUserDataMetaTable;
}

void fklVMvalueLibsReserve(FklVMvalueLibs *l, uint64_t count) {
    size_t new_size = (count + 1) * sizeof(FklVMlib);
    l->libs = (FklVMlib *)fklZrealloc(l->libs, new_size);
    FKL_ASSERT(l->libs);

    l->count = count;
}

static FKL_ALWAYS_INLINE FklVMvalueProtos *as_protos(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueProtos(v));
    return FKL_TYPE_CAST(FklVMvalueProtos *, v);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_protos_userdata_as_print, "protos");

static void _protos_userdata_atomic(const FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, FklFuncPrototypes, v);
    FklFuncPrototypes *t = &as_protos(v)->p;
    fklVMgcMarkPrototypes(gc, t);
}

static int _protos_userdata_finalizer(FklVMvalue *v, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(t, FklFuncPrototypes, v);
    FklFuncPrototypes *t = &as_protos(v)->p;
    fklUninitFuncPrototypes(t);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const ProtosUserDataMetaTable = {
    // .size = sizeof(FklFuncPrototypes),
    .size = sizeof(FklVMvalueProtos),
    .__as_princ = _protos_userdata_as_print,
    .__as_prin1 = _protos_userdata_as_print,
    .__atomic = _protos_userdata_atomic,
    .__finalizer = _protos_userdata_finalizer,
};

FklVMvalueProtos *fklCreateVMvalueProtos(FklVM *exe, uint32_t count) {
    FklVMvalueProtos *r = (FklVMvalueProtos *)fklCreateVMvalueUd(exe,
            &ProtosUserDataMetaTable,
            NULL);
    fklInitFuncPrototypes(&r->p, count);
    return r;
}

int fklIsVMvalueProtos(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &ProtosUserDataMetaTable;
}

void fklLockVMlibs(FklVMvalueLibs *libs) { uv_mutex_lock(&libs->lock); }

void fklUnlockVMlibs(FklVMvalueLibs *libs) { uv_mutex_unlock(&libs->lock); }

FklVMvalue *fklCreateVMvalueCproc(FklVM *exe,
        FklVMcFunc func,
        FklVMvalue *dll,
        FklVMvalue *pd,
        const char *name) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueCproc);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_CPROC;
    FklVMvalueCproc *dlp = FKL_VM_CPROC(r);
    dlp->func = func;
    dlp->dll = dll;
    dlp->pd = pd;
    dlp->name = name;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *
fklCreateVMvalueUd(FklVM *exe, const FklVMudMetaTable *t, FklVMvalue *dll) {
    // FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, sizeof(FklVMvalueUd) +
    // t->size);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, t->size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_USERDATA;
    FklVMvalueUd *ud = FKL_VM_UD(r);
    ud->mt_ = t;
    ud->dll_ = dll;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueUd2(FklVM *exe,
        const FklVMudMetaTable *t,
        size_t extra_size,
        FklVMvalue *dll) {
    // FklVMvalue *r = (FklVMvalue *)fklZcalloc(1,
    //         sizeof(FklVMvalueUd) + t->size + extra_size);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, t->size + extra_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_USERDATA;
    FklVMvalueUd *ud = FKL_VM_UD(r);
    ud->mt_ = t;
    ud->dll_ = dll;
    fklAddToGC(r, exe);
    return r;
}

#undef NEW_OBJ

static void
_eof_userdata_as_print(const FklVMvalue *ud, FklCodeBuilder *buf, FklVM *exe) {
    fklCodeBuilderPuts(buf, "#<eof>");
}

static FklVMudMetaTable const EofUserDataMetaTable = {
    // .size = 0,
    .size = sizeof(FklVMvalueUd),
    .__as_princ = _eof_userdata_as_print,
    .__as_prin1 = _eof_userdata_as_print,
};

static const alignas(8) FklVMvalueUd FklVMvalueEof = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_USERDATA,
    .mt_ = &EofUserDataMetaTable,
    .dll_ = NULL,
};

FklVMvalue *fklVMvalueEof(void) {
    return FKL_TYPE_CAST(FklVMvalue *, &FklVMvalueEof);
}

void fklAtomicVMvec(FklVMvalue *pVec, FklVMgc *gc) {
    FklVMvec *vec = FKL_VM_VEC(pVec);
    for (size_t i = 0; i < vec->size; i++)
        fklVMgcToGray(vec->base[i], gc);
}

void fklAtomicVMpair(FklVMvalue *root, FklVMgc *gc) {
    fklVMgcToGray(FKL_VM_CAR(root), gc);
    fklVMgcToGray(FKL_VM_CDR(root), gc);
}

void fklAtomicVMproc(FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueProc *proc = FKL_VM_PROC(root);
    fklVMgcToGray(proc->name, gc);
    fklVMgcToGray(proc->codeObj, gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, proc->pts), gc);
    uint32_t count = proc->rcount;
    FklVMvalue **ref = proc->closure;
    for (uint32_t i = 0; i < count; i++)
        fklVMgcToGray(ref[i], gc);
}

void fklAtomicVMcproc(FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueCproc *cproc = FKL_VM_CPROC(root);
    fklVMgcToGray(cproc->dll, gc);
    fklVMgcToGray(cproc->pd, gc);
}

void fklAtomicVMbox(FklVMvalue *root, FklVMgc *gc) {
    fklVMgcToGray(FKL_VM_BOX(root), gc);
}

void fklAtomicVMuserdata(FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueUd *ud = FKL_VM_UD(root);
    fklVMgcToGray(ud->dll_, gc);
    if (ud->mt_->__atomic)
        ud->mt_->__atomic(root, gc);
}

static inline int is_callable_ud(const FklVMvalueUd *ud) {
    return ud->mt_->__call != NULL;
}

int fklIsCallable(FklVMvalue *v) {
    return FKL_IS_PROC(v) || FKL_IS_CPROC(v)
        || (FKL_IS_USERDATA(v) && is_callable_ud(FKL_VM_UD(v)));
}

static inline int is_writable_ud(const FklVMvalueUd *u) {
    return u->mt_->__write != NULL;
}

// static inline int is_to_string_castable_ud(const FklVMvalueUd *u) {
//     return u->mt_->__as_prin1 != NULL || u->mt_->__as_princ != NULL;
// }

static inline int is_ud_has_length(const FklVMvalueUd *u) {
    return u->mt_->__length != NULL;
}

static inline size_t ud_length(const FklVMvalue *a) {
    return FKL_VM_UD(a)->mt_->__length(a);
}

static inline void write_vm_ud(const FklVMvalue *a, FklCodeBuilder *b) {
    FKL_VM_UD(a)->mt_->__write(a, b);
}

int fklWriteVMvalue(const FklVMvalue *r, FklCodeBuilder *b) {
    if (FKL_IS_STR(r)) {
        FklString *str = FKL_VM_STR(r);
        fklCodeBuilderWrite(b, str->size, str->str);
    } else if (FKL_IS_BYTEVECTOR(r)) {
        FklBytevector *bvec = FKL_VM_BVEC(r);
        fklCodeBuilderWrite(b, bvec->size, bvec->ptr);
    } else if (FKL_IS_USERDATA(r) && is_writable_ud(FKL_VM_UD(r))) {
        write_vm_ud(r, b);
    } else
        return 1;
    return 0;
}

int fklVMvalueLength(const FklVMvalue *obj, size_t *len) {
    if (obj == FKL_VM_NIL)
        *len = 0;
    else if (FKL_IS_PAIR(obj)) {
        return !fklIsList2(obj, len);
    } else if (FKL_IS_STR(obj))
        *len = FKL_VM_STR(obj)->size;
    else if (FKL_IS_VECTOR(obj))
        *len = FKL_VM_VEC(obj)->size;
    else if (FKL_IS_BYTEVECTOR(obj))
        *len = FKL_VM_BVEC(obj)->size;
    else if (FKL_IS_HASHTABLE(obj))
        *len = FKL_VM_HASH(obj)->ht.count;
    else if (FKL_IS_USERDATA(obj) && is_ud_has_length(FKL_VM_UD(obj)))
        *len = ud_length(obj);
    else {
        return 1;
    }
    return 0;
}

void *
fklVMvalueTerminalCreate(const char *s, size_t len, size_t line, void *c) {
    FklVMparseCtx *ctx = c;
    return fklCreateVMvalueStr2(ctx->exe, len, s);
}
