#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CONST_TABLE_SIZE (32)
#define DEFAULT_CONST_TABLE_INC (16)

static inline void init_const_i64_table(FklConstI64Table *ki64t) {
    fklKi64HashMapInit(&ki64t->ht);
    ki64t->count = 0;
    ki64t->size = DEFAULT_CONST_TABLE_SIZE;
    ki64t->base =
            (int64_t *)fklZmalloc(DEFAULT_CONST_TABLE_SIZE * sizeof(int64_t));
    FKL_ASSERT(ki64t->base);
}

struct ConstF64HashItem {
    double k;
    uint32_t idx;
};

static inline void init_const_f64_table(FklConstF64Table *kf64t) {
    fklKf64HashMapInit(&kf64t->ht);
    kf64t->count = 0;
    kf64t->size = DEFAULT_CONST_TABLE_SIZE;
    kf64t->base =
            (double *)fklZmalloc(DEFAULT_CONST_TABLE_SIZE * sizeof(double));
    FKL_ASSERT(kf64t->base);
}

static inline void init_const_str_table(FklConstStrTable *kstrt) {
    fklKstrHashMapInit(&kstrt->ht);
    kstrt->count = 0;
    kstrt->size = DEFAULT_CONST_TABLE_SIZE;
    kstrt->base = (FklString **)fklZmalloc(
            DEFAULT_CONST_TABLE_SIZE * sizeof(FklString *));
    FKL_ASSERT(kstrt->base);
}

static inline void init_const_bvec_table(FklConstBvecTable *kbvect) {
    fklKbvecHashMapInit(&kbvect->ht);
    kbvect->count = 0;
    kbvect->size = DEFAULT_CONST_TABLE_SIZE;
    kbvect->base = (FklBytevector **)fklZmalloc(
            DEFAULT_CONST_TABLE_SIZE * sizeof(FklBytevector *));
    FKL_ASSERT(kbvect->base);
}

static inline void init_const_bi_table(FklConstBiTable *kbit) {
    fklKbiHashMapInit(&kbit->ht);
    kbit->count = 0;
    kbit->size = DEFAULT_CONST_TABLE_SIZE;
    kbit->base = (FklBigInt const **)fklZmalloc(
            DEFAULT_CONST_TABLE_SIZE * sizeof(FklBigInt const *));
    FKL_ASSERT(kbit->base);
}

void fklInitConstTable(FklConstTable *kt) {
    init_const_i64_table(&kt->ki64t);
    init_const_f64_table(&kt->kf64t);
    init_const_str_table(&kt->kstrt);
    init_const_bvec_table(&kt->kbvect);
    init_const_bi_table(&kt->kbit);
}

FklConstTable *fklCreateConstTable(void) {
    FklConstTable *kt = (FklConstTable *)fklZmalloc(sizeof(FklConstTable));
    FKL_ASSERT(kt);
    fklInitConstTable(kt);
    return kt;
}

static void uninit_const_i64_table(FklConstI64Table *ki64t) {
    fklKi64HashMapUninit(&ki64t->ht);
    fklZfree(ki64t->base);
}

static void uninit_const_f64_table(FklConstF64Table *kf64t) {
    fklKf64HashMapUninit(&kf64t->ht);
    fklZfree(kf64t->base);
}

static void uninit_const_str_table(FklConstStrTable *kstrt) {
    fklKstrHashMapUninit(&kstrt->ht);
    fklZfree(kstrt->base);
}

static void uninit_const_bvec_table(FklConstBvecTable *kbvect) {
    fklKbvecHashMapUninit(&kbvect->ht);
    fklZfree(kbvect->base);
}

static void uninit_const_bi_table(FklConstBiTable *kbit) {
    fklKbiHashMapUninit(&kbit->ht);
    fklZfree(kbit->base);
}

void fklUninitConstTable(FklConstTable *kt) {
    uninit_const_i64_table(&kt->ki64t);
    uninit_const_f64_table(&kt->kf64t);
    uninit_const_str_table(&kt->kstrt);
    uninit_const_bvec_table(&kt->kbvect);
    uninit_const_bi_table(&kt->kbit);
}

void fklDestroyConstTable(FklConstTable *kt) {
    fklUninitConstTable(kt);
    fklZfree(kt);
}

#define REALLOC_CONST_TABLE_BASE(K, TYPE)                                      \
    {                                                                          \
        if ((K)->size == (K)->count) {                                         \
            (K)->size += DEFAULT_CONST_TABLE_INC;                              \
            (K)->base =                                                        \
                    (TYPE *)fklZrealloc((K)->base, (K)->size * sizeof(TYPE));  \
            FKL_ASSERT((K)->base);                                             \
        }                                                                      \
        (K)->count++;                                                          \
    }

uint32_t fklAddI64Const(FklConstTable *kt, int64_t k) {
    FklConstI64Table *ki64t = &kt->ki64t;
    uint32_t idx = ki64t->count;
    FklKi64HashMapElm *i = fklKi64HashMapInsert2(&ki64t->ht, k, idx);
    if (i->v == ki64t->count) {
        REALLOC_CONST_TABLE_BASE(ki64t, int64_t);
        ki64t->base[idx] = i->k;
    } else
        idx = i->v;
    return idx;
}

uint32_t fklAddF64Const(FklConstTable *kt, double k) {
    FklConstF64Table *kf64t = &kt->kf64t;
    uint32_t idx = kf64t->count;
    FklKf64HashMapElm *i = fklKf64HashMapInsert2(&kf64t->ht, k, idx);
    if (i->v == kf64t->count) {
        REALLOC_CONST_TABLE_BASE(kf64t, double);
        kf64t->base[idx] = i->k;
    } else
        idx = i->v;
    return idx;
}

uint32_t fklAddStrConst(FklConstTable *kt, const FklString *k) {
    FklConstStrTable *kstrt = &kt->kstrt;
    uint32_t idx = kstrt->count;
    FklKstrHashMapElm *i = fklKstrHashMapInsert2(&kstrt->ht,
            FKL_REMOVE_CONST(FklString, k),
            idx);
    if (i->v == kstrt->count) {
        REALLOC_CONST_TABLE_BASE(kstrt, FklString *);
        kstrt->base[idx] = i->k;
    } else
        idx = i->v;
    return idx;
}

uint32_t fklAddBvecConst(FklConstTable *kt, const FklBytevector *k) {
    FklConstBvecTable *kbvect = &kt->kbvect;
    uint32_t idx = kbvect->count;
    FklKbvecHashMapElm *i = fklKbvecHashMapInsert2(&kbvect->ht,
            FKL_REMOVE_CONST(FklBytevector, k),
            idx);
    if (i->v == kbvect->count) {
        REALLOC_CONST_TABLE_BASE(kbvect, FklBytevector *);
        kbvect->base[idx] = i->k;
    } else
        idx = i->v;
    return idx;
}

uint32_t fklAddBigIntConst(FklConstTable *kt, const FklBigInt *k) {
    FklConstBiTable *kbit = &kt->kbit;
    uint32_t idx = kbit->count;
    FklKbiHashMapElm *i = fklKbiHashMapInsert(&kbit->ht, k, &idx);
    if (i->v == kbit->count) {
        REALLOC_CONST_TABLE_BASE(kbit, FklBigInt const *);
        kbit->base[idx] = &i->k;
    } else
        idx = i->v;
    return idx;
}

int64_t fklGetI64ConstWithIdx(const FklConstTable *kt, uint32_t idx) {
    return kt->ki64t.base[idx];
}

double fklGetF64ConstWithIdx(const FklConstTable *kt, uint32_t idx) {
    return kt->kf64t.base[idx];
}

const FklString *fklGetStrConstWithIdx(const FklConstTable *kt, uint32_t idx) {
    return kt->kstrt.base[idx];
}

const FklBytevector *fklGetBvecConstWithIdx(const FklConstTable *kt,
        uint32_t idx) {
    return kt->kbvect.base[idx];
}

const FklBigInt *fklGetBiConstWithIdx(const FklConstTable *kt, uint32_t idx) {
    return kt->kbit.base[idx];
}

#undef REALLOC_CONST_TABLE_BASE

void fklLoadConstTable(FILE *fp, FklConstTable *kt) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; count--) {
        int64_t k = 0;
        fread(&k, sizeof(k), 1, fp);
        fklAddI64Const(kt, k);
    }

    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; count--) {
        double k = 0;
        fread(&k, sizeof(k), 1, fp);
        fklAddF64Const(kt, k);
    }

    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; count--) {
        uint64_t size = 0;
        fread(&size, sizeof(size), 1, fp);
        FklString *str = fklCreateString(size, NULL);
        fread(str->str, size, 1, fp);
        fklAddStrConst(kt, str);
        fklZfree(str);
    }

    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; count--) {
        uint64_t size = 0;
        fread(&size, sizeof(size), 1, fp);
        FklBytevector *bvec = fklCreateBytevector(size, NULL);
        fread(bvec->ptr, size, 1, fp);
        fklAddBvecConst(kt, bvec);
        fklZfree(bvec);
    }

    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; count--) {
        int64_t num = 0;
        fread(&num, sizeof(num), 1, fp);
        size_t size = fklAbs(num) * sizeof(FklBigIntDigit);
        FklBigIntDigit *mem;
        if (!num)
            mem = NULL;
        else {
            mem = (FklBigIntDigit *)fklZmalloc(size);
            FKL_ASSERT(mem);
            fread(mem, size, 1, fp);
        }
        FklBigInt bi = FKL_BIGINT_0;
        fklInitBigIntFromMem(&bi, num, mem);
        fklAddBigIntConst(kt, &bi);
        fklUninitBigInt(&bi);
    }
}

void fklWriteConstTable(const FklConstTable *kt, FILE *fp) {
    const FklConstI64Table *ki64t = &kt->ki64t;
    fwrite(&ki64t->count, sizeof(ki64t->count), 1, fp);
    fwrite(ki64t->base, sizeof(*ki64t->base), ki64t->count, fp);

    const FklConstF64Table *kf64t = &kt->kf64t;
    fwrite(&kf64t->count, sizeof(kf64t->count), 1, fp);
    fwrite(kf64t->base, sizeof(*kf64t->base), kf64t->count, fp);

    const FklConstStrTable *kstrt = &kt->kstrt;
    fwrite(&kstrt->count, sizeof(kstrt->count), 1, fp);
    for (uint32_t i = 0; i < kstrt->count; i++) {
        const FklString *cur = kstrt->base[i];
        fwrite(&cur->size, sizeof(cur->size), 1, fp);
        fwrite(&cur->str, cur->size, 1, fp);
    }

    const FklConstBvecTable *kbvect = &kt->kbvect;
    fwrite(&kbvect->count, sizeof(kbvect->count), 1, fp);
    for (uint32_t i = 0; i < kbvect->count; i++) {
        const FklBytevector *cur = kbvect->base[i];
        fwrite(&cur->size, sizeof(cur->size), 1, fp);
        fwrite(&cur->ptr, cur->size, 1, fp);
    }

    const FklConstBiTable *kbit = &kt->kbit;
    fwrite(&kbit->count, sizeof(kbit->count), 1, fp);
    for (uint32_t i = 0; i < kbit->count; i++) {
        const FklBigInt *cur = kbit->base[i];
        fwrite(&cur->num, sizeof(cur->num), 1, fp);
        fwrite(cur->digits, fklAbs(cur->num) * sizeof(FklBigIntDigit), 1, fp);
    }
}

#include <math.h>
void fklPrintConstTable(const FklConstTable *kt, FILE *fp) {
    const FklConstI64Table *ki64t = &kt->ki64t;
    int numLen = ki64t->count ? (int)(log10(ki64t->count) + 1) : 1;
    fprintf(fp, "i64:\t%u\n", ki64t->count);
    for (uint32_t i = 0; i < ki64t->count; i++)
        fprintf(fp, "\t%-*u:\t%" PRId64 "\n", numLen, i, ki64t->base[i]);

    const FklConstF64Table *kf64t = &kt->kf64t;
    numLen = kf64t->count ? (int)(log10(kf64t->count) + 1) : 1;
    fprintf(fp, "f64:\t%u\n", kf64t->count);
    for (uint32_t i = 0; i < kf64t->count; i++) {
        char buf[64] = { 0 };
        fklWriteDoubleToBuf(buf, 64, kf64t->base[i]);
        fprintf(fp, "\t%-*u:\t%s\n", numLen, i, buf);
    }

    const FklConstStrTable *kstrt = &kt->kstrt;
    numLen = kstrt->count ? (int)(log10(kstrt->count) + 1) : 1;
    fprintf(fp, "string:\t%u\n", kstrt->count);
    for (uint32_t i = 0; i < kstrt->count; i++) {
        const FklString *str = kstrt->base[i];
        fprintf(fp, "\t%-*u:\t%" PRIu64 "\t", numLen, i, str->size);
        fklPrintRawString(str, fp);
        fputc('\n', fp);
    }

    const FklConstBvecTable *kbvect = &kt->kbvect;
    numLen = kbvect->count ? (int)(log10(kbvect->count) + 1) : 1;
    fprintf(fp, "bvec:\t%u\n", kbvect->count);
    for (uint32_t i = 0; i < kbvect->count; i++) {
        const FklBytevector *bvec = kbvect->base[i];
        fprintf(fp, "\t%-*u:\t%" PRIu64 "\t", numLen, i, bvec->size);
        fklPrintRawBytevector(bvec, fp);
        fputc('\n', fp);
    }

    const FklConstBiTable *kbit = &kt->kbit;
    numLen = kbit->count ? (int)(log10(kbit->count) + 1) : 1;
    fprintf(fp, "bigint:\t%u\n", kbit->count);
    for (uint32_t i = 0; i < kbit->count; i++) {
        const FklBigInt *bi = kbit->base[i];
        fprintf(fp, "\t%-*u:\t", numLen, i);
        fklPrintBigInt(bi, fp);
        fputc('\n', fp);
    }
}

FklByteCode *fklCreateByteCode(size_t len) {
    FklByteCode *tmp = (FklByteCode *)fklZmalloc(sizeof(FklByteCode));
    FKL_ASSERT(tmp);
    tmp->len = len;
    if (len == 0)
        tmp->code = NULL;
    else {
        tmp->code = (FklInstruction *)fklZcalloc(len, sizeof(FklInstruction));
        FKL_ASSERT(tmp->code);
    }
    return tmp;
}

FklByteCodelnt *fklCreateByteCodelnt(FklByteCode *bc) {
    FklByteCodelnt *t = (FklByteCodelnt *)fklZmalloc(sizeof(FklByteCodelnt));
    FKL_ASSERT(t);
    t->ls = 0;
    t->l = NULL;
    t->bc = bc;
    return t;
}

FklByteCodelnt *fklCreateSingleInsBclnt(FklInstruction ins,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    FklByteCode *bc = fklCreateByteCode(1);
    bc->code[0] = ins;
    FklByteCodelnt *r = fklCreateByteCodelnt(bc);
    r->ls = 1;
    r->l = (FklLineNumberTableItem *)fklZmalloc(sizeof(FklLineNumberTableItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], fid, 0, line, scope);
    return r;
}

void fklDestroyByteCodelnt(FklByteCodelnt *t) {
    fklDestroyByteCode(t->bc);
    if (t->l)
        fklZfree(t->l);
    fklZfree(t);
}

void fklDestroyByteCode(FklByteCode *obj) {
    fklZfree(obj->code);
    fklZfree(obj);
}

void fklCodeConcat(FklByteCode *fir, const FklByteCode *sec) {
    uint32_t len = fir->len;
    fir->len = sec->len + fir->len;
    if (!fir->len)
        fir->code = NULL;
    else {
        fir->code = (FklInstruction *)fklZrealloc(fir->code,
                fir->len * sizeof(FklInstruction));
        FKL_ASSERT(fir->code);
        memcpy(&fir->code[len], sec->code, sizeof(FklInstruction) * sec->len);
    }
}

void fklCodeReverseConcat(const FklByteCode *fir, FklByteCode *sec) {
    uint32_t len = fir->len;
    if (fir->len + sec->len == 0)
        sec->code = NULL;
    else {
        sec->code = (FklInstruction *)fklZrealloc(sec->code,
                (fir->len + sec->len) * sizeof(FklInstruction));
        FKL_ASSERT(sec->code);
        memmove(&sec->code[len], sec->code, sec->len * sizeof(FklInstruction));
        memcpy(sec->code, fir->code, len * sizeof(FklInstruction));
    }

    sec->len = len + sec->len;
}

FklByteCode *fklCopyByteCode(const FklByteCode *obj) {
    FklByteCode *tmp = fklCreateByteCode(obj->len);
    memcpy(tmp->code, obj->code, sizeof(FklInstruction) * obj->len);
    return tmp;
}

FklByteCodelnt *fklCopyByteCodelnt(const FklByteCodelnt *obj) {
    FklByteCodelnt *tmp = fklCreateByteCodelnt(fklCopyByteCode(obj->bc));
    tmp->ls = obj->ls;
    if (!obj->ls)
        tmp->l = NULL;
    else {
        tmp->l = (FklLineNumberTableItem *)fklZmalloc(
                obj->ls * sizeof(FklLineNumberTableItem));
        FKL_ASSERT(tmp->l);
        for (size_t i = 0; i < obj->ls; i++)
            tmp->l[i] = obj->l[i];
    }
    return tmp;
}

typedef struct ByteCodePrintState {
    uint32_t tc;
    uint64_t cp;
    uint64_t cpc;
} ByteCodePrintState;

#define FKL_VECTOR_TYPE_PREFIX Bc
#define FKL_VECTOR_METHOD_PREFIX bc
#define FKL_VECTOR_ELM_TYPE ByteCodePrintState
#define FKL_VECTOR_ELM_TYPE_NAME BcPrintState
#include <fakeLisp/cont/vector.h>

static inline void push_bytecode_print_state(BcBcPrintStateVector *s,
        uint64_t tc,
        uint64_t cp,
        uint64_t cpc) {
    bcBcPrintStateVectorPushBack2(s,
            (ByteCodePrintState){ .tc = tc, .cp = cp, .cpc = cpc });
}

static inline uint32_t print_single_ins(const FklByteCode *tmpCode,
        uint64_t i,
        FILE *fp,
        struct ByteCodePrintState *cur_state,
        BcBcPrintStateVector *s,
        int *needBreak,
        const FklSymbolTable *st,
        const FklConstTable *kt,
        int numLen) {
    uint32_t tab_count = cur_state->tc;
    uint32_t proc_len = 0;
    fprintf(fp, "%-*" PRIu64 ":", numLen, i);
    for (uint32_t i = 0; i < tab_count; i++)
        fputs("    ", fp);
    const FklInstruction *ins = &tmpCode->code[i];
    FklOpcode op = ins->op;
    fprintf(fp, " %s", fklGetOpcodeName(op));
    FklOpcodeMode mode = fklGetOpcodeMode(op);
    if (mode == FKL_OP_MODE_I)
        goto end;
    fputc(' ', fp);
    FklInstructionArg ins_arg = { 0 };
    int len = fklGetInsOpArg(ins, &ins_arg);
    switch (ins->op) {
    case FKL_OP_PUSH_I64F:
    case FKL_OP_PUSH_I64F_C:
    case FKL_OP_PUSH_I64F_X:
    case FKL_OP_PUSH_I64B:
    case FKL_OP_PUSH_I64B_C:
    case FKL_OP_PUSH_I64B_X:
        fprintf(fp,
                "%" PRIu64 "\t#\t%" PRId64,
                ins_arg.ux,
                fklGetI64ConstWithIdx(kt, ins_arg.ux));
        break;
    case FKL_OP_PUSH_F64:
    case FKL_OP_PUSH_F64_C:
    case FKL_OP_PUSH_F64_X:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintDouble(fklGetF64ConstWithIdx(kt, ins_arg.ux), fp);
        break;
    case FKL_OP_PUSH_CHAR:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintRawChar(ins->bu, fp);
        break;

    case FKL_OP_PUSH_STR:
    case FKL_OP_PUSH_STR_C:
    case FKL_OP_PUSH_STR_X:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintRawString(fklGetStrConstWithIdx(kt, ins_arg.ux), fp);
        break;
    case FKL_OP_PUSH_BVEC:
    case FKL_OP_PUSH_BVEC_C:
    case FKL_OP_PUSH_BVEC_X:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintRawBytevector(fklGetBvecConstWithIdx(kt, ins_arg.ux), fp);
        break;

    case FKL_OP_PUSH_SYM:
    case FKL_OP_PUSH_SYM_C:
    case FKL_OP_PUSH_SYM_X:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintRawSymbol(fklGetSymbolWithId(ins_arg.ux, st)->k, fp);
        break;

    case FKL_OP_PUSH_BI:
    case FKL_OP_PUSH_BI_C:
    case FKL_OP_PUSH_BI_X:
        fprintf(fp, "%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrintBigInt(fklGetBiConstWithIdx(kt, ins_arg.ux), fp);
        break;
    case FKL_OP_PUSH_PROC:
    case FKL_OP_PUSH_PROC_X:
    case FKL_OP_PUSH_PROC_XX:
    case FKL_OP_PUSH_PROC_XXX:
        fprintf(fp, "%" PRIu64 "\t%" PRIu64, ins_arg.ux, ins_arg.uy);
        push_bytecode_print_state(s,
                cur_state->tc,
                i + len + ins_arg.uy,
                cur_state->cpc);
        push_bytecode_print_state(s,
                tab_count + 1,
                i + len,
                i + len + ins_arg.uy);
        if (len > 1)
            push_bytecode_print_state(s, cur_state->tc, i + 1, i + 1 + len - 1);
        proc_len = ins_arg.uy;
        *needBreak = 1;
        break;

    case FKL_OP_DROP:
    case FKL_OP_PAIR:
    case FKL_OP_VEC:
    case FKL_OP_STR:
    case FKL_OP_BVEC:
    case FKL_OP_BOX:
    case FKL_OP_HASH:
        fprintf(fp, "::%s", fklGetSubOpcodeName(ins->op, ins_arg.ix));
        break;

    default:
        switch (mode) {
        case FKL_OP_MODE_IsA:
        case FKL_OP_MODE_IsB:
        case FKL_OP_MODE_IsC:
        case FKL_OP_MODE_IsBB:
        case FKL_OP_MODE_IsCCB:
            fprintf(fp, "%" PRId64, ins_arg.ix);
            break;
        case FKL_OP_MODE_IuB:
        case FKL_OP_MODE_IuC:
        case FKL_OP_MODE_IuBB:
        case FKL_OP_MODE_IuCCB:
            fprintf(fp, "%" PRIu64, ins_arg.ux);
            break;
        case FKL_OP_MODE_IsAuB:
            fprintf(fp, "%" PRId64 "\t%" PRIu64, ins_arg.ix, ins_arg.uy);
            break;
        case FKL_OP_MODE_IuAuB:
        case FKL_OP_MODE_IuCuC:
        case FKL_OP_MODE_IuCAuBB:
        case FKL_OP_MODE_IuCAuBCC:
            fprintf(fp, "%" PRIu64 "\t%" PRIu64, ins_arg.ux, ins_arg.uy);
            break;
        case FKL_OP_MODE_I:
            break;

        case FKL_OP_MODE_IxAxB:
            fprintf(fp, "%#" PRIx64 "\t%#" PRIx64, ins_arg.ux, ins_arg.uy);
            break;
        }
        break;
    }
end:
    return proc_len + 1;
}

#include <math.h>
void fklPrintByteCode(const FklByteCode *tmpCode,
        FILE *fp,
        FklSymbolTable *st,
        FklConstTable *kt) {
    BcBcPrintStateVector s;
    bcBcPrintStateVectorInit(&s, 32);
    push_bytecode_print_state(&s, 0, 0, tmpCode->len);
    uint64_t codeLen = tmpCode->len;
    int numLen = codeLen ? (int)(log10(codeLen) + 1) : 1;

    if (!bcBcPrintStateVectorIsEmpty(&s)) {
        struct ByteCodePrintState cur_state =
                *bcBcPrintStateVectorPopBackNonNull(&s);
        uint64_t i = cur_state.cp;
        uint64_t cpc = cur_state.cpc;
        int needBreak = 0;
        if (i < cpc)
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
        while (i < cpc && !needBreak) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
        }
    }
    while (!bcBcPrintStateVectorIsEmpty(&s)) {
        struct ByteCodePrintState cur_state =
                *bcBcPrintStateVectorPopBackNonNull(&s);
        uint64_t i = cur_state.cp;
        uint64_t cpc = cur_state.cpc;
        int needBreak = 0;
        if (i < cpc) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
        }
        while (i < cpc && !needBreak) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
        }
    }
    bcBcPrintStateVectorUninit(&s);
}

static uint64_t skipToCall(uint64_t index, const FklByteCode *bc) {
    uint64_t r = 0;
    const FklInstruction *ins = NULL;
    while (index + r < bc->len
            && (ins = &bc->code[index + r])->op != FKL_OP_CALL) {
        if (fklIsPushProcIns(ins)) {
            FklInstructionArg arg = { 0 };
            int8_t l = fklGetInsOpArg(ins, &arg);
            r += arg.uy + l;
        } else
            r++;
    }
    return r;
}

static inline int64_t get_next(const FklInstruction *ins) {
    if (fklIsJmpIns(ins)) {
        FklInstructionArg arg = { 0 };
        int8_t l = fklGetInsOpArg(ins, &arg);
        return arg.ix + l;
    }
    return 1;
}

static inline int is_last_expression(uint64_t index, FklByteCode *bc) {
    uint64_t size = bc->len;
    FklInstruction *code = bc->code;
    for (uint64_t i = index; i < size && code[i].op != FKL_OP_RET;
            i += get_next(&code[i]))
        if (code[i].op != FKL_OP_JMP && code[i].op != FKL_OP_CLOSE_REF)
            return 0;
    return 1;
}

static inline int8_t get_op_ins_len(FklOpcode op) {
    switch (fklGetOpcodeMode(op)) {
    case FKL_OP_MODE_I:
        return 1;
        break;
    case FKL_OP_MODE_IsA:
        return 1;
        break;
    case FKL_OP_MODE_IuB:
        return 1;
        break;
    case FKL_OP_MODE_IsB:
        return 1;
        break;
    case FKL_OP_MODE_IuC:
        return 1;
        break;
    case FKL_OP_MODE_IsC:
        return 1;
        break;
    case FKL_OP_MODE_IuBB:
        return 2;
        break;
    case FKL_OP_MODE_IsBB:
        return 2;
        break;
    case FKL_OP_MODE_IuCCB:
        return 3;
        break;
    case FKL_OP_MODE_IsCCB:
        return 3;
        break;

    case FKL_OP_MODE_IsAuB:
    case FKL_OP_MODE_IuAuB:
        return 1;
        break;

    case FKL_OP_MODE_IuCuC:
        return 2;
        break;
    case FKL_OP_MODE_IuCAuBB:
        return 3;
        break;
    case FKL_OP_MODE_IuCAuBCC:
        return 4;
        break;

    case FKL_OP_MODE_IxAxB:
        return 1;
        break;
    }
    return 1;
}

static inline int get_ins_op_with_op(FklOpcode op,
        const FklInstruction *ins,
        FklInstructionArg *a) {
    switch (fklGetOpcodeMode(op)) {
    case FKL_OP_MODE_I:
        return 1;
        break;
    case FKL_OP_MODE_IsA:
        a->ix = ins->ai;
        return 1;
        break;
    case FKL_OP_MODE_IuB:
        a->ux = ins->bu;
        return 1;
        break;
    case FKL_OP_MODE_IsB:
        a->ix = ins->bi;
        return 1;
        break;
    case FKL_OP_MODE_IuC:
        a->ux = FKL_GET_INS_UC(ins);
        return 1;
        break;
    case FKL_OP_MODE_IsC:
        a->ix = FKL_GET_INS_IC(ins);
        return 1;
        break;
    case FKL_OP_MODE_IuBB:
        a->ux = FKL_GET_INS_UX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IsBB:
        a->ix = FKL_GET_INS_IX(ins);
        return 2;
        break;
    case FKL_OP_MODE_IuCCB:
        a->ux = FKL_GET_INS_UXX(ins);
        return 3;
        break;
    case FKL_OP_MODE_IsCCB:
        a->ix = FKL_GET_INS_IXX(ins);
        return 3;
        break;

    case FKL_OP_MODE_IsAuB:
        a->ix = ins->ai;
        a->uy = ins->bu;
        return 1;
        break;

    case FKL_OP_MODE_IuAuB:
        a->ux = ins->au;
        a->uy = ins->bu;
        return 1;
        break;

    case FKL_OP_MODE_IuCuC:
        a->ux = FKL_GET_INS_UC(ins);
        a->uy = FKL_GET_INS_UC(ins + 1);
        return 2;
        break;
    case FKL_OP_MODE_IuCAuBB:
        a->ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        a->uy = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
        return 3;
        break;
    case FKL_OP_MODE_IuCAuBCC:
        a->ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
        a->uy = ins[1].bu | (((uint64_t)ins[2].au) << FKL_I16_WIDTH)
              | (((uint64_t)ins[2].bu) << FKL_I24_WIDTH)
              | (((uint64_t)ins[3].au) << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH))
              | (((uint64_t)ins[3].bu)
                      << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH * 2));
        return 4;
        break;

    case FKL_OP_MODE_IxAxB:
        a->ux = ins->au;
        a->uy = ins->bu;
        return 1;
        break;
    }
    return 1;
}

int fklGetInsOpArg(const FklInstruction *ins, FklInstructionArg *a) {
    return get_ins_op_with_op(ins->op, ins, a);
}

int fklGetInsOpArgWithOp(FklOpcode op,
        const FklInstruction *ins,
        FklInstructionArg *a) {
    return get_ins_op_with_op(op, ins, a);
}

int fklIsJmpIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_JMP && ins->op <= FKL_OP_JMP_XX;
}

int fklIsCondJmpIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_JMP_IF_TRUE && ins->op <= FKL_OP_JMP_IF_FALSE_XX;
}

int fklIsPutLocIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUT_LOC && ins->op <= FKL_OP_PUT_LOC_X;
}

int fklIsPushProcIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUSH_PROC && ins->op <= FKL_OP_PUSH_PROC_XXX;
}

int fklIsPutVarRefIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUT_VAR_REF && ins->op <= FKL_OP_PUT_VAR_REF_X;
}

void fklScanAndSetTailCall(FklByteCode *bc) {
    for (uint64_t i = skipToCall(0, bc); i < bc->len; i += skipToCall(i, bc))
        if (is_last_expression(++i, bc))
            bc->code[i - 1].op = FKL_OP_TAIL_CALL;
}

void fklPrintByteCodelnt(const FklByteCodelnt *obj,
        FILE *fp,
        const FklSymbolTable *st,
        const FklConstTable *kt) {
    FklByteCode *tmpCode = obj->bc;
    BcBcPrintStateVector s;
    bcBcPrintStateVectorInit(&s, 32);
    push_bytecode_print_state(&s, 0, 0, tmpCode->len);
    uint64_t j = 0;
    uint64_t end = obj->ls - 1;
    FklSid_t fid = 0;
    uint64_t line = 0;

    uint64_t codeLen = tmpCode->len;
    int numLen = codeLen ? (int)(log10(codeLen) + 1) : 1;

    if (!bcBcPrintStateVectorIsEmpty(&s)) {
        struct ByteCodePrintState cur_state =
                *bcBcPrintStateVectorPopBackNonNull(&s);
        uint64_t i = cur_state.cp;
        uint64_t cpc = cur_state.cpc;
        int needBreak = 0;
        if (i < cpc) {
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
            if (j < end && obj->l[j + 1].scp < i)
                j++;
            if (obj->l[j].fid != fid || obj->l[j].line != line) {
                fid = obj->l[j].fid;
                line = obj->l[j].line;
                if (fid) {
                    fprintf(fp, "    ;");
                    fklPrintString(fklGetSymbolWithId(obj->l[j].fid, st)->k,
                            fp);
                    fprintf(fp, ":%u", obj->l[j].line);
                } else
                    fprintf(fp, "\t%u", obj->l[j].line);
            }
        }
        while (i < cpc && !needBreak) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
            if (j < end && obj->l[j + 1].scp < i)
                j++;
            if (obj->l[j].fid != fid || obj->l[j].line != line) {
                fid = obj->l[j].fid;
                line = obj->l[j].line;
                if (fid) {
                    fprintf(fp, "    ;");
                    fklPrintString(fklGetSymbolWithId(obj->l[j].fid, st)->k,
                            fp);
                    fprintf(fp, ":%u", obj->l[j].line);
                } else
                    fprintf(fp, "\t%u", obj->l[j].line);
            }
        }
    }
    while (!bcBcPrintStateVectorIsEmpty(&s)) {
        struct ByteCodePrintState cur_state =
                *bcBcPrintStateVectorPopBackNonNull(&s);
        uint64_t i = cur_state.cp;
        uint64_t cpc = cur_state.cpc;
        int needBreak = 0;
        if (i < cpc) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
            if (j < end && obj->l[j + 1].scp < i)
                j++;
            if (obj->l[j].fid != fid || obj->l[j].line != line) {
                fid = obj->l[j].fid;
                line = obj->l[j].line;
                if (fid) {
                    fprintf(fp, "    ;");
                    fklPrintString(fklGetSymbolWithId(obj->l[j].fid, st)->k,
                            fp);
                    fprintf(fp, ":%u", obj->l[j].line);
                } else
                    fprintf(fp, "\t%u", obj->l[j].line);
            }
        }
        while (i < cpc && !needBreak) {
            putc('\n', fp);
            i += print_single_ins(tmpCode,
                    i,
                    fp,
                    &cur_state,
                    &s,
                    &needBreak,
                    st,
                    kt,
                    numLen);
            if (j < end && obj->l[j + 1].scp < i)
                j++;
            if (obj->l[j].fid != fid || obj->l[j].line != line) {
                fid = obj->l[j].fid;
                line = obj->l[j].line;
                if (fid) {
                    fprintf(fp, "    ;");
                    fklPrintString(fklGetSymbolWithId(obj->l[j].fid, st)->k,
                            fp);
                    fprintf(fp, ":%u", obj->l[j].line);
                } else
                    fprintf(fp, "\t%u", obj->l[j].line);
            }
            if (needBreak)
                break;
        }
    }
    bcBcPrintStateVectorUninit(&s);
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *o, uint64_t size) {
    uint32_t i = 0;
    for (; i < o->ls; i++)
        o->l[i].scp += size;
}

#define FKL_INCREASE_ALL_SCP(l, ls, s)                                         \
    for (size_t i = 0; i < (ls); i++)                                          \
    (l)[i].scp += (s)
void fklCodeLntConcat(FklByteCodelnt *f, const FklByteCodelnt *s) {
    if (s->bc->len) {
        if (!f->l) {
            f->ls = s->ls;
            if (s->ls == 0)
                f->l = NULL;
            else {
                f->l = (FklLineNumberTableItem *)fklZmalloc(
                        s->ls * sizeof(FklLineNumberTableItem));
                FKL_ASSERT(f->l);
                FKL_INCREASE_ALL_SCP(s->l + 1, s->ls - 1, f->bc->len);
                memcpy(f->l, s->l, (s->ls) * sizeof(FklLineNumberTableItem));
            }
        } else {
            FKL_INCREASE_ALL_SCP(s->l, s->ls, f->bc->len);
            if (f->l[f->ls - 1].line == s->l[0].line
                    && f->l[f->ls - 1].scope == s->l[0].scope
                    && f->l[f->ls - 1].fid == s->l[0].fid) {
                uint32_t size = f->ls + s->ls - 1;
                if (!size)
                    f->l = NULL;
                else {
                    f->l = (FklLineNumberTableItem *)fklZrealloc(f->l,
                            size * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLineNumberTableItem));
                }
                f->ls += s->ls - 1;
            } else {
                uint32_t size = f->ls + s->ls;
                if (!size)
                    f->l = NULL;
                else {
                    f->l = (FklLineNumberTableItem *)fklZrealloc(f->l,
                            size * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(f->l);
                    memcpy(&f->l[f->ls],
                            s->l,
                            (s->ls) * sizeof(FklLineNumberTableItem));
                }
                f->ls += s->ls;
            }
        }
        fklCodeConcat(f->bc, s->bc);
    }
}

void fklCodeLntReverseConcat(const FklByteCodelnt *f, FklByteCodelnt *s) {
    if (f->bc->len) {
        if (!s->l) {
            s->ls = f->ls;
            if (f->ls == 0)
                s->l = NULL;
            else {
                s->l = (FklLineNumberTableItem *)fklZmalloc(
                        f->ls * sizeof(FklLineNumberTableItem));
                FKL_ASSERT(s->l);
                memcpy(s->l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
            }
        } else {
            FKL_INCREASE_ALL_SCP(s->l, s->ls, f->bc->len);
            if (f->l[f->ls - 1].line == s->l[0].line
                    && f->l[f->ls - 1].scope == s->l[0].scope
                    && f->l[f->ls - 1].fid == s->l[0].fid) {
                FklLineNumberTableItem *l;
                const uint32_t len = f->ls + s->ls - 1;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLineNumberTableItem *)fklZmalloc(
                            len * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
                    memcpy(&l[f->ls],
                            &s->l[1],
                            (s->ls - 1) * sizeof(FklLineNumberTableItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls - 1;
            } else {
                FklLineNumberTableItem *l;
                const uint32_t len = f->ls + s->ls;
                if (len == 0)
                    l = NULL;
                else {
                    l = (FklLineNumberTableItem *)fklZmalloc(
                            len * sizeof(FklLineNumberTableItem));
                    FKL_ASSERT(l);
                    memcpy(l, f->l, (f->ls) * sizeof(FklLineNumberTableItem));
                    memcpy(&l[f->ls],
                            s->l,
                            (s->ls) * sizeof(FklLineNumberTableItem));
                }
                fklZfree(s->l);
                s->l = l;
                s->ls += f->ls;
            }
        }
        fklCodeReverseConcat(f->bc, s->bc);
    }
}

void fklByteCodeInsertFront(FklInstruction ins, FklByteCode *bc) {
    FklInstruction *code = (FklInstruction *)fklZrealloc(bc->code,
            (bc->len + 1) * sizeof(FklInstruction));
    FKL_ASSERT(code);
    for (uint64_t end = bc->len; end > 0; end--)
        code[end] = code[end - 1];
    code[0] = ins;
    bc->len++;
    bc->code = code;
}

void fklByteCodePushBack(FklByteCode *bc, FklInstruction ins) {
    bc->code = (FklInstruction *)fklZrealloc(bc->code,
            (bc->len + 1) * sizeof(FklInstruction));
    FKL_ASSERT(bc->code);
    bc->code[bc->len] = ins;
    bc->len++;
}

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklInstruction *ins,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLineNumberTableItem *)fklZmalloc(
                sizeof(FklLineNumberTableItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(bcl->bc, *ins);
    } else {
        fklByteCodePushBack(bcl->bc, *ins);
    }
}

void fklByteCodeLntInsertFrontIns(const FklInstruction *ins,
        FklByteCodelnt *bcl,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    if (!bcl->l) {
        bcl->ls = 1;
        bcl->l = (FklLineNumberTableItem *)fklZmalloc(
                sizeof(FklLineNumberTableItem));
        FKL_ASSERT(bcl->l);
        fklInitLineNumTabNode(&bcl->l[0], fid, 0, line, scope);
        fklByteCodePushBack(bcl->bc, *ins);
    } else {
        fklByteCodeInsertFront(*ins, bcl->bc);
        FKL_INCREASE_ALL_SCP(bcl->l + 1, bcl->ls - 1, 1);
    }
}

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl,
        FklInstruction ins,
        uint64_t at) {
    FklByteCode *bc = bcl->bc;
    bc->code = (FklInstruction *)fklZrealloc(bc->code,
            (++bc->len) * sizeof(FklInstruction));
    FKL_ASSERT(bc->code);
    FklLineNumberTableItem *l =
            (FklLineNumberTableItem *)fklFindLineNumTabNode(at,
                    bcl->ls,
                    bcl->l);
    FKL_ASSERT(l);
    const FklLineNumberTableItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp++;
    for (uint64_t i = bc->len - 1; i > at; i++)
        bc->code[i] = bc->code[i - 1];
    bc->code[at] = ins;
}

FklInstruction fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t at) {
    FklByteCode *bc = bcl->bc;
    FklLineNumberTableItem *l =
            (FklLineNumberTableItem *)fklFindLineNumTabNode(at,
                    bcl->ls,
                    bcl->l);
    FKL_ASSERT(l);
    const FklLineNumberTableItem *end = &bcl->l[bcl->ls];
    for (l++; l < end; l++)
        l->scp--;
    FklInstruction ins = bc->code[at];
    for (uint64_t i = at; i < bc->len - 1; i++)
        bc->code[i] = bc->code[i + 1];
    if (!bc->len)
        bc->code = NULL;
    else {
        bc->code = (FklInstruction *)fklZrealloc(bc->code,
                (--bc->len) * sizeof(FklInstruction));
        FKL_ASSERT(bc->code);
    }
    return ins;
}

void fklInitLineNumTabNode(FklLineNumberTableItem *n,
        FklSid_t fid,
        uint64_t scp,
        uint32_t line,
        uint32_t scope) {
    n->fid = fid;
    n->scp = scp;
    n->line = line;
    n->scope = scope;
}

const FklLineNumberTableItem *fklFindLineNumTabNode(uint64_t cp,
        size_t ls,
        const FklLineNumberTableItem *line_numbers) {
    int64_t l = 0;
    int64_t h = ls - 1;
    int64_t mid = 0;
    int64_t end = ls - 1;
    while (l <= h) {
        mid = l + (h - l) / 2;
        const FklLineNumberTableItem *cur = &line_numbers[mid];
        if (cp < cur->scp)
            h = mid - 1;
        else if (mid < end && cp >= cur[1].scp)
            l = mid + 1;
        else
            return cur;
    }
    return NULL;
}

void fklWriteLineNumberTable(const FklLineNumberTableItem *line_numbers,
        uint32_t size,
        FILE *fp) {
    fwrite(&size, sizeof(size), 1, fp);
    for (uint32_t i = 0; i < size; i++) {
        const FklLineNumberTableItem *n = &line_numbers[i];
        fwrite(&n->fid, sizeof(n->fid), 1, fp);
        fwrite(&n->scp, sizeof(n->scp), 1, fp);
        fwrite(&n->line, sizeof(n->line), 1, fp);
    }
}

void fklLoadLineNumberTable(FILE *fp,
        FklLineNumberTableItem **plist,
        uint32_t *pnum) {
    size_t size = 0;
    fread(&size, sizeof(uint32_t), 1, fp);
    FklLineNumberTableItem *list;
    if (size == 0)
        list = NULL;
    else {
        list = (FklLineNumberTableItem *)fklZmalloc(
                size * sizeof(FklLineNumberTableItem));
        FKL_ASSERT(list);
        for (size_t i = 0; i < size; i++) {
            FklSid_t fid = 0;
            uint64_t scp = 0;
            uint32_t line = 0;
            fread(&fid, sizeof(fid), 1, fp);
            fread(&scp, sizeof(scp), 1, fp);
            fread(&line, sizeof(line), 1, fp);
            fklInitLineNumTabNode(&list[i], fid, scp, line, 0);
        }
    }
    *plist = list;
    *pnum = size;
}

void fklWriteByteCode(const FklByteCode *bc, FILE *outfp) {
    uint64_t len = bc->len;
    fwrite(&len, sizeof(bc->len), 1, outfp);
    const FklInstruction *end = bc->code + len;
    const FklInstruction *code = bc->code;
    while (code < end) {
        uint8_t op = code->op;
        fwrite(&op, sizeof(op), 1, outfp);
        int ins_len = get_op_ins_len(code->op);
        FklOpcodeMode mode = fklGetOpcodeMode(code->op);
        switch (mode) {
        case FKL_OP_MODE_I:
            break;
        case FKL_OP_MODE_IsA:
            fwrite(&code->ai, sizeof(code->ai), 1, outfp);
            break;
        case FKL_OP_MODE_IuB:
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            break;
        case FKL_OP_MODE_IsB:
            fwrite(&code->bi, sizeof(code->bi), 1, outfp);
            break;
        case FKL_OP_MODE_IuC:
        case FKL_OP_MODE_IsC:
        case FKL_OP_MODE_IuAuB:
        case FKL_OP_MODE_IsAuB:
            fwrite(&code->au, sizeof(code->au), 1, outfp);
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            break;
        case FKL_OP_MODE_IuBB:
        case FKL_OP_MODE_IsBB:
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            fwrite(&code[1].bu, sizeof(code[1].bu), 1, outfp);
            break;
        case FKL_OP_MODE_IuCCB:
        case FKL_OP_MODE_IsCCB:
        case FKL_OP_MODE_IuCAuBB:
            fwrite(&code->au, sizeof(code->au), 1, outfp);
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            fwrite(&code[1].au, sizeof(code[1].au), 1, outfp);
            fwrite(&code[1].bu, sizeof(code[1].bu), 1, outfp);
            fwrite(&code[2].bu, sizeof(code[2].bu), 1, outfp);
            break;

        case FKL_OP_MODE_IuCuC:
            fwrite(&code->au, sizeof(code->au), 1, outfp);
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            fwrite(&code[1].au, sizeof(code[1].au), 1, outfp);
            fwrite(&code[1].bu, sizeof(code[1].bu), 1, outfp);
            break;
        case FKL_OP_MODE_IuCAuBCC:
            fwrite(&code->au, sizeof(code->au), 1, outfp);
            fwrite(&code->bu, sizeof(code->bu), 1, outfp);
            fwrite(&code[1].au, sizeof(code[1].au), 1, outfp);

            fwrite(&code[1].bu, sizeof(code[1].bu), 1, outfp);
            fwrite(&code[2].au, sizeof(code[2].au), 1, outfp);
            fwrite(&code[2].bu, sizeof(code[2].bu), 1, outfp);
            fwrite(&code[3].au, sizeof(code[3].au), 1, outfp);
            fwrite(&code[3].bu, sizeof(code[3].bu), 1, outfp);
            break;
        case FKL_OP_MODE_IxAxB:
            abort();
            break;
        }
        code += ins_len;
    }
}

void fklWriteByteCodelnt(const FklByteCodelnt *bcl, FILE *outfp) {
    fklWriteLineNumberTable(bcl->l, bcl->ls, outfp);
    fklWriteByteCode(bcl->bc, outfp);
}

FklByteCode *fklLoadByteCode(FILE *fp) {
    uint64_t len = 0;
    fread(&len, sizeof(uint64_t), 1, fp);
    FklByteCode *tmp = fklCreateByteCode(len);

    const FklInstruction *end = tmp->code + len;
    FklInstruction *code = tmp->code;
    while (code < end) {
        uint8_t op = 0;
        fread(&op, sizeof(op), 1, fp);
        code->op = op;
        FklOpcodeMode mode = fklGetOpcodeMode(op);
        switch (mode) {
        case FKL_OP_MODE_I:
            break;
        case FKL_OP_MODE_IsA:
            fread(&code->ai, sizeof(code->ai), 1, fp);
            break;
        case FKL_OP_MODE_IuB:
            fread(&code->bu, sizeof(code->bu), 1, fp);
            break;
        case FKL_OP_MODE_IsB:
            fread(&code->bi, sizeof(code->bi), 1, fp);
            break;
        case FKL_OP_MODE_IuC:
        case FKL_OP_MODE_IsC:
        case FKL_OP_MODE_IuAuB:
        case FKL_OP_MODE_IsAuB:
            fread(&code->au, sizeof(code->au), 1, fp);
            fread(&code->bu, sizeof(code->bu), 1, fp);
            break;

        case FKL_OP_MODE_IuBB:
        case FKL_OP_MODE_IsBB:
            code[1].op = FKL_OP_EXTRA_ARG;
            fread(&code->bu, sizeof(code->bu), 1, fp);
            fread(&code[1].bu, sizeof(code[1].bu), 1, fp);
            break;
        case FKL_OP_MODE_IuCCB:
        case FKL_OP_MODE_IsCCB:
        case FKL_OP_MODE_IuCAuBB:
            code[1].op = FKL_OP_EXTRA_ARG;
            code[2].op = FKL_OP_EXTRA_ARG;
            fread(&code->au, sizeof(code->au), 1, fp);
            fread(&code->bu, sizeof(code->bu), 1, fp);
            fread(&code[1].au, sizeof(code[1].au), 1, fp);
            fread(&code[1].bu, sizeof(code[1].bu), 1, fp);
            fread(&code[2].bu, sizeof(code[2].bu), 1, fp);
            break;

        case FKL_OP_MODE_IuCuC:
            code[1].op = FKL_OP_EXTRA_ARG;
            fread(&code->au, sizeof(code->au), 1, fp);
            fread(&code->bu, sizeof(code->bu), 1, fp);
            fread(&code[1].au, sizeof(code[1].au), 1, fp);
            fread(&code[1].bu, sizeof(code[1].bu), 1, fp);
            break;
        case FKL_OP_MODE_IuCAuBCC:
            code[1].op = FKL_OP_EXTRA_ARG;
            code[2].op = FKL_OP_EXTRA_ARG;
            code[3].op = FKL_OP_EXTRA_ARG;

            fread(&code->au, sizeof(code->au), 1, fp);
            fread(&code->bu, sizeof(code->bu), 1, fp);
            fread(&code[1].au, sizeof(code[1].au), 1, fp);
            fread(&code[1].bu, sizeof(code[1].bu), 1, fp);
            fread(&code[2].au, sizeof(code[2].au), 1, fp);
            fread(&code[2].bu, sizeof(code[2].bu), 1, fp);
            fread(&code[3].au, sizeof(code[3].au), 1, fp);
            fread(&code[3].bu, sizeof(code[3].bu), 1, fp);
            break;
        case FKL_OP_MODE_IxAxB:
            abort();
            break;
        }
        code += get_op_ins_len(op);
    }
    return tmp;
}

FklByteCodelnt *fklLoadByteCodelnt(FILE *fp) {
    FklByteCodelnt *bcl = fklCreateByteCodelnt(NULL);
    fklLoadLineNumberTable(fp, &bcl->l, &bcl->ls);
    FklByteCode *bc = fklLoadByteCode(fp);
    bcl->bc = bc;
    return bcl;
}
