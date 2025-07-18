#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

static int ht_hashv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 0);
    uint64_t hashv = fklVMvalueEqHashv(key);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(hashv, exe));
    return 0;
}

static int ht_eqv_hashv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 0);
    uint64_t hashv = fklVMvalueEqvHashv(key);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(hashv, exe));
    return 0;
}

static int ht_equal_hashv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 0);
    uint64_t hashv = fklVMvalueEqualHashv(key);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(hashv, exe));
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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(ht_as_print, "ht");

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

static int ht_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(ht, HashTable, ud);
    fklVMvalueHashMapUninit(&ht->ht);
    return FKL_VM_UD_FINALIZE_NOW;
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
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *hashv = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *equal = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(hashv, fklIsCallable, exe);
    FKL_CHECK_TYPE(equal, fklIsCallable, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &HtUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(ht, HashTable, ud);
    ht->hash_func = hashv;
    ht->eq_func = equal;
    fklVMvalueHashMapInit(&ht->ht);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

#define IS_HASH_UD(V) (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &HtUdMetaTable)

static int ht_ht_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    if (IS_HASH_UD(val))
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_TRUE);
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int ht_ht_hashv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FKL_CPROC_RETURN(exe, ctx, ht->hash_func);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_equal(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        FKL_CPROC_RETURN(exe, ctx, ht->eq_func);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_clear(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    if (IS_HASH_UD(val)) {
        FKL_DECL_VM_UD_DATA(ht, HashTable, val);
        fklVMvalueHashMapClear(&ht->ht);
        FKL_CPROC_RETURN(exe, ctx, val);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_to_list(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
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
        FKL_CPROC_RETURN(exe, ctx, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_keys(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
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
        FKL_CPROC_RETURN(exe, ctx, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

static int ht_ht_values(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
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
        FKL_CPROC_RETURN(exe, ctx, r);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    return 0;
}

#define HT_CALL_STATE_ENTER (0)
#define HT_CALL_STATE_EQUAL (1)
#define HT_CALL_STATE_VALUE (2)

#define HT_CTX_STATE(CTX) (CTX)->c[0].u32a
#define HT_CTX_ARG_NUM(CTX) (CTX)->c[0].u32b
#define HT_CTX_NODE(CTX)                                                       \
    (*FKL_TYPE_CAST(FklVMvalueHashMapNode *const **, &(CTX)->c[1].ptr))
#define HT_CTX_HASHV(CTX) (CTX)->c[2].uptr

#define HT_STATE_UNREACHABLE()                                                 \
    fprintf(stderr,                                                            \
            "[%s: %d] %s: unreachable!\n",                                     \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __FUNCTION__);                                                     \
    abort();

static inline int key_equal(FklVM *exe,
        FklVMvalue *key,
        FklCprocFrameContext *ctx,
        FklVMvalue *eq_func,
        uintptr_t hashv,
        FklVMvalueHashMapNode *const *slot) {
    HT_CTX_STATE(ctx) = HT_CALL_STATE_VALUE;
    HT_CTX_NODE(ctx) = slot;
    HT_CTX_HASHV(ctx) = hashv;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, eq_func);
    FKL_VM_PUSH_VALUE(exe, key);
    FKL_VM_PUSH_VALUE(exe, (*slot)->k);
    fklCallObj(exe, eq_func);
    return 1;
}

static int ht_ht_set1(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else {
            FklVMvalueHashMapNode *node = fklVMvalueHashMapCreateNode2(hashv,
                    FKL_CPROC_GET_ARG(exe, ctx, 1));
            node->v = FKL_CPROC_GET_ARG(exe, ctx, 2);
            fklVMvalueHashMapInsertNode(&ht->ht, node);
            FKL_CPROC_RETURN(exe, ctx, node->v);
        }
    } break;
    case HT_CALL_STATE_VALUE: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        HT_CTX_HASHV(ctx),
                        node);
            } else {
                FklVMvalueHashMapNode *item =
                        fklVMvalueHashMapCreateNode2(hashv,
                                FKL_CPROC_GET_ARG(exe, ctx, 1));
                item->v = FKL_CPROC_GET_ARG(exe, ctx, 2);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                FKL_CPROC_RETURN(exe, ctx, item->v);
            }
        } else {
            FklVMvalueHashMapNode *i = *node;
            i->v = FKL_CPROC_GET_ARG(exe, ctx, 2);
            FKL_CPROC_RETURN(exe, ctx, i->v);
        }
    } break;
    default:
        HT_STATE_UNREACHABLE();
        break;
    }
    return 0;
}

static int ht_ht_set8(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, -1);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        uint32_t const rest_arg_num = argc - 1;
        if (rest_arg_num == 0) {
            FKL_CPROC_RETURN(exe, ctx, ht_ud);
            return 0;
        }
        if (rest_arg_num % 2)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        FklVMvalue *first_key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        HT_CTX_ARG_NUM(ctx) = rest_arg_num;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, first_key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        uint32_t const rest_arg_num = HT_CTX_ARG_NUM(ctx);
        uint32_t const key_idx =
                exe->tp - FKL_VM_FRAME_OF(ctx)->bp - rest_arg_num - 1;
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, key_idx);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe, key, ctx, ht->eq_func, hashv, node);
        } else {
            FklVMvalueHashMapNode *item =
                    fklVMvalueHashMapCreateNode2(hashv, key);
            item->v = FKL_CPROC_GET_ARG(exe, ctx, key_idx + 1);
            fklVMvalueHashMapInsertNode(&ht->ht, item);
            if (rest_arg_num > 2) {
                HT_CTX_ARG_NUM(ctx) = rest_arg_num - 2;
                fklSetBp(exe);
                FKL_VM_PUSH_VALUE(exe, ht->hash_func);
                FKL_VM_PUSH_VALUE(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, key_idx + 2));
                fklCallObj(exe, ht->hash_func);
                return 1;
            } else {
                FKL_CPROC_RETURN(exe, ctx, item->v);
            }
        }
    } break;
    case HT_CALL_STATE_VALUE: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);

        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        uint32_t const rest_arg_num = HT_CTX_ARG_NUM(ctx);
        uint32_t const key_idx =
                exe->tp - FKL_VM_FRAME_OF(ctx)->bp - rest_arg_num - 1;
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, key_idx);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        if (equal_result == FKL_VM_NIL) {
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe, key, ctx, ht->eq_func, hashv, node);
            } else {
                FklVMvalueHashMapNode *item =
                        fklVMvalueHashMapCreateNode2(hashv, key);
                item->v = FKL_CPROC_GET_ARG(exe, ctx, key_idx + 1);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                if (rest_arg_num > 2) {
                    HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
                    HT_CTX_ARG_NUM(ctx) = rest_arg_num - 2;
                    fklSetBp(exe);
                    FKL_VM_PUSH_VALUE(exe, ht->hash_func);
                    FKL_VM_PUSH_VALUE(exe,
                            FKL_CPROC_GET_ARG(exe, ctx, key_idx + 2));
                    fklCallObj(exe, ht->hash_func);
                    return 1;
                } else {
                    FKL_CPROC_RETURN(exe, ctx, item->v);
                }
            }
        } else {
            FklVMvalueHashMapNode *i = *node;
            i->v = FKL_CPROC_GET_ARG(exe, ctx, key_idx + 1);
            if (rest_arg_num > 2) {
                HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
                HT_CTX_ARG_NUM(ctx) = rest_arg_num - 2;
                fklSetBp(exe);
                FKL_VM_PUSH_VALUE(exe, ht->hash_func);
                FKL_VM_PUSH_VALUE(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, key_idx + 2));
                fklCallObj(exe, ht->hash_func);
                return 1;
            } else {
                FKL_CPROC_RETURN(exe, ctx, i->v);
            }
        }
    } break;
    default:
        HT_STATE_UNREACHABLE();
        break;
    }
    return 0;
}

static int ht_ht_ref(FKL_CPROC_ARGL) {

    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        HT_CTX_ARG_NUM(ctx) = argc;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else if (HT_CTX_ARG_NUM(ctx) > 2) {
            FklVMvalue *default_value = FKL_CPROC_GET_ARG(exe, ctx, 2);
            FKL_CPROC_RETURN(exe, ctx, default_value);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY, exe);
    } break;
    case HT_CALL_STATE_VALUE: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        hashv,
                        node);
            } else if (HT_CTX_ARG_NUM(ctx) > 2) {
                FklVMvalue *default_value = FKL_CPROC_GET_ARG(exe, ctx, 2);
                FKL_CPROC_RETURN(exe, ctx, default_value);
            } else
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY, exe);
        } else {
            FKL_CPROC_RETURN(exe, ctx, (*node)->v);
        }
    } break;
    default:
        HT_STATE_UNREACHABLE();
        break;
    }
    return 0;
}

static int ht_ht_ref1(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case 1: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else {
            FklVMvalueHashMapNode *item = fklVMvalueHashMapCreateNode2(hashv,
                    FKL_CPROC_GET_ARG(exe, ctx, 1));
            item->v = FKL_CPROC_GET_ARG(exe, ctx, 2);
            fklVMvalueHashMapInsertNode(&ht->ht, item);
            FKL_CPROC_RETURN(exe, ctx, item->v);
        }
    } break;
    default: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        hashv,
                        node);
            } else {
                FklVMvalueHashMapNode *item =
                        fklVMvalueHashMapCreateNode2(hashv,
                                FKL_CPROC_GET_ARG(exe, ctx, 1));
                item->v = FKL_CPROC_GET_ARG(exe, ctx, 2);
                fklVMvalueHashMapInsertNode(&ht->ht, item);
                FKL_CPROC_RETURN(exe, ctx, item->v);
            }
        } else {
            FKL_CPROC_RETURN(exe, ctx, (*node)->v);
        }
    } break;
    }
    return 0;
}

static int ht_ht_ref7(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } break;
    default: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        hashv,
                        node);
            } else
                FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        } else {
            FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueBox(exe, (*node)->v));
        }
    } break;
    }
    return 0;
}

static int ht_ht_ref4(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } break;
    case HT_CALL_STATE_VALUE: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        if (equal_result == FKL_VM_NIL) {
            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        hashv,
                        node);
            } else
                FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        } else {
            FklVMvalueHashMapNode *i = *node;
            FKL_CPROC_RETURN(exe, ctx, fklCreateVMvaluePair(exe, i->k, i->v));
        }
    } break;
    default:
        HT_STATE_UNREACHABLE();
        break;
    }
    return 0;
}

static int ht_ht_del1(FKL_CPROC_ARGL) {
    switch (HT_CTX_STATE(ctx)) {
    case HT_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *key = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(ht_ud, IS_HASH_UD, exe);
        HT_CTX_STATE(ctx) = HT_CALL_STATE_EQUAL;
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, ht->hash_func);
        FKL_VM_PUSH_VALUE(exe, key);
        fklCallObj(exe, ht->hash_func);
        return 1;
    } break;
    case HT_CALL_STATE_EQUAL: {
        FklVMvalue *hash_value = FKL_VM_POP_TOP_VALUE(exe);
        FKL_CHECK_TYPE(hash_value, fklIsVMint, exe);
        uintptr_t hashv = fklVMintToHashv(hash_value);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        FklVMvalueHashMapNode *const *node =
                fklVMvalueHashMapBucket(&ht->ht, hashv);
        if (*node) {
            return key_equal(exe,
                    FKL_CPROC_GET_ARG(exe, ctx, 1),
                    ctx,
                    ht->eq_func,
                    hashv,
                    node);
        } else
            FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } break;
    case HT_CALL_STATE_VALUE: {
        FklVMvalueHashMapNode *const *node = HT_CTX_NODE(ctx);
        uintptr_t hashv = HT_CTX_HASHV(ctx);
        FklVMvalue *equal_result = FKL_VM_POP_TOP_VALUE(exe);
        FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
        if (equal_result == FKL_VM_NIL) {
            node = &(*node)->bkt_next;
            if (*node) {
                return key_equal(exe,
                        FKL_CPROC_GET_ARG(exe, ctx, 1),
                        ctx,
                        ht->eq_func,
                        hashv,
                        node);
            } else
                FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
        } else {
            FklVMvalueHashMapNode *i = *node;
            FklVMvalue *key = i->k;
            FklVMvalue *val = i->v;

            FklVMvalue *ht_ud = FKL_CPROC_GET_ARG(exe, ctx, 0);
            FKL_DECL_VM_UD_DATA(ht, HashTable, ht_ud);
            fklVMvalueHashMapDelNode(&ht->ht,
                    FKL_REMOVE_CONST(FklVMvalueHashMapNode *, node));
            FKL_CPROC_RETURN(exe, ctx, fklCreateVMvaluePair(exe, key, val));
        }
    } break;
    default:
        HT_STATE_UNREACHABLE();
        break;
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

FKL_DLL_EXPORT FklSid_t *_fklExportSymbolInit(
        FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)fklZmalloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
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
