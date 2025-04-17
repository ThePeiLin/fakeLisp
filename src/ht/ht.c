#include <fakeLisp/vm.h>

static int ht_hashv(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(key, exe);
    FKL_CHECK_REST_ARG(exe);
    uint64_t hashv = fklVMvalueEqHashv(key);
    FKL_VM_PUSH_VALUE(exe, fklMakeVMuint(hashv, exe));
    return 0;
}

static int ht_eqv_hashv(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(key, exe);
    FKL_CHECK_REST_ARG(exe);
    uint64_t hashv = fklVMvalueEqvHashv(key);
    FKL_VM_PUSH_VALUE(exe, fklMakeVMuint(hashv, exe));
    return 0;
}

static int ht_equal_hashv(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(key, exe);
    FKL_CHECK_REST_ARG(exe);
    uint64_t hashv = fklVMvalueEqualHashv(key);
    FKL_VM_PUSH_VALUE(exe, fklMakeVMuint(hashv, exe));
    return 0;
}

typedef struct {
    FklVMvalue *hash_func;
    FklVMvalue *eq_func;
    FklVMvalueHashMap ht;
} HashTable;

static void ht_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(ht, HashTable, ud);
    fklVMgcToGray(ht->hash_func, gc);
    fklVMgcToGray(ht->eq_func, gc);
    FklVMvalueHashMap *t = &ht->ht;
    for (FklVMvalueHashMapNode *list = t->first; list; list = list->next) {
        fklVMgcToGray(list->k, gc);
        fklVMgcToGray(list->v, gc);
    }
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(ht_as_print, ht);

static int ht_equal(const FklVMud *a, const FklVMud *b) {
    FKL_DECL_UD_DATA(hta, HashTable, a);
    FKL_DECL_UD_DATA(htb, HashTable, b);
    if (fklVMvalueEqual(hta->hash_func, htb->hash_func)
        && fklVMvalueEqual(htb->eq_func, htb->eq_func)
        && hta->ht.count == htb->ht.count) {
        FklVMvalueHashMapNode *ia = hta->ht.first;
        FklVMvalueHashMapNode *ib = htb->ht.first;
        while (ia) {
            if (fklVMvalueEqual(ia->k, ib->k)
                && fklVMvalueEqual(ia->v, ib->v)) {
                ia = ia->next;
                ib = ib->next;
            } else
                return 0;
        }
        return 1;
    } else
        return 0;
}

static void ht_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(ht, HashTable, ud);
    fklVMvalueHashMapUninit(&ht->ht);
}

static size_t ht_length(const FklVMud *ud) {
    FKL_DECL_UD_DATA(ht, HashTable, ud);
    return ht->ht.count;
}

static FklVMudMetaTable HtUdMetaTable = {
    .size = sizeof(HashTable),
    .__length = ht_length,
    .__atomic = ht_atomic,
    .__equal = ht_equal,
    .__as_prin1 = ht_as_print,
    .__as_princ = ht_as_print,
    .__finalizer = ht_finalizer,
};

static int ht_make_ht(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(hashv, equal, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(hashv, fklIsCallable, exe);
    FKL_CHECK_TYPE(equal, fklIsCallable, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &HtUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(ht, HashTable, ud);
    ht->hash_func = hashv;
    ht->eq_func = equal;
    fklVMvalueHashMapInit(&ht->ht);
    FKL_VM_PUSH_VALUE(exe, ud);
    return 0;
}

#define IS_HASH_UD(V) (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &HtUdMetaTable)

static int ht_ht_p(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val))
        FKL_VM_PUSH_VALUE(exe, FKL_VM_TRUE);
    else
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    return 0;
}

static int ht_ht_hashv(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_equal(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FKL_VM_PUSH_VALUE(exe, ht->eq_func);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_clear(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        fklVMvalueHashMapClear(&ht->ht);
        FKL_VM_PUSH_VALUE(exe, val);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_to_list(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FklVMvalueHashMap *hash = &ht->ht;
        FklVMvalue *r = FKL_VM_NIL;
        FklVMvalue **cur = &r;
        for (FklVMvalueHashMapNode *list = hash->first; list;
             list = list->next) {
            FklVMvalue *pair = fklCreateVMvaluePair(exe, list->k, list->v);
            *cur = fklCreateVMvaluePairWithCar(exe, pair);
            cur = &FKL_VM_CDR(*cur);
        }
        FKL_VM_PUSH_VALUE(exe, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_keys(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FklVMvalueHashMap *hash = &ht->ht;
        FklVMvalue *r = FKL_VM_NIL;
        FklVMvalue **cur = &r;
        for (FklVMvalueHashMapNode *list = hash->first; list;
             list = list->next) {
            *cur = fklCreateVMvaluePairWithCar(exe, list->k);
            cur = &FKL_VM_CDR(*cur);
        }
        FKL_VM_PUSH_VALUE(exe, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_values(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(val, exe);
    FKL_CHECK_REST_ARG(exe);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FklVMvalueHashMap *hash = &ht->ht;
        FklVMvalue *r = FKL_VM_NIL;
        FklVMvalue **cur = &r;
        for (FklVMvalueHashMapNode *list = hash->first; list;
             list = list->next) {
            *cur = fklCreateVMvaluePairWithCar(exe, list->v);
            cur = &FKL_VM_CDR(*cur);
        }
        FKL_VM_PUSH_VALUE(exe, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static inline int key_equal(FklVM *exe, FklCprocFrameContext *ctx,
                            FklVMvalue *eq_func, uintptr_t hashv,
                            FklVMvalueHashMapNode *const *slot) {
    FklVMvalue *key = FKL_VM_GET_VALUE(exe, 2);
    ctx->ptr = FKL_TYPE_CAST(void *, slot);
    ctx->ctx1 = hashv;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, (*slot)->k);
    FKL_VM_PUSH_VALUE(exe, key);
    fklCallObj(exe, eq_func);
    return 1;
}

#define KEY_EQUAL()                                                            \
    FklVMhashTableItem *i = (FklVMhashTableItem *)((*slot)->data);             \
    FklVMvalue *key = FKL_VM_GET_VALUE(exe, 2);                                \
    ctx->pointer = slot;                                                       \
    fklSetBp(exe);                                                             \
    FKL_VM_PUSH_VALUE(exe, i->key);                                            \
    FKL_VM_PUSH_VALUE(exe, key);                                               \
    fklCallObj(exe, ht->eq_func);                                              \
    return 1

static int ht_ht_set1(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG3(ht_ud, key, val, exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, val);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else {
            FklVMvalueHashMapNode *node =
                fklVMvalueHashMapCreateNode2(hashv, FKL_VM_GET_VALUE(exe, 2));
            node->v = FKL_VM_GET_VALUE(exe, 3);
            fklVMvalueHashMapInsertNode(&ht->ht, node);
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, node->v);
        }
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, ctx->ctx1, node);
            } else {
                FklVMvalueHashMapNode *item = fklVMvalueHashMapCreateNode2(
                    hashv, FKL_VM_GET_VALUE(exe, 2));
                item->v = FKL_VM_GET_VALUE(exe, 3);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, item->v);
            }
        } else {
            FklVMvalueHashMapNode *i = *node;
            i->v = FKL_VM_GET_VALUE(exe, 3);
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, i->v);
        }
    } break;
    }
    return 0;
}

static int ht_ht_set8(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG(ht_ud, exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        size_t arg_num = exe->tp - exe->bp;
        if (arg_num == 0) {
            fklResBp(exe);
            FKL_VM_PUSH_VALUE(exe, ht_ud);
            return 0;
        }
        if (arg_num % 2)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        FklVMvalue *first_key = FKL_VM_GET_TOP_VALUE(exe);
        uint32_t rtp = fklResBpIn(exe, arg_num);
        ctx->context = 1;
        ctx->rtp = rtp;
        FKL_VM_GET_VALUE(exe, arg_num + 1) = FKL_VM_NIL;
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, first_key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else {
#define CONTINUE_ARG_NUM (4)

            FklVMvalueHashMapNode *item =
                fklVMvalueHashMapCreateNode2(hashv, FKL_VM_GET_VALUE(exe, 2));
            item->v = FKL_VM_GET_VALUE(exe, 3);
            fklVMvalueHashMapInsertNode(&ht->ht, item);
            if (exe->tp - ctx->rtp > CONTINUE_ARG_NUM) {
                exe->tp -= 3;
                FklVMvalue *key = FKL_VM_GET_TOP_VALUE(exe);
                FKL_VM_PUSH_VALUE(exe, ht_ud);
                fklSetBp(exe);
                FKL_VM_PUSH_VALUE(exe, key);
                fklCallObj(exe, ht->hash_func);
                return 1;
            } else
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, item->v);
        }
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);

        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        if (equal_result == FKL_VM_NIL) {
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else {
                FklVMvalueHashMapNode *item = fklVMvalueHashMapCreateNode2(
                    hashv, FKL_VM_GET_VALUE(exe, 2));
                item->v = FKL_VM_GET_VALUE(exe, 3);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                if (exe->tp - ctx->rtp > CONTINUE_ARG_NUM) {
                    ctx->context = 1;
                    exe->tp -= 3;
                    FklVMvalue *key = FKL_VM_GET_TOP_VALUE(exe);
                    FKL_VM_PUSH_VALUE(exe, ht_ud);
                    fklSetBp(exe);
                    FKL_VM_PUSH_VALUE(exe, key);
                    fklCallObj(exe, ht->hash_func);
                    return 1;
                } else
                    FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, item->v);
            }
        } else {
            FklVMvalueHashMapNode *i = *node;
            i->v = FKL_VM_GET_VALUE(exe, 3);
            if (exe->tp - ctx->rtp > CONTINUE_ARG_NUM) {
                ctx->context = 1;
                exe->tp -= 3;
                FklVMvalue *key = FKL_VM_GET_TOP_VALUE(exe);
                FKL_VM_PUSH_VALUE(exe, ht_ud);
                fklSetBp(exe);
                FKL_VM_PUSH_VALUE(exe, key);
                fklCallObj(exe, ht->hash_func);
                return 1;
            } else
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, i->v);
        }
    } break;
    }
    return 0;
}

static int ht_ht_ref(FKL_CPROC_ARGL) {
#define ARG_NUM_WITHOUT_DEFAULT_VALUE (3)

    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG2(ht_ud, key, exe);
        FklVMvalue *default_value = FKL_VM_POP_ARG(exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        if (default_value)
            FKL_VM_PUSH_VALUE(exe, default_value);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else if (exe->tp - ctx->rtp > ARG_NUM_WITHOUT_DEFAULT_VALUE) {
            FklVMvalue *default_value =
                FKL_VM_GET_VALUE(exe, ARG_NUM_WITHOUT_DEFAULT_VALUE);
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, default_value);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY, exe);
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else if (exe->tp - ctx->rtp > ARG_NUM_WITHOUT_DEFAULT_VALUE) {
                FklVMvalue *default_value =
                    FKL_VM_GET_VALUE(exe, ARG_NUM_WITHOUT_DEFAULT_VALUE);
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, default_value);
            } else
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY, exe);
        } else {
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, (*node)->v);
        }
    } break;
    }
    return 0;

#undef ARG_NUM_WITHOUT_DEFAULT_VALUE
}

static int ht_ht_ref1(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG3(ht_ud, key, val, exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, val);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else {
            FklVMvalueHashMapNode *item =
                fklVMvalueHashMapCreateNode2(hashv, FKL_VM_GET_VALUE(exe, 2));
            item->v = FKL_VM_GET_VALUE(exe, 3);
            fklVMvalueHashMapInsertNode(&ht->ht, item);
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, item->v);
        }
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else {
                FklVMvalueHashMapNode *item = fklVMvalueHashMapCreateNode2(
                    hashv, FKL_VM_GET_VALUE(exe, 2));
                item->v = FKL_VM_GET_VALUE(exe, 3);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, item->v);
            }
        } else {
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, (*node)->v);
        }
    } break;
    }
    return 0;
}

static int ht_ht_ref7(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG2(ht_ud, key, exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
        } else {
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp,
                                         fklCreateVMvalueBox(exe, (*node)->v));
        }
    } break;
    }
    return 0;
}

static int ht_ht_ref4(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG2(ht_ud, key, exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
        } else {
            FklVMvalueHashMapNode *i = *node;
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp,
                                         fklCreateVMvaluePair(exe, i->k, i->v));
        }
    } break;
    }
    return 0;
}

static int ht_ht_del1(FKL_CPROC_ARGL) {
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG2(ht_ud, key, exe);
        FKL_CHECK_REST_ARG(exe);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        ctx->context = 1;
        ctx->rtp = exe->tp;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_VM_PUSH_VALUE(exe, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, key);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
            fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, ctx, ht->eq_func, hashv, node);
        } else
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
    } break;
    default: {
        FklVMvalueHashMapNode *const *node =
            FKL_TYPE_CAST(FklVMvalueHashMapNode *const *, ctx->ptr);
        uintptr_t hashv = ctx->ctx1;
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        if (equal_result == FKL_VM_NIL) {
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, ctx, ht->eq_func, hashv, node);
            } else
                FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, FKL_VM_NIL);
        } else {
            FklVMvalueHashMapNode *i = *node;
            FklVMvalue *key = i->k;
            FklVMvalue *val = i->v;

            FklVMvalue *ht_ud = FKL_VM_GET_TOP_VALUE(exe);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            fklVMvalueHashMapDelNode(
                &ht->ht, FKL_REMOVE_CONST(FklVMvalueHashMapNode *, node));
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp,
                                         fklCreateVMvaluePair(exe, key, val));
        }
    } break;
    }
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    {"hashv",       ht_hashv       },
    {"eqv-hashv",   ht_eqv_hashv   },
    {"equal-hashv", ht_equal_hashv },
    {"make-ht",     ht_make_ht     },
    {"ht-hashv",    ht_ht_hashv    },
    {"ht-equal",    ht_ht_equal    },
    {"ht?",         ht_ht_p        },
    {"ht-ref",      ht_ht_ref      },
    {"ht-ref&",     ht_ht_ref7     },
    {"ht-ref$",     ht_ht_ref4     },
    {"ht-ref!",     ht_ht_ref1     },
    {"ht-set!",     ht_ht_set1     },
    {"ht-set*!",    ht_ht_set8     },
    {"ht-del!",     ht_ht_del1     },
    {"ht-clear!",   ht_ht_clear    },
    {"ht->list",    ht_ht_to_list  },
    {"ht-keys",     ht_ht_keys     },
    {"ht-values",   ht_ht_values   },
    // clang-format on
};

static const size_t EXPORT_NUM =
    sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklSid_t *
_fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)malloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc = (FklVMvalue **)malloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, NULL, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);
    return loc;
}
