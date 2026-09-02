#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
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

FklVMvalue **fklCopyVMlist1(FklVM *vm, FklVMvalue **pv) {
    FklVMvalue *v = *pv;
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v), pv = &FKL_VM_CDR(*pv))
        *pv = fklCreateVMvaluePair(vm, FKL_VM_CAR(v), FKL_VM_CDR(v));
    return pv;
}

FklVMvalue *fklCopyVMlist(FklVM *vm, const FklVMvalue *obj) {
    FklVMvalue *r = FKL_VM_VAL(obj);
    fklCopyVMlist1(vm, &r);
    return r;
}

static inline FklVMvalue *obj_copy(FklVM *vm, const FklVMvalue *obj) {
    switch (obj->type_) {
    case FKL_TYPE_F64:
        return fklCreateVMvalueF64(vm, FKL_VM_F64(obj));
        break;

    case FKL_TYPE_BIGINT:
        return fklCreateVMvalueBigIntWithOther(vm, FKL_VM_BI(obj));
        break;

    case FKL_TYPE_STR:
        return fklCreateVMvalueStr(vm, FKL_VM_STR(obj));
        break;

    case FKL_TYPE_BYTEVECTOR:
        return fklCreateVMvalueBvec(vm, FKL_VM_BVEC(obj));
        break;

    case FKL_TYPE_VECTOR: {
        FklVMvalueVec *vec = FKL_VM_VEC(obj);
        return fklCreateVMvalueVec2(vm, vec->size, vec->base);
    } break;

    case FKL_TYPE_PAIR:
        return fklCopyVMlist(vm, obj);
        break;

    case FKL_TYPE_BOX:
        return fklCreateVMvalueBox(vm, FKL_VM_BOX(obj));
        break;

    case FKL_TYPE_USERDATA: {
        FklVMudCopyAppendCb copy = FKL_VM_UD(obj)->tp_->mt.copy_append;
        if (copy == NULL)
            return NULL;
        return copy(vm, obj, 0, NULL);
    } break;

    case FKL_TYPE_HASHTABLE: {
        FklVMvalueHash *ht = FKL_VM_HASH(obj);
        FklVMvalue *r = fklCreateVMvalueHashEq(vm);
        FklVMvalueHash *nht = FKL_VM_HASH(r);
        nht->eq_type = ht->eq_type;
        for (FklValueHashMapNode *list = ht->ht.first; list;
                list = list->next) {
            fklVMhashTableSet(nht, list->k, list->v);
        }
        return r;
    } break;

    case FKL_TYPE_SYM:
    case FKL_TYPE_KEYWORD:
        return FKL_VM_VAL(obj);
        break;

    case FKL_TYPE_PROC:
    case FKL_TYPE_CPROC:
        return NULL;
        break;

    case FKL_TYPE_VAR_REF:
        FKL_UNREACHABLE();
        abort();
        break;
    }

    FKL_UNREACHABLE();
    abort();
}

FklVMvalue *fklCopyVMvalue(FklVM *vm, const FklVMvalue *obj) {
    switch ((FklVMptrTag)FKL_GET_TAG(obj)) {
    case FKL_TAG_NIL:
    case FKL_TAG_FIX:
    case FKL_TAG_CHR:
        return FKL_TYPE_CAST(FklVMvalue *, obj);
        break;
    case FKL_TAG_PTR:
        return obj_copy(vm, obj);
        break;
    }

    FKL_UNREACHABLE();
    return NULL;
}

static FKL_ALWAYS_INLINE FklVMvalue *str_copy_append(FklVM *exe,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    uint64_t new_size = FKL_VM_STR(v)->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (FKL_IS_CHR(cur))
            ++new_size;
        else if (FKL_IS_STR(cur))
            new_size += FKL_VM_STR(cur)->size;
        else
            return NULL;
    }
    FklVMvalue *retval = fklCreateVMvalueStr2(exe, new_size, NULL);
    FklString *str = FKL_VM_STR(retval);
    new_size = FKL_VM_STR(v)->size;
    memcpy(str->str, FKL_VM_STR(v)->str, new_size * sizeof(char));
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (FKL_IS_CHR(cur))
            str->str[new_size++] = FKL_GET_CHR(cur);
        else {
            size_t ss = FKL_VM_STR(cur)->size;
            memcpy(&str->str[new_size],
                    FKL_VM_STR(cur)->str,
                    ss * sizeof(char));
            new_size += ss;
        }
    }
    return retval;
}

static FKL_ALWAYS_INLINE FklVMvalue *vec_copy_append(FklVM *exe,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    size_t new_size = FKL_VM_VEC(v)->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (FKL_IS_VECTOR(cur))
            new_size += FKL_VM_VEC(cur)->size;
        else
            return NULL;
    }
    FklVMvalue *new_vec_val = fklCreateVMvalueVec(exe, new_size);
    FklVMvalueVec *new_vec = FKL_VM_VEC(new_vec_val);
    new_size = FKL_VM_VEC(v)->size;
    memcpy(new_vec->base, FKL_VM_VEC(v)->base, new_size * sizeof(FklVMvalue *));
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss;
        FklVMvalue **mem;
        ss = FKL_VM_VEC(cur)->size;
        mem = FKL_VM_VEC(cur)->base;
        memcpy(&new_vec->base[new_size], mem, ss * sizeof(FklVMvalue *));
        new_size += ss;
    }
    return new_vec_val;
}

static FKL_ALWAYS_INLINE FklVMvalue *pair_copy_append(FklVM *exe,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    if (argc) {
        FklVMvalue *retval = FKL_VM_NIL;
        FklVMvalue **prev = &retval;
        *prev = (FklVMvalue *)v;
        for (uint32_t i = 0; i < argc; ++i) {
            FklVMvalue *pr = *prev;
            FklVMvalue *cur = base[i];
            if (!fklIsList(pr))
                return NULL;

            if ((prev = fklCopyVMlist1(exe, prev), *prev == FKL_VM_NIL)) {
                *prev = cur;
            } else {
                return NULL;
            }
        }
        return retval;
    } else {
        return (FklVMvalue *)v;
    }
}

static FKL_ALWAYS_INLINE FklVMvalue *bytes_copy_append(FklVM *exe,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    uint64_t new_size = FKL_VM_BVEC(v)->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (FKL_IS_BYTEVECTOR(cur))
            new_size += FKL_VM_BVEC(cur)->size;
        else
            return NULL;
    }
    FklVMvalue *bv = fklCreateVMvalueBvec2(exe, new_size, NULL);
    FklBytevector *bvec = FKL_VM_BVEC(bv);
    new_size = FKL_VM_BVEC(v)->size;
    memcpy(bvec->ptr, FKL_VM_BVEC(v)->ptr, new_size * sizeof(char));
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss = FKL_VM_BVEC(cur)->size;
        memcpy(&bvec->ptr[new_size], FKL_VM_BVEC(cur)->ptr, ss * sizeof(char));
        new_size += ss;
    }
    return bv;
}

static FKL_ALWAYS_INLINE FklVMvalue *userdata_copy_append(FklVM *exe,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    FklVMudCopyAppendCb append = FKL_VM_UD(v)->tp_->mt.copy_append;
    if (append)
        return append(exe, v, argc, base);
    else
        return NULL;
}

static FKL_ALWAYS_INLINE FklVMvalue *obj_copy_append(FklVM *vm,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    switch (v->type_) {
    case FKL_TYPE_STR:
        return str_copy_append(vm, v, argc, base);
        break;
    case FKL_TYPE_VECTOR:
        return vec_copy_append(vm, v, argc, base);
        break;
    case FKL_TYPE_PAIR:
        return pair_copy_append(vm, v, argc, base);
        break;
    case FKL_TYPE_BYTEVECTOR:
        return bytes_copy_append(vm, v, argc, base);
        break;
    case FKL_TYPE_USERDATA:
        return userdata_copy_append(vm, v, argc, base);
        break;

    case FKL_TYPE_F64:
    case FKL_TYPE_SYM:
    case FKL_TYPE_BOX:
    case FKL_TYPE_PROC:
    case FKL_TYPE_CPROC:
    case FKL_TYPE_BIGINT:
    case FKL_TYPE_KEYWORD:
    case FKL_TYPE_HASHTABLE:
        return NULL;
        break;

    case FKL_TYPE_VAR_REF:
        FKL_UNREACHABLE();
        abort();
        break;
    }

    FKL_UNREACHABLE();
    abort();
    return NULL;
}

FklVMvalue *fklAppendVMvalue(FklVM *vm,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    switch ((FklVMptrTag)FKL_GET_TAG(v)) {
    case FKL_TAG_NIL:
    case FKL_TAG_FIX:
    case FKL_TAG_CHR:
        return NULL;
        break;

    case FKL_TAG_PTR:
        return obj_copy_append(vm, v, argc, base);
        break;
    }

    FKL_UNREACHABLE();
    return NULL;
}

static inline FklVMvalue *get_initial_fast_value(FklVMvalue *pr) {
    return FKL_IS_PAIR(pr) ? FKL_VM_CDR(pr) : FKL_VM_NIL;
}

static inline FklVMvalue *get_fast_value(FklVMvalue *head) {
    return FKL_IS_PAIR(head) && FKL_IS_PAIR(FKL_VM_CDR(head))
                        && FKL_IS_PAIR(FKL_VM_CDR(FKL_VM_CDR(head)))
                 ? FKL_VM_CDR(FKL_VM_CDR(head))
                 : FKL_VM_NIL;
}

static FKL_ALWAYS_INLINE FklVMvalue *pair_append(FklVM *vm,
        FklVMvalue *obj,
        uint32_t argc,
        FklVMvalue *const *base) {
    if (argc) {
        FklVMvalue *retval = FKL_VM_NIL;
        FklVMvalue **prev = &retval;
        *prev = obj;
        for (uint32_t i = 0; i < argc; ++i) {
            FklVMvalue *cur = base[i];
            FklVMvalue *head = get_initial_fast_value(*prev);
            while (FKL_IS_PAIR(*prev)) {
                if (head == *prev)
                    return NULL;
                prev = &FKL_VM_CDR(*prev);
                head = get_fast_value(head);
            }

            if (*prev == FKL_VM_NIL) {
                *prev = cur;
            } else {
                return NULL;
            }
        }
    }
    return obj;
}

static FKL_ALWAYS_INLINE FklVMvalue *userdata_append(FklVM *vm,
        FklVMvalue *obj,
        uint32_t argc,
        FklVMvalue *const *base) {
    FklVMudAppendCb append = FKL_VM_UD(obj)->tp_->mt.append;
    if (append) {
        return append(vm, obj, argc, base);
    } else {
        return NULL;
    }
}

static FKL_ALWAYS_INLINE FklVMvalue *
obj_append(FklVM *vm, FklVMvalue *v, uint32_t argc, FklVMvalue *const *base) {
    switch (v->type_) {
    case FKL_TYPE_PAIR:
        return pair_append(vm, v, argc, base);
        break;
    case FKL_TYPE_USERDATA:
        return userdata_append(vm, v, argc, base);
        break;

    case FKL_TYPE_STR:
    case FKL_TYPE_VECTOR:
    case FKL_TYPE_F64:
    case FKL_TYPE_SYM:
    case FKL_TYPE_BOX:
    case FKL_TYPE_PROC:
    case FKL_TYPE_CPROC:
    case FKL_TYPE_BIGINT:
    case FKL_TYPE_KEYWORD:
    case FKL_TYPE_HASHTABLE:
    case FKL_TYPE_BYTEVECTOR:
        return NULL;
        break;

    case FKL_TYPE_VAR_REF:
        FKL_UNREACHABLE();
        abort();
        break;
    }

    FKL_UNREACHABLE();
    abort();
    return NULL;
}

FklVMvalue *fklAppendVMvalue1(FklVM *vm,
        FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base) {
    switch ((FklVMptrTag)FKL_GET_TAG(v)) {
    case FKL_TAG_NIL:
    case FKL_TAG_FIX:
    case FKL_TAG_CHR:
        return NULL;
        break;

    case FKL_TAG_PTR:
        return obj_append(vm, v, argc, base);
        break;
    }

    FKL_UNREACHABLE();
    return NULL;
}

FklVMvalue *fklCreateTrueValue() { return FKL_MAKE_VM_FIX(1); }

FklVMvalue *fklCreateNilValue() { return FKL_VM_NIL; }

int fklVMvalueEqual(const FklVMvalue *fir, const FklVMvalue *sec) {
    if (fir == sec)
        return 1;

    FklPairVector s;

    if (FKL_IS_PTR(fir) && FKL_IS_PTR(sec)) {
        if (fir->type_ != sec->type_)
            return 0;
        switch (fir->type_) {
        case FKL_TYPE_PROC:
        case FKL_TYPE_CPROC:
        case FKL_TYPE_SYM:
        case FKL_TYPE_KEYWORD:
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
            if (ud1->tp_ != ud2->tp_ || !ud1->tp_->mt.equal)
                return 0;
            else
                return ud1->tp_->mt.equal(fir, sec);
        } break;
        case FKL_TYPE_PAIR:
        case FKL_TYPE_BOX:
        case FKL_TYPE_VECTOR:
        case FKL_TYPE_HASHTABLE:
            goto nested_equal;
            break;
        case FKL_TYPE_VAR_REF:
            FKL_UNREACHABLE();
            abort();
            break;
        }

        fprintf(stderr,
                "[%s: %d] %s: unknown value type!\n",
                __FILE__,
                __LINE__,
                __func__);
        abort();
    } else {
        return fir == sec;
    }

nested_equal:
    fklPairVectorInit(&s, 8);
    fklPairVectorPushBack2(&s,
            (FklPair){
                .car = FKL_TYPE_CAST(FklVMvalue *, fir),
                .cdr = FKL_TYPE_CAST(FklVMvalue *, sec),
            });
    int r = 1;
    while (!fklPairVectorIsEmpty(&s)) {
        const FklPair *top = fklPairVectorPopBackNonNull(&s);
        FklVMvalue *root1 = top->car;
        FklVMvalue *root2 = top->cdr;
        if (FKL_GET_TAG(root1) != FKL_GET_TAG(root2)) {
            r = 0;
        } else if (!FKL_IS_PTR(root1) && !FKL_IS_PTR(root2) && root1 != root2) {
            r = 0;
        } else if (FKL_IS_PTR(root1) && FKL_IS_PTR(root2)) {
            if (root1 == root2) {
                r = 1;
            } else if (root1->type_ != root2->type_) {
                r = 0;
            } else {
                switch (root1->type_) {
                case FKL_TYPE_SYM:
                case FKL_TYPE_KEYWORD:
                case FKL_TYPE_PROC:
                case FKL_TYPE_CPROC:
                    r = root1 == root2;
                    goto done;
                    break;
                case FKL_TYPE_F64:
                    r = FKL_VM_F64(root1) == FKL_VM_F64(root2);
                    goto done;
                    break;
                case FKL_TYPE_STR:
                    r = fklStringEqual(FKL_VM_STR(root1), FKL_VM_STR(root2));
                    goto done;
                    break;
                case FKL_TYPE_BYTEVECTOR:
                    r = fklBytevectorEqual(FKL_VM_BVEC(root1),
                            FKL_VM_BVEC(root2));
                    goto done;
                    break;
                case FKL_TYPE_PAIR:
                    r = 1;
                    fklPairVectorPushBack2(&s,
                            (FklPair){
                                .car = FKL_VM_CDR(root1),
                                .cdr = FKL_VM_CDR(root2),
                            });
                    fklPairVectorPushBack2(&s,
                            (FklPair){
                                .car = FKL_VM_CAR(root1),
                                .cdr = FKL_VM_CAR(root2),
                            });
                    goto done;
                    break;
                case FKL_TYPE_BOX:
                    r = 1;
                    fklPairVectorPushBack2(&s,
                            (FklPair){
                                .car = FKL_VM_BOX(root1),
                                .cdr = FKL_VM_BOX(root2),
                            });
                    goto done;
                    break;
                case FKL_TYPE_VECTOR: {
                    FklVMvalueVec *vec1 = FKL_VM_VEC(root1);
                    FklVMvalueVec *vec2 = FKL_VM_VEC(root2);
                    if (vec1->size != vec2->size) {
                        r = 0;
                    } else {
                        r = 1;
                        for (size_t i = vec1->size; i > 0; --i) {
                            fklPairVectorPushBack2(&s,
                                    (FklPair){
                                        .car = vec1->base[i - 1],
                                        .cdr = vec2->base[i - 1],
                                    });
                        }
                    }
                    goto done;
                } break;
                case FKL_TYPE_BIGINT:
                    r = fklVMbigIntEqual(FKL_VM_BI(root1), FKL_VM_BI(root2));
                    goto done;
                    break;
                case FKL_TYPE_USERDATA: {
                    FklVMvalueUd *ud1 = FKL_VM_UD(root1);
                    FklVMvalueUd *ud2 = FKL_VM_UD(root2);
                    if (ud1->tp_ != ud2->tp_ || !ud1->tp_->mt.equal)
                        r = 0;
                    else
                        r = ud1->tp_->mt.equal(root1, root2);
                    goto done;
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
                            fklPairVectorPushBack2(&s,
                                    (FklPair){ .car = i1->v, .cdr = i2->v });
                            fklPairVectorPushBack2(&s,
                                    (FklPair){ .car = i1->k, .cdr = i2->k });
                        }
                    }
                    goto done;
                } break;

                case FKL_TYPE_VAR_REF:
                    FKL_UNREACHABLE();
                    abort();
                    break;
                }

                FKL_UNREACHABLE();
                fprintf(stderr,
                        "[%s: %d] %s: unknown value type!\n",
                        __FILE__,
                        __LINE__,
                        __func__);
                abort();
                break;
            done:;
            }
        }
        if (!r)
            break;
    }
    fklPairVectorUninit(&s);
    return r;
}

static inline int
cmp_vm_ud(const FklVMvalue *a, const FklVMvalue *b, int *err) {
    return FKL_VM_UD(a)->tp_->mt.cmp(a, b, err);
}

static inline int is_cmpable_ud(const FklVMvalueUd *u) {
    return u->tp_->mt.cmp != NULL;
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

int fklVMfpRewind(FklVMvalueFp *vfp, FklStrBuf *b, size_t j) {
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

static inline void init_dll(FklVMvalueDll *dll, FklVM *exe) {
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

static FKL_ALWAYS_INLINE uintptr_t _f64_hashFunc(const FklVMvalue *v) {
    union {
        double f;
        uint64_t i;
    } t = {
        .f = FKL_VM_F64(v),
    };
    return t.i;
}

static FKL_ALWAYS_INLINE uintptr_t _str_hashFunc(const FklVMvalue *v) {
    return fklStringHash(FKL_VM_STR(v));
}

static FKL_ALWAYS_INLINE uintptr_t _bytevector_hashFunc(const FklVMvalue *v) {
    return fklBytevectorHash(FKL_VM_BVEC(v));
}

static FKL_ALWAYS_INLINE uintptr_t _vector_hashFunc(const FklVMvalue *v) {
    const FklVMvalueVec *vec = FKL_VM_VEC(v);
    uintptr_t seed = vec->size;
    for (size_t i = 0; i < vec->size; ++i)
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(vec->base[i]));
    return seed;
}

static FKL_ALWAYS_INLINE uintptr_t _pair_hashFunc(const FklVMvalue *v) {
    uintptr_t seed = 2;
    seed = fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_CAR(v)));
    seed = fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_CDR(v)));
    return seed;
}

static FKL_ALWAYS_INLINE uintptr_t _box_hashFunc(const FklVMvalue *v) {
    uintptr_t seed = 1;
    return fklHashCombine(seed, fklVMvalueEqualHashv(FKL_VM_BOX(v)));
}

static FKL_ALWAYS_INLINE size_t _userdata_hashFunc(const FklVMvalue *v) {
    size_t (*hashv)(const FklVMvalue *) = FKL_VM_UD(v)->tp_->mt.hash;
    if (hashv)
        return hashv(v);
    else {
        uintptr_t t = FKL_TYPE_CAST(uintptr_t, v) >> FKL_UNUSEDBITNUM;
        return fklHash64Shift(t);
    }
}

static FKL_ALWAYS_INLINE size_t _hashTable_hashFunc(const FklVMvalue *v) {
    FklVMvalueHash *hash = FKL_VM_HASH(v);
    uintptr_t seed = hash->ht.count + hash->eq_type;
    for (FklValueHashMapNode *list = hash->ht.first; list; list = list->next) {
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(list->k));
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(list->v));
    }
    return seed;
}

static inline uintptr_t obj_hash(const FklVMvalue *v) {
    switch (v->type_) {
    case FKL_TYPE_F64:
        return _f64_hashFunc(v);
        break;
    case FKL_TYPE_STR:
        return _str_hashFunc(v);
        break;
    case FKL_TYPE_VECTOR:
        return _vector_hashFunc(v);
        break;
    case FKL_TYPE_PAIR:
        return _pair_hashFunc(v);
        break;
    case FKL_TYPE_BOX:
        return _box_hashFunc(v);
        break;
    case FKL_TYPE_BYTEVECTOR:
        return _bytevector_hashFunc(v);
        break;
    case FKL_TYPE_USERDATA:
        return _userdata_hashFunc(v);
        break;
    case FKL_TYPE_HASHTABLE:
        return _hashTable_hashFunc(v);
        break;
    case FKL_TYPE_BIGINT:
        return fklVMbigIntHash(FKL_VM_BI(v));
        break;

    case FKL_TYPE_SYM:
    case FKL_TYPE_KEYWORD:
    case FKL_TYPE_PROC:
    case FKL_TYPE_CPROC:
    case FKL_TYPE_VAR_REF:
        return ((uintptr_t)v >> FKL_UNUSEDBITNUM);
    }

    FKL_UNREACHABLE();
    return 0;
}

uintptr_t fklVMvalueEqualHashv(const FklVMvalue *v) {
    if (fklIsVMint(v)) {
        return fklVMintegerHashv(v);
    } else if (FKL_IS_PTR(v)) {
        return obj_hash(v);
    } else {
        return ((uintptr_t)v >> FKL_UNUSEDBITNUM);
    }
}

void fklAtomicVMhashTable(const FklVMvalue *pht, FklVMgc *gc) {
    const FklVMvalueHash *table = FKL_VM_HASH(pht);
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

static uintptr_t (*const vm_hash_funcs[])(const FklVMvalue *k) = {
    [FKL_HASH_EQ] = fklVMvalueEqHashv,
    [FKL_HASH_EQV] = fklVMvalueEqvHashv,
    [FKL_HASH_EQUAL] = fklVMvalueEqualHashv,
};

typedef int (*VMhashEqFunc)(const FklVMvalue *a, const FklVMvalue *b);

static const VMhashEqFunc vm_hash_eq_funcs[] = {
    [FKL_HASH_EQ] = fklVMvalueEq,
    [FKL_HASH_EQV] = fklVMvalueEqv,
    [FKL_HASH_EQUAL] = fklVMvalueEqual,
};

static inline FklValueHashMapElm *
vmhash_find_node(const FklVMvalueHash *ht, FklVMvalue *key, uintptr_t *hashv) {
    *hashv = vm_hash_funcs[ht->eq_type](key);
    VMhashEqFunc eq_func = vm_hash_eq_funcs[ht->eq_type];

    FklValueHashMapNode *const *pp = fklValueHashMapBucket(&ht->ht, *hashv);

    for (; *pp; pp = &(*pp)->bkt_next) {
        if (eq_func(key, (*pp)->k)) {
            return &(*pp)->elm;
        }
    }
    return NULL;
}

FklValueHashMapElm *fklVMhashTableGet(const FklVMvalueHash *ht,
        FklVMvalue *key) {
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
    uintptr_t hashv = vm_hash_funcs[ht->eq_type](key);
    VMhashEqFunc eq_func = vm_hash_eq_funcs[ht->eq_type];

    FklValueHashMapNode *const *pp = fklValueHashMapBucket(&ht->ht, hashv);

    for (; *pp; pp = &(*pp)->bkt_next) {
        if (eq_func(key, (*pp)->k)) {
            if (pk)
                *pk = (*pp)->k;
            if (pv)
                *pv = (*pp)->v;
            fklValueHashMapDelNode(&ht->ht, (FklValueHashMapNode **)pp);
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

FklVMvalue *fklCreateVMvaluePair1(FklVM *exe, FklVMvalue *car) {
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

static const alignas(8) FklVMvalueVec ZeroLenVecSingleton = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_VECTOR,
    .size = 0,
};

FklVMvalue *fklCreateVMvalueVec(FklVM *exe, size_t size) {
    if (size == 0)
        return FKL_VM_VAL(&ZeroLenVecSingleton);

    size_t total_size = sizeof(FklVMvalueVec) + size * sizeof(FklVMvalue *);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvalueVec *v = FKL_VM_VEC(r);
    v->size = size;
    fklAddToGC(r, exe);
    return r;
}
FklVMvalue *fklCreateVMvalueVecExt(FklVM *exe, size_t size, ...) {
    if (size == 0)
        return FKL_VM_VAL(&ZeroLenVecSingleton);
    size_t total_size = sizeof(FklVMvalueVec) + size * sizeof(FklVMvalue *);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvalueVec *v = FKL_VM_VEC(r);
    v->size = size;
    fklAddToGC(r, exe);
    va_list ap;
    va_start(ap, size);
    for (size_t i = 0; i < size; ++i) {
        v->base[i] = va_arg(ap, FklVMvalue *);
    }
    va_end(ap);
    return r;
}

FklVMvalue *
fklCreateVMvalueVec2(FklVM *exe, size_t size, FklVMvalue *const *ptr) {
    if (size == 0)
        return FKL_VM_VAL(&ZeroLenVecSingleton);

    size_t ss = size * sizeof(FklVMvalue *);
    size_t total_size = sizeof(FklVMvalueVec) + ss;
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_VECTOR;
    FklVMvalueVec *v = FKL_VM_VEC(r);
    memcpy(v->base, ptr, ss);
    v->size = size;
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

static const alignas(8) FklVMvalueStr ZeroLenStrSingleton = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_STR,
    .str.size = 0,
};

FklVMvalue *fklCreateVMvalueStr(FklVM *exe, const FklString *s) {
    if (s->size == 0)
        return FKL_VM_VAL(&ZeroLenStrSingleton);

    size_t total_size = sizeof(FklVMvalueStr) + s->size * sizeof(s->str[0]);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_STR;
    FklString *rs = FKL_VM_STR(r);
    rs->size = s->size;
    memcpy(rs->str, s->str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueStr2(FklVM *exe, size_t size, const char *str) {
    if (size == 0)
        return FKL_VM_VAL(&ZeroLenStrSingleton);

    size_t total_size = sizeof(FklVMvalueStr) + size * sizeof(str[0]);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
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

FklVMvalue *fklCreateVMvalueKeyword(FklVM *exe, size_t size, const char *str) {
    size_t total_size = sizeof(FklVMvalueKeyword) + size * sizeof(str[0]);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_KEYWORD;
    FklString *rs = FKL_VM_KEYWORD(r);
    rs->size = size;
    if (str)
        memcpy(rs->str, str, rs->size * sizeof(rs->str[0]));
    fklAddToGC(r, exe);
    return r;
}

static const alignas(8) FklVMvalueBvec ZeroLenBvecSingleton = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_BYTEVECTOR,
    .bvec.size = 0,
};

FklVMvalue *fklCreateVMvalueBvec(FklVM *exe, const FklBytevector *b) {
    if (b->size == 0)
        return FKL_VM_VAL(&ZeroLenBvecSingleton);

    size_t total_size = sizeof(FklVMvalueBvec) + b->size * sizeof(b->ptr[0]);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BYTEVECTOR;
    FklBytevector *bvec = FKL_VM_BVEC(r);
    bvec->size = b->size;
    memcpy(bvec->ptr, b->ptr, bvec->size * sizeof(bvec->ptr[0]));
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueBvec2(FklVM *exe, size_t size, const uint8_t *ptr) {
    if (size == 0)
        return FKL_VM_VAL(&ZeroLenBvecSingleton);

    size_t total_size = sizeof(FklVMvalueBvec) + size * sizeof(ptr[0]);
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_BYTEVECTOR;
    FklBytevector *bvec = FKL_VM_BVEC(r);
    bvec->size = size;
    if (ptr)
        memcpy(bvec->ptr, ptr, bvec->size * sizeof(bvec->ptr[0]));
    fklAddToGC(r, exe);
    return r;
}

static void
_error_userdata_princ(const FklVMvalue *ud, FklCodeBuilder *build, FklVM *exe) {
    FklVMvalueError *err = FKL_VM_ERR(ud);
    fklPrintString2(FKL_VM_STR(err->message), build);
}

static void
_error_userdata_prin1(const FklVMvalue *ud, FklCodeBuilder *build, FklVM *exe) {
    FklVMvalueError *err = FKL_VM_ERR(ud);
    fklVMformat(exe,
            build,
            "#<err t: %S, message: %S>",
            NULL,
            2,
            (FklVMvalue *[]){ err->type, err->message });
}

static void _error_userdata_atomic(const FklVMvalue *v, FklVMgc *gc) {
    FklVMvalueError *err = FKL_VM_ERR(v);
    fklVMgcToGray(err->message, gc);
    fklVMgcToGray(err->type, gc);
}

FKL_VM_TYPE_ATTR
FklVMvalueType ErrorType = FKL_VM_TYPE_STATIC_INIT(ErrorType,
        {
            .name = "error",
            .size = sizeof(FklVMvalueError),
            .princ = _error_userdata_princ,
            .prin1 = _error_userdata_prin1,
            .atomic = _error_userdata_atomic,
        });

FklVMvalue *
fklCreateVMvalueError(FklVM *exe, FklVMvalue *type, FklVMvalue *message) {
    FKL_ASSERT(FKL_IS_SYM(type) && FKL_IS_STR(message));
    FklVMvalue *r = fklCreateVMvalueUd(exe, &ErrorType);
    FklVMvalueError *err = FKL_VM_ERR(r);
    err->type = type;
    err->message = message;
    return r;
}

FklVMvalue *fklCreateVMvalueError2(FklVM *exe,
        FklVMvalue *type,
        const char *fmt,
        size_t count,
        FklVMvalue *values[]) {
    FklVMvalue *msg = fklVMformatToString(exe, fmt, count, values);
    FklVMvalue *r = fklCreateVMvalueError(exe, type, msg);
    return r;
}

int fklIsVMvalueError(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &ErrorType.mt;
}

static inline void init_chanl_sendq(struct FklVMchanlSendq *q) {
    q->head = NULL;
    q->tail = &q->head;
}

static inline void init_chanl_recvq(struct FklVMchanlRecvq *q) {
    q->head = NULL;
    q->tail = &q->head;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(_chanl_userdata_print, "chanl");

static void _chanl_userdata_atomic(const FklVMvalue *root, FklVMgc *gc) {
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

FKL_VM_TYPE_ATTR FklVMvalueType ChanlType = FKL_VM_TYPE_STATIC_INIT(ChanlType,
        {
            .name = "chanl",
            .size = sizeof(FklVMvalueChanl),
            .princ = _chanl_userdata_print,
            .prin1 = _chanl_userdata_print,
            .atomic = _chanl_userdata_atomic,
        });

FklVMvalue *fklCreateVMvalueChanl(FklVM *exe, uint32_t qsize) {
    size_t extra_size = qsize * sizeof(FklVMvalue *);
    FklVMvalue *r = fklCreateVMvalueUd2(exe, &ChanlType, extra_size);
    FklVMvalueChanl *ch = FKL_VM_CHANL(r);
    ch->qsize = qsize;
    uv_mutex_init(&ch->lock);
    init_chanl_sendq(&ch->sendq);
    init_chanl_recvq(&ch->recvq);
    return r;
}

int fklIsVMvalueChanl(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &ChanlType.mt;
}

static FklVMudFinalizeResult _fp_userdata_finalize(FklVMvalue *ud,
        FklVMgc *gc) {
    fklVMfpClose(FKL_VM_FP(ud));
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(_fp_userdata_print, "fp");

FKL_VM_TYPE_ATTR FklVMvalueType FpType = FKL_VM_TYPE_STATIC_INIT(FpType,
        {
            .name = "fp",
            .size = sizeof(FklVMvalueFp),
            .princ = _fp_userdata_print,
            .prin1 = _fp_userdata_print,
            .finalize = _fp_userdata_finalize,
        });

#define VM_FP_STATIC_INIT(FP, RW)                                              \
    ((FklVMvalueFp){                                                           \
        .next_ = NULL,                                                         \
        .gray_next_ = NULL,                                                    \
        .mark_ = FKL_MARK_B,                                                   \
        .type_ = FKL_TYPE_USERDATA,                                            \
        .tp_ = &FpType,                                                        \
        .fp = (FP),                                                            \
        .rw = (RW),                                                            \
    })

void fklInitVMvalueFp(FklVMvalueFp *vfp, FILE *fp, FklVMfpRW rw) {
    *vfp = VM_FP_STATIC_INIT(fp, rw);
}

FklVMvalue *fklCreateVMvalueFp(FklVM *exe, FILE *fp, FklVMfpRW rw) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &FpType);
    FklVMvalueFp *vfp = FKL_VM_FP(r);
    vfp->fp = fp;
    vfp->rw = rw;
    return r;
}

int fklIsVMvalueFp(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &FpType.mt;
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
    FKL_ASSERT(fklIsBigIntGtLtFix(bi));
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
    FKL_ASSERT(fklIsBigIntGtLtFix(&t));
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
    FKL_ASSERT(fklIsBigIntGtLtFix(&bi));
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
    FKL_ASSERT(fklIsBigIntGtLtFix(&bi));
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
    FKL_ASSERT(fklIsBigIntGtLtFix(&bi));
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
        const FklIns *spc,
        uint64_t cpc,
        FklVMvalue *bcl,
        FklVMvalueProto *pt) {
    uint32_t ref_count = pt->ref_count;
    size_t total_size = sizeof(FklVMvalueProc) //
                      + ref_count * sizeof(FklVMvalue *);
    FklVMvalueProc *r = (FklVMvalueProc *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_PROC;

    r->proto = pt;
    r->spc = spc;
    r->end = spc + cpc;
    r->name = pt->name;
    r->local_count = pt->local_count;
    r->ref_count = ref_count;
    r->bcl = bcl;

    fklAddToGC(FKL_VM_VAL(r), exe);
    return FKL_VM_VAL(r);
}

FklVMvalue *
fklCreateVMvalueProc(FklVM *exe, FklVMvalue *codeObj, FklVMvalueProto *pt) {
    FklByteCode *bc = &FKL_VM_CO(codeObj)->bc;
    return fklCreateVMvalueProc2(exe, bc->code, bc->len, codeObj, pt);
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

FKL_VM_USER_DATA_DEFAULT_PRINT(_code_obj_userdata_print, "code-obj");

static FklVMudFinalizeResult _code_obj_userdata_finalize(FklVMvalue *v,
        FklVMgc *gc) {
    FklByteCodelnt *t = &as_co(v)->bcl;
    fklUninitByteCodelnt(t);
    return FKL_VM_UD_FINALIZE_NOW;
}

static void code_obj_atomic(const FklVMvalue *v, FklVMgc *gc) {
    FklByteCodelnt *t = &as_co(v)->bcl;
    fklVMgcMarkCodeObject(gc, t);
}

FKL_VM_TYPE_ATTR FklVMvalueType CodeObjType =
        FKL_VM_TYPE_STATIC_INIT(CodeObjType,
                {
                    .name = "code-obj",
                    .size = sizeof(FklVMvalueCodeObj),
                    .princ = _code_obj_userdata_print,
                    .prin1 = _code_obj_userdata_print,
                    .atomic = code_obj_atomic,
                    .finalize = _code_obj_userdata_finalize,
                });

FklVMvalue *fklCreateVMvalueCodeObj1(FklVM *exe) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &CodeObjType);
    fklInitByteCodelnt(FKL_VM_CO(r), 0);
    return r;
}

FklVMvalue *fklCreateVMvalueCodeObjExt(FklVM *exe,
        FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklVMvalue *r = fklCreateVMvalueCodeObj1(exe);
    fklInitByteCodelnt(FKL_VM_CO(r), 1);
    fklInitSingleInsBcl(FKL_VM_CO(r), ins, fid, line, scope);
    return r;
}

int fklIsVMvalueCodeObj(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &CodeObjType.mt;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(_dll_userdata_print, "dll");

static void _dll_userdata_atomic(const FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueDll *dll = FKL_VM_DLL(root);
    if (dll->desc != NULL && dll->desc->atomic != NULL)
        dll->desc->atomic(root, gc);
}

static FklVMudFinalizeResult _dll_userdata_finalize(FklVMvalue *v,
        FklVMgc *gc) {
    FklVMvalueDll *dll = FKL_VM_DLL(v);
    FklVMudFinalizeResult r = FKL_VM_UD_FINALIZE_NOW;
    if (dll->desc != NULL && dll->desc->finalizer != NULL) {
        r = dll->desc->finalizer(v, gc);
        switch (r) {
        case FKL_VM_UD_FINALIZE_NOW:
            break;
        case FKL_VM_UD_FINALIZE_DELAY:
            return FKL_VM_UD_FINALIZE_DELAY;
            break;
        }
    }

    dll->desc = NULL;

    FklDllUninitFunc uninit = fklVMdllGetUninitCb(dll);
    if (uninit != NULL)
        uninit();
    uninit = NULL;
    uv_dlclose(&dll->dll);
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_TYPE_ATTR
FklVMvalueType DllType = FKL_VM_TYPE_STATIC_INIT(DllType,
        {
            .name = "dll",
            .size = sizeof(FklVMvalueDll),
            .princ = _dll_userdata_print,
            .prin1 = _dll_userdata_print,
            .atomic = _dll_userdata_atomic,
            .finalize = _dll_userdata_finalize,
        });

FklVMvalue *
fklCreateVMvalueDll(FklVM *exe, FklVMvalue *rp_v, FklVMvalue **errorStr) {
    FKL_ASSERT(FKL_IS_SYM(rp_v));
    const char *rp = FKL_VM_SYM(rp_v)->str;
    uv_lib_t lib;
    if (uv_dlopen(rp, &lib)) {
        *errorStr = fklCreateVMvalueStr1(exe, uv_dlerror(&lib));
        uv_dlclose(&lib);
        return NULL;
    }

    FklDllStateDescGet state_desc_get =
            (FklDllStateDescGet)fklGetAddress("_fklDllStateDescGet", &lib);
    const FklDllStateDesc *state_desc = NULL;
    if (state_desc_get != NULL) {
        state_desc = state_desc_get();
    }

    FklVMvalue *r = NULL;
    if (state_desc == NULL) {
        r = fklCreateVMvalueUd(exe, &DllType);
    } else {
        r = fklCreateVMvalueUdSized(exe, &DllType, state_desc->size);
    }

    FklVMvalueDll *dll = FKL_VM_DLL(r);
    dll->dll = lib;
    dll->desc = state_desc;

    init_dll(dll, exe);
    return r;
}

int fklIsVMvalueDll(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &DllType.mt;
}

FklVMvalue *fklCreateVMvalueCproc(FklVM *exe,
        FklVMcFunc func,
        FklVMvalue *dll,
        const char *name) {
    FklVMvalue *r = NEW_OBJ(FklVMvalueCproc);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_CPROC;
    FklVMvalueCproc *dlp = FKL_VM_CPROC(r);
    dlp->func = func;
    dlp->dll = dll;
    dlp->name = name;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueUd(FklVM *exe, const FklVMvalueType *t) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, t->mt.size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_USERDATA;
    FklVMvalueUd *ud = FKL_VM_UD(r);
    ud->tp_ = t;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *
fklCreateVMvalueUd2(FklVM *exe, const FklVMvalueType *t, size_t extra_size) {
    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, t->mt.size + extra_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_USERDATA;
    FklVMvalueUd *ud = FKL_VM_UD(r);
    ud->tp_ = t;
    fklAddToGC(r, exe);
    return r;
}

FklVMvalue *fklCreateVMvalueUdSized(FklVM *exe,
        const FklVMvalueType *t,
        size_t actual_size) {
    if (actual_size < t->mt.size) {
        FKL_UNREACHABLE();
        abort();
    }

    FklVMvalue *r = (FklVMvalue *)fklZcalloc(1, actual_size);
    FKL_ASSERT(r);
    r->type_ = FKL_TYPE_USERDATA;
    FklVMvalueUd *ud = FKL_VM_UD(r);
    ud->tp_ = t;
    fklAddToGC(r, exe);
    return r;
}

#undef NEW_OBJ

static void
_eof_userdata_print(const FklVMvalue *ud, FklCodeBuilder *buf, FklVM *exe) {
    fklCodeBuilderPuts(buf, "#<eof>");
}

FKL_VM_TYPE_ATTR
FklVMvalueType EofType = FKL_VM_TYPE_STATIC_INIT(EofType,
        {
            .name = "eof",
            .size = sizeof(FklVMvalueUd),
            .princ = _eof_userdata_print,
            .prin1 = _eof_userdata_print,
        });

FKL_VM_TYPE_ATTR
FklVMvalueUd FklVMvalueEof = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_USERDATA,
    .tp_ = &EofType,
};

FklVMvalue *fklVMvalueEof(void) {
    return FKL_TYPE_CAST(FklVMvalue *, &FklVMvalueEof);
}

static void _undefined_userdata_print(const FklVMvalue *ud,
        FklCodeBuilder *buf,
        FklVM *exe) {
    fklCodeBuilderPuts(buf, "#<undefined>");
}

FKL_VM_TYPE_ATTR
FklVMvalueType UndefinedType = FKL_VM_TYPE_STATIC_INIT(UndefinedType,
        {
            .name = "undefined",
            .size = sizeof(FklVMvalueUd),
            .princ = _undefined_userdata_print,
            .prin1 = _undefined_userdata_print,
        });

FKL_VM_TYPE_ATTR FklVMvalueUd FklVMvalueUndefined = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_USERDATA,
    .tp_ = &UndefinedType,
};

FklVMvalue *fklVMvalueUndefined(void) {
    return FKL_TYPE_CAST(FklVMvalue *, &FklVMvalueUndefined);
}

void fklAtomicVMvec(const FklVMvalue *pVec, FklVMgc *gc) {
    const FklVMvalueVec *vec = FKL_VM_VEC(pVec);
    for (size_t i = 0; i < vec->size; i++)
        fklVMgcToGray(vec->base[i], gc);
}

void fklAtomicVMpair(const FklVMvalue *root, FklVMgc *gc) {
    fklVMgcToGray(FKL_VM_CAR(root), gc);
    fklVMgcToGray(FKL_VM_CDR(root), gc);
}

void fklAtomicVMproc(const FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueProc *proc = FKL_VM_PROC(root);
    fklVMgcToGray(proc->name, gc);
    fklVMgcToGray(proc->bcl, gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, proc->proto), gc);
    uint32_t count = proc->ref_count;
    FklVMvalue **ref = proc->closure;
    for (uint32_t i = 0; i < count; i++)
        fklVMgcToGray(ref[i], gc);
}

void fklAtomicVMcproc(const FklVMvalue *root, FklVMgc *gc) {
    FklVMvalueCproc *cproc = FKL_VM_CPROC(root);
    fklVMgcToGray(cproc->dll, gc);
}

void fklAtomicVMbox(const FklVMvalue *root, FklVMgc *gc) {
    fklVMgcToGray(FKL_VM_BOX(root), gc);
}

void fklAtomicVMuserdata(const FklVMvalue *root, FklVMgc *gc) {
    const FklVMvalueUd *ud = FKL_VM_UD(root);
    fklVMgcToGray(FKL_VM_VAL(ud->tp_), gc);
    if (ud->tp_->mt.atomic)
        ud->tp_->mt.atomic(root, gc);
}

static inline int is_callable_ud(const FklVMvalueUd *ud) {
    return ud->tp_->mt.call != NULL;
}

int fklIsCallable(FklVMvalue *v) {
    return FKL_IS_PROC(v) || FKL_IS_CPROC(v)
        || (FKL_IS_USERDATA(v) && is_callable_ud(FKL_VM_UD(v)));
}

static inline int is_writable_ud(const FklVMvalueUd *u) {
    return u->tp_->mt.write != NULL;
}

static inline int is_ud_has_length(const FklVMvalueUd *u) {
    return u->tp_->mt.length != NULL;
}

static inline size_t ud_length(const FklVMvalue *a) {
    return FKL_VM_UD(a)->tp_->mt.length(a);
}

static inline void write_vm_ud(const FklVMvalue *a, FklCodeBuilder *b) {
    FKL_VM_UD(a)->tp_->mt.write(a, b);
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

static void
_lib_userdata_print(const FklVMvalue *ud, FklCodeBuilder *buf, FklVM *exe) {
    FklBuiltinErrorType r = fklVMformat(exe,
            buf,
            "#<lib %S>",
            NULL,
            1,
            (FklVMvalue *[]){ fklVMvalueLib(ud)->name });
    (void)r;
    FKL_ASSERT(r == FKL_ERR_DUMMY);
}

static void _lib_userdata_atomic(const FklVMvalue *v, FklVMgc *gc) {
    FklVMvalueLib *t = fklVMvalueLib(v);

    fklVMgcToGray(t->name, gc);
    fklVMgcToGray(t->proc, gc);

    // 将 count 乘二，标记导出的名字
    size_t total_count = t->count << 1;
    FklVMvalue **cur = t->values;
    FklVMvalue **const end = cur + total_count;

    for (; cur < end; ++cur)
        fklVMgcToGray(*cur, gc);
}

static FklVMudFinalizeResult _lib_userdata_finalize(FklVMvalue *v,
        FklVMgc *gc) {
    FklVMvalueLib *t = fklVMvalueLib(v);
    uv_mutex_destroy(&t->lock);
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_TYPE_ATTR
FklVMvalueType LibType = FKL_VM_TYPE_STATIC_INIT(LibType,
        {
            .name = "lib",
            .size = sizeof(FklVMvalueLib),
            .princ = _lib_userdata_print,
            .prin1 = _lib_userdata_print,
            .atomic = _lib_userdata_atomic,
            .finalize = _lib_userdata_finalize,
        });

int fklIsVMvalueLib(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &LibType.mt;
}

FklVMvalueLib *
fklCreateVMvalueLib(FklVM *exe, FklVMvalue *name, const FklVMvalueVec *names) {
    FKL_ASSERT(FKL_IS_SYM(name));
    FklVMvalueLib *r = NULL;
    size_t const count = names->size;
    size_t const total_count = count << 1;
    size_t extra_size = total_count * sizeof(r->values[0]);

    r = (FklVMvalueLib *)fklCreateVMvalueUd2(exe, &LibType, extra_size);
    uv_mutex_init_recursive(&r->lock);
    r->name = name;
    r->count = count;

    FklVMvalue **cur = &r->values[count];
    for (size_t i = 0; i < count; ++i) {
        cur[i] = names->base[i];
    }

    return r;
}

void fklLockVMlib(FklVMvalueLib *libs) { uv_mutex_lock(&libs->lock); }

void fklUnlockVMlib(FklVMvalueLib *libs) { uv_mutex_unlock(&libs->lock); }

FKL_VM_USER_DATA_DEFAULT_PRINT(weak_hash_eq_print, "weak-hash");

static void weak_hash_eq_atomic(const FklVMvalue *v, FklVMgc *gc) {
    FklVMvalueWeakHashEq *h_v = fklVMvalueWeakHashEq(v);
    if ((h_v->weak_mode & FKL_WEAK_MAP_V) && h_v->weak_mode & FKL_WEAK_MAP_K)
        return;
    for (const FklValueEqHashMapNode *cur = h_v->ht.first; cur;
            cur = cur->next) {
        if (!(h_v->weak_mode & FKL_WEAK_MAP_K))
            fklVMgcToGray(cur->k, gc);
        if (!(h_v->weak_mode & FKL_WEAK_MAP_V))
            fklVMgcToGray(cur->v, gc);
    }
}

static void weak_hash_eq_update_weak_ref(const FklVMvalue *v, FklVMgc *gc) {
    FklVMvalueWeakHashEq *h_v = fklVMvalueWeakHashEq(v);
    FklValueEqHashMap *ht = &h_v->ht;
    const FklValueEqHashMapNode *cur = ht->first;
    while (cur) {
        const FklValueEqHashMapNode *next = cur->next;

        FklVMvalue *k = cur->k;
        FklVMvalue *v = cur->v;

        int k_dead = (h_v->weak_mode & FKL_WEAK_MAP_K) && !fklVMgcIsMarked(k);
        int v_dead = (h_v->weak_mode & FKL_WEAK_MAP_V) && !fklVMgcIsMarked(v);
        if (k_dead || v_dead)
            fklValueEqHashMapDel2(ht, k);

        cur = next;
    }
}

static FklVMudFinalizeResult weak_hash_eq_finalize(FklVMvalue *ud,
        FklVMgc *gc) {
    fklValueEqHashMapUninit(&fklVMvalueWeakHashEq(ud)->ht);
    return FKL_VM_UD_FINALIZE_NOW;
}

static size_t weak_hash_eq_length(const FklVMvalue *v) {
    return fklVMvalueWeakHashEq(v)->ht.count;
}

FKL_VM_TYPE_ATTR
FklVMvalueType WeakHashEqType = FKL_VM_TYPE_STATIC_INIT(WeakHashEqType,
        {
            .name = "weak-hash",
            .size = sizeof(FklVMvalueWeakHashEq),
            .princ = weak_hash_eq_print,
            .prin1 = weak_hash_eq_print,
            .atomic = weak_hash_eq_atomic,
            .finalize = weak_hash_eq_finalize,
            .update_weak_ref = weak_hash_eq_update_weak_ref,
            .length = weak_hash_eq_length,
        });

int fklIsVMvalueWeakHashEq(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &WeakHashEqType.mt;
}

FklVMvalueWeakHashEq *fklCreateVMvalueWeakHashEq(FklVM *vm) {
    return fklCreateVMvalueWeakHashEq2(vm, FKL_WEAK_MAP_V);
}

FklVMvalueWeakHashEq *fklCreateVMvalueWeakHashEq2(FklVM *vm,
        FklWeakMapMode weak_mode) {
    FklVMvalueWeakHashEq *r =
            (FklVMvalueWeakHashEq *)fklCreateVMvalueUd(vm, &WeakHashEqType);

    r->weak_mode = weak_mode;
    fklValueEqHashMapInit(&r->ht);
    return r;
}

FklVMvalue **fklVMvalueWeakHashEqGet(FklVMvalueWeakHashEq *h, FklVMvalue *k) {
    return fklValueEqHashMapGet2(&h->ht, k);
}

FklValueEqHashMapElm *fklVMvalueWeakHashEqInsert(FklVMvalueWeakHashEq *h,
        FklVMvalue *k) {
    return fklValueEqHashMapInsert(&h->ht, &k, NULL);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(obarray_print, "obarray");

FKL_VM_TYPE_ATTR FklVMvalueType ObarrayType;

static FKL_ALWAYS_INLINE FKL_UNUSED int is_obarray(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &ObarrayType.mt;
}

static FKL_ALWAYS_INLINE FklVMvalueObarray *as_obarray(const FklVMvalue *v) {
    FKL_ASSERT(is_obarray(v));
    return FKL_TYPE_CAST(FklVMvalueObarray *, v);
}

static FklVMudFinalizeResult obarray_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueObarray *obarray = as_obarray(ud);
    uv_mutex_destroy(&obarray->lock);
    fklStrValueHashMapUninit(&obarray->map);
    return FKL_VM_UD_FINALIZE_NOW;
}

static void obarray_update_weak_ref(const FklVMvalue *ud, FklVMgc *gc) {
    FklStrValueHashMap *ht = &as_obarray(ud)->map;
    const FklStrValueHashMapNode *cur = ht->first;
    while (cur) {
        const FklStrValueHashMapNode *next = cur->next;
        if (!fklVMgcIsMarked(cur->v)) {
            fklStrValueHashMapDel2(ht, cur->k);
        }
        cur = next;
    }
}

FKL_VM_TYPE_ATTR
FklVMvalueType ObarrayType = FKL_VM_TYPE_STATIC_INIT(ObarrayType,
        {
            .name = "obarray",
            .size = sizeof(FklVMvalueObarray),
            .princ = obarray_print,
            .prin1 = obarray_print,
            .finalize = obarray_finalize,
            .update_weak_ref = obarray_update_weak_ref,
        });

FklVMvalueObarray *fklCreateVMvalueObarray(FklVM *vm) {
    FklVMvalueObarray *obarray =
            (FklVMvalueObarray *)fklCreateVMvalueUd(vm, &ObarrayType);
    uv_mutex_init(&obarray->lock);
    fklStrValueHashMapInit(&obarray->map);

    return obarray;
}

void fklAtomicVMtype(const FklVMvalue *ud, FklVMgc *gc) {
    fklVMgcToGray(FKL_VM_TYPE(ud)->dll, gc);
}

void fklVMtypePrint(const FklVMvalue *v, FklCodeBuilder *b, FklVM *exe) {
    const FklVMvalueType *t = FKL_VM_TYPE(v);
    fklCodeBuilderFmt(b, "#<type %s>", t->mt.name);
}

void fklVMtypeCall(FklVMvalue *v, FklVM *vm) { FKL_TODO(); }

alignas(8) const FklVMvalueType FklVMtypeType = {
    .next_ = NULL,
    .gray_next_ = NULL,
    .mark_ = FKL_MARK_B,
    .type_ = FKL_TYPE_USERDATA,

    .tp_ = &FklVMtypeType,
    .dll = NULL,
    .token = &FklVMtypeType.mt,

    .mt.name = "type",
    .mt.size = sizeof(FklVMvalueType),
    .mt.atomic = fklAtomicVMtype,
    .mt.call = fklVMtypeCall,
    .mt.princ = fklVMtypePrint,
    .mt.prin1 = fklVMtypePrint,
};

FklVMvalueType *fklCreateVMvalueType(FklVM *vm,
        FklVMvalue *dll,
        const void *token,
        const FklVMudMetaTable *mt) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FklVMtypeType);
    FklVMvalueType *t = FKL_VM_TYPE(v);
    t->dll = dll;
    t->token = token;
    t->mt = *mt;
    return t;
}

FklDllUninitFunc fklVMdllGetUninitCb(FklVMvalueDll *dll) {
    FklDllUninitFunc r =
            (FklDllUninitFunc)fklGetAddress("_fklUninit", &dll->dll);
    return r;
}
