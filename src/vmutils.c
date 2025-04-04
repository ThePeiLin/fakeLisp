#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/vm.h>

#include <math.h>
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

int fklIsFixint(const FklVMvalue *p) { return FKL_IS_FIX(p); }

int fklIsVMint(const FklVMvalue *p) {
    return FKL_IS_FIX(p) || FKL_IS_BIG_INT(p);
}

int fklIsVMnumber(const FklVMvalue *p) {
    return FKL_IS_FIX(p) || FKL_IS_BIG_INT(p) || FKL_IS_F64(p);
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
    return FKL_IS_FIX(p)       ? FKL_GET_FIX(p)
         : (FKL_IS_BIG_INT(p)) ? fklVMbigIntToD(FKL_VM_BI(p))
                               : FKL_VM_F64(p);
}

FklVMerrorHandler *fklCreateVMerrorHandler(FklSid_t *typeIds,
                                           uint32_t errTypeNum,
                                           FklInstruction *spc, uint64_t cpc) {
    FklVMerrorHandler *t =
        (FklVMerrorHandler *)malloc(sizeof(FklVMerrorHandler));
    FKL_ASSERT(t);
    t->typeIds = typeIds;
    t->num = errTypeNum;
    t->proc.spc = spc;
    t->proc.end = spc + cpc;
    t->proc.sid = 0;
    return t;
}

void fklDestroyVMerrorHandler(FklVMerrorHandler *h) {
    free(h->typeIds);
    free(h);
}

static inline FklVMvalue *get_compound_frame_code_obj(FklVMframe *frame) {
    return FKL_VM_PROC(frame->c.proc)->codeObj;
}

void fklPrintFrame(FklVMframe *cur, FklVM *exe, FILE *fp) {
    if (cur->type == FKL_FRAME_COMPOUND) {
        FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(cur));
        if (proc->sid) {
            fprintf(fp, "at proc: ");
            fklPrintRawSymbol(fklVMgetSymbolWithId(exe->gc, proc->sid)->symbol,
                              fp);
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
                fklPrintRawSymbol(
                    fklVMgetSymbolWithId(exe->gc, pt->sid)->symbol, fp);
            } else {
                fprintf(fp, "at proc: <");
                if (pt->fid)
                    fklPrintRawString(
                        fklVMgetSymbolWithId(exe->gc, pt->fid)->symbol, fp);
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
            fklPrintString(fklVMgetSymbolWithId(exe->gc, node->fid)->symbol,
                           fp);
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
        fklPrintRawSymbol(fklVMgetSymbolWithId(exe->gc, err->type)->symbol, fp);
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
    longjmp(exe->buf, FKL_VM_ERR_RAISE);
}

FklVMframe *fklCreateVMframeWithCompoundFrame(const FklVMframe *f,
                                              FklVMframe *prev) {
    FklVMframe *tmp = (FklVMframe *)malloc(sizeof(FklVMframe));
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
    fd->tail = pfd->tail;
    FklVMCompoundFrameVarRef *lr = &fd->lr;
    const FklVMCompoundFrameVarRef *plr = &pfd->lr;
    *lr = *plr;
    FklVMvalue **lref =
        plr->lref ? fklCopyMemory(plr->lref, sizeof(FklVMvalue *) * plr->lcount)
                  : NULL;
    FklVMvarRefList *nl = NULL;
    for (FklVMvarRefList *ll = lr->lrefl; ll; ll = ll->next) {
        FklVMvarRefList *rl =
            (FklVMvarRefList *)malloc(sizeof(FklVMvarRefList));
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
    lr->base = 0;
    lr->lcount = 0;
    lr->loc = NULL;
    lr->lref = NULL;
    lr->ref = NULL;
    lr->rcount = 0;
    lr->lrefl = NULL;
}

void fklUpdateAllVarRef(FklVMframe *f, FklVMvalue **locv) {
    for (; f; f = f->prev)
        if (f->type == FKL_FRAME_COMPOUND) {
            FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(f);
            FklVMvalue **loc = &locv[lr->base];
            for (FklVMvarRefList *ll = lr->lrefl; ll; ll = ll->next) {
                FklVMvalueVarRef *ref = FKL_VM_VAR_REF(ll->ref);
                if (ref->ref != &ref->v)
                    ref->ref = &loc[ref->idx];
            }
            lr->loc = loc;
        }
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
        frame = (FklVMframe *)malloc(sizeof(FklVMframe));
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
    f->mark = 0;
    f->tail = 0;
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
        r = (FklVMframe *)malloc(sizeof(FklVMframe));
        FKL_ASSERT(r);
    }
    r->prev = prev;
    r->errorCallBack = NULL;
    r->type = FKL_FRAME_OTHEROBJ;
    r->t = t;
    return r;
}

FklVMframe *fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable *t,
                                        FklVMframe *prev) {
    FklVMframe *r = (FklVMframe *)calloc(1, sizeof(FklVMframe));
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

typedef struct PrtElem {
    enum PrintingState {
        PRT_CAR,
        PRT_CDR,
        PRT_REC_CAR,
        PRT_REC_CDR,
        PRT_BOX,
        PRT_REC_BOX,
        PRT_HASH_ITEM,
    } state;
    FklVMvalue *v;
} PrtElem;

static PrtElem *createPrtElem(enum PrintingState state, FklVMvalue *v) {
    PrtElem *r = (PrtElem *)malloc(sizeof(PrtElem));
    FKL_ASSERT(r);
    r->state = state;
    r->v = v;
    return r;
}

typedef struct {
    FklVMvalue *v;
    size_t w;
} VMvalueHashItem;

static uintptr_t _VMvalue_hashFunc(const void *key) {
    const FklVMvalue *v = *(const FklVMvalue **)key;
    return (uintptr_t)v >> 3;
}

static void _VMvalue_setVal(void *d0, const void *d1) {
    *(VMvalueHashItem *)d0 = *(const VMvalueHashItem *)d1;
}

static FklHashTableMetaTable VMvalueHashMetaTable = {
    .size = sizeof(VMvalueHashItem),
    .__setKey = fklPtrKeySet,
    .__setVal = _VMvalue_setVal,
    .__hashFunc = _VMvalue_hashFunc,
    .__uninitItem = fklDoNothingUninitHashItem,
    .__keyEqual = fklPtrKeyEqual,
    .__getKey = fklHashDefaultGetKey,
};

FklHashTable *fklCreateValueSetHashtable(void) {
    return fklCreateHashTable(&VMvalueHashMetaTable);
}

void fklInitValueSetHashTable(FklHashTable *ht) {
    fklInitHashTable(ht, &VMvalueHashMetaTable);
}

static inline void putValueInSet(FklHashTable *s, FklVMvalue *v) {
    uint32_t num = s->num;
    VMvalueHashItem *h = fklPutHashItem(&v, s);
    h->w = num;
}

static inline int isInValueSet(FklVMvalue *v, FklHashTable *t, size_t *w) {
    VMvalueHashItem *item = fklGetHashItem(&v, t);
    if (item && w)
        *w = item->w;
    return item != NULL;
}

struct VMvalueDegreeHashItem {
    FklVMvalue *v;
    uint64_t degree;
};

static void _VMvalue_degree_setVal(void *d0, const void *d1) {
    *(struct VMvalueDegreeHashItem *)d0 =
        *(const struct VMvalueDegreeHashItem *)d1;
}

static FklHashTableMetaTable VMvalueDegreeHashMetaTable = {
    .size = sizeof(struct VMvalueDegreeHashItem),
    .__setKey = fklPtrKeySet,
    .__setVal = _VMvalue_degree_setVal,
    .__hashFunc = _VMvalue_hashFunc,
    .__uninitItem = fklDoNothingUninitHashItem,
    .__keyEqual = fklPtrKeyEqual,
    .__getKey = fklHashDefaultGetKey,
};

static inline void init_vmvalue_degree_hash_table(FklHashTable *ht) {
    fklInitHashTable(ht, &VMvalueDegreeHashMetaTable);
}

static inline void inc_value_degree(FklHashTable *ht, FklVMvalue *v) {
    struct VMvalueDegreeHashItem *degree_item = fklPutHashItem(&v, ht);
    degree_item->degree++;
}

static inline void dec_value_degree(FklHashTable *ht, FklVMvalue *v) {
    struct VMvalueDegreeHashItem *degree_item = fklGetHashItem(&v, ht);
    if (degree_item && degree_item->degree)
        degree_item->degree--;
}

static inline void scan_value_and_find_value_in_circle(
    FklHashTable *ht, FklHashTable *circle_heads, FklVMvalue *first_value) {
    FklVMvalueVector stack;
    fklVMvalueVectorInit(&stack, 16);
    fklVMvalueVectorPushBack2(&stack, first_value);
    while (!fklVMvalueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklVMvalueVectorPopBack(&stack);
        if (FKL_IS_PAIR(v)) {
            inc_value_degree(ht, v);
            if (!isInValueSet(v, circle_heads, NULL)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CAR(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_VECTOR(v)) {
            inc_value_degree(ht, v);
            if (!isInValueSet(v, circle_heads, NULL)) {
                FklVMvec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklVMvalueVectorPushBack2(&stack, vec->base[i - 1]);
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_BOX(v)) {
            inc_value_degree(ht, v);
            if (!isInValueSet(v, circle_heads, NULL)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_BOX(v));
                putValueInSet(circle_heads, v);
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            inc_value_degree(ht, v);
            if (!isInValueSet(v, circle_heads, NULL)) {
                for (FklHashTableItem *tail = FKL_VM_HASH(v)->last; tail;
                     tail = tail->prev) {
                    FklVMhashTableItem *item = (FklVMhashTableItem *)tail->data;
                    fklVMvalueVectorPushBack2(&stack, item->v);
                    fklVMvalueVectorPushBack2(&stack, item->key);
                }
                putValueInSet(circle_heads, v);
            }
        }
    }
    dec_value_degree(ht, first_value);

    // remove value not in circle

    do {
        stack.top = 0;
        for (FklHashTableItem *list = ht->first; list; list = list->next) {
            struct VMvalueDegreeHashItem *item =
                (struct VMvalueDegreeHashItem *)list->data;
            if (!item->degree)
                fklVMvalueVectorPushBack2(&stack, item->v);
        }
        FklVMvalue **base = (FklVMvalue **)stack.base;
        FklVMvalue **const end = &base[stack.top];
        for (; base < end; base++) {
            fklDelHashItem(base, ht, NULL);
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
                for (FklHashTableItem *list = FKL_VM_HASH(v)->first; list;
                     list = list->next) {
                    FklVMhashTableItem *item = (FklVMhashTableItem *)list->data;
                    dec_value_degree(ht, item->key);
                    dec_value_degree(ht, item->v);
                }
            }
        }
    } while (!fklVMvalueVectorIsEmpty(&stack));

    // get all circle heads

    fklClearHashTable(circle_heads);
    while (ht->num) {
        struct VMvalueDegreeHashItem *value_degree =
            ((struct VMvalueDegreeHashItem *)ht->first->data);
        FklVMvalue *v = value_degree->v;
        putValueInSet(circle_heads, v);
        value_degree->degree = 0;

        do {
            stack.top = 0;
            for (FklHashTableItem *list = ht->first; list; list = list->next) {
                struct VMvalueDegreeHashItem *item =
                    (struct VMvalueDegreeHashItem *)list->data;
                if (!item->degree)
                    fklVMvalueVectorPushBack2(&stack, item->v);
            }
            FklVMvalue **base = (FklVMvalue **)stack.base;
            FklVMvalue **const end = &base[stack.top];
            for (; base < end; base++) {
                fklDelHashItem(base, ht, NULL);
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
                    for (FklHashTableItem *list = FKL_VM_HASH(v)->first; list;
                         list = list->next) {
                        FklVMhashTableItem *item =
                            (FklVMhashTableItem *)list->data;
                        dec_value_degree(ht, item->key);
                        dec_value_degree(ht, item->v);
                    }
                }
            }
        } while (!fklVMvalueVectorIsEmpty(&stack));
    }
    fklVMvalueVectorUninit(&stack);
}

static inline void scan_cir_ref(FklVMvalue *s, FklHashTable *circle_head_set) {
    FklHashTable degree_table;
    init_vmvalue_degree_hash_table(&degree_table);
    scan_value_and_find_value_in_circle(&degree_table, circle_head_set, s);
    fklUninitHashTable(&degree_table);
}

static inline int is_ptr_in_set(FklHashTable *ht, void *ptr) {
    return fklGetHashItem(&ptr, ht) != NULL;
}

static inline void put_ptr_in_set(FklHashTable *ht, void *ptr) {
    fklPutHashItem(&ptr, ht);
}

int fklHasCircleRef(const FklVMvalue *first_value) {
    if (FKL_GET_TAG(first_value) != FKL_TAG_PTR
        || (!FKL_IS_PAIR(first_value) && !FKL_IS_BOX(first_value)
            && !FKL_IS_VECTOR(first_value) && !FKL_IS_HASHTABLE(first_value)))
        return 0;
    FklHashTable value_set;
    fklInitPtrSet(&value_set);

    FklHashTable degree_table;
    init_vmvalue_degree_hash_table(&degree_table);

    FklVMvalueVector stack;
    fklVMvalueVectorInit(&stack, 16);

    fklVMvalueVectorPushBack2(&stack, first_value);
    while (!fklVMvalueVectorIsEmpty(&stack)) {
        FklVMvalue *v = *fklVMvalueVectorPopBack(&stack);
        if (FKL_IS_PAIR(v)) {
            inc_value_degree(&degree_table, v);
            if (!is_ptr_in_set(&value_set, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CDR(v));
                fklVMvalueVectorPushBack2(&stack, FKL_VM_CAR(v));
                put_ptr_in_set(&value_set, v);
            }
        } else if (FKL_IS_VECTOR(v)) {
            inc_value_degree(&degree_table, v);
            if (!is_ptr_in_set(&value_set, v)) {
                FklVMvec *vec = FKL_VM_VEC(v);
                for (size_t i = vec->size; i > 0; i--)
                    fklVMvalueVectorPushBack2(&stack, vec->base[i - 1]);
                put_ptr_in_set(&value_set, v);
            }
        } else if (FKL_IS_BOX(v)) {
            inc_value_degree(&degree_table, v);
            if (!is_ptr_in_set(&value_set, v)) {
                fklVMvalueVectorPushBack2(&stack, FKL_VM_BOX(v));
                put_ptr_in_set(&value_set, v);
            }
        } else if (FKL_IS_HASHTABLE(v)) {
            inc_value_degree(&degree_table, v);
            if (!is_ptr_in_set(&value_set, v)) {
                for (FklHashTableItem *tail = FKL_VM_HASH(v)->last; tail;
                     tail = tail->prev) {
                    FklVMhashTableItem *item = (FklVMhashTableItem *)tail->data;
                    fklVMvalueVectorPushBack2(&stack, item->v);
                    fklVMvalueVectorPushBack2(&stack, item->key);
                }
                put_ptr_in_set(&value_set, v);
            }
        }
    }
    dec_value_degree(&degree_table, first_value);
    fklUninitHashTable(&value_set);

    // remove value not in circle

    do {
        stack.top = 0;
        for (FklHashTableItem *list = degree_table.first; list;
             list = list->next) {
            struct VMvalueDegreeHashItem *item =
                (struct VMvalueDegreeHashItem *)list->data;
            if (!item->degree)
                fklVMvalueVectorPushBack2(&stack, item->v);
        }
        FklVMvalue **base = (FklVMvalue **)stack.base;
        FklVMvalue **const end = &base[stack.top];
        for (; base < end; base++) {
            fklDelHashItem(base, &degree_table, NULL);
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
                for (FklHashTableItem *list = FKL_VM_HASH(v)->first; list;
                     list = list->next) {
                    FklVMhashTableItem *item = (FklVMhashTableItem *)list->data;
                    dec_value_degree(&degree_table, item->key);
                    dec_value_degree(&degree_table, item->v);
                }
            }
        }
    } while (!fklVMvalueVectorIsEmpty(&stack));

    int r = degree_table.num > 0;
    fklUninitHashTable(&degree_table);

    fklVMvalueVectorUninit(&stack);
    return r;
}

#define VMVALUE_PRINTER_ARGS                                                   \
    FklVMvalue *v, FILE *fp, FklStringBuffer *buffer, FklVMgc *gc
static void vmvalue_f64_printer(VMVALUE_PRINTER_ARGS) {
    char buf[64] = {0};
    fklWriteDoubleToBuf(buf, 64, FKL_VM_F64(v));
    fputs(buf, fp);
}

static void vmvalue_big_int_printer(VMVALUE_PRINTER_ARGS) {
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
        fklPrintRawSymbol(fklVMgetSymbolWithId(gc, proc->sid)->symbol, fp);
        fputc('>', fp);
    } else
        fprintf(fp, "#<proc %p>", proc);
}

static void vmvalue_cproc_printer(VMVALUE_PRINTER_ARGS) {
    FklVMcproc *cproc = FKL_VM_CPROC(v);
    if (cproc->sid) {
        fprintf(fp, "#<cproc ");
        fklPrintRawSymbol(fklVMgetSymbolWithId(gc, cproc->sid)->symbol, fp);
        fputc('>', fp);
    } else
        fprintf(fp, "#<cproc %p>", cproc);
}

static void (*VMvaluePtrPrincTable[FKL_VM_VALUE_GC_TYPE_NUM])(
    VMVALUE_PRINTER_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_printer,
    [FKL_TYPE_BIG_INT] = vmvalue_big_int_printer,
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
    fklPrintString(fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->symbol, fp);
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
    fklPrintRawSymbol(fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->symbol, fp);
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
    [FKL_TYPE_BIG_INT] = vmvalue_big_int_printer,
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

void fklPrin1VMvalue(FklVMvalue *v, FILE *fp, FklVMgc *gc) {
    fklPrintVMvalue(v, fp, prin1VMatom, gc);
}

void fklPrincVMvalue(FklVMvalue *v, FILE *fp, FklVMgc *gc) {
    fklPrintVMvalue(v, fp, princVMatom, gc);
}

typedef struct {
    PrtElem *key;
    PrtElem *v;
} HashPrtElem;

static HashPrtElem *createHashPrtElem(PrtElem *key, PrtElem *v) {
    HashPrtElem *r = (HashPrtElem *)malloc(sizeof(HashPrtElem));
    FKL_ASSERT(r);
    r->key = key;
    r->v = v;
    return r;
}

void fklPrintVMvalue(FklVMvalue *value, FILE *fp,
                     void (*atomPrinter)(VMVALUE_PRINTER_ARGS), FklVMgc *gc) {
    FklStringBuffer string_buffer;
    fklInitStringBuffer(&string_buffer);

    if (!FKL_IS_VECTOR(value) && !FKL_IS_PAIR(value) && !FKL_IS_BOX(value)
        && !FKL_IS_HASHTABLE(value)) {
        atomPrinter(value, fp, &string_buffer, gc);
        fklUninitStringBuffer(&string_buffer);
        return;
    }

    FklHashTable circel_head_set;
    fklInitValueSetHashTable(&circel_head_set);

    FklHashTable has_print_circle_head_set;
    fklInitPtrSet(&has_print_circle_head_set);

    scan_cir_ref(value, &circel_head_set);
    FklPtrQueue *queue = fklCreatePtrQueue();
    FklQueueVector queueStack;
    fklQueueVectorInit(&queueStack, 32);
    fklPushPtrQueue(createPrtElem(PRT_CAR, value), queue);
    fklQueueVectorPushBack2(&queueStack, queue);
    while (!fklQueueVectorIsEmpty(&queueStack)) {
        FklPtrQueue *cQueue = *fklQueueVectorBack(&queueStack);
        while (fklLengthPtrQueue(cQueue)) {
            PrtElem *e = fklPopPtrQueue(cQueue);
            FklVMvalue *v = e->v;
            if (e->state == PRT_CDR || e->state == PRT_REC_CDR)
                fputc(',', fp);
            if (e->state == PRT_REC_CAR || e->state == PRT_REC_CDR
                || e->state == PRT_REC_BOX) {
                fprintf(fp, "#%" FKL_PRT64U "#", (uintptr_t)e->v);
                free(e);
            } else if (e->state == PRT_HASH_ITEM) {
                fputc('(', fp);
                FklPtrQueue *iQueue = fklCreatePtrQueue();
                HashPrtElem *elem = (void *)e->v;
                fklPushPtrQueue(elem->key, iQueue);
                fklPushPtrQueue(elem->v, iQueue);
                fklQueueVectorPushBack2(&queueStack, iQueue);
                cQueue = iQueue;
                free(elem);
                free(e);
                continue;
            } else {
                free(e);
                if (!FKL_IS_VECTOR(v) && !FKL_IS_PAIR(v) && !FKL_IS_BOX(v)
                    && !FKL_IS_HASHTABLE(v))
                    atomPrinter(v, fp, &string_buffer, gc);
                else {
                    size_t i = 0;
                    if (isInValueSet(v, &circel_head_set, &i))
                        fprintf(fp, "#%" FKL_PRT64U "=", i);
                    if (FKL_IS_VECTOR(v)) {
                        fputs("#(", fp);
                        FklPtrQueue *vQueue = fklCreatePtrQueue();
                        FklVMvec *vec = FKL_VM_VEC(v);
                        for (size_t i = 0; i < vec->size; i++) {
                            size_t w = 0;
                            int is_in_rec_set = isInValueSet(
                                vec->base[i], &circel_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  vec->base[i]))
                                || vec->base[i] == v)
                                fklPushPtrQueue(
                                    createPrtElem(PRT_REC_CAR, (void *)w),
                                    vQueue);
                            else {
                                fklPushPtrQueue(
                                    createPrtElem(PRT_CAR, vec->base[i]),
                                    vQueue);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   vec->base[i]);
                            }
                        }
                        fklQueueVectorPushBack2(&queueStack, vQueue);
                        cQueue = vQueue;
                        continue;
                    } else if (FKL_IS_BOX(v)) {
                        fputs("#&", fp);
                        size_t w = 0;
                        FklVMvalue *box = FKL_VM_BOX(v);
                        int is_in_rec_set =
                            isInValueSet(box, &circel_head_set, &w);
                        if ((is_in_rec_set
                             && is_ptr_in_set(&has_print_circle_head_set, box))
                            || box == v)
                            fklPushPtrQueueToFront(
                                createPrtElem(PRT_REC_BOX, (void *)w), cQueue);
                        else {
                            fklPushPtrQueueToFront(createPrtElem(PRT_BOX, box),
                                                   cQueue);
                            if (is_in_rec_set)
                                put_ptr_in_set(&has_print_circle_head_set, box);
                        }
                        continue;
                    } else if (FKL_IS_HASHTABLE(v)) {
                        FklHashTable *hash = FKL_VM_HASH(v);
                        fputs(fklGetVMhashTablePrefix(hash), fp);
                        FklPtrQueue *hQueue = fklCreatePtrQueue();
                        for (FklHashTableItem *list = hash->first; list;
                             list = list->next) {
                            FklVMhashTableItem *item =
                                (FklVMhashTableItem *)list->data;
                            PrtElem *keyElem = NULL;
                            PrtElem *vElem = NULL;
                            size_t w = 0;
                            int is_in_rec_set =
                                isInValueSet(item->key, &circel_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  item->key))
                                || item->key == v)
                                keyElem = createPrtElem(PRT_REC_CAR, (void *)w);
                            else {
                                keyElem = createPrtElem(PRT_CAR, item->key);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   item->key);
                            }
                            is_in_rec_set =
                                isInValueSet(item->v, &circel_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  item->key))
                                || item->v == v)
                                vElem = createPrtElem(PRT_REC_CDR, (void *)w);
                            else {
                                vElem = createPrtElem(PRT_CDR, item->v);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   item->v);
                            }
                            fklPushPtrQueue(
                                createPrtElem(
                                    PRT_HASH_ITEM,
                                    (void *)createHashPrtElem(keyElem, vElem)),
                                hQueue);
                        }
                        fklQueueVectorPushBack2(&queueStack, hQueue);
                        cQueue = hQueue;
                        continue;
                    } else {
                        fputc('(', fp);
                        FklPtrQueue *lQueue = fklCreatePtrQueue();
                        FklVMvalue *car = FKL_VM_CAR(v);
                        FklVMvalue *cdr = FKL_VM_CDR(v);
                        for (;;) {
                            PrtElem *ce = NULL;
                            size_t w = 0;
                            int is_in_rec_set =
                                isInValueSet(car, &circel_head_set, &w);
                            if (is_in_rec_set
                                && (is_ptr_in_set(&has_print_circle_head_set,
                                                  car)
                                    || car == v))
                                ce = createPrtElem(PRT_REC_CAR, (void *)w);
                            else {
                                ce = createPrtElem(PRT_CAR, car);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   car);
                            }
                            fklPushPtrQueue(ce, lQueue);
                            if (isInValueSet(cdr, &circel_head_set, &w)) {
                                PrtElem *cdre = NULL;
                                if (cdr != v
                                    && !is_ptr_in_set(
                                        &has_print_circle_head_set, cdr)) {
                                    cdre = createPrtElem(PRT_CDR, cdr);
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   cdr);
                                } else {
                                    cdre =
                                        createPrtElem(PRT_REC_CDR, (void *)w);
                                }
                                fklPushPtrQueue(cdre, lQueue);
                                break;
                            }
                            if (FKL_IS_PAIR(cdr)) {
                                car = FKL_VM_CAR(cdr);
                                cdr = FKL_VM_CDR(cdr);
                            } else {
                                if (cdr != FKL_VM_NIL)
                                    fklPushPtrQueue(createPrtElem(PRT_CDR, cdr),
                                                    lQueue);
                                break;
                            }
                        }
                        fklQueueVectorPushBack2(&queueStack, lQueue);
                        cQueue = lQueue;
                        continue;
                    }
                }
            }
            if (fklLengthPtrQueue(cQueue)
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_BOX
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_BOX)
                fputc(' ', fp);
        }
        fklQueueVectorPopBack(&queueStack);
        fklDestroyPtrQueue(cQueue);
        if (!fklQueueVectorIsEmpty(&queueStack)) {
            fputc(')', fp);
            cQueue = *fklQueueVectorBack(&queueStack);
            if (fklLengthPtrQueue(cQueue)
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_BOX
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_BOX)
                fputc(' ', fp);
        }
    }
    fklQueueVectorUninit(&queueStack);
    fklUninitHashTable(&circel_head_set);
    fklUninitHashTable(&has_print_circle_head_set);
    fklUninitStringBuffer(&string_buffer);
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

static inline void print_big_int_to_string_buffer(FklStringBuffer *s,
                                                  const FklVMbigInt *a) {
    if (a->num == 0)
        fklStringBufferPutc(s, '0');
    else {
        const FklBigInt bi = fklVMbigIntToBigInt(a);
        char *str = fklBigIntToCstr(&bi, 10, FKL_BIGINT_FMT_FLAG_NONE);
        fklStringBufferConcatWithCstr(s, str);
        free(str);
    }
}

#define VMVALUE_TO_UTSTRING_ARGS                                               \
    FklStringBuffer *result, FklVMvalue *v, FklVMgc *gc

static void nil_ptr_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferConcatWithCstr(result, "()");
}

static void fix_ptr_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    fklStringBufferPrintf(result, "%" FKL_PRT64D "", FKL_GET_FIX(v));
}

static void sym_ptr_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    print_raw_symbol_to_string_buffer(
        result, fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->symbol);
}

static void chr_ptr_as_prin1(VMVALUE_TO_UTSTRING_ARGS) {
    print_raw_char_to_string_buffer(result, FKL_GET_CHR(v));
}

static void vmvalue_f64_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    char buf[64] = {0};
    fklWriteDoubleToBuf(buf, 64, FKL_VM_F64(v));
    fklStringBufferConcatWithCstr(result, buf);
}

static void vmvalue_big_int_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    print_big_int_to_string_buffer(result, FKL_VM_BI(v));
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
            result, fklVMgetSymbolWithId(gc, proc->sid)->symbol);
        fklStringBufferPutc(result, '>');
    } else
        fklStringBufferPrintf(result, "#<proc %p>", proc);
}

static void vmvalue_cproc_as_print(VMVALUE_TO_UTSTRING_ARGS) {
    FklVMcproc *cproc = FKL_VM_CPROC(v);
    if (cproc->sid) {
        fklStringBufferConcatWithCstr(result, "#<cproc ");
        print_raw_symbol_to_string_buffer(
            result, fklVMgetSymbolWithId(gc, cproc->sid)->symbol);
        fklStringBufferPutc(result, '>');
    } else
        fklStringBufferPrintf(result, "#<cproc %p>", cproc);
}

static void (
    *atom_ptr_ptr_to_string_buffer_prin1_table[FKL_VM_VALUE_GC_TYPE_NUM])(
    VMVALUE_TO_UTSTRING_ARGS) = {
    [FKL_TYPE_F64] = vmvalue_f64_as_print,
    [FKL_TYPE_BIG_INT] = vmvalue_big_int_as_print,
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
    [FKL_TYPE_BIG_INT] = vmvalue_big_int_as_print,
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
        result, fklVMgetSymbolWithId(gc, FKL_GET_SYM(v))->symbol);
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

static inline void stringify_value_to_string_buffer(
    FklVMvalue *value, FklStringBuffer *result,
    void (*atom_stringifier)(VMVALUE_TO_UTSTRING_ARGS), FklVMgc *gc) {
    if (!FKL_IS_VECTOR(value) && !FKL_IS_PAIR(value) && !FKL_IS_BOX(value)
        && !FKL_IS_HASHTABLE(value)) {
        atom_stringifier(result, value, gc);
        return;
    }

    FklHashTable circle_head_set;
    fklInitValueSetHashTable(&circle_head_set);

    FklHashTable has_print_circle_head_set;
    fklInitPtrSet(&has_print_circle_head_set);

    scan_cir_ref(value, &circle_head_set);
    FklPtrQueue *queue = fklCreatePtrQueue();
    FklQueueVector queueStack;
    fklQueueVectorInit(&queueStack, 32);
    fklPushPtrQueue(createPrtElem(PRT_CAR, value), queue);
    fklQueueVectorPushBack2(&queueStack, queue);
    while (!fklQueueVectorIsEmpty(&queueStack)) {
        FklPtrQueue *cQueue = *fklQueueVectorBack(&queueStack);
        while (fklLengthPtrQueue(cQueue)) {
            PrtElem *e = fklPopPtrQueue(cQueue);
            FklVMvalue *v = e->v;
            if (e->state == PRT_CDR || e->state == PRT_REC_CDR)
                fklStringBufferPutc(result, ',');
            if (e->state == PRT_REC_CAR || e->state == PRT_REC_CDR
                || e->state == PRT_REC_BOX) {
                fklStringBufferPrintf(result, "#%" FKL_PRT64U "#",
                                      (uintptr_t)e->v);
                free(e);
            } else if (e->state == PRT_HASH_ITEM) {
                fklStringBufferPutc(result, '(');
                FklPtrQueue *iQueue = fklCreatePtrQueue();
                HashPrtElem *elem = (void *)e->v;
                fklPushPtrQueue(elem->key, iQueue);
                fklPushPtrQueue(elem->v, iQueue);
                fklQueueVectorPushBack2(&queueStack, iQueue);
                cQueue = iQueue;
                free(elem);
                free(e);
                continue;
            } else {
                free(e);
                if (!FKL_IS_VECTOR(v) && !FKL_IS_PAIR(v) && !FKL_IS_BOX(v)
                    && !FKL_IS_HASHTABLE(v))
                    atom_stringifier(result, v, gc);
                else {
                    size_t i = 0;
                    if (isInValueSet(v, &circle_head_set, &i))
                        fklStringBufferPrintf(result, "#%" FKL_PRT64U "=", i);
                    if (FKL_IS_VECTOR(v)) {
                        fklStringBufferConcatWithCstr(result, "#(");
                        FklPtrQueue *vQueue = fklCreatePtrQueue();
                        FklVMvec *vec = FKL_VM_VEC(v);
                        for (size_t i = 0; i < vec->size; i++) {
                            size_t w = 0;
                            int is_in_rec_set = isInValueSet(
                                vec->base[i], &circle_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  vec->base[i]))
                                || vec->base[i] == v)
                                fklPushPtrQueue(
                                    createPrtElem(PRT_REC_CAR, (void *)w),
                                    vQueue);
                            else {
                                fklPushPtrQueue(
                                    createPrtElem(PRT_CAR, vec->base[i]),
                                    vQueue);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   vec->base[i]);
                            }
                        }
                        fklQueueVectorPushBack2(&queueStack, vQueue);
                        cQueue = vQueue;
                        continue;
                    } else if (FKL_IS_BOX(v)) {
                        fklStringBufferConcatWithCstr(result, "#&");
                        size_t w = 0;
                        FklVMvalue *box = FKL_VM_BOX(v);
                        int is_in_rec_set =
                            isInValueSet(box, &circle_head_set, &w);
                        if ((is_in_rec_set
                             && is_ptr_in_set(&has_print_circle_head_set, box))
                            || box == v)
                            fklPushPtrQueueToFront(
                                createPrtElem(PRT_REC_BOX, (void *)w), cQueue);
                        else {
                            fklPushPtrQueueToFront(createPrtElem(PRT_BOX, box),
                                                   cQueue);
                            if (is_in_rec_set)
                                put_ptr_in_set(&has_print_circle_head_set, box);
                        }
                        continue;
                    } else if (FKL_IS_HASHTABLE(v)) {
                        FklHashTable *hash = FKL_VM_HASH(v);
                        fklStringBufferConcatWithCstr(
                            result, fklGetVMhashTablePrefix(hash));
                        FklPtrQueue *hQueue = fklCreatePtrQueue();
                        for (FklHashTableItem *list = hash->first; list;
                             list = list->next) {
                            FklVMhashTableItem *item =
                                (FklVMhashTableItem *)list->data;
                            PrtElem *keyElem = NULL;
                            PrtElem *vElem = NULL;
                            size_t w = 0;
                            int is_in_rec_set =
                                isInValueSet(item->key, &circle_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  item->key))
                                || item->key == v)
                                keyElem = createPrtElem(PRT_REC_CAR, (void *)w);
                            else {
                                keyElem = createPrtElem(PRT_CAR, item->key);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   item->key);
                            }
                            is_in_rec_set =
                                isInValueSet(item->v, &circle_head_set, &w);
                            if ((is_in_rec_set
                                 && is_ptr_in_set(&has_print_circle_head_set,
                                                  item->key))
                                || item->v == v)
                                vElem = createPrtElem(PRT_REC_CDR, (void *)w);
                            else {
                                vElem = createPrtElem(PRT_CDR, item->v);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   item->v);
                            }
                            fklPushPtrQueue(
                                createPrtElem(
                                    PRT_HASH_ITEM,
                                    (void *)createHashPrtElem(keyElem, vElem)),
                                hQueue);
                        }
                        fklQueueVectorPushBack2(&queueStack, hQueue);
                        cQueue = hQueue;
                        continue;
                    } else {
                        fklStringBufferPutc(result, '(');
                        FklPtrQueue *lQueue = fklCreatePtrQueue();
                        FklVMvalue *car = FKL_VM_CAR(v);
                        FklVMvalue *cdr = FKL_VM_CDR(v);
                        for (;;) {
                            PrtElem *ce = NULL;
                            size_t w = 0;
                            int is_in_rec_set =
                                isInValueSet(car, &circle_head_set, &w);
                            if (is_in_rec_set
                                && (is_ptr_in_set(&has_print_circle_head_set,
                                                  car)
                                    || car == v))
                                ce = createPrtElem(PRT_REC_CAR, (void *)w);
                            else {
                                ce = createPrtElem(PRT_CAR, car);
                                if (is_in_rec_set)
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   car);
                            }
                            fklPushPtrQueue(ce, lQueue);
                            if (isInValueSet(cdr, &circle_head_set, &w)) {
                                PrtElem *cdre = NULL;
                                if (cdr != v
                                    && !is_ptr_in_set(
                                        &has_print_circle_head_set, cdr)) {
                                    cdre = createPrtElem(PRT_CDR, cdr);
                                    put_ptr_in_set(&has_print_circle_head_set,
                                                   cdr);
                                } else {
                                    cdre =
                                        createPrtElem(PRT_REC_CDR, (void *)w);
                                }
                                fklPushPtrQueue(cdre, lQueue);
                                break;
                            }
                            if (FKL_IS_PAIR(cdr)) {
                                car = FKL_VM_CAR(cdr);
                                cdr = FKL_VM_CDR(cdr);
                            } else {
                                if (cdr != FKL_VM_NIL)
                                    fklPushPtrQueue(createPrtElem(PRT_CDR, cdr),
                                                    lQueue);
                                break;
                            }
                        }
                        fklQueueVectorPushBack2(&queueStack, lQueue);
                        cQueue = lQueue;
                        continue;
                    }
                }
            }
            if (fklLengthPtrQueue(cQueue)
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_BOX
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_BOX)
                fklStringBufferPutc(result, ' ');
        }
        fklQueueVectorPopBack(&queueStack);
        fklDestroyPtrQueue(cQueue);
        if (!fklQueueVectorIsEmpty(&queueStack)) {
            fklStringBufferPutc(result, ')');
            cQueue = *fklQueueVectorBack(&queueStack);
            if (fklLengthPtrQueue(cQueue)
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_CDR
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_BOX
                && ((PrtElem *)fklFirstPtrQueue(cQueue))->state != PRT_REC_BOX)
                fklStringBufferPutc(result, ' ');
        }
    }
    fklQueueVectorUninit(&queueStack);
    fklUninitHashTable(&circle_head_set);
    fklUninitHashTable(&has_print_circle_head_set);
}

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

FklVMvalue *fklGetTopValue(FklVM *exe) { return exe->base[exe->tp - 1]; }

FklVMvalue *fklGetValue(FklVM *exe, uint32_t i) {
    return exe->base[exe->tp - i];
}

FklVMvalue **fklGetStackSlot(FklVM *exe, uint32_t i) {
    return &exe->base[exe->tp - i];
}

size_t fklVMlistLength(FklVMvalue *v) {
    size_t len = 0;
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v))
        len++;
    return len;
}

#define IS_DEFAULT_BIGINT(bi) (bi->digits == NULL)
#define INIT_DEFAULT_BIG_INT_1(bi)                                             \
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
    } else if (FKL_IS_BIG_INT(cur))
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
            INIT_DEFAULT_BIG_INT_1(bi);
            fklMulBigIntI(bi, c64);
        } else
            *pr64 *= c64;
    } else if (FKL_IS_BIG_INT(cur)) {
        INIT_DEFAULT_BIG_INT_1(bi);
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
            INIT_DEFAULT_BIG_INT_1(bi);
            fklMulBigIntI(bi, c64);
        } else
            *pr64 *= c64;
    } else if (FKL_IS_BIG_INT(cur)) {
        INIT_DEFAULT_BIG_INT_1(bi);
        fklMulVMbigInt(bi, FKL_VM_BI(cur));
    } else {
        fklUninitBigInt(bi);
        return 1;
    }
    return 0;
}

FklVMvalue *fklProcessVMnumInc(FklVM *exe, FklVMvalue *arg) {
    if (FKL_IS_FIX(arg)) {
        int64_t i = FKL_GET_FIX(arg) + 1;
        if (i > FKL_FIX_INT_MAX)
            return fklCreateVMvalueBigIntWithI64(exe, i);
        else
            return FKL_MAKE_VM_FIX(i);
    } else if (FKL_IS_BIG_INT(arg)) {
        const FklVMbigInt *bigint = FKL_VM_BI(arg);
        if (fklIsVMbigIntAdd1InFixIntRange(bigint))
            return FKL_MAKE_VM_FIX(fklVMbigIntToI(bigint) + 1);
        else
            return fklVMbigIntAddI(exe, bigint, 1);
    } else if (FKL_IS_F64(arg))
        return fklCreateVMvalueF64(exe, FKL_VM_F64(arg) + 1.0);
    return NULL;
}

FklVMvalue *fklProcessVMnumDec(FklVM *exe, FklVMvalue *arg) {
    if (FKL_IS_FIX(arg)) {
        int64_t i = FKL_GET_FIX(arg) - 1;
        if (i < FKL_FIX_INT_MIN)
            return fklCreateVMvalueBigIntWithI64(exe, i);
        else
            return FKL_MAKE_VM_FIX(i);
    } else if (FKL_IS_BIG_INT(arg)) {
        const FklVMbigInt *bigint = FKL_VM_BI(arg);
        if (fklIsVMbigIntSub1InFixIntRange(bigint))
            return FKL_MAKE_VM_FIX(fklVMbigIntToI(bigint) - 1);
        else
            return fklVMbigIntSubI(exe, bigint, 1);
    } else if (FKL_IS_F64(arg))
        return fklCreateVMvalueF64(exe, FKL_VM_F64(arg) - 1.0);
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
    else if (FKL_IS_BIG_INT(prev) || !FKL_BIGINT_IS_0(bi)) {
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
        INIT_DEFAULT_BIG_INT_1(bi);
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
    if (FKL_IS_BIG_INT(prev)
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
        || (FKL_IS_BIG_INT(prev)
            && ((!IS_DEFAULT_BIGINT(bi) && !FKL_BIGINT_IS_1(bi)
                 && !fklIsVMbigIntDivisible(FKL_VM_BI(prev), bi))
                || !fklIsVMbigIntDivisibleI(FKL_VM_BI(prev), r64)))) {
        if (IS_DEFAULT_BIGINT(bi))
            r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) / rd / r64);
        else
            r = fklCreateVMvalueF64(exe, fklVMgetDouble(prev) / rd / r64
                                             / fklBigIntToD(bi));
    } else if (FKL_IS_BIG_INT(prev)
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
        if (FKL_IS_BIG_INT(fir))
            fklSetBigIntWithVMbigInt(&rem, FKL_VM_BI(fir));
        else
            fklSetBigIntI(&rem, fklVMgetInt(fir));
        if (FKL_IS_BIG_INT(sec)) {
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
        errorTypeId[i] = fklAddSymbolCstr(builtInErrorType[i], table)->id;
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

static inline uint64_t format_big_int(FklStringBuffer *buf,
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
    void (*outs)(void *, const char *, size_t len), void *arg, uint64_t *plen) {
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
            FklVMvalue *width_obj = FKL_VM_POP_ARG(exe);
            if (width_obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else if (FKL_IS_FIX(width_obj)) {
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
                FklVMvalue *prec_obj = FKL_VM_POP_ARG(exe);
                if (prec_obj == NULL) {
                    err = FKL_ERR_TOOFEWARG;
                    goto exit;
                } else if (FKL_IS_FIX(prec_obj)) {
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
            FklVMvalue *integer_obj = FKL_VM_POP_ARG(exe);
            if (integer_obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else if (fklIsVMint(integer_obj)) {
                if (FKL_IS_FIX(integer_obj))
                    length +=
                        format_fix_int(FKL_GET_FIX(integer_obj), flags, base,
                                       width, precision, outc, (void *)arg);
                else
                    length += format_big_int(&buf, FKL_VM_BI(integer_obj),
                                             flags, base, width, precision,
                                             outc, (void *)arg);
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
            FklVMvalue *f64_obj = FKL_VM_POP_ARG(exe);
            if (f64_obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else if (FKL_IS_F64(f64_obj))
                length += format_f64(&buf, *fmt, FKL_VM_F64(f64_obj), flags,
                                     width, precision, outc, (void *)arg);
            else {
                err = FKL_ERR_INCORRECT_TYPE_VALUE;
                goto exit;
            }
        } break;
        case 'c': {
            FklVMvalue *chr_obj = FKL_VM_POP_ARG(exe);
            if (chr_obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else if (FKL_IS_CHR(chr_obj)) {
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
            FklVMvalue *obj = FKL_VM_POP_ARG(exe);
            if (obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else {
                stringify_value_to_string_buffer(obj, &buf,
                                                 atom_as_prin1_string, exe->gc);

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
            }
        } break;
        case 's': {
            FklVMvalue *obj = FKL_VM_POP_ARG(exe);
            if (obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                goto exit;
            } else {
                stringify_value_to_string_buffer(obj, &buf,
                                                 atom_as_princ_string, exe->gc);

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
            }
        } break;
        case 'n': {
            FklVMvalue *box_obj = FKL_VM_POP_ARG(exe);
            if (box_obj == NULL) {
                err = FKL_ERR_TOOFEWARG;
                ;
                goto exit;
            } else if (FKL_IS_BOX(box_obj)) {
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

    *plen = length;
exit:
    fklUninitStringBuffer(&buf);
    return err;
}

static void print_out_str_buf(void *arg, const char *buf, size_t len) {
    FILE *fp = arg;
    fwrite(buf, len, 1, fp);
}

FklBuiltinErrorType fklVMprintf(FklVM *exe, FILE *fp, const FklString *fmt_str,
                                uint64_t *plen) {
    return vm_format_to_buf(exe, fmt_str->str, &fmt_str->str[fmt_str->size],
                            print_out_char, print_out_str_buf, (void *)fp,
                            plen);
}

static void format_out_str_buf(void *arg, const char *buf, size_t len) {
    FklStringBuffer *result = arg;
    fklStringBufferBincpy(result, buf, len);
}

FklBuiltinErrorType fklVMformat(FklVM *exe, FklStringBuffer *result,
                                const FklString *fmt_str, uint64_t *plen) {
    return vm_format_to_buf(exe, fmt_str->str, &fmt_str->str[fmt_str->size],
                            format_out_char, format_out_str_buf, (void *)result,
                            plen);
}

FklBuiltinErrorType fklVMformat2(FklVM *exe, FklStringBuffer *result,
                                 const char *fmt, const char *end,
                                 uint64_t *plen) {
    return vm_format_to_buf(exe, fmt, end, format_out_char, format_out_str_buf,
                            (void *)result, plen);
}

static inline void vformat(FklVM *exe, FklStringBuffer *arg, const char *fmt,
                           const char *end, FklVMvalue **cur_val,
                           FklVMvalue **const end_val) {
#define GET_NEXT_VAL() (cur_val < end_val ? *(cur_val++) : NULL)
    uint32_t base;
    uint32_t flags;
    uint64_t width;
    uint64_t precision;

    FklStringBuffer buf;
    fklInitStringBuffer(&buf);

    uint64_t length = 0;
    while (fmt < end) {
        if (*fmt != '%') {
            fklStringBufferPutc(arg, *fmt);
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
            FklVMvalue *width_obj = GET_NEXT_VAL();
            if (width_obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else if (FKL_IS_FIX(width_obj)) {
                int64_t w = FKL_GET_FIX(width_obj);
                if (w < 0) {
                    flags |= FLAGS_LEFT;
                    width = -w;
                } else
                    width = w;
            } else {
                fklStringBufferConcatWithCstr(arg, "#<err>");
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
                FklVMvalue *prec_obj = GET_NEXT_VAL();
                if (prec_obj == NULL) {
                    fklStringBufferConcatWithCstr(arg, "#<err>");
                    goto exit;
                } else if (FKL_IS_FIX(prec_obj)) {
                    int64_t prec = FKL_GET_FIX(prec_obj);
                    if (prec < 0)

                    {
                        fklStringBufferConcatWithCstr(arg, "#<err>");
                        goto exit;
                    } else
                        precision = prec;
                } else {
                    fklStringBufferConcatWithCstr(arg, "#<err>");
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
            FklVMvalue *integer_obj = GET_NEXT_VAL();
            if (integer_obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else if (fklIsVMint(integer_obj)) {
                if (FKL_IS_FIX(integer_obj))
                    length += format_fix_int(FKL_GET_FIX(integer_obj), flags,
                                             base, width, precision,
                                             format_out_char, (void *)arg);
                else
                    length += format_big_int(&buf, FKL_VM_BI(integer_obj),
                                             flags, base, width, precision,
                                             format_out_char, (void *)arg);
            } else {
                fklStringBufferConcatWithCstr(arg, "#<err>");
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
            FklVMvalue *f64_obj = GET_NEXT_VAL();
            if (f64_obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else if (FKL_IS_F64(f64_obj))
                length +=
                    format_f64(&buf, *fmt, FKL_VM_F64(f64_obj), flags, width,
                               precision, format_out_char, (void *)arg);
            else {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            }
        } break;
        case 'c': {
            FklVMvalue *chr_obj = GET_NEXT_VAL();
            if (chr_obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else if (FKL_IS_CHR(chr_obj)) {
                int ch = FKL_GET_CHR(chr_obj);
                uint64_t len = 1;

                if (!(flags & FLAGS_LEFT))
                    for (; len < width; len++)
                        format_out_char(arg, ' ');
                format_out_char(arg, ch);

                if (flags & FLAGS_LEFT)
                    for (; len < width; len++)
                        format_out_char(arg, ' ');
                length += len;
            } else {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            }
        } break;
        case 'S': {
            FklVMvalue *obj = GET_NEXT_VAL();
            if (obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else {
                stringify_value_to_string_buffer(obj, &buf,
                                                 atom_as_prin1_string, exe->gc);

                uint64_t len = buf.index;

                if (!(flags & FLAGS_LEFT))
                    for (; len < width; len++)
                        format_out_char(arg, ' ');

                format_out_str_buf(arg, buf.buf, buf.index);

                if (flags & FLAGS_LEFT)
                    for (; len < width; len++)
                        format_out_char(arg, ' ');

                buf.index = 0;

                length += len;
            }
        } break;
        case 's': {
            FklVMvalue *obj = GET_NEXT_VAL();
            if (obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else {
                stringify_value_to_string_buffer(obj, &buf,
                                                 atom_as_princ_string, exe->gc);

                uint64_t len = buf.index;

                if (!(flags & FLAGS_LEFT))
                    for (; len < width; len++)
                        format_out_char(arg, ' ');

                format_out_str_buf(arg, buf.buf, buf.index);

                if (flags & FLAGS_LEFT)
                    for (; len < width; len++)
                        format_out_char(arg, ' ');

                buf.index = 0;

                length += len;
            }
        } break;
        case 'n': {
            FklVMvalue *box_obj = GET_NEXT_VAL();
            if (box_obj == NULL) {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            } else if (FKL_IS_BOX(box_obj)) {
                FklVMvalue *len_obj = fklMakeVMuint(length, exe);
                FKL_VM_BOX(box_obj) = len_obj;
            } else {
                fklStringBufferConcatWithCstr(arg, "#<err>");
                goto exit;
            }
        } break;
        default:
            format_out_char(arg, *fmt);
            length++;
            break;
        }
        fmt++;
    }

exit:
    fklUninitStringBuffer(&buf);
}

void fklVMformatToBuf(FklVM *exe, FklStringBuffer *buf, const char *fmt,
                      FklVMvalue **base, size_t len) {
    vformat(exe, buf, fmt, &fmt[strlen(fmt)], base, &base[len]);
}

FklString *fklVMformatToString(FklVM *exe, const char *fmt, FklVMvalue **base,
                               size_t len) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    fklVMformatToBuf(exe, &buf, fmt, base, len);
    FklString *s = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return s;
}
