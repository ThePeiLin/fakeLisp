#ifndef PRINT_INCLUDED
#include "vmutils.c"
#endif

#ifndef NAME
#define NAME stringify_value_to_string_buffer
#endif

#ifndef LOCAL_VAR
#define LOCAL_VAR
#endif

#ifndef UNINIT_LOCAL_VAR
#define UNINIT_LOCAL_VAR
#endif

#ifndef OUTPUT_TYPE
#define OUTPUT_TYPE FklStringBuffer *
#endif

#ifndef OUTPUT_TYPE_NAME
#define OUTPUT_TYPE_NAME StrBuf
#endif

#ifndef PUTS
#define PUTS(OUT, S) fklStringBufferConcatWithCstr((OUT), (S));
#endif

#ifndef PUTC
#define PUTC(OUT, C) fklStringBufferPutc((OUT), (C));
#endif

#ifndef PRINTF
#define PRINTF fklStringBufferPrintf
#endif

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define METHOD(name) CONCAT(CONCAT(OUTPUT_TYPE_NAME, __), name)

#ifndef PUT_ATOMIC_METHOD_ARG
#define PUT_ATOMIC_METHOD_ARG void (*atom_stringifier)(VMVALUE_TO_UTSTRING_ARGS)
#endif

#ifndef PUT_ATOMIC_METHOD
#define PUT_ATOMIC_METHOD(OUT, V, GC) atom_stringifier((OUT), (V), (GC))
#endif

#if !defined(PUT_ATOMIC_METHOD) || !defined(PUT_ATOMIC_METHOD_ARG)
#error "PUT_ATOMIC_METHOD and PUT_ATOMIC_METHOD_ARG should be defined"
#endif

static inline int METHOD(print_circle_head)(OUTPUT_TYPE result,
        const FklVMvalue *v,
        const VmCircleHeadHashMap *circle_head_set) {
    PrtSt *item = vmCircleHeadHashMapGet2(circle_head_set, v);
    if (item) {
        if (item->printed) {
            PRINTF(result, "#%u#", item->i);
            return 1;
        } else {
            item->printed = 1;
            PRINTF(result, "#%u=", item->i);
        }
    }
    return 0;
}

static inline const FklVMvalue *METHOD(
        print_method)(PrintCtx *ctx, OUTPUT_TYPE buf) {
#define pair_ctx (FKL_TYPE_CAST(PrintPairCtx *, ctx))
#define vec_ctx (FKL_TYPE_CAST(PrintVectorCtx *, ctx))
#define hash_ctx (FKL_TYPE_CAST(PrintHashCtx *, ctx))
#define vec (FKL_VM_VEC(vec_ctx->vec))
#define hash_table (FKL_VM_HASH(hash_ctx->hash))

    const FklVMvalue *r = NULL;
    switch (ctx->state) {
    case PRINT_METHOD_FIRST:
        ctx->state = PRINT_METHOD_CONT;
        goto first_call;
        break;
    case PRINT_METHOD_CONT:
        goto cont_call;
        break;
    default:
        abort();
    }
    return NULL;
first_call:
    switch (ctx->type) {
    case FKL_TYPE_PAIR:
        PUTC(buf, '(');
        r = FKL_VM_CAR(pair_ctx->cur);
        pair_ctx->cur = FKL_VM_CDR(pair_ctx->cur);
        break;

    case FKL_TYPE_VECTOR:
        if (vec->size == 0) {
            PUTS(buf, "#()");
            return NULL;
        }
        PUTS(buf, "#(");
        r = vec->base[vec_ctx->index++];
        break;

    case FKL_TYPE_HASHTABLE:
        if (hash_table->ht.count == 0) {
            PUTS(buf, fklGetVMhashTablePrefix(hash_table));
            PUTC(buf, ')');
            return NULL;
        }
        PUTS(buf, fklGetVMhashTablePrefix(hash_table));
        PUTC(buf, '(');
        r = hash_ctx->cur->k;
        break;
    default:
        fprintf(stderr,
                "[ERROR %s: %u]\tunreachable \n",
                __FUNCTION__,
                __LINE__);
        abort();
        break;
    }
    return r;

cont_call:
    switch (ctx->type) {
    case FKL_TYPE_PAIR:
        if (pair_ctx->cur == NULL) {
            PUTC(buf, ')');
            return NULL;
        } else if (FKL_IS_PAIR(pair_ctx->cur)) {
            if (vmCircleHeadHashMapGet2(pair_ctx->circle_head_set,
                        pair_ctx->cur))
                goto print_cdr;
            PUTC(buf, ' ');
            r = FKL_VM_CAR(pair_ctx->cur);
            pair_ctx->cur = FKL_VM_CDR(pair_ctx->cur);
        } else if (FKL_IS_NIL(pair_ctx->cur)) {
            PUTC(buf, ')');
            r = NULL;
            pair_ctx->cur = NULL;
        } else {
        print_cdr:
            PUTC(buf, ',');
            r = pair_ctx->cur;
            pair_ctx->cur = NULL;
        }
        break;

    case FKL_TYPE_VECTOR:
        if (vec_ctx->index >= vec->size) {
            PUTC(buf, ')');
            return NULL;
        }
        PUTC(buf, ' ');
        r = vec->base[vec_ctx->index++];
        break;

    case FKL_TYPE_HASHTABLE:
        if (hash_ctx->cur == NULL) {
            PUTS(buf, "))");
            return NULL;
        }
        switch (hash_ctx->place) {
        case PRT_CAR:
            hash_ctx->place = PRT_CDR;
            PUTC(buf, ',');
            r = hash_ctx->cur->v;
            break;
        case PRT_CDR:
            hash_ctx->place = PRT_CAR;
            hash_ctx->cur = hash_ctx->cur->next;
            if (hash_ctx->cur == NULL) {
                PUTS(buf, "))");
                return NULL;
            }
            PUTS(buf, ") (");
            r = hash_ctx->cur->k;
            break;
        }
        break;
    default:
        fprintf(stderr,
                "[ERROR %s: %u]\tunreachable \n",
                __FUNCTION__,
                __LINE__);
        abort();
        break;
    }
    return r;
#undef pair_ctx
#undef vec_ctx
#undef hash_ctx
#undef vec
#undef hash_table
}

static inline void NAME(const FklVMvalue *v,
        OUTPUT_TYPE result,
        PUT_ATOMIC_METHOD_ARG,
        FklVMgc *gc) {
    LOCAL_VAR;
    if (!FKL_IS_VECTOR(v) && !FKL_IS_PAIR(v) && !FKL_IS_BOX(v)
            && !FKL_IS_HASHTABLE(v)) {
        PUT_ATOMIC_METHOD(result, v, gc);
        UNINIT_LOCAL_VAR;
        return;
    }

    VmCircleHeadHashMap circle_head_set;
    vmCircleHeadHashMapInit(&circle_head_set);

    scan_cir_ref(v, &circle_head_set);

    VmPrintCtxVector print_ctxs;
    vmPrintCtxVectorInit(&print_ctxs, 8);

    for (; v;) {
        if (FKL_IS_PAIR(v)) {
            PrtSt *item = vmCircleHeadHashMapGet2(&circle_head_set, v);
            if (item) {
                if (item->printed) {
                    PRINTF(result, "#%u#", item->i);
                    goto get_next;
                } else {
                    item->printed = 1;
                    PRINTF(result, "#%u=", item->i);
                }
            }
            PrintCtx *ctx = vmPrintCtxVectorPushBack(&print_ctxs, NULL);
            init_common_print_ctx(ctx, FKL_TYPE_PAIR);
            print_pair_ctx_init(ctx, v, &circle_head_set);
        } else if (FKL_IS_VECTOR(v)) {
            if (METHOD(print_circle_head)(result, v, &circle_head_set))
                goto get_next;
            PrintCtx *ctx = vmPrintCtxVectorPushBack(&print_ctxs, NULL);
            init_common_print_ctx(ctx, FKL_TYPE_VECTOR);
            print_vector_ctx_init(ctx, v);
        } else if (FKL_IS_BOX(v)) {
            if (METHOD(print_circle_head)(result, v, &circle_head_set))
                goto get_next;
            PUTS(result, "#&");
            v = FKL_VM_BOX(v);
            continue;
        } else if (FKL_IS_HASHTABLE(v)) {
            if (METHOD(print_circle_head)(result, v, &circle_head_set))
                goto get_next;
            PrintCtx *ctx = vmPrintCtxVectorPushBack(&print_ctxs, NULL);
            init_common_print_ctx(ctx, FKL_TYPE_HASHTABLE);
            print_hash_ctx_init(ctx, v);
        } else {
            PUT_ATOMIC_METHOD(result, v, gc);
        }
    get_next:
        if (vmPrintCtxVectorIsEmpty(&print_ctxs))
            break;
        while (!vmPrintCtxVectorIsEmpty(&print_ctxs)) {
            PrintCtx *ctx = vmPrintCtxVectorBack(&print_ctxs);
            v = METHOD(print_method)(ctx, result);
            if (v)
                break;
            else
                vmPrintCtxVectorPopBack(&print_ctxs);
        }
    }
    vmPrintCtxVectorUninit(&print_ctxs);
    vmCircleHeadHashMapUninit(&circle_head_set);
    UNINIT_LOCAL_VAR;
}

#undef PRINT_INCLUDED
#undef OUTPUT_TYPE
#undef OUTPUT_TYPE_NAME
#undef CONCAT
#undef CONCAT_
#undef METHOD
#undef PUTS
#undef PUTC
#undef PRINTF
#undef NAME
#undef LOCAL_VAR
#undef UNINIT_LOCAL_VAR
#undef PUT_ATOMIC_METHOD_ARG
#undef PUT_ATOMIC_METHOD
#undef FUNC_ATTR
