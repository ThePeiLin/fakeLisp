#include <fakeLisp/bigint.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FklVMvalue *fklMakeVMint(int64_t r64, FklVM *vm) {
    if (r64 > FKL_FIX_INT_MAX || r64 < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithI64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

FklVMvalue *fklMakeVMuint(uint64_t r64, FklVM *vm) {
    if (r64 > FKL_FIX_INT_MAX)
        return fklCreateVMvalueBigIntWithU64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

FklVMvalue *fklMakeVMintD(double r64, FklVM *vm) {
    if (isgreater(r64, (double)FKL_FIX_INT_MAX) || isless(r64, FKL_FIX_INT_MIN))
        return fklCreateVMvalueBigIntWithF64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

static inline FklVMvalue *get_initial_fast_value(const FklVMvalue *pr) {
    return FKL_IS_PAIR(pr) ? FKL_VM_CDR(pr) : FKL_VM_NIL;
}

static inline FklVMvalue *get_fast_value(const FklVMvalue *head) {
    return (FKL_IS_PAIR(head)                       //
                   && FKL_IS_PAIR(FKL_VM_CDR(head)) //
                   && FKL_IS_PAIR(FKL_VM_CDR(FKL_VM_CDR(head))))
                 ? FKL_VM_CDR(FKL_VM_CDR(head))
                 : FKL_VM_NIL;
}

int fklIsList(const FklVMvalue *p) {
    FklVMvalue *fast = get_initial_fast_value(p);
    while (FKL_IS_PAIR(p)) {
        if (fast == p)
            return 0;
        p = FKL_VM_CDR(p);
        fast = get_fast_value(fast);
    }
    if (p != FKL_VM_NIL)
        return 0;
    return 1;
}

int fklIsList2(const FklVMvalue *p, size_t *plen) {
    size_t len = 0;
    FklVMvalue *fast = get_initial_fast_value(p);
    while (FKL_IS_PAIR(p)) {
        if (fast == p)
            return 0;
        p = FKL_VM_CDR(p);
        fast = get_fast_value(fast);
        ++len;
    }
    if (p != FKL_VM_NIL)
        return 0;
    if (plen)
        *plen = len;
    return 1;
}

int64_t fklVMgetInt(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? FKL_GET_FIX(p) : fklVMbigIntToI(FKL_VM_BI(p));
}

uint64_t fklVMintToHashv(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? (uint64_t)FKL_GET_FIX(p)
                         : fklVMbigIntHash(FKL_VM_BI(p));
}

uint64_t fklVMgetUint(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? (uint64_t)FKL_GET_FIX(p)
                         : fklVMbigIntToU(FKL_VM_BI(p));
}

int fklIsVMnumberLt0(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? FKL_GET_FIX(p) < 0
         : FKL_IS_F64(p) ? isless(FKL_VM_F64(p), 0.0)
                         : fklIsVMbigIntLt0(FKL_VM_BI(p));
}

double fklVMgetDouble(const FklVMvalue *p) {
    return FKL_IS_FIX(p)      ? FKL_GET_FIX(p)
         : (FKL_IS_BIGINT(p)) ? fklVMbigIntToD(FKL_VM_BI(p))
                              : FKL_VM_F64(p);
}

static inline FklVMvalue *get_compound_frame_code_obj(FklVMframe *frame) {
    return FKL_VM_PROC(frame->proc)->bcl;
}

static inline void
print_back_trace(FklVMframe *f, FklCodeBuilder *build, FklVMgc *gc) {
    void (*backtrace)(void *data, FklCodeBuilder *, FklVMgc *) =
            f->t->print_backtrace;
    if (backtrace)
        backtrace(f->data, build, gc);
    else
        fklCodeBuilderPuts(build, "at callable-obj\n");
}

void fklPrintFrame(FklVMframe *f, FklVM *exe, FklCodeBuilder *build) {
    if (f->type == FKL_FRAME_COMPOUND) {
        FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
        if (proc->name != FKL_VM_NIL) {
            fklCodeBuilderPuts(build, "at proc: ");
            fklPrintSymbolLiteral2(FKL_VM_SYM(proc->name), build);
        } else if (f->prev) {
            FklVMvalueProto *pt = FKL_VM_PROC(f->proc)->proto;
            FklVMvalue *sid = FKL_VM_PROC(f->proc)->name;
            if (sid == FKL_VM_NIL) {
                sid = pt->name;
            }

            if (pt->name != FKL_VM_NIL) {
                fklCodeBuilderPuts(build, "at proc: ");
                fklPrintSymbolLiteral2(FKL_VM_SYM(pt->name), build);
            } else {
                fklCodeBuilderPuts(build, "at proc: <");
                if (pt->file != FKL_VM_NIL) {
                    fklPrintStringLiteral2(FKL_VM_SYM(pt->file), build);
                } else {
                    fklCodeBuilderPuts(build, "stdin");
                }
                fklCodeBuilderFmt(build, ": %" PRIu64 ">", pt->line);
            }
        } else {
            fklCodeBuilderPuts(build, "at <top>");
        }
        FklByteCodelnt *co = FKL_VM_CO(get_compound_frame_code_obj(f));
        uint64_t const pc_idx = f->pc - co->bc.code - 1;
        const FklLntItem *node = fklFindLntItem(pc_idx, co->ls, co->l);
        if (node->fid != FKL_VM_NIL) {
            fklCodeBuilderFmt(build, " (%" PRIu32 ": ", node->line);
            fklPrintString2(FKL_VM_SYM(node->fid), build);
            fklCodeBuilderPuts(build, ")\n");
        } else {
            fklCodeBuilderFmt(build, " (%" PRIu32 ")\n", node->line);
        }
    } else
        print_back_trace(f, build, exe->gc);
}

void fklPrintBacktrace(FklVM *exe, FklCodeBuilder *build) {
    for (FklVMframe *cur = exe->top_frame; cur; cur = cur->prev)
        fklPrintFrame(cur, exe, build);
}

void fklPrintErrBacktrace(FklVMvalue *ev, FklVM *exe, FklCodeBuilder *build_) {
    FklCodeBuilder *build = NULL;
    if (build_ == NULL) {
        uv_mutex_lock(&exe->gc->print_backtrace_lock);
        build = &exe->gc->err_out;
    } else {
        build = build_;
    }

    if (fklIsVMvalueError(ev)) {
        FklVMvalueError *err = FKL_VM_ERR(ev);
        fklPrintSymbolLiteral2(FKL_VM_SYM(err->type), build);
        fklCodeBuilderPuts(build, ": ");
        fklPrincVMvalue2(err->message, build, exe);
        fklCodeBuilderPutc(build, '\n');
        fklPrintBacktrace(exe, build);
    } else {
        fklCodeBuilderPuts(build, "interrupt with value: ");
        fklPrin1VMvalue2(ev, build, exe);
        fklCodeBuilderPutc(build, '\n');
        fklPrintBacktrace(exe, build);
    }

    if (build_ == NULL) {
        uv_mutex_unlock(&exe->gc->print_backtrace_lock);
    }
}

void fklRaiseVMerror(FklVMvalue *ev, FklVM *exe) {
    FKL_VM_PUSH_VALUE(exe, ev);
    longjmp(*exe->buf, FKL_VM_ERR_RAISE);
}

static inline void init_frame_var_ref(FklVMframe *f) {
    f->lref = FKL_VM_NIL;
    f->lrefl = FKL_VM_NIL;
    f->ref = NULL;

    f->lcount = 0;
    f->rcount = 0;
}

FklVMframe *fklCreateVMframeWithProc(FklVM *exe, FklVMvalue *proc) {
    FklVMvalueProc *code = FKL_VM_PROC(proc);
    FklVMframe *f;
    if (exe->frame_cache_head) {
        f = exe->frame_cache_head;
        exe->frame_cache_head = f->prev;
        if (f->prev == NULL)
            exe->frame_cache_tail = &exe->frame_cache_head;
        memset(f, 0, sizeof(FklVMframe));
    } else {
        f = (FklVMframe *)fklZcalloc(1, sizeof(FklVMframe));
        FKL_ASSERT(f);
    }
    f->type = FKL_FRAME_COMPOUND;

    f->konsts = NULL;
    f->pc = NULL;
    f->spc = NULL;
    f->end = NULL;
    f->proc = NULL;
    f->mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
    init_frame_var_ref(f);
    if (code) {
        f->ref = code->closure;
        f->rcount = code->ref_count;
        f->lcount = code->local_count;
        f->pc = code->spc;
        f->spc = code->spc;
        f->end = code->end;
        f->konsts = fklVMvalueProtoConsts(code->proto);
        f->libs = fklVMvalueProtoUsedLibs(code->proto);
        f->proc = proc;
    }
    return f;
}

FklVMframe *fklCreateOtherObjVMframe(FklVM *exe,
        const FklVMframeContextMethodTable *t) {
    FklVMframe *r;
    if (exe->frame_cache_head) {
        r = exe->frame_cache_head;
        exe->frame_cache_head = r->prev;
        if (r->prev == NULL)
            exe->frame_cache_tail = &exe->frame_cache_head;
        memset(r, 0, sizeof(FklVMframe));
    } else {
        r = (FklVMframe *)fklZcalloc(1, sizeof(FklVMframe));
        FKL_ASSERT(r);
    }
    r->bp = exe->bp;
    r->type = FKL_FRAME_OTHEROBJ;
    r->t = t;
    return r;
}

FklVMframe *fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable *t) {
    FklVMframe *r = (FklVMframe *)fklZcalloc(1, sizeof(FklVMframe));
    FKL_ASSERT(r);
    r->type = FKL_FRAME_OTHEROBJ;
    r->t = t;
    return r;
}

FklVMvalue *fklGenErrorMessage(FklBuiltinErrorType type, FklVM *exe) {
    static const char *builtinErrorMessages[FKL_BUILTIN_ERR_NUM] = {
        NULL,
        "Symbol undefined",
        "Syntax error",
        "Invalid expression",
        "Circular load file",
        "Invalid pattern ",
        "Incorrect type of values",
        "Stack error",
        "Too many arguements",
        "Too few arguements",
        "Can't create thread",
        "Thread error",
        "macro expand failed",
        "Try to call an object that can't be call",
        "Faild to load dll",
        "Invalid symbol",
        "Library undefined",
        "Unexpected eof",
        "Divided by zero",
        "File failed",
        "Invalid value",
        "Invalid assign",
        "Invalid access",
        "Failed to import dll",
        "Invalid macro pattern",
        "Failed to create big-int from mem",
        "List differ in length",
        "Attempt to get a continuation cross C-call boundary",
        "Radix for integer should be 8, 10 or 16",
        "No value for key",
        "Number should not be less than 0",
        "It's unserializable to bytecode file",
        "Unsupported operation",
        "Import missing",
        "Exporting production groups with reference to other group",
        "Failed to import reader macro",
        "Analysis table generate failed",
        "Regex compile failed",
        "Grammer create failed",
        "Radix for float should be 10 or 16 for float",
        "attempt to assign constant",
        "attempt to redefine variable as constant",
    };
    const char *s = builtinErrorMessages[type];
    FKL_ASSERT(s);
    return fklCreateVMvalueStrFromCstr(exe, s);
}

typedef enum PrintingState {
    PRT_CAR,
    PRT_CDR,
} PrintingState;

typedef enum {
    PRINT_METHOD_FIRST = 0,
    PRINT_METHOD_CONT,
} PrintState;

typedef struct {
    uint32_t i;
    int printed;
} PrtSt;

// VmValueDegreeHashMap
#define FKL_HASH_TYPE_PREFIX Vm
#define FKL_HASH_METHOD_PREFIX vm
#define FKL_HASH_KEY_TYPE FklVMvalue const *
#define FKL_HASH_VAL_TYPE PrtSt
#define FKL_HASH_ELM_NAME CircleHead
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, (*pk)));
#include <fakeLisp/cont/hash.h>
static inline void putValueInSet(VmCircleHeadHashMap *s, const FklVMvalue *v) {
    uint32_t num = s->count;
    vmCircleHeadHashMapPut2(s,
            v,
            (PrtSt){
                .i = num,
                .printed = 0,
            });
}

// VmValueDegreeHashMap
#define FKL_HASH_TYPE_PREFIX Vm
#define FKL_HASH_METHOD_PREFIX vm
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE uint64_t
#define FKL_HASH_ELM_NAME ValueDegree
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, (*pk)));
#include <fakeLisp/cont/hash.h>

static inline void inc_value_degree(VmValueDegreeHashMap *ht, FklVMvalue *v) {
    VmValueDegreeHashMapElm *degree_item =
            vmValueDegreeHashMapInsert2(ht, v, 0);
    ++degree_item->v;
}

static inline void dec_value_degree(VmValueDegreeHashMap *ht, FklVMvalue *v) {
    uint64_t *degree = vmValueDegreeHashMapGet2(ht, v);
    if (degree && *degree)
        --(*degree);
}

static inline void scan_value_and_find_value_in_circle(VmValueDegreeHashMap *ht,
        VmCircleHeadHashMap *circle_heads,
        const FklVMvalue *first_value) {
    FklValueVector stack;
    fklValueVectorInit(&stack, 16);
    fklValueVectorPushBack2(&stack, FKL_TYPE_CAST(FklVMvalue *, first_value));
    while (!fklValueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklValueVectorPopBackNonNull(&stack);
        if (FKL_IS_PAIR(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                fklValueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklValueVectorPushBack2(&stack, FKL_VM_CAR(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_VECTOR(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                FklVMvalueVec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklValueVectorPushBack2(&stack, vec->base[i - 1]);
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_BOX(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                fklValueVectorPushBack2(&stack, FKL_VM_BOX(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                for (FklValueHashMapNode *tail = FKL_VM_HASH(v)->ht.last; tail;
                        tail = tail->prev) {
                    fklValueVectorPushBack2(&stack, tail->v);
                    fklValueVectorPushBack2(&stack, tail->k);
                }
                putValueInSet(circle_heads, v);
            }
        }
    }
    dec_value_degree(ht, FKL_TYPE_CAST(FklVMvalue *, first_value));

    // remove value not in circle

    do {
        stack.size = 0;
        for (VmValueDegreeHashMapNode *list = ht->first; list;
                list = list->next) {
            if (!list->v)
                fklValueVectorPushBack2(&stack, list->k);
        }
        FklVMvalue **base = (FklVMvalue **)stack.base;
        FklVMvalue **const end = &base[stack.size];
        for (; base < end; base++) {
            vmValueDegreeHashMapDel2(ht, *base);
            FklVMvalue *v = *base;
            if (FKL_IS_PAIR(v)) {
                dec_value_degree(ht, FKL_VM_CAR(v));
                dec_value_degree(ht, FKL_VM_CDR(v));
            } else if (FKL_IS_VECTOR(v)) {
                FklVMvalueVec *vec = FKL_VM_VEC(v);
                FklVMvalue **base = vec->base;
                FklVMvalue **const end = &base[vec->size];
                for (; base < end; base++)
                    dec_value_degree(ht, *base);
            } else if (FKL_IS_BOX(v))
                dec_value_degree(ht, FKL_VM_BOX(v));
            else if (FKL_IS_HASHTABLE(v)) {
                for (FklValueHashMapNode *list = FKL_VM_HASH(v)->ht.first; list;
                        list = list->next) {
                    dec_value_degree(ht, list->k);
                    dec_value_degree(ht, list->v);
                }
            }
        }
    } while (!fklValueVectorIsEmpty(&stack));

    // get all circle heads

    vmCircleHeadHashMapClear(circle_heads);
    while (ht->first) {
        VmValueDegreeHashMapNode *value_degree = ht->first;
        FklVMvalue const *v = value_degree->k;
        putValueInSet(circle_heads, v);
        value_degree->v = 0;

        do {
            stack.size = 0;
            for (VmValueDegreeHashMapNode *list = ht->first; list;
                    list = list->next) {
                if (!list->v)
                    fklValueVectorPushBack2(&stack, list->k);
            }
            FklVMvalue **base = (FklVMvalue **)stack.base;
            FklVMvalue **const end = &base[stack.size];
            for (; base < end; base++) {
                vmValueDegreeHashMapDel2(ht, *base);
                FklVMvalue *v = *base;
                if (FKL_IS_PAIR(v)) {
                    dec_value_degree(ht, FKL_VM_CAR(v));
                    dec_value_degree(ht, FKL_VM_CDR(v));
                } else if (FKL_IS_VECTOR(v)) {
                    FklVMvalueVec *vec = FKL_VM_VEC(v);
                    FklVMvalue **base = vec->base;
                    FklVMvalue **const end = &base[vec->size];
                    for (; base < end; base++)
                        dec_value_degree(ht, *base);
                } else if (FKL_IS_BOX(v))
                    dec_value_degree(ht, FKL_VM_BOX(v));
                else if (FKL_IS_HASHTABLE(v)) {
                    for (FklValueHashMapNode *list = FKL_VM_HASH(v)->ht.first;
                            list;
                            list = list->next) {
                        dec_value_degree(ht, list->k);
                        dec_value_degree(ht, list->v);
                    }
                }
            }
        } while (!fklValueVectorIsEmpty(&stack));
    }
    fklValueVectorUninit(&stack);
}

static inline void traverse_value_dfs(const FklVMvalue *first_value,
        int (*callback)(const FklVMvalue *v, void *ctx),
        void *ctx) {
    FklValueHashSet value_set;
    fklValueHashSetInit(&value_set);

    FklValueVector stack;
    fklValueVectorInit(&stack, 16);

    fklValueVectorPushBack2(&stack, FKL_TYPE_CAST(FklVMvalue *, first_value));
    while (!fklValueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklValueVectorPopBackNonNull(&stack);
        if (callback(v, ctx))
            break;
        if (FKL_IS_PAIR(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                fklValueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklValueVectorPushBack2(&stack, FKL_VM_CAR(v));
            }
        } else if (FKL_IS_VECTOR(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                FklVMvalueVec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklValueVectorPushBack2(&stack, vec->base[i - 1]);
            }
        } else if (FKL_IS_BOX(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                fklValueVectorPushBack2(&stack, FKL_VM_BOX(v));
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                for (FklValueHashMapNode *tail = FKL_VM_HASH(v)->ht.last; tail;
                        tail = tail->prev) {
                    fklValueVectorPushBack2(&stack, tail->v);
                    fklValueVectorPushBack2(&stack, tail->k);
                }
            }
        }
    }

    fklValueHashSetUninit(&value_set);
    fklValueVectorUninit(&stack);
}

static inline void traverse_value_bfs(const FklVMvalue *first_value,
        int (*callback)(const FklVMvalue *v, void *ctx),
        void *ctx) {
    FklValueHashSet value_set;
    fklValueHashSetInit(&value_set);

    FklValueQueue queue;
    fklValueQueueInit(&queue);

    fklValueQueuePush2(&queue, FKL_TYPE_CAST(FklVMvalue *, first_value));
    while (!fklValueQueueIsEmpty(&queue)) {
        FklVMvalue *v = *fklValueQueuePopNonNull(&queue);
        if (callback(v, ctx))
            break;
        if (FKL_IS_PAIR(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                fklValueQueuePush2(&queue, FKL_VM_CDR(v));
                fklValueQueuePush2(&queue, FKL_VM_CAR(v));
            }
        } else if (FKL_IS_VECTOR(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                FklVMvalueVec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklValueQueuePush2(&queue, vec->base[i - 1]);
            }
        } else if (FKL_IS_BOX(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                fklValueQueuePush2(&queue, FKL_VM_BOX(v));
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            if (!fklValueHashSetPut2(&value_set, v)) {
                for (FklValueHashMapNode *tail = FKL_VM_HASH(v)->ht.last; tail;
                        tail = tail->prev) {
                    fklValueQueuePush2(&queue, tail->v);
                    fklValueQueuePush2(&queue, tail->k);
                }
            }
        }
    }

    fklValueHashSetUninit(&value_set);
    fklValueQueueUninit(&queue);
}

static int has_circle_ref_cb(const FklVMvalue *v, void *ctx) {
    VmValueDegreeHashMap *degree_table = ctx;
    if (FKL_IS_PAIR(v) ||       //
            FKL_IS_VECTOR(v) || //
            FKL_IS_BOX(v) ||    //
            FKL_IS_HASHTABLE(v))
        inc_value_degree(degree_table, FKL_TYPE_CAST(FklVMvalue *, v));
    return 0;
}

static inline void reduce_degrees(VmValueDegreeHashMap *degree_table) {
    FklValueVector stack;
    fklValueVectorInit(&stack, 16);

    do {
        stack.size = 0;
        for (VmValueDegreeHashMapNode *list = degree_table->first; list;
                list = list->next) {
            if (!list->v)
                fklValueVectorPushBack2(&stack, list->k);
        }
        FklVMvalue **base = (FklVMvalue **)stack.base;
        FklVMvalue **const end = &base[stack.size];
        for (; base < end; base++) {
            vmValueDegreeHashMapDel2(degree_table, *base);
            FklVMvalue *v = *base;
            if (FKL_IS_PAIR(v)) {
                dec_value_degree(degree_table, FKL_VM_CAR(v));
                dec_value_degree(degree_table, FKL_VM_CDR(v));
            } else if (FKL_IS_VECTOR(v)) {
                FklVMvalueVec *vec = FKL_VM_VEC(v);
                FklVMvalue **base = vec->base;
                FklVMvalue **const end = &base[vec->size];
                for (; base < end; base++)
                    dec_value_degree(degree_table, *base);
            } else if (FKL_IS_BOX(v))
                dec_value_degree(degree_table, FKL_VM_BOX(v));
            else if (FKL_IS_HASHTABLE(v)) {
                for (FklValueHashMapNode *list = FKL_VM_HASH(v)->ht.first; list;
                        list = list->next) {
                    dec_value_degree(degree_table, list->k);
                    dec_value_degree(degree_table, list->v);
                }
            }
        }
    } while (!fklValueVectorIsEmpty(&stack));

    fklValueVectorUninit(&stack);
}

int fklHasCircleRef(const FklVMvalue *v) {
    if (FKL_GET_TAG(v) != FKL_TAG_PTR
            || (!FKL_IS_PAIR(v) &&       //
                    !FKL_IS_BOX(v) &&    //
                    !FKL_IS_VECTOR(v) && //
                    !FKL_IS_HASHTABLE(v)))
        return 0;

    VmValueDegreeHashMap degree_table;
    vmValueDegreeHashMapInit(&degree_table);

    traverse_value_dfs(v, has_circle_ref_cb, &degree_table);

    dec_value_degree(&degree_table, FKL_TYPE_CAST(FklVMvalue *, v));

    reduce_degrees(&degree_table);

    int r = degree_table.count > 0;
    vmValueDegreeHashMapUninit(&degree_table);

    return r;
}

static inline int is_serializable_leaf_node(const FklVMvalue *v) {
    return v == FKL_VM_NIL ||      //
           FKL_IS_FIX(v) ||        //
           FKL_IS_CHR(v) ||        //
           FKL_IS_F64(v) ||        //
           FKL_IS_BIGINT(v) ||     //
           FKL_IS_STR(v) ||        //
           FKL_IS_SYM(v) ||        //
           FKL_IS_BYTEVECTOR(v) || //
           fklIsVMvalueSlot(v) ||  //
           v == FKL_VM_HEADER_WILDCARD;
}

static int is_serializable_to_bytecode_value(const FklVMvalue *v) {
    return is_serializable_leaf_node(v) || //
           FKL_IS_VECTOR(v) ||             //
           FKL_IS_PAIR(v) ||               //
           FKL_IS_BOX(v) ||                //
           FKL_IS_HASHTABLE(v);
}

struct SerializableCtx {
    VmValueDegreeHashMap *degree_table;
    FklVMvalueLnt *lnt;
    uint64_t line;
    int r;
};

static int serializable_to_bytecode_file_cb(const FklVMvalue *v, void *ctx) {
    struct SerializableCtx *c = ctx;
    if (!is_serializable_to_bytecode_value(v)) {
        c->r = 1;
        return 1;
    }
    if (FKL_IS_PAIR(v) ||       //
            FKL_IS_VECTOR(v) || //
            FKL_IS_BOX(v) ||    //
            FKL_IS_HASHTABLE(v)) {
        inc_value_degree(c->degree_table, FKL_TYPE_CAST(FklVMvalue *, v));
        if (c->lnt) {
            fklVMvalueLntPut(c->lnt, v, c->line);
        }
    }
    return 0;
}

int fklIsSerializableToByteCodeFile(const FklVMvalue *v,
        FklVMvalueLnt *lnt,
        uint64_t line) {
    if (!is_serializable_to_bytecode_value(v))
        return 0;
    if (FKL_GET_TAG(v) != FKL_TAG_PTR
            || (!FKL_IS_PAIR(v) &&       //
                    !FKL_IS_BOX(v) &&    //
                    !FKL_IS_VECTOR(v) && //
                    !FKL_IS_HASHTABLE(v)))
        return 1;

    VmValueDegreeHashMap degree_table;
    vmValueDegreeHashMapInit(&degree_table);

    struct SerializableCtx ctx = {
        .degree_table = &degree_table,
        .r = 0,
        .lnt = lnt,
        .line = line,
    };

    int r = 1;
    traverse_value_dfs(v, serializable_to_bytecode_file_cb, &ctx);
    if (ctx.r) {
        r = 0;
        goto exit;
    }

    dec_value_degree(&degree_table, FKL_TYPE_CAST(FklVMvalue *, v));

    reduce_degrees(&degree_table);

    r = degree_table.count == 0;

exit:
    vmValueDegreeHashMapUninit(&degree_table);

    return r;
}

#define VMVALUE_PRINTER_ARGS                                                   \
    const FklVMvalue *v, FklCodeBuilder *build, FklVM *exe
static void vmvalue_f64_printer(VMVALUE_PRINTER_ARGS) {
    char buf[64] = { 0 };
    fklWriteDoubleToBuf(buf, 64, FKL_VM_F64(v));
    fklCodeBuilderPuts(build, buf);
}

static void vmvalue_bigint_printer(VMVALUE_PRINTER_ARGS) {
    fklPrintVMbigInt(FKL_VM_BI(v), build);
}

static void vmvalue_string_princ(VMVALUE_PRINTER_ARGS) {
    fklPrintString2(FKL_VM_STR(v), build);
}

static void vmvalue_bytevector_printer(VMVALUE_PRINTER_ARGS) {
    fklPrintBytesLiteral2(FKL_VM_BVEC(v), build);
}

static void vmvalue_userdata_princ(VMVALUE_PRINTER_ARGS) {
    const FklVMvalueUd *ud = FKL_VM_UD(v);
    const FklVMudMetaTable *const t = ud->mt_;
    FklVMudPrintCb princ = NULL;
    // void (*as_princ)(const FklVMud *, FklCodeBuilder *, FklVM *) = NULL;
    princ = t->princ ? t->princ : t->prin1;
    if (princ) {
        princ(v, build, exe);
    } else
        fklCodeBuilderFmt(build, "#<userdata %p>", ud);
}

static void vmvalue_proc_printer(VMVALUE_PRINTER_ARGS) {
    FklVMvalueProc *proc = FKL_VM_PROC(v);
    if (proc->name) {
        fklVMformat(exe,
                build,
                "#<proc %S>",
                NULL,
                1,
                (FklVMvalue *[]){ proc->name });
    } else
        fklCodeBuilderFmt(build, "#<proc %p>", proc);
}

static void vmvalue_cproc_printer(VMVALUE_PRINTER_ARGS) {
    FklVMvalueCproc *cproc = FKL_VM_CPROC(v);
    if (cproc->name) {
        fklCodeBuilderPuts(build, "#<cproc ");
        fklPrintSymLiteral2(cproc->name, build);
        fklCodeBuilderPutc(build, '>');
    } else
        fklCodeBuilderFmt(build, "#<cproc %p>", cproc);
}

static void vmvalue_symbol_princ(VMVALUE_PRINTER_ARGS) {
    fklPrintString2(FKL_VM_SYM(v), build);
}

static void (*VMvaluePtrPrincTable[FKL_VM_VALUE_GC_TYPE_NUM])(
        VMVALUE_PRINTER_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_printer,
    [FKL_TYPE_BIGINT] = vmvalue_bigint_printer,
    [FKL_TYPE_SYM] = vmvalue_symbol_princ,
    [FKL_TYPE_STR] = vmvalue_string_princ,
    [FKL_TYPE_BYTEVECTOR] = vmvalue_bytevector_printer,
    [FKL_TYPE_USERDATA] = vmvalue_userdata_princ,
    [FKL_TYPE_PROC] = vmvalue_proc_printer,
    [FKL_TYPE_CPROC] = vmvalue_cproc_printer,
};

static void vmvalue_ptr_ptr_princ(VMVALUE_PRINTER_ARGS) {
    VMvaluePtrPrincTable[v->type_](v, build, exe);
}

static void vmvalue_nil_ptr_print(VMVALUE_PRINTER_ARGS) {
    fklCodeBuilderPuts(build, "()");
}

static void vmvalue_fix_ptr_print(VMVALUE_PRINTER_ARGS) {
    fklCodeBuilderFmt(build, "%" PRId64 "", FKL_GET_FIX(v));
}

static void vmvalue_chr_ptr_princ(VMVALUE_PRINTER_ARGS) {
    fklCodeBuilderPutc(build, FKL_GET_CHR(v));
}

static void (*VMvaluePrincTable[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS) = {
    [FKL_TAG_PTR] = vmvalue_ptr_ptr_princ,
    [FKL_TAG_NIL] = vmvalue_nil_ptr_print,
    [FKL_TAG_FIX] = vmvalue_fix_ptr_print,
    [FKL_TAG_CHR] = vmvalue_chr_ptr_princ,
};

static void princVMatom(VMVALUE_PRINTER_ARGS) {
    VMvaluePrincTable[(FklVMptrTag)FKL_GET_TAG(v)](v, build, exe);
}

static void vmvalue_sym_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintSymbolLiteral2(FKL_VM_SYM(v), build);
}

static void vmvalue_chr_ptr_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintCharLiteral2(FKL_GET_CHR(v), build);
}

static void vmvalue_string_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintStringLiteral2(FKL_VM_STR(v), build);
}

static void vmvalue_userdata_prin1(VMVALUE_PRINTER_ARGS) {
    const FklVMvalueUd *ud = FKL_VM_UD(v);
    const FklVMudMetaTable *const t = ud->mt_;
    FklVMudPrintCb prin1 = NULL;
    // void (*as_prin1)(const FklVMvalue *, FklCodeBuilder *, FklVM *) = NULL;
    prin1 = t->prin1 ? t->prin1 : t->princ;
    if (prin1) {
        prin1(v, build, exe);
    } else
        fklCodeBuilderFmt(build, "#<userdata %p>", ud);
}

static void (*VMvaluePtrPrin1Table[FKL_VM_VALUE_GC_TYPE_NUM])(
        VMVALUE_PRINTER_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_printer,
    [FKL_TYPE_BIGINT] = vmvalue_bigint_printer,
    [FKL_TYPE_STR] = vmvalue_string_prin1,
    [FKL_TYPE_BYTEVECTOR] = vmvalue_bytevector_printer,
    [FKL_TYPE_USERDATA] = vmvalue_userdata_prin1,
    [FKL_TYPE_PROC] = vmvalue_proc_printer,
    [FKL_TYPE_CPROC] = vmvalue_cproc_printer,
    [FKL_TYPE_SYM] = vmvalue_sym_prin1,
};

static void vmvalue_ptr_ptr_prin1(VMVALUE_PRINTER_ARGS) {
    VMvaluePtrPrin1Table[v->type_](v, build, exe);
}

static void (*VMvaluePrin1Table[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS) = {
    [FKL_TAG_PTR] = vmvalue_ptr_ptr_prin1,
    [FKL_TAG_NIL] = vmvalue_nil_ptr_print,
    [FKL_TAG_FIX] = vmvalue_fix_ptr_print,
    [FKL_TAG_CHR] = vmvalue_chr_ptr_prin1,
};

static void prin1VMatom(VMVALUE_PRINTER_ARGS) {
    VMvaluePrin1Table[(FklVMptrTag)FKL_GET_TAG(v)](v, build, exe);
}

#define PRINT_CTX_COMMON_HEADER                                                \
    FklValueType type;                                                         \
    PrintState state;                                                          \
    PrintingState place

typedef struct PrintCtx {
    PRINT_CTX_COMMON_HEADER;
    intptr_t data[2];
} PrintCtx;

typedef struct PrintPairCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklVMvalue *cur;
    const VmCircleHeadHashMap *circle_head_set;
} PrintPairCtx;

static_assert(sizeof(PrintPairCtx) <= sizeof(PrintCtx),
        "print pair ctx too large");

typedef struct PrintVectorCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklVMvalue *vec;
    uintptr_t index;
} PrintVectorCtx;

static_assert(sizeof(PrintVectorCtx) <= sizeof(PrintCtx),
        "print vector ctx too large");

typedef struct PrintHashCtx {
    PRINT_CTX_COMMON_HEADER;
    const FklVMvalue *hash;
    const FklValueHashMapNode *cur;
} PrintHashCtx;

static_assert(sizeof(PrintHashCtx) <= sizeof(PrintCtx),
        "print hash ctx too large");

// VmPrintCtxVector
#define FKL_VECTOR_TYPE_PREFIX Vm
#define FKL_VECTOR_METHOD_PREFIX vm
#define FKL_VECTOR_ELM_TYPE PrintCtx
#define FKL_VECTOR_ELM_TYPE_NAME PrintCtx
#include <fakeLisp/cont/vector.h>

static inline void print_pair_ctx_init(PrintCtx *ctx,
        const FklVMvalue *pair,
        const VmCircleHeadHashMap *circle_head_set) {
    PrintPairCtx *pair_ctx = FKL_TYPE_CAST(PrintPairCtx *, ctx);
    pair_ctx->cur = pair;
    pair_ctx->circle_head_set = circle_head_set;
}

static inline void print_vector_ctx_init(PrintCtx *ctx, const FklVMvalue *vec) {
    PrintVectorCtx *vec_ctx = FKL_TYPE_CAST(PrintVectorCtx *, ctx);
    vec_ctx->vec = vec;
    vec_ctx->index = 0;
}

static inline void print_hash_ctx_init(PrintCtx *ctx, const FklVMvalue *hash) {
    PrintHashCtx *hash_ctx = FKL_TYPE_CAST(PrintHashCtx *, ctx);
    hash_ctx->hash = hash;
    hash_ctx->cur = FKL_VM_HASH(hash)->ht.first;
}

static inline void init_common_print_ctx(PrintCtx *ctx, FklValueType type) {
    ctx->type = type;
    ctx->state = PRINT_METHOD_FIRST;
    ctx->place = PRT_CAR;
}

static inline void scan_cir_ref(const FklVMvalue *s,
        VmCircleHeadHashMap *circle_head_set) {
    VmValueDegreeHashMap degree_table;
    vmValueDegreeHashMapInit(&degree_table);
    scan_value_and_find_value_in_circle(&degree_table, circle_head_set, s);
    vmValueDegreeHashMapUninit(&degree_table);
}

#define PUTS(OUT, S) fklCodeBuilderPuts((OUT), (S))
#define PUTC(OUT, C) fklCodeBuilderPutc((OUT), (C))
#define PRINTF fklCodeBuilderFmt

static inline int print_circle_head(FklCodeBuilder *result,
        const FklVMvalue *v,
        const VmCircleHeadHashMap *circle_head_set) {
    PrtSt *item = vmCircleHeadHashMapGet2(circle_head_set, v);
    if (item) {
        if (item->printed) {
            fklCodeBuilderFmt(result, "#%u#", item->i);
            return 1;
        } else {
            item->printed = 1;
            fklCodeBuilderFmt(result, "#%u=", item->i);
        }
    }
    return 0;
}

static inline const FklVMvalue *print_method(PrintCtx *ctx,
        FklCodeBuilder *buf) {
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
        FKL_UNREACHABLE();
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
        FKL_UNREACHABLE();
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
        FKL_UNREACHABLE();
        break;
    }
    return r;
#undef pair_ctx
#undef vec_ctx
#undef hash_ctx
#undef vec
#undef hash_table
}

static inline void print_value(const FklVMvalue *v,
        FklCodeBuilder *result,
        void (*p_atom)(VMVALUE_PRINTER_ARGS),
        FklVM *exe) {
    if (!FKL_IS_VECTOR(v) && !FKL_IS_PAIR(v) && !FKL_IS_BOX(v)
            && !FKL_IS_HASHTABLE(v)) {
        p_atom(v, result, exe);
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
                    fklCodeBuilderFmt(result, "#%u#", item->i);
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
            if (print_circle_head(result, v, &circle_head_set))
                goto get_next;
            PrintCtx *ctx = vmPrintCtxVectorPushBack(&print_ctxs, NULL);
            init_common_print_ctx(ctx, FKL_TYPE_VECTOR);
            print_vector_ctx_init(ctx, v);
        } else if (FKL_IS_BOX(v)) {
            if (print_circle_head(result, v, &circle_head_set))
                goto get_next;
            PUTS(result, "#&");
            v = FKL_VM_BOX(v);
            continue;
        } else if (FKL_IS_HASHTABLE(v)) {
            if (print_circle_head(result, v, &circle_head_set))
                goto get_next;
            PrintCtx *ctx = vmPrintCtxVectorPushBack(&print_ctxs, NULL);
            init_common_print_ctx(ctx, FKL_TYPE_HASHTABLE);
            print_hash_ctx_init(ctx, v);
        } else {
            p_atom(v, result, exe);
        }
    get_next:
        if (vmPrintCtxVectorIsEmpty(&print_ctxs))
            break;
        while (!vmPrintCtxVectorIsEmpty(&print_ctxs)) {
            PrintCtx *ctx = vmPrintCtxVectorBack(&print_ctxs);
            v = print_method(ctx, result);
            if (v)
                break;
            else
                vmPrintCtxVectorPopBack(&print_ctxs);
        }
    }
    vmPrintCtxVectorUninit(&print_ctxs);
    vmCircleHeadHashMapUninit(&circle_head_set);
}

void fklPrin1VMvalue(FklVMvalue *v, FILE *fp, FklVM *exe) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrin1VMvalue2(v, &builder, exe);
}

void fklPrin1VMvalue2(FklVMvalue *v, FklCodeBuilder *build, FklVM *exe) {
    print_value(v, build, prin1VMatom, exe);
}

#undef PUTS
#undef PUTC
#undef PRINTF

void fklPrincVMvalue(FklVMvalue *v, FILE *fp, FklVM *exe) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrincVMvalue2(v, &builder, exe);
}

void fklPrincVMvalue2(FklVMvalue *v, FklCodeBuilder *build, FklVM *exe) {
    print_value(v, build, princVMatom, exe);
}

#undef VMVALUE_PRINTER_ARGS

FklVMvalue *fklVMstringify(FklVMvalue *value, FklVM *exe, char mode) {
    FKL_ASSERT(mode == '1' || mode == 'c');
    FklStrBuf result;
    fklInitStrBuf(&result);
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &result, NULL);
    print_value(value, &builder, mode == '1' ? prin1VMatom : princVMatom, exe);

    FklVMvalue *retval = fklCreateVMvalueStr2(exe,
            fklStrBufLen(&result),
            fklStrBufBody(&result));
    fklUninitStrBuf(&result);
    return retval;
}

size_t fklVMlistLength(const FklVMvalue *v) {
    size_t len = 0;
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v))
        len++;
    return len;
}

#define IS_DEFAULT_BIGINT(bi) (bi->digits == NULL)
#define INIT_DEFAULT_BIGINT_1(bi)                                              \
    if (IS_DEFAULT_BIGINT(bi))                                                 \
    fklInitBigInt1(bi)

int fklProcessVMnumAdd(FklVMvalue *cur,
        int64_t *pr64,
        double *pf64,
        FklBigInt *bi) {
    if (FKL_IS_FIX(cur)) {
        int64_t c64 = FKL_GET_FIX(cur);
        if (fklIsFixAddOverflow(*pr64, c64))
            fklAddBigIntI(bi, c64);
        else
            *pr64 += c64;
    } else if (FKL_IS_BIGINT(cur))
        fklAddVMbigInt(bi, FKL_VM_BI(cur));
    else if (FKL_IS_F64(cur))
        *pf64 += FKL_VM_F64(cur);
    else {
        fklUninitBigInt(bi);
        return 1;
    }
    return 0;
}

int fklProcessVMnumMul(FklVMvalue *cur,
        int64_t *pr64,
        double *pf64,
        FklBigInt *bi) {
    if (FKL_IS_FIX(cur)) {
        int64_t c64 = FKL_GET_FIX(cur);
        if (fklIsFixMulOverflow(*pr64, c64)) {
            INIT_DEFAULT_BIGINT_1(bi);
            fklMulBigIntI(bi, c64);
        } else
            *pr64 *= c64;
    } else if (FKL_IS_BIGINT(cur)) {
        INIT_DEFAULT_BIGINT_1(bi);
        fklMulVMbigInt(bi, FKL_VM_BI(cur));
    } else if (FKL_IS_F64(cur))
        *pf64 *= FKL_VM_F64(cur);
    else {
        fklUninitBigInt(bi);
        return 1;
    }
    return 0;
}

int fklProcessVMintMul(FklVMvalue *cur, int64_t *pr64, FklBigInt *bi) {
    if (FKL_IS_FIX(cur)) {
        int64_t c64 = FKL_GET_FIX(cur);
        if (fklIsFixMulOverflow(*pr64, c64)) {
            INIT_DEFAULT_BIGINT_1(bi);
            fklMulBigIntI(bi, c64);
        } else
            *pr64 *= c64;
    } else if (FKL_IS_BIGINT(cur)) {
        INIT_DEFAULT_BIGINT_1(bi);
        fklMulVMbigInt(bi, FKL_VM_BI(cur));
    } else {
        fklUninitBigInt(bi);
        return 1;
    }
    return 0;
}

FklVMvalue *fklProcessVMnumAddk(FklVM *exe, FklVMvalue *arg, int8_t k) {
    if (FKL_IS_FIX(arg)) {
        int64_t i = FKL_GET_FIX(arg) + k;
        if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
            return fklCreateVMvalueBigIntWithI64(exe, i);
        else
            return FKL_MAKE_VM_FIX(i);
    } else if (FKL_IS_BIGINT(arg)) {
        const FklVMvalueBigInt *bigint = FKL_VM_BI(arg);
        if (fklIsVMbigIntAddkInFixIntRange(bigint, k))
            return FKL_MAKE_VM_FIX(fklVMbigIntToI(bigint) + k);
        else
            return fklVMbigIntAddI(exe, bigint, k);
    } else if (FKL_IS_F64(arg))
        return fklCreateVMvalueF64(exe,
                FKL_VM_F64(arg) + FKL_TYPE_CAST(double, k));
    return NULL;
}

FklVMvalue *
fklProcessVMnumAddResult(FklVM *exe, int64_t r64, double rd, FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (rd != 0.0)
        r = fklCreateVMvalueF64(exe, rd + r64 + fklBigIntToD(bi));
    else if (FKL_BIGINT_IS_0(bi))
        r = FKL_MAKE_VM_FIX(r64);
    else {
        fklAddBigIntI(bi, r64);
        if (fklIsBigIntGtLtFix(bi))
            r = fklCreateVMvalueBigInt2(exe, bi);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(bi));
    }
    fklUninitBigInt(bi);
    return r;
}

FklVMvalue *fklProcessVMnumSubResult(FklVM *exe,
        FklVMvalue *prev,
        int64_t r64,
        double rd,
        FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev) || rd != 0.0)
        r = fklCreateVMvalueF64(exe,
                fklVMgetDouble(prev) - rd - r64 - fklBigIntToD(bi));
    else if (FKL_IS_BIGINT(prev) || !FKL_BIGINT_IS_0(bi)) {
        if (FKL_IS_FIX(prev))
            fklSubBigIntI(bi, FKL_GET_FIX(prev));
        else
            fklSubVMbigInt(bi, FKL_VM_BI(prev));
        bi->num = -bi->num;
        fklSubBigIntI(bi, r64);
        if (fklIsBigIntGtLtFix(bi))
            r = fklCreateVMvalueBigInt2(exe, bi);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(bi));
    } else {
        int64_t p64 = FKL_GET_FIX(prev);
        if (fklIsFixAddOverflow(p64, -r64)) {
            fklAddBigIntI(bi, p64);
            fklSubBigIntI(bi, r64);
            r = fklCreateVMvalueBigInt2(exe, bi);
        } else
            r = FKL_MAKE_VM_FIX(p64 - r64);
    }
    fklUninitBigInt(bi);
    return r;
}

FklVMvalue *fklProcessVMnumNeg(FklVM *exe, FklVMvalue *prev) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev))
        r = fklCreateVMvalueF64(exe, -FKL_VM_F64(prev));
    else if (FKL_IS_FIX(prev)) {
        int64_t p64 = -FKL_GET_FIX(prev);
        if (p64 > FKL_FIX_INT_MAX)
            r = fklCreateVMvalueBigIntWithI64(exe, p64);
        else
            r = FKL_MAKE_VM_FIX(p64);
    } else {
        const FklVMvalueBigInt *b = FKL_VM_BI(prev);
        const FklBigInt bi = {
            .digits = (FklBigIntDigit *)b->digits,
            .num = -b->num,
            .size = fklAbs(b->num),
            .const_size = 1,
        };

        if (fklIsBigIntGtLtFix(&bi))
            r = fklCreateVMvalueBigInt2(exe, &bi);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(&bi));
    }
    return r;
}

FklVMvalue *
fklProcessVMnumMulResult(FklVM *exe, int64_t r64, double rd, FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (rd != 1.0)
        r = fklCreateVMvalueF64(exe, rd * r64 * fklBigIntToD(bi));
    else if (IS_DEFAULT_BIGINT(bi) || FKL_BIGINT_IS_1(bi))
        r = FKL_MAKE_VM_FIX(r64);
    else {
        INIT_DEFAULT_BIGINT_1(bi);
        fklMulBigIntI(bi, r64);
        if (fklIsBigIntGtLtFix(bi))
            r = fklCreateVMvalueBigInt2(exe, bi);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(bi));
    }
    fklUninitBigInt(bi);
    return r;
}

FklVMvalue *fklProcessVMnumIdivResult(FklVM *exe,
        FklVMvalue *prev,
        int64_t r64,
        FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (FKL_IS_BIGINT(prev)
            || (!IS_DEFAULT_BIGINT(bi) && !FKL_BIGINT_IS_1(bi))) {
        FklBigInt t = FKL_BIGINT_0;
        if (FKL_IS_FIX(prev))
            fklSetBigIntI(&t, FKL_GET_FIX(prev));
        else
            fklSetBigIntWithVMbigInt(&t, FKL_VM_BI(prev));
        if (!IS_DEFAULT_BIGINT(bi))
            fklDivBigInt(&t, bi);
        fklDivBigIntI(&t, r64);
        if (fklIsBigIntGtLtFix(&t))
            r = fklCreateVMvalueBigInt2(exe, &t);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(&t));
    } else
        r = FKL_MAKE_VM_FIX(FKL_GET_FIX(prev) / r64);
    fklUninitBigInt(bi);
    return r;
}

FklVMvalue *fklProcessVMnumDivResult(FklVM *exe,
        FklVMvalue *prev,
        int64_t r64,
        double rd,
        FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev) || rd != 1.0
            || (FKL_IS_FIX(prev)
                    && (IS_DEFAULT_BIGINT(bi) || FKL_BIGINT_IS_1(bi))
                    && FKL_GET_FIX(prev) % (r64))
            || (FKL_IS_BIGINT(prev)
                    && ((!IS_DEFAULT_BIGINT(bi) && !FKL_BIGINT_IS_1(bi)
                                && !fklIsVMbigIntDivisible(FKL_VM_BI(prev), bi))
                            || !fklIsVMbigIntDivisibleI(FKL_VM_BI(prev),
                                    r64)))) {
        if (IS_DEFAULT_BIGINT(bi))
            r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) / rd / r64);
        else
            r = fklCreateVMvalueF64(exe,
                    fklVMgetDouble(prev) / rd / r64 / fklBigIntToD(bi));
    } else if (FKL_IS_BIGINT(prev)
               || (!IS_DEFAULT_BIGINT(bi) && !FKL_BIGINT_IS_1(bi))) {
        FklBigInt t = FKL_BIGINT_0;
        if (FKL_IS_FIX(prev))
            fklSetBigIntI(&t, FKL_GET_FIX(prev));
        else
            fklSetBigIntWithVMbigInt(&t, FKL_VM_BI(prev));

        if (!IS_DEFAULT_BIGINT(bi))
            fklDivBigInt(&t, bi);
        fklDivBigIntI(&t, r64);
        if (fklIsBigIntGtLtFix(&t))
            r = fklCreateVMvalueBigInt2(exe, &t);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(&t));
    } else
        r = FKL_MAKE_VM_FIX(FKL_GET_FIX(prev) / r64);
    fklUninitBigInt(bi);
    return r;
}

FklVMvalue *fklProcessVMnumRec(FklVM *exe, FklVMvalue *prev) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev)) {
        double d = FKL_VM_F64(prev);
        r = fklCreateVMvalueF64(exe, 1.0 / d);
    } else {
        if (FKL_IS_FIX(prev)) {
            int64_t r64 = FKL_GET_FIX(prev);
            if (!r64)
                return NULL;
            if (r64 == 1)
                r = FKL_MAKE_VM_FIX(1);
            else if (r64 == -1)
                r = FKL_MAKE_VM_FIX(-1);
            else
                r = fklCreateVMvalueF64(exe, 1.0 / r64);
        } else {
            const FklBigInt bi = fklVMbigIntToBigInt(FKL_VM_BI(prev));
            if (FKL_BIGINT_IS_1(&bi))
                r = FKL_MAKE_VM_FIX(1);
            else if (FKL_BIGINT_IS_N1(&bi))
                r = FKL_MAKE_VM_FIX(-1);
            else
                r = fklCreateVMvalueF64(exe, 1.0 / fklBigIntToD(&bi));
        }
    }
    return r;
}

FklVMvalue *fklProcessVMnumMod(FklVM *exe, FklVMvalue *fir, FklVMvalue *sec) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(fir) || FKL_IS_F64(sec)) {
        double af = fklVMgetDouble(fir);
        double as = fklVMgetDouble(sec);
        r = fklCreateVMvalueF64(exe, fmod(af, as));
    } else if (FKL_IS_FIX(fir) && FKL_IS_FIX(sec)) {
        int64_t si = FKL_GET_FIX(sec);
        if (si == 0)
            return NULL;
        r = FKL_MAKE_VM_FIX(FKL_GET_FIX(fir) % si);
    } else {
        FklBigInt rem = FKL_BIGINT_0;
        if (FKL_IS_BIGINT(fir))
            fklSetBigIntWithVMbigInt(&rem, FKL_VM_BI(fir));
        else
            fklSetBigIntI(&rem, fklVMgetInt(fir));
        if (FKL_IS_BIGINT(sec)) {
            const FklBigInt bi = fklVMbigIntToBigInt(FKL_VM_BI(sec));
            if (FKL_BIGINT_IS_0(&bi)) {
                fklUninitBigInt(&rem);
                return NULL;
            }
            fklRemBigInt(&rem, &bi);
        } else {
            int64_t si = fklVMgetInt(sec);
            if (si == 0) {
                fklUninitBigInt(&rem);
                return NULL;
            }
            fklRemBigIntI(&rem, si);
        }
        if (fklIsBigIntGtLtFix(&rem))
            r = fklCreateVMvalueBigInt2(exe, &rem);
        else
            r = FKL_MAKE_VM_FIX(fklBigIntToI(&rem));
        fklUninitBigInt(&rem);
    }
    return r;
}

void *fklGetAddress(const char *funcname, uv_lib_t *dll) {
    void *pfunc = NULL;
    if (uv_dlsym(dll, funcname, &pfunc))
        return NULL;
    return pfunc;
}

void fklVMsleep(FklVM *exe, uint64_t ms) {
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_sleep(ms); }
}

void fklVMread(FklVM *exe, FILE *fp, FklStrBuf *buf, uint64_t len, int d) {
    FKL_VM_UNLOCK_BLOCK(exe, flag) {
        if (d != EOF)
            fklGetDelim(fp, buf, d);
        else
            buf->index = fread(buf->buf, sizeof(char), len, fp);
    }
}

void fklInitBuiltinErrorType(FklVMvalue *errorTypeId[FKL_BUILTIN_ERR_NUM],
        FklVMgc *gc) {
    static const char *builtInErrorType[FKL_BUILTIN_ERR_NUM] = {
#define X(A, B) B,
        FKL_BUILTIN_ERR_MAP
#undef X
    };

    for (size_t i = 0; i < FKL_BUILTIN_ERR_NUM; i++)
        errorTypeId[i] = fklVMaddSymbolCstr(&gc->gcvm, builtInErrorType[i]);
}

#define FLAGS_ZEROPAD (1u << 0u)
#define FLAGS_LEFT (1u << 1u)
#define FLAGS_PLUS (1u << 2u)
#define FLAGS_SPACE (1u << 3u)
#define FLAGS_HASH (1u << 4u)
#define FLAGS_UPPERCASE (1u << 5u)
#define FLAGS_CHAR (1u << 6u)
#define FLAGS_PRECISION (1u << 7u)

static void format_out_char(void *arg, char c) {
    FklCodeBuilder *build = arg;
    fklCodeBuilderPutc(build, c);
}

static inline void
out_cstr(void (*outc)(void *, char), void *arg, const char *s) {
    while (*s)
        outc(arg, *s++);
}

static inline uint64_t format_fix_int(int64_t integer_val,
        uint32_t flags,
        uint32_t base,
        uint64_t width,
        uint64_t precision,
        void (*outc)(void *, char c),
        void *buffer) {
    static const char digits[] = "0123456789abcdef0123456789ABCDEF";
#define MAXBUF (sizeof(int64_t) * 8)
    uint64_t length = 0;
    if (integer_val == 0)
        flags &= ~FLAGS_HASH;

    char sign_char = 0;
    if (integer_val < 0) {
        integer_val = -integer_val;
        sign_char = '-';
    }
    char buf[MAXBUF];

    const char *prefix = NULL;
    char *p = &buf[MAXBUF - 1];
    if (integer_val != 0 && (flags & FLAGS_HASH)) {
        if (base == 8)
            prefix = "0";
        else if (base == 16)
            prefix = (flags & FLAGS_UPPERCASE) ? "0X" : "0x";
    }

    uint32_t capitals = (flags & FLAGS_UPPERCASE) ? 16 : 0;
    do {
        *p-- = digits[(integer_val % base) + capitals];
        integer_val /= base;
    } while (integer_val);

    length += (&buf[MAXBUF - 1] - p);
    if (prefix)
        length += strlen(prefix);
    if (sign_char)
        length++;
    else if (flags & FLAGS_PLUS) {
        sign_char = '+';
        length++;
    }

    if (flags & FLAGS_PRECISION && width >= precision && precision >= length)
        width -= (precision - length);

    uint64_t space_len = 0;
    if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD))
        for (; length < width; length++, space_len++)
            outc(buffer, ' ');

    if (sign_char)
        outc(buffer, sign_char);
    if (prefix)
        out_cstr(outc, buffer, prefix);

    if ((flags & FLAGS_PRECISION))
        for (uint64_t len = precision + space_len; length < len; length++)
            outc(buffer, '0');

    if (flags & FLAGS_ZEROPAD)
        for (; length < width; length++)
            outc(buffer, '0');

    while (++p != &buf[MAXBUF])
        outc(buffer, *p);

    if (flags & FLAGS_LEFT)
        for (; length < width; length++)
            outc(buffer, ' ');
#undef MAXBUF
    return length;
}

static inline void
format_prin1_value(FklVMvalue *v, FklStrBuf *buf, FklVM *exe) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, buf, NULL);
    return print_value(v, &builder, prin1VMatom, exe);
}

static inline void
format_princ_value(FklVMvalue *v, FklStrBuf *buf, FklVM *exe) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, buf, NULL);
    return print_value(v, &builder, princVMatom, exe);
}

static inline uint64_t format_bigint(FklStrBuf *buf,
        const FklVMvalueBigInt *bi,
        uint32_t flags,
        uint32_t base,
        uint64_t width,
        uint64_t precision,
        void(outc)(void *, char),
        void *arg) {
    uint64_t length = 0;
    if (FKL_BIGINT_IS_0(bi))
        flags &= ~FLAGS_HASH;

    FklBigIntFmtFlags bigint_fmt_flags = FKL_BIGINT_FMT_FLAG_NONE;
    if (flags & FLAGS_HASH)
        bigint_fmt_flags |= FKL_BIGINT_FMT_FLAG_ALTERNATE;
    if (flags & FLAGS_UPPERCASE)
        bigint_fmt_flags |= FKL_BIGINT_FMT_FLAG_CAPITALS;
    const FklBigInt bigint = fklVMbigIntToBigInt(bi);
    fklBigIntToStrBuf(&bigint, buf, base, bigint_fmt_flags);

    const char *p = buf->buf;
    length += buf->index;

    int print_plus = bi->num >= 0 && (flags & FLAGS_PLUS);

    if (print_plus)
        length++;

    if (flags & FLAGS_PRECISION && width >= precision && precision >= length)
        width -= (precision - length);

    uint64_t space_len = 0;
    if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD))
        for (; length < width; length++, space_len++)
            outc(arg, ' ');

    if (bi->num < 0)
        outc(arg, *p++);
    else if (print_plus)
        outc(arg, '+');

    if (flags & FLAGS_HASH) {
        if (base == 8)
            outc(arg, *p++);
        else if (base == 16) {
            outc(arg, *p++);
            outc(arg, *p++);
        }
    }

    if ((flags & FLAGS_PRECISION))
        for (uint64_t len = precision + space_len; length < len; length++)
            outc(arg, '0');

    if (flags & FLAGS_ZEROPAD)
        for (; length < width; length++)
            outc(arg, '0');

    const char *const end = &buf->buf[buf->index];
    while (p < end)
        outc(arg, *p++);

    if (flags & FLAGS_LEFT)
        for (; length < width; length++)
            outc(arg, ' ');
    buf->index = 0;
    return length;
}

static inline uint64_t format_f64(FklStrBuf *buf,
        char ch,
        double value,
        uint32_t flags,
        uint64_t width,
        uint64_t precision,
        void (*outc)(void *, char),
        void *buffer) {
    if (isnan(value)) {
        out_cstr(outc, buffer, isupper(ch) ? "NAN" : "nan");
        return 3;
    }
    int neg = signbit(value);
    if (isinf(value)) {
        if (isupper(ch)) {
            if (neg)
                out_cstr(outc, buffer, "-inf");
            else if (flags & FLAGS_PLUS)
                out_cstr(outc, buffer, "+inf");
            else
                out_cstr(outc, buffer, "inf");
        } else {
            if (neg)
                out_cstr(outc, buffer, "-INF");
            else if (flags & FLAGS_PLUS)
                out_cstr(outc, buffer, "+INF");
            else
                out_cstr(outc, buffer, "INF");
        }
        return (neg || flags & FLAGS_PLUS) ? 4 : 3;
    }

    uint64_t length = 0;
    if (ch == 'a' || ch == 'A') {
        if (!(flags & FLAGS_PRECISION)) {
            char fmt[] = "%*a";
            fmt[sizeof(fmt) - 2] = ch;
            fklStrBufPrintf(buf, fmt, width, value);
        } else {
            char fmt[] = "%.*a";
            fmt[sizeof(fmt) - 2] = ch;
            fklStrBufPrintf(buf, fmt, precision, value);
        }
    } else {
        char fmt[] = "%.*f";
        fmt[sizeof(fmt) - 2] = ch;
        if (!(flags & FLAGS_PRECISION))
            precision = 6;

        fklStrBufPrintf(buf, fmt, precision, value);
    }
    const char *p = buf->buf;
    length += buf->index;

    int print_plus = !neg && (flags & FLAGS_PLUS);
    if (print_plus)
        length++;

    if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD))
        for (; length < width; length++)
            outc(buffer, ' ');

    if (neg)
        outc(buffer, *p++);
    else if (print_plus)
        outc(buffer, '+');

    if (ch == 'a' || ch == 'A') {
        outc(buffer, *p++);
        outc(buffer, *p++);
    }

    if (flags & FLAGS_ZEROPAD && precision < width)
        for (; length < width; length++)
            outc(buffer, '0');

    const char *const end = &buf->buf[buf->index];
    const char *dot_idx = strchr(buf->buf, '.');
    uint64_t prec_len = (end - dot_idx) + (dot_idx - p - 1);

    while (p < end)
        outc(buffer, *p++);

    if ((flags & FLAGS_HASH))
        for (; prec_len < precision; prec_len++, length++)
            outc(buffer, '0');

    if (flags & FLAGS_LEFT)
        for (; length < width; length++)
            outc(buffer, ' ');

    buf->index = 0;
    return length;
}

static inline FklBuiltinErrorType vm_format_to_buf(FklVM *exe,
        const char *fmt,
        const char *end,
        void (*outc)(void *, char),
        void (*outs)(void *, const char *, size_t len),
        void *arg,
        uint64_t *plen,
        FklVMvalue **cur_val,
        FklVMvalue **const val_end) {
    uint32_t base;
    uint32_t flags;
    uint64_t width;
    uint64_t precision;
    FklBuiltinErrorType err = 0;

    FklStrBuf buf;
    fklInitStrBuf(&buf);

    uint64_t length = 0;
    while (fmt < end) {
        if (*fmt != '%') {
            outc(arg, *fmt);
            length++;
            fmt++;
            continue;
        }

        fmt++;

        flags = 0;
        for (;;) {
            switch (*fmt) {
            case '0':
                flags |= FLAGS_ZEROPAD;
                fmt++;
                break;
            case '-':
                flags |= FLAGS_LEFT;
                fmt++;
                break;
            case '+':
                flags |= FLAGS_PLUS;
                fmt++;
                break;
            case ' ':
                flags |= FLAGS_SPACE;
                fmt++;
                break;
            case '#':
                flags |= FLAGS_HASH;
                fmt++;
                break;
            default:
                goto break_loop;
                break;
            }
        }
    break_loop:
        width = 0;
        if (isdigit(*fmt))
            width = strtol(fmt, (char **)&fmt, 10);
        else if (*fmt == '*') {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *width_obj = *(cur_val++);
            if (FKL_IS_FIX(width_obj)) {
                int64_t w = FKL_GET_FIX(width_obj);
                if (w < 0) {
                    flags |= FLAGS_LEFT;
                    width = -w;
                } else
                    width = w;
            } else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
            fmt++;
        }

        precision = 0;
        if (*fmt == '.') {
            flags |= FLAGS_PRECISION;
            fmt++;
            if (isdigit(*fmt))
                precision = strtol(fmt, (char **)&fmt, 10);
            else if (*fmt == '*') {
                if (cur_val >= val_end) {
                    err = FKL_ERR_TOOFEWARG;
                    goto exit;
                }
                FklVMvalue *prec_obj = *(cur_val++);
                if (FKL_IS_FIX(prec_obj)) {
                    int64_t prec = FKL_GET_FIX(prec_obj);
                    if (prec < 0) {
                        err = FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0;
                        goto exit;
                    } else
                        precision = prec;
                } else {
                    err = FKL_ERR_INCORRECT_TYPE_VALUE;
                    goto exit;
                }
                fmt++;
            }
        }

        switch (*fmt) {
        case 'X':
        case 'x':
            if (*fmt == 'X')
                flags |= FLAGS_UPPERCASE;
            base = 16;
            goto print_integer;
            break;
        case 'o':
            base = 8;
            goto print_integer;
            break;
        case 'd':
        case 'i':
            base = 10;
        print_integer: {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *integer_obj = *(cur_val++);
            if (fklIsVMint(integer_obj)) {
                if (FKL_IS_FIX(integer_obj))
                    length += format_fix_int(FKL_GET_FIX(integer_obj),
                            flags,
                            base,
                            width,
                            precision,
                            outc,
                            (void *)arg);
                else
                    length += format_bigint(&buf,
                            FKL_VM_BI(integer_obj),
                            flags,
                            base,
                            width,
                            precision,
                            outc,
                            (void *)arg);
            } else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
        } break;
        case 'f':
        case 'F':
        case 'G':
        case 'g':
        case 'e':
        case 'E':
        case 'a':
        case 'A': {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *f64_obj = *(cur_val++);
            if (FKL_IS_F64(f64_obj))
                length += format_f64(&buf,
                        *fmt,
                        FKL_VM_F64(f64_obj),
                        flags,
                        width,
                        precision,
                        outc,
                        (void *)arg);
            else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
        } break;
        case 'c': {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *chr_obj = *(cur_val++);
            if (FKL_IS_CHR(chr_obj)) {
                int ch = FKL_GET_CHR(chr_obj);
                uint64_t len = 1;

                if (!(flags & FLAGS_LEFT))
                    for (; len < width; len++)
                        outc(arg, ' ');
                outc(arg, ch);

                if (flags & FLAGS_LEFT)
                    for (; len < width; len++)
                        outc(arg, ' ');
                length += len;
            } else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
        } break;
        case 'S': {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *obj = *(cur_val++);
            format_prin1_value(obj, &buf, exe);

            uint64_t len = buf.index;

            if (!(flags & FLAGS_LEFT))
                for (; len < width; len++)
                    outc(arg, ' ');

            outs(arg, buf.buf, buf.index);

            if (flags & FLAGS_LEFT)
                for (; len < width; len++)
                    outc(arg, ' ');

            buf.index = 0;

            length += len;
        } break;
        case 's': {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *obj = *(cur_val++);
            format_princ_value(obj, &buf, exe);

            uint64_t len = buf.index;

            if (!(flags & FLAGS_LEFT))
                for (; len < width; len++)
                    outc(arg, ' ');

            outs(arg, buf.buf, buf.index);

            if (flags & FLAGS_LEFT)
                for (; len < width; len++)
                    outc(arg, ' ');

            buf.index = 0;

            length += len;
        } break;
        case 'n': {
            if (cur_val >= val_end) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            }
            FklVMvalue *box_obj = *(cur_val++);
            if (FKL_IS_BOX(box_obj)) {
                FklVMvalue *len_obj = fklMakeVMuint(length, exe);
                FKL_VM_BOX(box_obj) = len_obj;
            } else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
        } break;
        default:
            outc(arg, *fmt);
            length++;
            break;
        }
        fmt++;
    }

    if (cur_val < val_end)
        err = FKL_ERR_TOOMANYARG;
    if (plen)
        *plen = length;
exit:
    fklUninitStrBuf(&buf);
    return err;
}

static void format_out_str_buf(void *arg, const char *buf, size_t len) {
    FklCodeBuilder *build = arg;
    fklCodeBuilderWrite(build, len, buf);
}

FklBuiltinErrorType fklVMformat(FklVM *exe,
        FklCodeBuilder *result,
        const char *fmt_str,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *values[]) {
    return vm_format_to_buf(exe,
            fmt_str,
            &fmt_str[strlen(fmt_str)],
            format_out_char,
            format_out_str_buf,
            (void *)result,
            plen,
            values,
            &values[value_count]);
}

FklBuiltinErrorType fklVMformat2(FklVM *exe,
        FklCodeBuilder *result,
        const FklString *fmt_str,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *values[]) {
    return vm_format_to_buf(exe,
            fmt_str->str,
            &fmt_str->str[fmt_str->size],
            format_out_char,
            format_out_str_buf,
            (void *)result,
            plen,
            values,
            &values[value_count]);
}

FklBuiltinErrorType fklVMformat3(FklVM *exe,
        FklCodeBuilder *result,
        size_t fmt_len,
        const char *fmt,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *values[]) {
    return vm_format_to_buf(exe,
            fmt,
            &fmt[fmt_len],
            format_out_char,
            format_out_str_buf,
            (void *)result,
            plen,
            values,
            &values[value_count]);
}

FklVMvalue *fklVMformatToString(FklVM *exe,
        const char *fmt,
        size_t len,
        FklVMvalue *base[]) {
    FklStrBuf buf;
    fklInitStrBuf(&buf);
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &buf, NULL);
    fklVMformat(exe, &builder, fmt, NULL, len, base);
    FklVMvalue *s = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
    fklUninitStrBuf(&buf);
    return s;
}

void fklInitValueTable(FklValueTable *t) {
    t->next_id = 1;
    fklValueIdHashMapInit(&t->ht);
}

void fklUninitValueTable(FklValueTable *t) {
    t->next_id = 0;
    fklValueIdHashMapUninit(&t->ht);
}

FklValueId fklValueTableAdd(FklValueTable *t, const FklVMvalue *v) {
    if (v == NULL)
        return 0;
    FklValueId *n = fklValueIdHashMapPut2(&t->ht, v, t->next_id);
    FklValueId r = n == NULL ? t->next_id++ : *n;
    FKL_ASSERT(r);
    return r;
}

FklValueId fklValueTableGet(const FklValueTable *t, const FklVMvalue *v) {
    if (v == NULL)
        return 0;
    FklValueId *n = fklValueIdHashMapGet2(&t->ht, v);
    if (n == NULL)
        return 0;
    return *n;
}

struct TraverseSerializableArgs {
    FklValueVector *leafs;
    FklValueVector *non_leafs;
};

static int traverse_serializable_value_cb(const FklVMvalue *v, void *ctx) {
    struct TraverseSerializableArgs *args = ctx;
    FKL_ASSERT(is_serializable_to_bytecode_value(v));
    if (is_serializable_leaf_node(v))
        fklValueVectorPushBack2(args->leafs, FKL_TYPE_CAST(FklVMvalue *, v));
    else
        fklValueVectorPushBack2(args->non_leafs,
                FKL_TYPE_CAST(FklVMvalue *, v));
    return 0;
}

void fklTraverseSerializableValue(FklValueTable *t, const FklVMvalue *v) {
    if (v == NULL)
        return;
    if (is_serializable_leaf_node(v)) {
        if (fklIsVMvalueSlot(v))
            fklValueTableAdd(t, v);
        fklValueTableAdd(t, v);
        return;
    }

    FklValueVector leafs;
    FklValueVector non_leafs;

    fklValueVectorInit(&leafs, 8);
    fklValueVectorInit(&non_leafs, 8);

    struct TraverseSerializableArgs args = {
        .leafs = &leafs,
        .non_leafs = &non_leafs,
    };

    traverse_value_bfs(v, traverse_serializable_value_cb, &args);

    for (size_t i = 0; i < leafs.size; ++i) {
        FklVMvalue *v = leafs.base[i];
        if (fklIsVMvalueSlot(v)) {
            FklVMvalueSlot *s = FKL_TYPE_CAST(FklVMvalueSlot *, v);
            fklValueTableAdd(t, s->s);
        }
        fklValueTableAdd(t, v);
    }

    for (size_t i = non_leafs.size; i > 0; --i) {
        FklVMvalue *v = non_leafs.base[i - 1];
        fklValueTableAdd(t, v);
    }

    fklValueVectorUninit(&leafs);
    fklValueVectorUninit(&non_leafs);
}
