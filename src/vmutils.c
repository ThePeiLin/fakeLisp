#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <math.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <tchar.h>
#endif

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
    return (FKL_IS_PAIR(head) && FKL_IS_PAIR(FKL_VM_CDR(head))
            && FKL_IS_PAIR(FKL_VM_CDR(FKL_VM_CDR(head))))
             ? FKL_VM_CDR(FKL_VM_CDR(head))
             : FKL_VM_NIL;
}

int fklIsList(const FklVMvalue *p) {
    FklVMvalue *fast = get_initial_fast_value(p);
    for (; FKL_IS_PAIR(p); p = FKL_VM_CDR(p), fast = get_fast_value(fast))
        if (fast == p)
            return 0;
    if (p != FKL_VM_NIL)
        return 0;
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

FklVMerrorHandler *fklCreateVMerrorHandler(FklSid_t *typeIds,
                                           uint32_t errTypeNum,
                                           FklInstruction *spc, uint64_t cpc) {
    FklVMerrorHandler *t =
        (FklVMerrorHandler *)fklZmalloc(sizeof(FklVMerrorHandler));
    FKL_ASSERT(t);
    t->typeIds = typeIds;
    t->num = errTypeNum;
    t->proc.spc = spc;
    t->proc.end = spc + cpc;
    t->proc.sid = 0;
    return t;
}

void fklDestroyVMerrorHandler(FklVMerrorHandler *h) {
    fklZfree(h->typeIds);
    fklZfree(h);
}

static inline FklVMvalue *get_compound_frame_code_obj(FklVMframe *frame) {
    return FKL_VM_PROC(frame->c.proc)->codeObj;
}

void fklPrintFrame(FklVMframe *cur, FklVM *exe, FILE *fp) {
    if (cur->type == FKL_FRAME_COMPOUND) {
        FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(cur));
        if (proc->sid) {
            fprintf(fp, "at proc: ");
            fklPrintRawSymbol(fklVMgetSymbolWithId(exe->gc, proc->sid)->k, fp);
        } else if (cur->prev) {
            FklFuncPrototype *pt = NULL;
            fklGetCompoundFrameProcPrototype(cur, exe);
            FklSid_t sid = fklGetCompoundFrameSid(cur);
            if (!sid) {
                pt = fklGetCompoundFrameProcPrototype(cur, exe);
                sid = pt->sid;
            }
            if (pt->sid) {
                fprintf(fp, "at proc: ");
                fklPrintRawSymbol(fklVMgetSymbolWithId(exe->gc, pt->sid)->k,
                                  fp);
            } else {
                fprintf(fp, "at proc: <");
                if (pt->fid)
                    fklPrintRawString(fklVMgetSymbolWithId(exe->gc, pt->fid)->k,
                                      fp);
                else
                    fputs("stdin", fp);
                fprintf(fp, ":%" FKL_PRT64U ">", pt->line);
            }
        } else
            fprintf(fp, "at <top>");
        FklByteCodelnt *codeObj = FKL_VM_CO(get_compound_frame_code_obj(cur));
        const FklLineNumberTableItem *node = fklFindLineNumTabNode(
            fklGetCompoundFrameCode(cur) - codeObj->bc->code - 1, codeObj->ls,
            codeObj->l);
        if (node->fid) {
            fprintf(fp, " (%u:", node->line);
            fklPrintString(fklVMgetSymbolWithId(exe->gc, node->fid)->k, fp);
            fprintf(fp, ")\n");
        } else
            fprintf(fp, " (%u)\n", node->line);
    } else
        fklDoPrintBacktrace(cur, fp, exe->gc);
}

void fklPrintBacktrace(FklVM *exe, FILE *fp) {
    for (FklVMframe *cur = exe->top_frame; cur; cur = cur->prev)
        fklPrintFrame(cur, exe, fp);
}

void fklPrintErrBacktrace(FklVMvalue *ev, FklVM *exe, FILE *fp) {
    if (fklIsVMvalueError(ev)) {
        FklVMerror *err = FKL_VM_ERR(ev);
        fklPrintRawSymbol(fklVMgetSymbolWithId(exe->gc, err->type)->k, fp);
        fputs(": ", fp);
        fklPrintString(err->message, fp);
        fputc('\n', fp);
        fklPrintBacktrace(exe, fp);
    } else {
        fprintf(fp, "interrupt with value: ");
        fklPrin1VMvalue(ev, fp, exe->gc);
        fputc('\n', fp);
    }
}

void fklRaiseVMerror(FklVMvalue *ev, FklVM *exe) {
    FKL_VM_PUSH_VALUE(exe, ev);
    longjmp(*exe->buf, FKL_VM_ERR_RAISE);
}

FklVMframe *fklCreateVMframeWithCompoundFrame(const FklVMframe *f,
                                              FklVMframe *prev) {
    FklVMframe *tmp = (FklVMframe *)fklZmalloc(sizeof(FklVMframe));
    FKL_ASSERT(tmp);
    tmp->type = FKL_FRAME_COMPOUND;
    tmp->prev = prev;
    tmp->errorCallBack = f->errorCallBack;
    FklVMCompoundFrameData *fd = &tmp->c;
    const FklVMCompoundFrameData *pfd = &f->c;
    fd->sid = pfd->sid;
    fd->pc = pfd->pc;
    fd->spc = pfd->spc;
    fd->end = pfd->end;
    fd->proc = FKL_VM_NIL;
    fd->proc = pfd->proc;
    fd->mark = pfd->mark;
    FklVMCompoundFrameVarRef *lr = &fd->lr;
    const FklVMCompoundFrameVarRef *plr = &pfd->lr;
    *lr = *plr;
    FklVMvalue **lref =
        plr->lref ? fklCopyMemory(plr->lref, sizeof(FklVMvalue *) * plr->lcount)
                  : NULL;
    FklVMvarRefList *nl = NULL;
    for (FklVMvarRefList *ll = lr->lrefl; ll; ll = ll->next) {
        FklVMvarRefList *rl =
            (FklVMvarRefList *)fklZmalloc(sizeof(FklVMvarRefList));
        FKL_ASSERT(rl);
        rl->ref = ll->ref;
        rl->next = nl;
        nl = rl;
    }
    lr->lref = lref;
    lr->lrefl = nl;
    return tmp;
}

FklVMframe *fklCopyVMframe(FklVM *target_vm, FklVMframe *f, FklVMframe *prev) {
    switch (f->type) {
    case FKL_FRAME_COMPOUND:
        return fklCreateVMframeWithCompoundFrame(f, prev);
        break;
    case FKL_FRAME_OTHEROBJ: {
        FklVMframe *r = fklCreateNewOtherObjVMframe(f->t, prev);
        fklDoCopyObjFrameContext(f, r, target_vm);
        return r;
    } break;
    }
    return NULL;
}

static inline void init_frame_var_ref(FklVMCompoundFrameVarRef *lr) {
    lr->lcount = 0;
    lr->lref = NULL;
    lr->ref = NULL;
    lr->rcount = 0;
    lr->lrefl = NULL;
}

FklVMframe *fklCreateVMframeWithProcValue(FklVM *exe, FklVMvalue *proc,
                                          FklVMframe *prev) {
    FklVMproc *code = FKL_VM_PROC(proc);
    FklVMframe *frame;
    if (exe->frame_cache_head) {
        frame = exe->frame_cache_head;
        exe->frame_cache_head = frame->prev;
        if (frame->prev == NULL)
            exe->frame_cache_tail = &exe->frame_cache_head;
    } else {
        frame = (FklVMframe *)fklZmalloc(sizeof(FklVMframe));
        FKL_ASSERT(frame);
    }
    frame->errorCallBack = NULL;
    frame->type = FKL_FRAME_COMPOUND;
    frame->prev = prev;

    FklVMCompoundFrameData *f = &frame->c;
    f->sid = 0;
    f->pc = NULL;
    f->spc = NULL;
    f->end = NULL;
    f->proc = NULL;
    f->mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
    init_frame_var_ref(&f->lr);
    if (code) {
        f->lr.ref = code->closure;
        f->lr.rcount = code->rcount;
        f->pc = code->spc;
        f->spc = code->spc;
        f->end = code->end;
        f->sid = code->sid;
        f->proc = proc;
    }
    return frame;
}

FklVMframe *fklCreateOtherObjVMframe(FklVM *exe,
                                     const FklVMframeContextMethodTable *t,
                                     FklVMframe *prev) {
    FklVMframe *r;
    if (exe->frame_cache_head) {
        r = exe->frame_cache_head;
        exe->frame_cache_head = r->prev;
        if (r->prev == NULL)
            exe->frame_cache_tail = &exe->frame_cache_head;
    } else {
        r = (FklVMframe *)fklZmalloc(sizeof(FklVMframe));
        FKL_ASSERT(r);
    }
    r->bp = exe->bp;
    r->prev = prev;
    r->errorCallBack = NULL;
    r->type = FKL_FRAME_OTHEROBJ;
    r->t = t;
    return r;
}

FklVMframe *fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable *t,
                                        FklVMframe *prev) {
    FklVMframe *r = (FklVMframe *)fklZcalloc(1, sizeof(FklVMframe));
    FKL_ASSERT(r);
    r->prev = prev;
    r->errorCallBack = NULL;
    r->type = FKL_FRAME_OTHEROBJ;
    r->t = t;
    return r;
}

void fklDestroyVMframe(FklVMframe *frame, FklVM *exe) {
    if (frame->type == FKL_FRAME_OTHEROBJ)
        fklDoFinalizeObjFrame(exe, frame);
    else
        fklDoFinalizeCompoundFrame(exe, frame);
}

static inline void print_raw_symbol_to_string_buffer(FklStringBuffer *s,
                                                     FklString *f);

FklString *fklGenErrorMessage(FklBuiltinErrorType type) {
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
        "Circular reference occurs",
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
    return fklCreateStringFromCstr(s);
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
#include <fakeLisp/hash.h>
static inline void putValueInSet(VmCircleHeadHashMap *s, const FklVMvalue *v) {
    uint32_t num = s->count;
    vmCircleHeadHashMapPut2(s, v,
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
#include <fakeLisp/hash.h>

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

static inline void
scan_value_and_find_value_in_circle(VmValueDegreeHashMap *ht,
                                    VmCircleHeadHashMap *circle_heads,
                                    const FklVMvalue *first_value) {
    FklVMvalueVector stack;
    fklVMvalueVectorInit(&stack, 16);
    fklVMvalueVectorPushBack2(&stack,
                              FKL_REMOVE_CONST(FklVMvalue, first_value));
    while (!fklVMvalueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklVMvalueVectorPopBack(&stack);
        if (FKL_IS_PAIR(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CAR(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_VECTOR(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                FklVMvec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklVMvalueVectorPushBack2(&stack, vec->base[i - 1]);
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_BOX(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_BOX(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            inc_value_degree(ht, v);
            if (!vmCircleHeadHashMapGet2(circle_heads, v)) {
                for (FklVMvalueHashMapNode *tail = FKL_VM_HASH(v)->ht.last;
                     tail; tail = tail->prev) {
                    fklVMvalueVectorPushBack2(&stack, tail->v);
                    fklVMvalueVectorPushBack2(&stack, tail->k);
                }
                putValueInSet(circle_heads, v);
            }
        }
    }
    dec_value_degree(ht, FKL_REMOVE_CONST(FklVMvalue, first_value));

    // remove value not in circle

    do {
        stack.size = 0;
        for (VmValueDegreeHashMapNode *list = ht->first; list;
             list = list->next) {
            if (!list->v)
                fklVMvalueVectorPushBack2(&stack, list->k);
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
                FklVMvec *vec = FKL_VM_VEC(v);
                FklVMvalue **base = vec->base;
                FklVMvalue **const end = &base[vec->size];
                for (; base < end; base++)
                    dec_value_degree(ht, *base);
            } else if (FKL_IS_BOX(v))
                dec_value_degree(ht, FKL_VM_BOX(v));
            else if (FKL_IS_HASHTABLE(v)) {
                for (FklVMvalueHashMapNode *list = FKL_VM_HASH(v)->ht.first;
                     list; list = list->next) {
                    dec_value_degree(ht, list->k);
                    dec_value_degree(ht, list->v);
                }
            }
        }
    } while (!fklVMvalueVectorIsEmpty(&stack));

    // get all circle heads

    vmCircleHeadHashMapClear(circle_heads);
    while (ht->count) {
        VmValueDegreeHashMapNode *value_degree = ht->first;
        FklVMvalue const *v = value_degree->k;
        putValueInSet(circle_heads, v);
        value_degree->v = 0;

        do {
            stack.size = 0;
            for (VmValueDegreeHashMapNode *list = ht->first; list;
                 list = list->next) {
                if (!list->v)
                    fklVMvalueVectorPushBack2(&stack, list->k);
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
                    FklVMvec *vec = FKL_VM_VEC(v);
                    FklVMvalue **base = vec->base;
                    FklVMvalue **const end = &base[vec->size];
                    for (; base < end; base++)
                        dec_value_degree(ht, *base);
                } else if (FKL_IS_BOX(v))
                    dec_value_degree(ht, FKL_VM_BOX(v));
                else if (FKL_IS_HASHTABLE(v)) {
                    for (FklVMvalueHashMapNode *list = FKL_VM_HASH(v)->ht.first;
                         list; list = list->next) {
                        dec_value_degree(ht, list->k);
                        dec_value_degree(ht, list->v);
                    }
                }
            }
        } while (!fklVMvalueVectorIsEmpty(&stack));
    }
    fklVMvalueVectorUninit(&stack);
}

static inline void scan_cir_ref(const FklVMvalue *s,
                                VmCircleHeadHashMap *circle_head_set) {
    VmValueDegreeHashMap degree_table;
    vmValueDegreeHashMapInit(&degree_table);
    scan_value_and_find_value_in_circle(&degree_table, circle_head_set, s);
    vmValueDegreeHashMapUninit(&degree_table);
}

int fklHasCircleRef(const FklVMvalue *first_value) {
    if (FKL_GET_TAG(first_value) != FKL_TAG_PTR
        || (!FKL_IS_PAIR(first_value) && !FKL_IS_BOX(first_value)
            && !FKL_IS_VECTOR(first_value) && !FKL_IS_HASHTABLE(first_value)))
        return 0;
    FklVMvalueHashSet value_set;
    fklVMvalueHashSetInit(&value_set);

    VmValueDegreeHashMap degree_table;
    vmValueDegreeHashMapInit(&degree_table);

    FklVMvalueVector stack;
    fklVMvalueVectorInit(&stack, 16);

    fklVMvalueVectorPushBack2(&stack,
                              FKL_REMOVE_CONST(FklVMvalue, first_value));
    while (!fklVMvalueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklVMvalueVectorPopBack(&stack);
        if (FKL_IS_PAIR(v)) {
            inc_value_degree(&degree_table, v);
            if (!fklVMvalueHashSetPut2(&value_set, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CAR(v));
            }
        } else if (FKL_IS_VECTOR(v)) {
            inc_value_degree(&degree_table, v);
            if (!fklVMvalueHashSetPut2(&value_set, v)) {
                FklVMvec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklVMvalueVectorPushBack2(&stack, vec->base[i - 1]);
            }
        } else if (FKL_IS_BOX(v)) {
            inc_value_degree(&degree_table, v);
            if (!fklVMvalueHashSetPut2(&value_set, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_BOX(v));
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            inc_value_degree(&degree_table, v);
            if (!fklVMvalueHashSetPut2(&value_set, v)) {
                for (FklVMvalueHashMapNode *tail = FKL_VM_HASH(v)->ht.last;
                     tail; tail = tail->prev) {
                    fklVMvalueVectorPushBack2(&stack, tail->v);
                    fklVMvalueVectorPushBack2(&stack, tail->k);
                }
            }
        }
    }
    dec_value_degree(&degree_table, FKL_REMOVE_CONST(FklVMvalue, first_value));
    fklVMvalueHashSetUninit(&value_set);

    // remove value not in circle

    do {
        stack.size = 0;
        for (VmValueDegreeHashMapNode *list = degree_table.first; list;
             list = list->next) {
            if (!list->v)
                fklVMvalueVectorPushBack2(&stack, list->k);
        }
        FklVMvalue **base = (FklVMvalue **)stack.base;
        FklVMvalue **const end = &base[stack.size];
        for (; base < end; base++) {
            vmValueDegreeHashMapDel2(&degree_table, *base);
            FklVMvalue *v = *base;
            if (FKL_IS_PAIR(v)) {
                dec_value_degree(&degree_table, FKL_VM_CAR(v));
                dec_value_degree(&degree_table, FKL_VM_CDR(v));
            } else if (FKL_IS_VECTOR(v)) {
                FklVMvec *vec = FKL_VM_VEC(v);
                FklVMvalue **base = vec->base;
                FklVMvalue **const end = &base[vec->size];
                for (; base < end; base++)
                    dec_value_degree(&degree_table, *base);
            } else if (FKL_IS_BOX(v))
                dec_value_degree(&degree_table, FKL_VM_BOX(v));
            else if (FKL_IS_HASHTABLE(v)) {
                for (FklVMvalueHashMapNode *list = FKL_VM_HASH(v)->ht.first;
                     list; list = list->next) {
                    dec_value_degree(&degree_table, list->k);
                    dec_value_degree(&degree_table, list->v);
                }
            }
        }
    } while (!fklVMvalueVectorIsEmpty(&stack));

    int r = degree_table.count > 0;
    vmValueDegreeHashMapUninit(&degree_table);
    fklVMvalueVectorUninit(&stack);
    return r;
}

#define VMVALUE_PRINTER_ARGS                                                   \
    const FklVMvalue *v, FILE *fp, FklStringBuffer *buffer, FklVMgc *gc
static void vmvalue_f64_printer(VMVALUE_PRINTER_ARGS) {
    char buf[64] = {0};
    fklWriteDoubleToBuf(buf, 64, FKL_VM_F64(v));
    fputs(buf, fp);
}

static void vmvalue_bigint_printer(VMVALUE_PRINTER_ARGS) {
    fklPrintVMbigInt(FKL_VM_BI(v), fp);
}

static void vmvalue_string_princ(VMVALUE_PRINTER_ARGS) {
    FklString *ss = FKL_VM_STR(v);
    fwrite(ss->str, ss->size, 1, fp);
}

static void vmvalue_bytevector_printer(VMVALUE_PRINTER_ARGS) {
    fklPrintRawBytevector(FKL_VM_BVEC(v), fp);
}

static void vmvalue_userdata_princ(VMVALUE_PRINTER_ARGS) {
    const FklVMud *ud = FKL_VM_UD(v);
    void (*as_princ)(const FklVMud *, FklStringBuffer *, FklVMgc *) =
        ud->t->__as_princ;
    if (as_princ) {
        as_princ(ud, buffer, gc);
        fputs(buffer->buf, fp);
        buffer->index = 0;
    } else
        fprintf(fp, "#<userdata %p>", ud);
}

static void vmvalue_proc_printer(VMVALUE_PRINTER_ARGS) {
    FklVMproc *proc = FKL_VM_PROC(v);
    if (proc->sid) {
        fprintf(fp, "#<proc ");
        fklPrintRawSymbol(fklVMgetSymbolWithId(gc, proc->sid)->k, fp);
        fputc('>', fp);
    } else
        fprintf(fp, "#<proc %p>", proc);
}

static void vmvalue_cproc_printer(VMVALUE_PRINTER_ARGS) {
    FklVMcproc *cproc = FKL_VM_CPROC(v);
    if (cproc->sid) {
        fprintf(fp, "#<cproc ");
        fklPrintRawSymbol(fklVMgetSymbolWithId(gc, cproc->sid)->k, fp);
        fputc('>', fp);
    } else
        fprintf(fp, "#<cproc %p>", cproc);
}

static void (*VMvaluePtrPrincTable[FKL_VM_VALUE_GC_TYPE_NUM])(
    VMVALUE_PRINTER_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_printer,
    [FKL_TYPE_BIGINT] = vmvalue_bigint_printer,
    [FKL_TYPE_STR] = vmvalue_string_princ,
    [FKL_TYPE_BYTEVECTOR] = vmvalue_bytevector_printer,
    [FKL_TYPE_USERDATA] = vmvalue_userdata_princ,
    [FKL_TYPE_PROC] = vmvalue_proc_printer,
    [FKL_TYPE_CPROC] = vmvalue_cproc_printer,
};

static void vmvalue_ptr_ptr_princ(VMVALUE_PRINTER_ARGS) {
    VMvaluePtrPrincTable[v->type](v, fp, buffer, gc);
}

static void vmvalue_nil_ptr_print(VMVALUE_PRINTER_ARGS) { fputs("()", fp); }

static void vmvalue_fix_ptr_print(VMVALUE_PRINTER_ARGS) {
    fprintf(fp, "%" FKL_PRT64D "", FKL_GET_FIX(v));
}

static void vmvalue_sym_ptr_princ(VMVALUE_PRINTER_ARGS) {
    fklPrintString(fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->k, fp);
}

static void vmvalue_chr_ptr_princ(VMVALUE_PRINTER_ARGS) {
    putc(FKL_GET_CHR(v), fp);
}

static void (*VMvaluePrincTable[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS) = {
    vmvalue_ptr_ptr_princ, vmvalue_nil_ptr_print, vmvalue_fix_ptr_print,
    vmvalue_sym_ptr_princ, vmvalue_chr_ptr_princ,
};

static void princVMatom(VMVALUE_PRINTER_ARGS) {
    VMvaluePrincTable[(FklVMptrTag)FKL_GET_TAG(v)](v, fp, buffer, gc);
}

static void vmvalue_sym_ptr_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintRawSymbol(fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->k, fp);
}

static void vmvalue_chr_ptr_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintRawChar(FKL_GET_CHR(v), fp);
}

static void vmvalue_string_prin1(VMVALUE_PRINTER_ARGS) {
    fklPrintRawString(FKL_VM_STR(v), fp);
}

static void vmvalue_userdata_prin1(VMVALUE_PRINTER_ARGS) {
    const FklVMud *ud = FKL_VM_UD(v);
    void (*as_prin1)(const FklVMud *, FklStringBuffer *, FklVMgc *) =
        ud->t->__as_prin1;
    if (as_prin1) {
        as_prin1(ud, buffer, gc);
        fputs(buffer->buf, fp);
        buffer->index = 0;
    } else
        fprintf(fp, "#<userdata %p>", ud);
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
};

static void vmvalue_ptr_ptr_prin1(VMVALUE_PRINTER_ARGS) {
    VMvaluePtrPrin1Table[v->type](v, fp, buffer, gc);
}

static void (*VMvaluePrin1Table[FKL_PTR_TAG_NUM])(VMVALUE_PRINTER_ARGS) = {
    vmvalue_ptr_ptr_prin1, vmvalue_nil_ptr_print, vmvalue_fix_ptr_print,
    vmvalue_sym_ptr_prin1, vmvalue_chr_ptr_prin1,
};

static void prin1VMatom(VMVALUE_PRINTER_ARGS) {
    VMvaluePrin1Table[(FklVMptrTag)FKL_GET_TAG(v)](v, fp, buffer, gc);
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
    const FklVMvalueHashMapNode *cur;
} PrintHashCtx;

static_assert(sizeof(PrintHashCtx) <= sizeof(PrintCtx),
              "print hash ctx too large");

// VmPrintCtxVector
#define FKL_VECTOR_TYPE_PREFIX Vm
#define FKL_VECTOR_METHOD_PREFIX vm
#define FKL_VECTOR_ELM_TYPE PrintCtx
#define FKL_VECTOR_ELM_TYPE_NAME PrintCtx
#include <fakeLisp/vector.h>

static inline void
print_pair_ctx_init(PrintCtx *ctx, const FklVMvalue *pair,
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

#define PRINT_INCLUDED
#define NAME print_value
#define LOCAL_VAR                                                              \
    FklStringBuffer string_buffer;                                             \
    fklInitStringBuffer(&string_buffer);
#define UNINIT_LOCAL_VAR fklUninitStringBuffer(&string_buffer);
#define OUTPUT_TYPE FILE *
#define OUTPUT_TYPE_NAME File
#define PUTS(OUT, S) fputs((S), (OUT))
#define PUTC(OUT, C) fputc((C), (OUT))
#define PRINTF fprintf
#define PUT_ATOMIC_METHOD_ARG void (*atomPrinter)(VMVALUE_PRINTER_ARGS)
#define PUT_ATOMIC_METHOD(OUT, V, GC)                                          \
    atomPrinter((V), (OUT), &string_buffer, (GC))
#include "vmprint.h"

void fklPrin1VMvalue(FklVMvalue *v, FILE *fp, FklVMgc *gc) {
    print_value(v, fp, prin1VMatom, gc);
}

void fklPrincVMvalue(FklVMvalue *v, FILE *fp, FklVMgc *gc) {
    print_value(v, fp, princVMatom, gc);
}

#undef VMVALUE_PRINTER_ARGS
#include <ctype.h>

static inline void print_raw_char_to_string_buffer(FklStringBuffer *s, char c) {
    fklStringBufferConcatWithCstr(s, "#\\");
    if (c == ' ')
        fklStringBufferConcatWithCstr(s, "\\s");
    else if (c == '\0')
        fklStringBufferConcatWithCstr(s, "\\0");
    else if (fklIsSpecialCharAndPrintToStringBuffer(s, c))
        ;
    else if (isgraph(c)) {
        if (c == '\\')
            fklStringBufferPutc(s, '\\');
        else
            fklStringBufferPutc(s, c);
    } else {
        uint8_t j = c;
        fklStringBufferConcatWithCstr(s, "\\x");
        fklStringBufferPrintf(s, "%02X", j);
    }
}

static inline void print_raw_string_to_string_buffer(FklStringBuffer *s,
                                                     FklString *f) {
    fklPrintRawStringToStringBuffer(s, f, "\"", "\"", '"');
}

static inline void print_raw_symbol_to_string_buffer(FklStringBuffer *s,
                                                     FklString *f) {
    fklPrintRawStringToStringBuffer(s, f, "|", "|", '|');
}

static inline void print_bigint_to_string_buffer(FklStringBuffer *s,
                                                 const FklVMbigInt *a) {
    if (a->num == 0)
        fklStringBufferPutc(s, '0');
    else {
        const FklBigInt bi = fklVMbigIntToBigInt(a);
        char *str = fklBigIntToCstr(&bi, 10, FKL_BIGINT_FMT_FLAG_NONE);
        fklStringBufferConcatWithCstr(s, str);
        fklZfree(str);
    }
}

#define VMVALUE_TO_UTSTRING_ARGS                                               \
    FklStringBuffer *result, const FklVMvalue *v, FklVMgc *gc

static void nil_ptr_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferConcatWithCstr(result, "()");
}

static void fix_ptr_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferPrintf(result, "%" FKL_PRT64D "", FKL_GET_FIX(v));
}

static void sym_ptr_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    print_raw_symbol_to_string_buffer(
        result, fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->k);
}

static void chr_ptr_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    print_raw_char_to_string_buffer(result, FKL_GET_CHR(v));
}

static void vmvalue_f64_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    char buf[64] = {0};
    fklWriteDoubleToBuf(buf, 64, FKL_VM_F64(v));
    fklStringBufferConcatWithCstr(result, buf);
}

static void vmvalue_bigint_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    print_bigint_to_string_buffer(result, FKL_VM_BI(v));
}

static void vmvalue_string_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    print_raw_string_to_string_buffer(result, FKL_VM_STR(v));
}

static void vmvalue_bytevector_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    fklPrintBytevectorToStringBuffer(result, FKL_VM_BVEC(v));
}

static void vmvalue_userdata_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    FklVMud *ud = FKL_VM_UD(v);
    if (fklIsAbleToStringUd(ud))
        fklUdAsPrin1(ud, result, gc);
    else
        fklStringBufferPrintf(result, "#<userdata %p>", ud);
}

static void vmvalue_proc_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    FklVMproc *proc = FKL_VM_PROC(v);
    if (proc->sid) {
        fklStringBufferConcatWithCstr(result, "#<proc ");
        print_raw_symbol_to_string_buffer(
            result, fklVMgetSymbolWithId(gc, proc->sid)->k);
        fklStringBufferPutc(result, '>');
    } else
        fklStringBufferPrintf(result, "#<proc %p>", proc);
}

static void vmvalue_cproc_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    FklVMcproc *cproc = FKL_VM_CPROC(v);
    if (cproc->sid) {
        fklStringBufferConcatWithCstr(result, "#<cproc ");
        print_raw_symbol_to_string_buffer(
            result, fklVMgetSymbolWithId(gc, cproc->sid)->k);
        fklStringBufferPutc(result, '>');
    } else
        fklStringBufferPrintf(result, "#<cproc %p>", cproc);
}

static void (
    *atom_ptr_ptr_to_string_buffer_prin1_table[FKL_VM_VALUE_GC_TYPE_NUM])(
    VMVALUE_TO_UTSTRING_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_as_print,
    [FKL_TYPE_BIGINT] = vmvalue_bigint_as_print,
    [FKL_TYPE_STR] = vmvalue_string_as_prin1,
    [FKL_TYPE_BYTEVECTOR] = vmvalue_bytevector_as_print,
    [FKL_TYPE_USERDATA] = vmvalue_userdata_as_prin1,
    [FKL_TYPE_PROC] = vmvalue_proc_as_print,
    [FKL_TYPE_CPROC] = vmvalue_cproc_as_print,
};

static void ptr_ptr_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    atom_ptr_ptr_to_string_buffer_prin1_table[v->type](result, v, gc);
}

static void (*atom_ptr_to_string_buffer_prin1_table[FKL_PTR_TAG_NUM])(
    VMVALUE_TO_UTSTRING_ARGS) = {
    ptr_ptr_as_prin1, nil_ptr_as_print, fix_ptr_as_print,
    sym_ptr_as_prin1, chr_ptr_as_prin1,
};

static void atom_as_prin1_string(VMVALUE_TO_UTSTRING_ARGS) {
    atom_ptr_to_string_buffer_prin1_table[(FklVMptrTag)FKL_GET_TAG(v)](result,
                                                                       v, gc);
}

static void vmvalue_string_as_princ(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferConcatWithString(result, FKL_VM_STR(v));
}

static void vmvalue_userdata_as_princ(VMVALUE_TO_UTSTRING_ARGS) {
    FklVMud *ud = FKL_VM_UD(v);
    if (fklIsAbleAsPrincUd(ud))
        fklUdAsPrinc(ud, result, gc);
    else
        fklStringBufferPrintf(result, "#<userdata %p>", ud);
}

static void (
    *atom_ptr_ptr_to_string_buffer_princ_table[FKL_VM_VALUE_GC_TYPE_NUM])(
    VMVALUE_TO_UTSTRING_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_as_print,
    [FKL_TYPE_BIGINT] = vmvalue_bigint_as_print,
    [FKL_TYPE_STR] = vmvalue_string_as_princ,
    [FKL_TYPE_BYTEVECTOR] = vmvalue_bytevector_as_print,
    [FKL_TYPE_USERDATA] = vmvalue_userdata_as_princ,
    [FKL_TYPE_PROC] = vmvalue_proc_as_print,
    [FKL_TYPE_CPROC] = vmvalue_cproc_as_print,
};

static void ptr_ptr_as_princ(VMVALUE_TO_UTSTRING_ARGS) {
    atom_ptr_ptr_to_string_buffer_princ_table[v->type](result, v, gc);
}

static void sym_ptr_as_princ(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferConcatWithString(
        result, fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->k);
}

static void chr_ptr_as_princ(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferPutc(result, FKL_GET_CHR(v));
}

static void (*atom_ptr_to_string_buffer_princ_table[FKL_PTR_TAG_NUM])(
    VMVALUE_TO_UTSTRING_ARGS) = {
    ptr_ptr_as_princ, nil_ptr_as_print, fix_ptr_as_print,
    sym_ptr_as_princ, chr_ptr_as_princ,
};

static void atom_as_princ_string(VMVALUE_TO_UTSTRING_ARGS) {
    atom_ptr_to_string_buffer_princ_table[(FklVMptrTag)FKL_GET_TAG(v)](result,
                                                                       v, gc);
}

#define PRINT_INCLUDED
#include "vmprint.h"

FklVMvalue *fklVMstringify(FklVMvalue *value, FklVM *exe) {
    FklStringBuffer result;
    fklInitStringBuffer(&result);
    stringify_value_to_string_buffer(value, &result, atom_as_prin1_string,
                                     exe->gc);
    FklVMvalue *retval = fklCreateVMvalueStr2(exe, fklStringBufferLen(&result),
                                              fklStringBufferBody(&result));
    fklUninitStringBuffer(&result);
    return retval;
}

FklVMvalue *fklVMstringifyAsPrinc(FklVMvalue *value, FklVM *exe) {
    FklStringBuffer result;
    fklInitStringBuffer(&result);
    stringify_value_to_string_buffer(value, &result, atom_as_princ_string,
                                     exe->gc);
    FklVMvalue *retval = fklCreateVMvalueStr2(exe, fklStringBufferLen(&result),
                                              fklStringBufferBody(&result));
    fklUninitStringBuffer(&result);
    return retval;
}

size_t fklVMlistLength(FklVMvalue *v) {
    size_t len = 0;
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v))
        len++;
    return len;
}

#define IS_DEFAULT_BIGINT(bi) (bi->digits == NULL)
#define INIT_DEFAULT_BIGINT_1(bi)                                              \
    if (IS_DEFAULT_BIGINT(bi))                                                 \
    fklInitBigInt1(bi)

int fklProcessVMnumAdd(FklVMvalue *cur, int64_t *pr64, double *pf64,
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

int fklProcessVMnumMul(FklVMvalue *cur, int64_t *pr64, double *pf64,
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
        const FklVMbigInt *bigint = FKL_VM_BI(arg);
        if (fklIsVMbigIntAddkInFixIntRange(bigint, k))
            return FKL_MAKE_VM_FIX(fklVMbigIntToI(bigint) + k);
        else
            return fklVMbigIntAddI(exe, bigint, k);
    } else if (FKL_IS_F64(arg))
        return fklCreateVMvalueF64(exe,
                                   FKL_VM_F64(arg) + FKL_TYPE_CAST(double, k));
    return NULL;
}

FklVMvalue *fklProcessVMnumAddResult(FklVM *exe, int64_t r64, double rd,
                                     FklBigInt *bi) {
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

FklVMvalue *fklProcessVMnumSubResult(FklVM *exe, FklVMvalue *prev, int64_t r64,
                                     double rd, FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev) || rd != 0.0)
        r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) - rd - r64
                                         - fklBigIntToD(bi));
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
        const FklVMbigInt *b = FKL_VM_BI(prev);
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

FklVMvalue *fklProcessVMnumMulResult(FklVM *exe, int64_t r64, double rd,
                                     FklBigInt *bi) {
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

FklVMvalue *fklProcessVMnumIdivResult(FklVM *exe, FklVMvalue *prev, int64_t r64,
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

FklVMvalue *fklProcessVMnumDivResult(FklVM *exe, FklVMvalue *prev, int64_t r64,
                                     double rd, FklBigInt *bi) {
    FklVMvalue *r = NULL;
    if (FKL_IS_F64(prev) || rd != 1.0
        || (FKL_IS_FIX(prev) && (IS_DEFAULT_BIGINT(bi) || FKL_BIGINT_IS_1(bi))
            && FKL_GET_FIX(prev) % (r64))
        || (FKL_IS_BIGINT(prev)
            && ((!IS_DEFAULT_BIGINT(bi) && !FKL_BIGINT_IS_1(bi)
                 && !fklIsVMbigIntDivisible(FKL_VM_BI(prev), bi))
                || !fklIsVMbigIntDivisibleI(FKL_VM_BI(prev), r64)))) {
        if (IS_DEFAULT_BIGINT(bi))
            r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) / rd / r64);
        else
            r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) / rd / r64
                                             / fklBigIntToD(bi));
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
    fklUnlockThread(exe);
    uv_sleep(ms);
    fklLockThread(exe);
}

void fklVMread(FklVM *exe, FILE *fp, FklStringBuffer *buf, uint64_t len,
               int d) {
    fklUnlockThread(exe);
    if (d != EOF)
        fklGetDelim(fp, buf, d);
    else
        buf->index = fread(buf->buf, sizeof(char), len, fp);
    fklLockThread(exe);
}

void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],
                             FklSymbolTable *table) {
    static const char *builtInErrorType[FKL_BUILTIN_ERR_NUM] = {
        // clang-format off
        "dummy",
        "symbol-error",
        "syntax-error",
        "read-error",
        "load-error",
        "pattern-error",
        "type-error",
        "stack-error",
        "arg-error",
        "arg-error",
        "thread-error",
        "thread-error",
        "macro-error",
        "call-error",
        "load-error",
        "symbol-error",
        "library-error",
        "eof-error",
        "div-zero-error",
        "file-error",
        "value-error",
        "access-error",
        "access-error",
        "import-error",
        "macro-error",
        "type-error",
        "type-error",
        "call-error",
        "value-error",
        "value-error",
        "value-error",
        "value-error",
        "operation-error",
        "import-error",
        "export-error",
        "import-error",
        "grammer-error",
        "grammer-error",
        "grammer-error",
        "value-error",
        "symbol-error",
        "symbol-error",
        "syntax-error",
        // clang-format on
    };

    for (size_t i = 0; i < FKL_BUILTIN_ERR_NUM; i++)
        errorTypeId[i] = fklAddSymbolCstr(builtInErrorType[i], table)->v;
}

#define FLAGS_ZEROPAD (1u << 0u)
#define FLAGS_LEFT (1u << 1u)
#define FLAGS_PLUS (1u << 2u)
#define FLAGS_SPACE (1u << 3u)
#define FLAGS_HASH (1u << 4u)
#define FLAGS_UPPERCASE (1u << 5u)
#define FLAGS_CHAR (1u << 6u)
#define FLAGS_PRECISION (1u << 7u)

static void print_out_char(void *arg, char c) {
    FILE *fp = arg;
    fputc(c, fp);
}

static void format_out_char(void *arg, char c) {
    FklStringBuffer *buf = arg;
    fklStringBufferPutc(buf, c);
}

static inline void out_cstr(void (*outc)(void *, char), void *arg,
                            const char *s) {
    while (*s)
        outc(arg, *s++);
}

static inline uint64_t format_fix_int(int64_t integer_val, uint32_t flags,
                                      uint32_t base, uint64_t width,
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

static inline uint64_t format_bigint(FklStringBuffer *buf,
                                     const FklVMbigInt *bi, uint32_t flags,
                                     uint32_t base, uint64_t width,
                                     uint64_t precision,
                                     void(outc)(void *, char), void *arg) {
    uint64_t length = 0;
    if (FKL_BIGINT_IS_0(bi))
        flags &= ~FLAGS_HASH;

    FklBigIntFmtFlags bigint_fmt_flags = FKL_BIGINT_FMT_FLAG_NONE;
    if (flags & FLAGS_HASH)
        bigint_fmt_flags |= FKL_BIGINT_FMT_FLAG_ALTERNATE;
    if (flags & FLAGS_UPPERCASE)
        bigint_fmt_flags |= FKL_BIGINT_FMT_FLAG_CAPITALS;
    const FklBigInt bigint = fklVMbigIntToBigInt(bi);
    fklBigIntToStringBuffer(&bigint, buf, base, bigint_fmt_flags);

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

static inline uint64_t format_f64(FklStringBuffer *buf, char ch, double value,
                                  uint32_t flags, uint64_t width,
                                  uint64_t precision,
                                  void (*outc)(void *, char), void *buffer) {
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
            fklStringBufferPrintf(buf, fmt, width, value);
        } else {
            char fmt[] = "%.*a";
            fmt[sizeof(fmt) - 2] = ch;
            fklStringBufferPrintf(buf, fmt, precision, value);
        }
    } else {
        char fmt[] = "%.*f";
        fmt[sizeof(fmt) - 2] = ch;
        if (!(flags & FLAGS_PRECISION))
            precision = 6;

        fklStringBufferPrintf(buf, fmt, precision, value);
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

static inline FklBuiltinErrorType vm_format_to_buf(
    FklVM *exe, const char *fmt, const char *end, void (*outc)(void *, char),
    void (*outs)(void *, const char *, size_t len), void *arg, uint64_t *plen,
    FklVMvalue **cur_val, FklVMvalue **const val_end) {
    uint32_t base;
    uint32_t flags;
    uint64_t width;
    uint64_t precision;
    FklBuiltinErrorType err = 0;

    FklStringBuffer buf;
    fklInitStringBuffer(&buf);

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
                    length +=
                        format_fix_int(FKL_GET_FIX(integer_obj), flags, base,
                                       width, precision, outc, (void *)arg);
                else
                    length +=
                        format_bigint(&buf, FKL_VM_BI(integer_obj), flags, base,
                                      width, precision, outc, (void *)arg);
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
                length += format_f64(&buf, *fmt, FKL_VM_F64(f64_obj), flags,
                                     width, precision, outc, (void *)arg);
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
            stringify_value_to_string_buffer(obj, &buf, atom_as_prin1_string,
                                             exe->gc);

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
            stringify_value_to_string_buffer(obj, &buf, atom_as_princ_string,
                                             exe->gc);

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
    fklUninitStringBuffer(&buf);
    return err;
}

static void print_out_str_buf(void *arg, const char *buf, size_t len) {
    FILE *fp = arg;
    fwrite(buf, len, 1, fp);
}

FklBuiltinErrorType fklVMprintf2(FklVM *exe, FILE *fp, const FklString *fmt_str,
                                 uint64_t *plen, FklVMvalue **start,
                                 FklVMvalue **const end) {
    return vm_format_to_buf(exe, fmt_str->str, &fmt_str->str[fmt_str->size],
                            print_out_char, print_out_str_buf, (void *)fp, plen,
                            start, end);
}

FklBuiltinErrorType fklVMprintf(FklVM *exe, FILE *fp, const char *fmt_str,
                                uint64_t *plen, FklVMvalue **start,
                                FklVMvalue **const end) {
    return vm_format_to_buf(exe, fmt_str, &fmt_str[strlen(fmt_str)],
                            print_out_char, print_out_str_buf, (void *)fp, plen,
                            start, end);
}

static void format_out_str_buf(void *arg, const char *buf, size_t len) {
    FklStringBuffer *result = arg;
    fklStringBufferBincpy(result, buf, len);
}

FklBuiltinErrorType fklVMformat(FklVM *exe, FklStringBuffer *result,
                                const char *fmt_str, uint64_t *plen,
                                FklVMvalue **cur_val,
                                FklVMvalue **const val_end) {
    return vm_format_to_buf(exe, fmt_str, &fmt_str[strlen(fmt_str)],
                            format_out_char, format_out_str_buf, (void *)result,
                            plen, cur_val, val_end);
}

FklBuiltinErrorType fklVMformat2(FklVM *exe, FklStringBuffer *result,
                                 const FklString *fmt_str, uint64_t *plen,
                                 FklVMvalue **cur_val,
                                 FklVMvalue **const val_end) {
    return vm_format_to_buf(exe, fmt_str->str, &fmt_str->str[fmt_str->size],
                            format_out_char, format_out_str_buf, (void *)result,
                            plen, cur_val, val_end);
}

FklBuiltinErrorType fklVMformat3(FklVM *exe, FklStringBuffer *result,
                                 const char *fmt, const char *end,
                                 uint64_t *plen, FklVMvalue **cur_val,
                                 FklVMvalue **const val_end) {
    return vm_format_to_buf(exe, fmt, end, format_out_char, format_out_str_buf,
                            (void *)result, plen, cur_val, val_end);
}

FklString *fklVMformatToString(FklVM *exe, const char *fmt, FklVMvalue **base,
                               size_t len) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    fklVMformat(exe, &buf, fmt, NULL, base, &base[len]);
    FklString *s = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return s;
}
