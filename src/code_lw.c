#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/string_table.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"

enum ValueCreateOpcode {
    NOP = 0,
    MAKE_NIL,
    MAKE_FIX,
    MAKE_CHR,
    MAKE_F64,
    MAKE_BIGINT,
    MAKE_STR,
    MAKE_SYM,
    MAKE_BYTES,
    MAKE_SLOT,
    MAKE_HEADER_WILDCARD,

    CREATE_PAIR,
    CREATE_VECTOR,
    CREATE_BOX,
    CREATE_HASHTABLE,
};

typedef uint32_t ValueId;

static inline void write_value_create_op(enum ValueCreateOpcode o, FILE *fp) {
    uint8_t op = o;
    fwrite(&op, sizeof(op), 1, fp);
}

static inline void write_value_id(const FklValueTable *t,
        ValueId v_id,
        const FklVMvalue *v,
        FILE *fp) {
    ValueId u = fklValueTableGet(t, FKL_TYPE_CAST(FklVMvalue *, v));
    FKL_ASSERT((u || v == NULL) && (v_id == 0 || u <= v_id));
    fwrite(&u, sizeof(u), 1, fp);
}

static inline FklVMvalue *load_value_id(FILE *fp,
        const FklLoadValueArgs *values) {
    ValueId id = 0;
    fread(&id, sizeof(id), 1, fp);
    if (id == 0)
        return NULL;

    FklVMvalue *r = values->values[id - 1];
    FKL_ASSERT(r);

    return r;
}

static inline void write_bigint(const FklVMvalueBigInt *bi, FILE *fp) {
    fwrite(&bi->num, sizeof(bi->num), 1, fp);
    fwrite(bi->digits, fklAbs(bi->num) * sizeof(*(bi->digits)), 1, fp);
}

static inline void write_value_create_instructions(const FklVMvalue *v,
        ValueId value_id,
        const FklValueTable *vt,
        const FklValueHashSet *written_values,
        FILE *fp) {
    if (v == FKL_VM_NIL) {
        write_value_create_op(MAKE_NIL, fp);
    } else if (FKL_IS_FIX(v)) {
        write_value_create_op(MAKE_FIX, fp);
        int64_t i = FKL_GET_FIX(v);
        fwrite(&i, sizeof(i), 1, fp);
    } else if (FKL_IS_CHR(v)) {
        write_value_create_op(MAKE_CHR, fp);
        char c = FKL_GET_CHR(v);
        fwrite(&c, sizeof(c), 1, fp);
    } else if (FKL_IS_F64(v)) {
        write_value_create_op(MAKE_F64, fp);
        double f = FKL_VM_F64(v);
        fwrite(&f, sizeof(f), 1, fp);
    } else if (FKL_IS_BIGINT(v)) {
        write_value_create_op(MAKE_BIGINT, fp);
        write_bigint(FKL_VM_BI(v), fp);
    } else if (FKL_IS_STR(v)) {
        write_value_create_op(MAKE_STR, fp);
        fklWriteString(FKL_VM_STR(v), fp);
    } else if (FKL_IS_SYM(v)) {
        write_value_create_op(MAKE_SYM, fp);
        int8_t interned = FKL_VM_SYM_INTERNED(v);
        fwrite(&interned, sizeof(interned), 1, fp);
        fklWriteString(FKL_VM_SYM(v), fp);
    } else if (FKL_IS_BYTEVECTOR(v)) {
        write_value_create_op(MAKE_BYTES, fp);
        fklWriteBytevector(FKL_VM_BVEC(v), fp);
    } else if (v == FKL_VM_HEADER_WILDCARD) {
        write_value_create_op(MAKE_HEADER_WILDCARD, fp);
    } else if (fklIsVMvalueSlot(v)) {
        const FklVMvalueSlot *s = FKL_TYPE_CAST(const FklVMvalueSlot *, v);
        write_value_create_op(MAKE_SLOT, fp);
        write_value_id(vt, value_id, s->s, fp);

        int8_t expand_type = s->expand;
        fwrite(&expand_type, sizeof(expand_type), 1, fp);
    } else if (FKL_IS_PAIR(v)) {
        write_value_create_op(CREATE_PAIR, fp);
        write_value_id(vt, value_id, FKL_VM_CAR(v), fp);
        write_value_id(vt, value_id, FKL_VM_CDR(v), fp);
    } else if (FKL_IS_VECTOR(v)) {
        write_value_create_op(CREATE_VECTOR, fp);
        uint64_t const len = FKL_VM_VEC(v)->size;
        fwrite(&len, sizeof(len), 1, fp);
        FklVMvalue **const base = FKL_VM_VEC(v)->base;
        for (uint64_t i = 0; i < len; ++i) {
            write_value_id(vt, value_id, base[i], fp);
        }
    } else if (FKL_IS_BOX(v)) {
        write_value_create_op(CREATE_BOX, fp);
        write_value_id(vt, value_id, FKL_VM_BOX(v), fp);
    } else if (FKL_IS_HASHTABLE(v)) {
        write_value_create_op(CREATE_HASHTABLE, fp);
        uint8_t eq_type = FKL_VM_HASH(v)->eq_type;
        fwrite(&eq_type, sizeof(eq_type), 1, fp);
        uint32_t count = FKL_VM_HASH(v)->ht.count;
        fwrite(&count, sizeof(count), 1, fp);
        for (const FklValueHashMapNode *cur = FKL_VM_HASH(v)->ht.first; cur;
                cur = cur->next) {
            write_value_id(vt, value_id, cur->k, fp);
            write_value_id(vt, value_id, cur->v, fp);
        }
    } else {
        FKL_UNREACHABLE();
    }
}

void fklWriteValueTable(const FklValueTable *vt, FILE *fp) {
    FklValueHashSet written_values;
    fklValueHashSetInit(&written_values);

    uint32_t count = vt->next_id - 1;
    fwrite(&count, sizeof(count), 1, fp);

    for (const FklValueIdHashMapNode *cur = vt->ht.first; cur;
            cur = cur->next) {
        if (fklValueHashSetHas2(&written_values, cur->k))
            continue;
        fklValueHashSetPut2(&written_values, cur->k);

        write_value_create_instructions(cur->k,
                cur->v,
                vt,
                &written_values,
                fp);
    }

    fklValueHashSetUninit(&written_values);
}

static inline FklVMvalue *load_and_make_values(FILE *fp,
        FklVM *vm,
        ValueId self_id,
        const FklLoadValueArgs *args) {
    uint8_t op = NOP;
    fread(&op, sizeof(op), 1, fp);
    switch ((enum ValueCreateOpcode)op) {
    case NOP:
        break;
    case MAKE_NIL:
        return FKL_VM_NIL;
        break;

    case MAKE_FIX: {
        int64_t i = 0;
        fread(&i, sizeof(i), 1, fp);
        return FKL_MAKE_VM_FIX(i);
    } break;

    case MAKE_CHR: {
        char c = 0;
        fread(&c, sizeof(c), 1, fp);
        return FKL_MAKE_VM_CHR(c);
    } break;

    case MAKE_F64: {
        double f = 0.0;
        fread(&f, sizeof(f), 1, fp);
        return fklCreateVMvalueF64(vm, f);
    } break;
    case MAKE_BIGINT: {
        int64_t num = 0;
        fread(&num, sizeof(num), 1, fp);
        size_t len = fklAbs(num);
        FklVMvalue *bi = fklCreateVMvalueBigInt(vm, len);
        FklVMvalueBigInt *v = FKL_VM_BI(bi);
        v->num = num;
        fread(v->digits, len * sizeof(*(v->digits)), 1, fp);
        return bi;
    } break;

    case MAKE_STR: {
        uint64_t size = 0;
        fread(&size, sizeof(size), 1, fp);
        FklVMvalue *s = fklCreateVMvalueStr2(vm, size, NULL);
        FklString *ss = FKL_VM_STR(s);
        fread(ss->str, size * sizeof(*(ss->str)), 1, fp);
        return s;
    } break;

    case MAKE_SYM: {
        uint8_t interned = 0;
        fread(&interned, sizeof(interned), 1, fp);
        FklString *s = fklLoadString(fp);

        FklVMvalue *r = NULL;
        if (interned) {
            r = fklVMaddSymbol(vm, s);
        } else {
            r = fklCreateVMvalueSym(vm, s);
        }
        fklZfree(s);

        return r;
    } break;

    case MAKE_BYTES: {
        uint64_t size = 0;
        fread(&size, sizeof(size), 1, fp);
        FklVMvalue *b = fklCreateVMvalueBvec2(vm, size, NULL);
        FklBytevector *bb = FKL_VM_BVEC(b);
        fread(bb->ptr, size * sizeof(*(bb->ptr)), 1, fp);
        return b;
    } break;

    case MAKE_SLOT: {
        FklVMvalue *s = load_value_id(fp, args);
        int8_t need_expand = 0;
        fread(&need_expand, sizeof(need_expand), 1, fp);
        return fklCreateVMvalueSlot(vm, s, need_expand);
    } break;

    case MAKE_HEADER_WILDCARD:
        return FKL_VM_HEADER_WILDCARD;
        break;

    case CREATE_PAIR: {
        // 防止循环引用找不到被引用的对象
        FklVMvalue *r = fklCreateVMvaluePair(vm, FKL_VM_NIL, FKL_VM_NIL);
        args->values[self_id - 1] = r;

        FKL_VM_CAR(r) = load_value_id(fp, args);
        FKL_VM_CDR(r) = load_value_id(fp, args);

        return r;
    } break;

    case CREATE_VECTOR: {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);

        FklVMvalue *r = fklCreateVMvalueVec(vm, len);
        args->values[self_id - 1] = r;
        for (uint64_t i = 0; i < len; ++i) {
            FKL_VM_VEC(r)->base[i] = load_value_id(fp, args);
        }

        return r;
    } break;

    case CREATE_BOX: {
        // 防止循环引用找不到被引用的对象
        FklVMvalue *r = fklCreateVMvalueBoxNil(vm);
        args->values[self_id - 1] = r;
        FKL_VM_BOX(r) = load_value_id(fp, args);

        return r;
    } break;

    case CREATE_HASHTABLE: {
        uint8_t eq_type = 0;
        fread(&eq_type, sizeof(eq_type), 1, fp);
        FklVMvalue *r = fklCreateVMvalueHash(vm, eq_type);

        args->values[self_id - 1] = r;
        uint32_t count = 0;
        fread(&count, sizeof(count), 1, fp);

        for (uint32_t i = 0; i < count; ++i) {
            FklVMvalue *k = load_value_id(fp, args);
            FklVMvalue *v = load_value_id(fp, args);
            fklVMhashTableSet(FKL_VM_HASH(r), k, v);
        }

        return r;

    } break;
    }

    return NULL;
}

int fklLoadValueTable(FILE *fp, FklLoadValueArgs *args) {
    FklVM *vm = args->vm;
    fread(&args->count, sizeof(args->count), 1, fp);

    FklVMvalue **values =
            (FklVMvalue **)fklZcalloc(args->count, sizeof(FklVMvalue *));
    FKL_ASSERT(values);
    args->values = values;

    for (uint32_t i = 0; i < args->count; ++i) {
        values[i] = load_and_make_values(fp, vm, i + 1, args);
    }

    return 0;
}

static inline void write_prototype_pass_1(const FklFuncPrototype *pt,
        FklValueTable *vt) {
    FklSymDefHashMapMutElm *defs = pt->refs;
    for (uint32_t i = 0; i < pt->rcount; i++) {
        fklTraverseSerializableValue(vt, defs[i].k.id);
    }

    fklTraverseSerializableValue(vt, pt->name);
    fklTraverseSerializableValue(vt, pt->file);
    for (uint32_t i = 0; i < pt->konsts_count; ++i) {
        fklTraverseSerializableValue(vt, pt->konsts[i]);
    }
}

static inline void write_prototypes_pass_1(const FklVMvalueProtos *pts,
        FklValueTable *vt) {
    const FklFuncPrototypes *p = &pts->p;
    for (uint32_t i = 1; i <= p->count; ++i) {
        const FklFuncPrototype *pt = &p->pa[i];
        write_prototype_pass_1(pt, vt);
    }
}

static inline void write_symbol_def_pass_2(const FklSymDefHashMapMutElm *def,
        const FklValueTable *vt,
        FILE *fp) {
    write_value_id(vt, 0, def->k.id, fp);
    fwrite(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fwrite(&def->v.idx, sizeof(def->v.idx), 1, fp);
    fwrite(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    fwrite(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void write_prototype_pass_2(const FklFuncPrototype *pt,
        const FklValueTable *vt,
        FILE *fp) {
    uint32_t count = pt->lcount;
    fwrite(&count, sizeof(count), 1, fp);
    count = pt->rcount;
    fwrite(&count, sizeof(count), 1, fp);

    FklSymDefHashMapMutElm *defs = pt->refs;
    for (uint32_t i = 0; i < count; i++)
        write_symbol_def_pass_2(&defs[i], vt, fp);
    write_value_id(vt, 0, pt->name, fp);
    write_value_id(vt, 0, pt->file, fp);
    fwrite(&pt->line, sizeof(pt->line), 1, fp);
    fwrite(&pt->konsts_count, sizeof(pt->konsts_count), 1, fp);
    for (uint32_t i = 0; i < pt->konsts_count; ++i) {
        write_value_id(vt, 0, pt->konsts[i], fp);
    }
}
static inline void write_prototypes_pass_2(const FklVMvalueProtos *pts,
        const FklValueTable *vt,
        FILE *fp) {
    uint32_t count = pts->p.count;
    FklFuncPrototype *pta = pts->p.pa;
    fwrite(&count, sizeof(count), 1, fp);
    uint32_t const end = count + 1;
    for (uint32_t i = 1; i < end; i++)
        write_prototype_pass_2(&pta[i], vt, fp);
}

void fklWriteFuncPrototypes(const FklVMvalueProtos *pts,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        write_prototypes_pass_1(pts, vt);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_prototypes_pass_2(pts, vt, fp);
        break;
    }
}

static inline void load_symbol_def(FILE *fp,
        const FklLoadValueArgs *values,
        FklSymDefHashMapMutElm *def) {
    def->k.id = load_value_id(fp, values);
    fread(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fread(&def->v.idx, sizeof(def->v.idx), 1, fp);
    fread(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    fread(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void
load_prototype(FILE *fp, const FklLoadValueArgs *values, FklFuncPrototype *pt) {
    uint32_t count = 0;

    fread(&count, sizeof(count), 1, fp);
    pt->lcount = count;
    fread(&count, sizeof(count), 1, fp);
    pt->rcount = count;
    FklSymDefHashMapMutElm *defs;
    if (count == 0)
        defs = NULL;
    else {
        defs = (FklSymDefHashMapMutElm *)fklZcalloc(count,
                sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(defs);
        for (uint32_t i = 0; i < count; i++)
            load_symbol_def(fp, values, &defs[i]);
    }

    pt->refs = defs;
    pt->name = load_value_id(fp, values);
    pt->file = load_value_id(fp, values);

    fread(&pt->line, sizeof(pt->line), 1, fp);

    uint32_t konsts_count = 0;
    fread(&konsts_count, sizeof(konsts_count), 1, fp);

    if (konsts_count == 0) {
        pt->konsts = NULL;
    } else {
        pt->konsts =
                (FklVMvalue **)fklZmalloc(konsts_count * sizeof(FklVMvalue *));
        FKL_ASSERT(pt->konsts);
    }

    pt->konsts_count = konsts_count;
    for (uint32_t i = 0; i < konsts_count; ++i) {
        pt->konsts[i] = load_value_id(fp, values);
    }
}

int fklLoadFuncPrototypes(FILE *fp,
        const FklLoadValueArgs *values,
        FklVMvalueProtos *pts) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint32_t i = 1; i <= count; i++) {
        uint32_t new_prototype_id = fklInsertEmptyFuncPrototype(&pts->p);
        load_prototype(fp, values, &pts->p.pa[new_prototype_id]);
    }
    return 0;
}

void fklWriteLineNumberTable(const FklLineNumberTableItem *items,
        uint32_t count,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        for (uint32_t i = 0; i < count; i++) {
            fklTraverseSerializableValue(vt, items[i].fid);
        }
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        fwrite(&count, sizeof(count), 1, fp);
        for (uint32_t i = 0; i < count; i++) {
            const FklLineNumberTableItem *n = &items[i];
            write_value_id(vt, 0, n->fid, fp);
            fwrite(&n->scp, sizeof(n->scp), 1, fp);
            fwrite(&n->line, sizeof(n->line), 1, fp);
        }
        break;
    }
}

void fklLoadLineNumberTable(FILE *fp,
        const FklLoadValueArgs *values,
        FklLineNumberTableItem **plist,
        uint32_t *pnum) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    FklLineNumberTableItem *list;
    if (count == 0)
        list = NULL;
    else {
        list = (FklLineNumberTableItem *)fklZmalloc(
                count * sizeof(FklLineNumberTableItem));
        FKL_ASSERT(list);
        for (uint32_t i = 0; i < count; i++) {
            FklLineNumberTableItem *item = &list[i];
            item->fid = load_value_id(fp, values);
            fread(&item->scp, sizeof(item->scp), 1, fp);
            fread(&item->line, sizeof(item->line), 1, fp);
            fklInitLineNumTabNode(&list[i],
                    item->fid,
                    item->scp,
                    item->line,
                    0);
        }
    }
    *plist = list;
    *pnum = count;
}

void fklWriteByteCode(const FklByteCode *bc, FILE *outfp) {
    uint64_t len = bc->len;
    fwrite(&len, sizeof(bc->len), 1, outfp);
    const FklInstruction *end = bc->code + len;
    const FklInstruction *code = bc->code;
    while (code < end) {
        uint8_t op = code->op;
        fwrite(&op, sizeof(op), 1, outfp);
        int ins_len = fklGetOpcodeModeLen(code->op);
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
            FKL_UNREACHABLE();
            break;
        }
        code += ins_len;
    }
}

void fklWriteByteCodelnt(const FklByteCodelnt *bcl,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp) {
    fklWriteLineNumberTable(bcl->l, bcl->ls, vt, pass, fp);
    if (pass == FKL_WRITE_CODE_PASS_SECOND)
        fklWriteByteCode(&bcl->bc, fp);
}

void fklLoadByteCode(FklByteCode *tmp, FILE *fp) {
    uint64_t len = 0;
    fread(&len, sizeof(uint64_t), 1, fp);
    fklInitByteCode(tmp, len);

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
            FKL_UNREACHABLE();
            break;
        }
        code += fklGetOpcodeModeLen(op);
    }
}

int fklLoadByteCodelnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklByteCodelnt *bcl) {
    fklLoadLineNumberTable(fp, values, &bcl->l, &bcl->ls);
    fklLoadByteCode(&bcl->bc, fp);
    return 0;
}

static void write_vm_libs_pass_1(const FklVMvalueLibs *l, FklValueTable *vt) {
    for (size_t i = 1; i <= l->count; ++i) {
        const FklVMlib *lib = &l->libs[i];
        if (FKL_IS_PROC(lib->proc)) {
            const FklByteCodelnt *bcl =
                    FKL_VM_CO(FKL_VM_PROC(lib->proc)->codeObj);
            fklWriteByteCodelnt(bcl, vt, FKL_WRITE_CODE_PASS_FIRST, NULL);
        } else {
            fklTraverseSerializableValue(vt, lib->proc);
        }
    }
}

static void
write_vm_libs_pass_2(const FklVMvalueLibs *l, FklValueTable *vt, FILE *fp) {
    fwrite(&l->count, sizeof(l->count), 1, fp);
    for (size_t i = 1; i <= l->count; ++i) {
        const FklVMlib *lib = &l->libs[i];
        FKL_ASSERT(FKL_IS_PROC(lib->proc) || FKL_IS_STR(lib->proc));
        uint8_t type_byte = FKL_IS_PROC(lib->proc) ? FKL_CODEGEN_LIB_SCRIPT
                                                   : FKL_CODEGEN_LIB_DLL;
        fwrite(&type_byte, sizeof(type_byte), 1, fp);
        switch (type_byte) {
        case FKL_CODEGEN_LIB_SCRIPT: {
            FklVMvalueProc *proc = FKL_VM_PROC(lib->proc);
            fwrite(&lib->epc, sizeof(lib->epc), 1, fp);
            uint32_t proto_id = proc->protoId;
            fwrite(&proto_id, sizeof(proto_id), 1, fp);

            const FklByteCodelnt *bcl =
                    FKL_VM_CO(FKL_VM_PROC(lib->proc)->codeObj);
            fklWriteByteCodelnt(bcl, vt, FKL_WRITE_CODE_PASS_SECOND, fp);
        } break;
        case FKL_CODEGEN_LIB_DLL: {
            write_value_id(vt, 0, lib->proc, fp);
        } break;
        }
    }
}

void fklWriteVMlibs(const FklVMvalueLibs *l,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        write_vm_libs_pass_1(l, vt);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_vm_libs_pass_2(l, vt, fp);
        break;
    }
}

void fklLoadVMlibs(FILE *fp,
        FklVM *vm,
        const FklVMvalueProtos *pts,
        const FklLoadValueArgs *values,
        FklVMvalueLibs *l) {
    fread(&l->count, sizeof(l->count), 1, fp);
    fklVMvalueLibsReserve(l, l->count);

    for (uint64_t i = 1; i <= l->count; ++i) {
        FklVMlib *lib = &l->libs[i];
        memset(lib, 0, sizeof(*lib));
        uint8_t type_byte = 0;
        fread(&type_byte, sizeof(type_byte), 1, fp);
        switch ((FklCgLibType)type_byte) {
        case FKL_CODEGEN_LIB_SCRIPT: {
            fread(&lib->epc, sizeof(lib->epc), 1, fp);
            uint32_t proto_id = 0;
            fread(&proto_id, sizeof(proto_id), 1, fp);

            FklVMvalue *bcl = fklCreateVMvalueCodeObj1(vm);
            fklLoadByteCodelnt(fp, values, FKL_VM_CO(bcl));

            FklVMvalue *p = fklCreateVMvalueProc(vm, bcl, proto_id, pts);
            fklInitMainProcRefs(vm, p);

            lib->proc = p;
        } break;
        case FKL_CODEGEN_LIB_DLL: {
            lib->proc = load_value_id(fp, values);
        } break;
        default:
            FKL_UNREACHABLE();
            break;
        }
    }
}

static inline void write_code_file_passes(FILE *fp,
        FklValueTable *value_table,
        FklWriteCodePass pass,
        const FklWriteCodeFileArgs *args) {
    fklWriteFuncPrototypes(args->pts, value_table, pass, fp);
    fklWriteByteCodelnt(args->main_func, value_table, pass, fp);
    fklWriteVMlibs(args->libs, value_table, pass, fp);
}

void fklWriteCodeFile(FILE *fp, const FklWriteCodeFileArgs *const args) {
    FklValueTable value_table;
    fklInitValueTable(&value_table);

    write_code_file_passes(fp, &value_table, FKL_WRITE_CODE_PASS_FIRST, args);

    fklWriteValueTable(&value_table, fp);

    write_code_file_passes(fp, &value_table, FKL_WRITE_CODE_PASS_SECOND, args);

    fklUninitValueTable(&value_table);
}

int fklLoadCodeFile(FILE *fp, FklLoadCodeFileArgs *const args) {
    FklVM *vm = args->vm;
    FklLoadValueArgs load_values = { .vm = vm };

    fklLoadValueTable(fp, &load_values);

    fklLoadFuncPrototypes(fp, &load_values, args->pts);
    FklVMvalue *main_code = fklCreateVMvalueCodeObj1(vm);
    fklLoadByteCodelnt(fp, &load_values, FKL_VM_CO(main_code));

    args->main_func = fklCreateVMvalueProc(vm, main_code, 1, args->pts);
    fklInitMainProcRefs(vm, args->main_func);

    fklLoadVMlibs(fp, vm, args->pts, &load_values, args->libs);

    load_values.count = 0;
    fklZfree(load_values.values);
    return 0;
}

// write pre compile file

static inline void write_nonterm(const FklGrammerNonterm *nt,
        FklWriteCodePass pass,
        FklValueTable *vt,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        fklTraverseSerializableValue(vt, nt->group);
        fklTraverseSerializableValue(vt, nt->sid);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_value_id(vt, 0, nt->group, fp);
        write_value_id(vt, 0, nt->sid, fp);
        break;
    }
}

static inline void write_production_rule_action_pass_1(
        const FklGrammerProduction *prod,
        FklValueTable *vt) {
    if (fklIsCustomActionProd(prod)) {
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        fklWriteByteCodelnt(FKL_VM_CO(ctx->bcl),
                vt,
                FKL_WRITE_CODE_PASS_FIRST,
                NULL);
    } else if (fklIsSimpleActionProd(prod)) {
        FklVMvalueSimpleActCtx *ctx = prod->ctx;
        fklTraverseSerializableValue(vt, ctx->vec);
    } else {
        // is replace or builtin prod
        FklVMvalue *r = prod->ctx;
        fklTraverseSerializableValue(vt, r);
    }
}

static inline void write_production_rule_action_pass_2(
        const FklGrammerProduction *prod,
        FklValueTable *vt,
        FILE *fp) {
    uint8_t type;
    if (fklIsCustomActionProd(prod)) {
        type = FKL_CODEGEN_PROD_CUSTOM;
        fwrite(&type, sizeof(type), 1, fp);
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        fwrite(&ctx->prototype_id, sizeof(ctx->prototype_id), 1, fp);
        fklWriteByteCodelnt(FKL_VM_CO(ctx->bcl),
                vt,
                FKL_WRITE_CODE_PASS_SECOND,
                fp);
    } else if (fklIsSimpleActionProd(prod)) {
        type = FKL_CODEGEN_PROD_SIMPLE;
        fwrite(&type, sizeof(type), 1, fp);
        FklVMvalueSimpleActCtx *ctx = prod->ctx;
        write_value_id(vt, 0, ctx->vec, fp);
    } else {
        type = fklIsReplaceActionProd(prod) ? FKL_CODEGEN_PROD_REPLACE
                                            : FKL_CODEGEN_PROD_BUILTIN;
        fwrite(&type, sizeof(type), 1, fp);
        write_value_id(vt, 0, prod->ctx, fp);
    }
}

static inline void write_grammer_in_binary_pass_1(const FklGrammer *g,
        FklValueTable *vt) {
    FKL_ASSERT(g->start.group == NULL && g->start.sid == NULL);
    for (const FklProdHashMapNode *cur = g->productions.first; cur;
            cur = cur->next) {
        FKL_ASSERT(!(cur->k.group == NULL && cur->k.sid == NULL));

        write_nonterm(&cur->k, FKL_WRITE_CODE_PASS_FIRST, vt, NULL);

        for (const FklGrammerProduction *prod = cur->v; prod;
                prod = prod->next) {
            for (size_t i = 0; i < prod->len; ++i) {
                const FklGrammerSym *cur = &prod->syms[i];
                switch (cur->type) {
                case FKL_TERM_NONTERM:
                    write_nonterm(&cur->nt,
                            FKL_WRITE_CODE_PASS_FIRST,
                            vt,
                            NULL);
                    break;
                case FKL_TERM_STRING:
                case FKL_TERM_KEYWORD:
                case FKL_TERM_REGEX:
                case FKL_TERM_BUILTIN:
                case FKL_TERM_IGNORE:
                    break;
                case FKL_TERM_EOF:
                case FKL_TERM_NONE:
                    FKL_UNREACHABLE();
                }
            }

            write_production_rule_action_pass_1(prod, vt);
        }
    }
}

static inline void write_grammer_in_binary_pass_2(const FklGrammer *g,
        FklValueTable *vt,
        FILE *fp) {
    FKL_ASSERT(g->start.group == 0 && g->start.sid == 0);
    fklWriteStringTable(&g->delimiters, fp);
    uint64_t left_count = g->productions.count;
    fwrite(&left_count, sizeof(left_count), 1, fp);
    for (const FklProdHashMapNode *cur = g->productions.first; cur;
            cur = cur->next) {
        FKL_ASSERT(!(cur->k.group == 0 && cur->k.sid == 0));

        {
            uint64_t prod_count = 0;
            for (const FklGrammerProduction *prod = cur->v; prod;
                    prod = prod->next, ++prod_count)
                ;
            fwrite(&prod_count, sizeof(prod_count), 1, fp);
        }

        write_nonterm(&cur->k, FKL_WRITE_CODE_PASS_SECOND, vt, fp);

        for (const FklGrammerProduction *prod = cur->v; prod;
                prod = prod->next) {
            uint64_t prod_len = prod->len;
            fwrite(&prod_len, sizeof(prod_len), 1, fp);
            for (size_t i = 0; i < prod_len; ++i) {
                const FklGrammerSym *cur = &prod->syms[i];
                fwrite(&cur->type, 1, 1, fp);
                switch (cur->type) {
                case FKL_TERM_NONTERM:
                    write_nonterm(&cur->nt, FKL_WRITE_CODE_PASS_SECOND, vt, fp);
                    break;
                case FKL_TERM_STRING:
                case FKL_TERM_KEYWORD: {
                    const FklString *t = cur->str;
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                } break;

                case FKL_TERM_REGEX: {
                    const FklString *t =
                            fklGetStringWithRegex(&g->regexes, cur->re, NULL);
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                } break;

                case FKL_TERM_BUILTIN: {
                    uint64_t str_len = strlen(cur->b.t->name);
                    fwrite(&str_len, sizeof(str_len), 1, fp);
                    fputs(cur->b.t->name, fp);

                    uint64_t len = cur->b.len;
                    fwrite(&len, sizeof(len), 1, fp);
                    for (size_t i = 0; i < len; ++i) {
                        const FklString *t = cur->b.args[i];
                        fwrite(t, sizeof(t->size) + t->size, 1, fp);
                    }
                } break;

                case FKL_TERM_IGNORE:
                    break;
                case FKL_TERM_EOF:
                case FKL_TERM_NONE:
                    FKL_UNREACHABLE();
                }
            }

            write_production_rule_action_pass_2(prod, vt, fp);
        }
    }

    {
        uint64_t ignore_count = 0;
        for (const FklGrammerIgnore *ig = g->ignores; ig;
                ig = ig->next, ++ignore_count)
            ;
        fwrite(&ignore_count, sizeof(ignore_count), 1, fp);
    }

    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        uint64_t len = ig->len;
        fwrite(&len, sizeof(len), 1, fp);
        for (uint64_t i = 0; i < len; ++i) {
            const FklGrammerIgnoreSym *cur = &ig->ig[i];
            fwrite(&cur->term_type, 1, 1, fp);
            switch (cur->term_type) {

            case FKL_TERM_STRING: {
                const FklString *t = cur->str;
                fwrite(t, sizeof(t->size) + t->size, 1, fp);
            } break;

            case FKL_TERM_REGEX: {
                const FklString *t =
                        fklGetStringWithRegex(&g->regexes, cur->re, NULL);
                fwrite(t, sizeof(t->size) + t->size, 1, fp);
            } break;

            case FKL_TERM_BUILTIN: {
                uint64_t str_len = strlen(cur->b.t->name);
                fwrite(&str_len, sizeof(str_len), 1, fp);
                fputs(cur->b.t->name, fp);

                uint64_t len = cur->b.len;
                fwrite(&len, sizeof(len), 1, fp);
                for (size_t i = 0; i < len; ++i) {
                    const FklString *t = cur->b.args[i];
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                }
            } break;

            case FKL_TERM_NONTERM:
            case FKL_TERM_KEYWORD:
            case FKL_TERM_IGNORE:
            case FKL_TERM_EOF:
            case FKL_TERM_NONE:
                FKL_UNREACHABLE();
            }
        }
    }
}

static inline void write_named_prods_pass_1(
        const FklGraProdGroupHashMap *named_prod_groups,
        FklValueTable *vt) {
    if (named_prod_groups == NULL)
        return;

    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
            list = list->next) {
        fklTraverseSerializableValue(vt, list->k);
        write_grammer_in_binary_pass_1(&list->v.g, vt);
    }
}

static inline void write_named_prods_pass_2(
        const FklGraProdGroupHashMap *named_prod_groups,
        FklValueTable *vt,
        FILE *fp) {
    uint8_t has_named_prod = named_prod_groups != NULL //
                          && named_prod_groups->buckets != NULL;
    fwrite(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (!has_named_prod)
        return;
    fwrite(&named_prod_groups->count, sizeof(named_prod_groups->count), 1, fp);
    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
            list = list->next) {
        write_value_id(vt, 0, list->k, fp);
        write_grammer_in_binary_pass_2(&list->v.g, vt, fp);
    }
}

static inline void load_nonterm(FILE *fp,
        const FklLoadValueArgs *const values,
        FklGrammerNonterm *nt) {
    nt->group = load_value_id(fp, values);
    nt->sid = load_value_id(fp, values);
}

static inline FklGrammerProduction *read_production_rule_action(FILE *fp,
        uint64_t prod_len,
        FklCgCtx *ctx,
        const FklGrammerNonterm *nt,
        const FklGrammerSym *syms,
        const FklLoadValueArgs *const values) {
    uint8_t type = 255;
    fread(&type, sizeof(type), 1, fp);
    FKL_ASSERT(type != 255);
    FklGrammerProduction *prod = NULL;
    switch ((FklCgProdActionType)type) {
    default:
        FKL_UNREACHABLE();
        break;
    case FKL_CODEGEN_PROD_BUILTIN: {
        FklVMvalue *id = load_value_id(fp, values);
        prod = fklCreateBuiltinActionProd(ctx,
                nt->group,
                nt->sid,
                prod_len,
                syms,
                id);
        FKL_ASSERT(prod);
    } break;

    case FKL_CODEGEN_PROD_CUSTOM: {
        prod = fklCreateCustomActionProd(ctx,
                nt->group,
                nt->sid,
                prod_len,
                syms,
                0);
        FklVMvalueCustomActCtx *actx = prod->ctx;
        fread(&actx->prototype_id, sizeof(actx->prototype_id), 1, fp);
        actx->bcl = fklCreateVMvalueCodeObj1(ctx->vm);
        fklLoadByteCodelnt(fp, values, FKL_VM_CO(actx->bcl));
    } break;

    case FKL_CODEGEN_PROD_SIMPLE: {
        FklVMvalue *action_ast = load_value_id(fp, values);
        prod = fklCreateSimpleActionProd(ctx,
                nt->group,
                nt->sid,
                prod_len,
                syms,
                action_ast);
        FKL_ASSERT(prod);
    } break;

    case FKL_CODEGEN_PROD_REPLACE: {
        FklVMvalue *v = load_value_id(fp, values);
        prod = fklCreateReplaceActionProd(nt->group,
                nt->sid,
                prod_len,
                syms,
                v);
        FKL_ASSERT(prod);
    } break;
    }

    return prod;
}

static inline void load_grammer_in_binary(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        FklGrammer *g) {
    fklLoadStringTable(fp, &g->delimiters);
    uint64_t left_count = 0;
    fread(&left_count, sizeof(left_count), 1, fp);
    for (uint64_t i = 0; i < left_count; ++i) {
        FklGrammerNonterm nt;
        uint64_t prod_count = 0;
        fread(&prod_count, sizeof(prod_count), 1, fp);

        load_nonterm(fp, values, &nt);
        FKL_ASSERT(!(nt.group == NULL && nt.sid == NULL));

        for (uint64_t i = 0; i < prod_count; ++i) {
            uint64_t prod_len;
            fread(&prod_len, sizeof(prod_len), 1, fp);

            FklGrammerSym *syms = (FklGrammerSym *)fklZmalloc(
                    prod_len * sizeof(FklGrammerSym));
            FKL_ASSERT(syms);

            for (size_t i = 0; i < prod_len; ++i) {
                uint8_t type;
                fread(&type, sizeof(type), 1, fp);
                FklGrammerSym *cur = &syms[i];
                cur->type = type;
                switch (cur->type) {
                case FKL_TERM_NONTERM:
                    load_nonterm(fp, values, &cur->nt);
                    break;

                case FKL_TERM_STRING:
                case FKL_TERM_KEYWORD: {
                    FklString *str = fklLoadString(fp);
                    cur->str = fklAddString(&g->terminals, str);
                    if (type == FKL_TERM_STRING)
                        fklAddString(&g->delimiters, str);
                    fklZfree(str);
                } break;

                case FKL_TERM_BUILTIN: {
                    FklString *str = fklLoadString(fp);
                    FklVMvalue *id = fklVMaddSymbol(ctx->vm, str);
                    fklZfree(str);
                    const FklLalrBuiltinMatch *t =
                            fklGetBuiltinMatch(&g->builtins, id);
                    FKL_ASSERT(t);
                    cur->b.t = t;

                    uint64_t len = 0;
                    fread(&len, sizeof(len), 1, fp);
                    cur->b.len = len;
                    cur->b.args = fklZmalloc(len * sizeof(FklString *));
                    FKL_ASSERT(cur->b.args);
                    for (size_t i = 0; i < len; ++len) {
                        FklString *s = fklLoadString(fp);
                        cur->b.args[i] = fklAddString(&g->terminals, s);
                        fklAddString(&g->delimiters, s);
                        fklZfree(s);
                    }
                    fklZfree(str);
                } break;

                case FKL_TERM_REGEX: {
                    FklString *s = fklLoadString(fp);
                    cur->re = fklAddRegexStr(&g->regexes, s);
                    FKL_ASSERT(cur->re);
                    fklZfree(s);
                } break;

                case FKL_TERM_IGNORE:
                    break;
                case FKL_TERM_NONE:
                case FKL_TERM_EOF:
                    FKL_UNREACHABLE();
                    break;
                }
            }

            FklGrammerProduction *prod = read_production_rule_action(fp,
                    prod_len,
                    ctx,
                    &nt,
                    syms,
                    values);

            if (fklAddProdToProdTableNoRepeat(g, prod)) {
                FKL_UNREACHABLE();
            }

            fklZfree(syms);
            syms = NULL;
        }
    }

    uint64_t ignore_count = 0;
    fread(&ignore_count, sizeof(ignore_count), 1, fp);
    for (size_t i = 0; i < ignore_count; ++i) {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        FklGrammerIgnore *ig = fklCreateEmptyGrammerIgnore(len);
        for (uint64_t i = 0; i < len; ++i) {
            FklGrammerIgnoreSym *cur = &ig->ig[i];
            fread(&cur->term_type, 1, 1, fp);
            switch (cur->term_type) {
            case FKL_TERM_STRING: {
                FklString *str = fklLoadString(fp);
                cur->str = fklAddString(&g->terminals, str);
                fklAddString(&g->delimiters, str);
                fklZfree(str);
            } break;
            case FKL_TERM_REGEX: {
                FklString *str = fklLoadString(fp);
                cur->re = fklAddRegexStr(&g->regexes, str);
                FKL_ASSERT(cur->re);
                fklZfree(str);
            } break;

            case FKL_TERM_BUILTIN: {
                FklString *str = fklLoadString(fp);
                FklVMvalue *id = fklVMaddSymbol(ctx->vm, str);
                fklZfree(str);
                const FklLalrBuiltinMatch *t =
                        fklGetBuiltinMatch(&g->builtins, id);
                FKL_ASSERT(t);
                cur->b.t = t;

                uint64_t len = 0;
                fread(&len, sizeof(len), 1, fp);
                cur->b.len = len;
                cur->b.args = fklZmalloc(len * sizeof(FklString *));
                FKL_ASSERT(cur->b.args);
                for (size_t i = 0; i < len; ++len) {
                    FklString *s = fklLoadString(fp);
                    cur->b.args[i] = fklAddString(&g->terminals, s);
                    fklAddString(&g->delimiters, s);
                    fklZfree(s);
                }
                fklZfree(str);
            } break;

            case FKL_TERM_NONTERM:
            case FKL_TERM_KEYWORD:
            case FKL_TERM_IGNORE:
            case FKL_TERM_EOF:
            case FKL_TERM_NONE:
                FKL_UNREACHABLE();
            }
        }

        if (fklAddIgnoreToIgnoreList(&g->ignores, ig)) {
            fklDestroyIgnore(ig);
        }
    }
}

static inline FklGraProdGroupHashMap *load_named_prods(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values) {
    uint8_t has_named_prod = 0;
    fread(&has_named_prod, sizeof(has_named_prod), 1, fp);
    FklGraProdGroupHashMap *ht = fklGraProdGroupHashMapCreate();
    if (!has_named_prod)
        return ht;

    uint32_t num = 0;
    fread(&num, sizeof(num), 1, fp);
    for (; num > 0; num--) {
        FklVMvalue *group_id = load_value_id(fp, values);
        FklGrammerProdGroupItem *group = add_production_group(ht, //
                ctx->vm,
                group_id);
        load_grammer_in_binary(fp, ctx, values, &group->g);
    }

    return ht;
}

static inline char *load_script_lib_path(const char *main_dir, FILE *fp) {
    FklStrBuf buf;
    fklInitStrBuf(&buf);
    fklStrBufConcatWithCstr(&buf, main_dir);
    int ch = fgetc(fp);
    for (;;) {
        while (ch) {
            fklStrBufPutc(&buf, ch);
            ch = fgetc(fp);
        }
        ch = fgetc(fp);
        if (!ch)
            break;
        fklStrBufPutc(&buf, FKL_PATH_SEPARATOR);
    }

    fklStrBufPutc(&buf, FKL_PRE_COMPILE_FKL_SUFFIX);

    char *path = fklZstrdup(buf.buf);
    fklUninitStrBuf(&buf);
    return path;
}

static inline void load_export_sid_idx_table(FILE *fp,
        const FklLoadValueArgs *values,
        FklCgExportSidIdxHashMap *t) {
    fklCgExportSidIdxHashMapInit(t);
    fread(&t->count, sizeof(t->count), 1, fp);
    uint32_t num = t->count;
    t->count = 0;
    for (uint32_t i = 0; i < num; i++) {
        FklCgExportIdx idxs = { 0 };
        FklVMvalue *sid = load_value_id(fp, values);
        fread(&idxs.idx, sizeof(idxs.idx), 1, fp);
        fread(&idxs.oidx, sizeof(idxs.oidx), 1, fp);
        fklCgExportSidIdxHashMapPut(t, &sid, &idxs);
    }
}

static inline FklMacroHashMap *load_compiler_macros(FklVM *exe,
        FILE *fp,
        const FklLoadValueArgs *const values) {
    FklMacroHashMap *macros = fklMacroHashMapCreate();
    uint64_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint64_t i = 0; i < count; ++i) {
        FklVMvalue *head = load_value_id(fp, values);
        FklCgMacro **pr = fklMacroHashMapAdd1(macros, head);

        uint64_t count = 0;
        fread(&count, sizeof(count), 1, fp);
        for (uint64_t j = 0; j < count; ++j) {
            FklVMvalue *pattern = load_value_id(fp, values);
            uint32_t prototype_id = 0;
            fread(&prototype_id, sizeof(prototype_id), 1, fp);
            FklVMvalue *bcl = fklCreateVMvalueCodeObj1(exe);
            fklLoadByteCodelnt(fp, values, FKL_VM_CO(bcl));

            FklCgMacro *cur = fklCreateCgMacro(pattern, //
                    bcl,                                //
                    NULL,                               //
                    prototype_id);

            *pr = cur;
            pr = &cur->next;
        }
    }
    return macros;
}

static inline FklReplacementHashMap *load_replacements(FILE *fp,
        const FklLoadValueArgs *const values) {
    FklReplacementHashMap *ht = fklReplacementHashMapCreate();
    fread(&ht->count, sizeof(ht->count), 1, fp);
    uint32_t num = ht->count;
    ht->count = 0;
    for (uint32_t i = 0; i < num; i++) {
        FklVMvalue *id = load_value_id(fp, values);
        FklVMvalue *v = load_value_id(fp, values);
        *fklReplacementHashMapAdd1(ht, id) = v;
    }
    return ht;
}

static inline void load_script_lib_from_pre_compile(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        FklCgLib *lib,
        FklLoadPreCompileArgs *const args) {
    memset(lib, 0, sizeof(*lib));
    lib->type = FKL_CODEGEN_LIB_SCRIPT;
    lib->rp = load_script_lib_path(main_dir, fp);
    fread(&lib->prototypeId, sizeof(lib->prototypeId), 1, fp);
    FklVMvalue *bcl = fklCreateVMvalueCodeObj1(args->ctx->vm);
    fklLoadByteCodelnt(fp, values, FKL_VM_CO(bcl));
    lib->bcl = bcl;
    fread(&lib->epc, sizeof(lib->epc), 1, fp);
    load_export_sid_idx_table(fp, values, &lib->exports);
    lib->macros = load_compiler_macros(args->ctx->vm, fp, values);
    lib->replacements = load_replacements(fp, values);
    lib->named_prod_groups = load_named_prods(fp, args->ctx, values);
}

static inline char *load_dll_lib_path(const char *main_dir, FILE *fp) {
    FklStrBuf buf;
    fklInitStrBuf(&buf);
    fklStrBufConcatWithCstr(&buf, main_dir);
    int ch = fgetc(fp);
    for (;;) {
        while (ch) {
            fklStrBufPutc(&buf, ch);
            ch = fgetc(fp);
        }
        ch = fgetc(fp);
        if (!ch)
            break;
        fklStrBufPutc(&buf, FKL_PATH_SEPARATOR);
    }

    fklStrBufConcatWithCstr(&buf, FKL_DLL_FILE_TYPE);

    char *path = fklZstrdup(buf.buf);
    fklUninitStrBuf(&buf);
    char *rp = fklRealpath(path);
    fklZfree(path);
    return rp;
}

static inline int load_dll_lib_from_pre_compile(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        FklCgLib *lib,
        FklLoadPreCompileArgs *const args) {
    memset(lib, 0, sizeof(*lib));
    lib->type = FKL_CODEGEN_LIB_DLL;
    lib->rp = load_dll_lib_path(main_dir, fp);
    if (!lib->rp || !fklIsAccessibleRegFile(lib->rp)) {
        fklZfree(lib->rp);
        return 1;
    }

    if (uv_dlopen(lib->rp, &lib->dll)) {
        args->error = fklZstrdup(uv_dlerror(&lib->dll));
        uv_dlclose(&lib->dll);
        fklZfree(lib->rp);
        return 1;
    }
    load_export_sid_idx_table(fp, values, &lib->exports);
    return 0;
}

static inline int load_lib_from_pre_compile(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        FklCgLib *lib,
        FklLoadPreCompileArgs *const args) {
    uint8_t type = 0;
    fread(&type, sizeof(type), 1, fp);
    lib->type = type;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        load_script_lib_from_pre_compile(fp, main_dir, values, lib, args);
        break;
    case FKL_CODEGEN_LIB_DLL:
        if (load_dll_lib_from_pre_compile(fp, main_dir, values, lib, args))
            return 1;
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static inline FklVMvalueCgLibs *load_lib_vector(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        FklLoadPreCompileArgs *const args) {
    uint64_t num = 0;
    fread(&num, sizeof(num), 1, fp);
    FklVMvalueCgLibs *libs = fklCreateVMvalueCgLibs1(args->ctx->vm, num);
    for (size_t i = 0; i < num; i++) {
        FklCgLib lib = { 0 };
        if (load_lib_from_pre_compile(fp, main_dir, values, &lib, args))
            return NULL;
        fklVMvalueCgLibsEmplaceBack(libs, &lib);
    }
    return libs;
}

static inline FklVMvalueCgLibs *load_lib_vector_and_main(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        FklLoadPreCompileArgs *const args) {
    FklVMvalueCgLibs *l = load_lib_vector(fp, main_dir, values, args);
    if (l == NULL)
        return NULL;
    FklCgLib main_lib = { 0 };
    load_script_lib_from_pre_compile(fp, main_dir, values, &main_lib, args);
    fklVMvalueCgLibsEmplaceBack(l, &main_lib);
    return l;
}

#define IS_LOAD_DLL_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_DLL && (OP) <= FKL_OP_LOAD_DLL_X)
#define IS_LOAD_LIB_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_LIB && (OP) <= FKL_OP_LOAD_LIB_X)
#define IS_PUSH_PROC_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_PROC && (OP) <= FKL_OP_PUSH_PROC_XXX)
#define IS_EXPORT_TO_OP(OP)                                                    \
    ((OP) >= FKL_OP_EXPORT_TO && (OP) <= FKL_OP_EXPORT_TO_XX)

typedef struct {
    uint32_t lib_count;
    uint32_t pts_count;
} IncCtx;

static int increase_bcl_lib_prototype_id_predicate(FklOpcode op) {
    return IS_LOAD_LIB_OP(op) || IS_LOAD_DLL_OP(op) || IS_PUSH_PROC_OP(op)
        || IS_EXPORT_TO_OP(op);
}

static int increase_bcl_lib_prototype_id_func(void *cctx,
        FklOpcode *popcode,
        FklOpcodeMode *pmode,
        FklInstructionArg *ins_arg) {
    IncCtx *ctx = cctx;
    FklOpcode op = *popcode;
    if (IS_LOAD_DLL_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_DLL;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_LOAD_LIB_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_LIB;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_EXPORT_TO_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_EXPORT_TO;
        *pmode = FKL_OP_MODE_IuAuB;
    } else if (IS_PUSH_PROC_OP(op)) {
        ins_arg->ux += ctx->pts_count;
        *popcode = FKL_OP_PUSH_PROC;
        *pmode = FKL_OP_MODE_IuAuB;
    }
    return 0;
}

static inline void increase_bcl_lib_prototype_id(FklByteCodelnt *bcl,
        const IncCtx *args) {
    fklRecomputeInsImm(bcl,
            FKL_TYPE_CAST(void *, args),
            increase_bcl_lib_prototype_id_predicate,
            increase_bcl_lib_prototype_id_func);
}

static inline void //
increase_compiler_macros_lib_prototype_id(FklMacroHashMap *macros,
        const IncCtx *args) {
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        for (FklCgMacro *c = cur->v; c; c = c->next) {
            c->prototype_id += args->pts_count;
            increase_bcl_lib_prototype_id(FKL_VM_CO(c->bcl), args);
        }
    }
}

static inline void increase_prototype_and_lib_id(FklVMvalueCgLibs *libraries,
        const IncCtx *args) {
    uint32_t top = fklVMvalueCgLibsCount(libraries);
    FklCgLib *base = fklVMvalueCgLibs(libraries);
    for (uint32_t i = 0; i < top; i++) {
        FklCgLib *lib = &base[i];
        if (lib->type == FKL_CODEGEN_LIB_SCRIPT) {
            lib->prototypeId += args->pts_count;
            increase_bcl_lib_prototype_id(FKL_VM_CO(lib->bcl), args);
        }
    }
}

static inline void increase_reader_macro_lib_prototype_id(
        FklGraProdGroupHashMap *named_prod_groups,
        const IncCtx *args) {
    if (named_prod_groups == NULL || named_prod_groups->buckets == NULL)
        return;
    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
            list = list->next) {

        for (const FklProdHashMapNode *cur = list->v.g.productions.first; cur;
                cur = cur->next) {
            for (const FklGrammerProduction *prod = cur->v; prod;
                    prod = prod->next) {
                if (fklIsCustomActionProd(prod)) {
                    FklVMvalueCustomActCtx *ctx = prod->ctx;
                    ctx->prototype_id += args->pts_count;
                    increase_bcl_lib_prototype_id(FKL_VM_CO(ctx->bcl), args);
                }
            }
        }
    }
}

static inline void increase_macro_prototype_and_lib_id(
        FklVMvalueCgLibs *libraries,
        const IncCtx *args) {
    uint32_t top = fklVMvalueCgLibsCount(libraries);
    FklCgLib *base = fklVMvalueCgLibs(libraries);
    for (uint32_t i = 0; i < top; i++) {
        FklCgLib *lib = &base[i];
        if (lib->type == FKL_CODEGEN_LIB_SCRIPT) {
            increase_compiler_macros_lib_prototype_id(lib->macros, args);
            increase_reader_macro_lib_prototype_id(lib->named_prod_groups,
                    args);
        }
    }
}

static inline void merge_prototypes(FklVMvalueProtos *a,
        const FklVMvalueProtos *b) {
    FklFuncPrototypes *out = &a->p;
    const FklFuncPrototypes *in = &b->p;
    uint32_t o_count = out->count;
    out->count += in->count;
    FklFuncPrototype *pa = (FklFuncPrototype *)fklZrealloc(out->pa,
            (out->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pa);
    out->pa = pa;
    uint32_t i = o_count + 1;
    memcpy(&pa[i], &in->pa[1], in->count * sizeof(FklFuncPrototype));
    memset(&in->pa[1], 0, in->count * sizeof(FklFuncPrototype));
}

static inline void write_export_sid_idx_table_pass_2(
        const FklCgExportSidIdxHashMap *t,
        const FklValueTable *vt,
        FILE *fp) {
    fwrite(&t->count, sizeof(t->count), 1, fp);
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        write_value_id(vt, 0, sid_idx->k, fp);
        fwrite(&sid_idx->v.idx, sizeof(sid_idx->v.idx), 1, fp);
        fwrite(&sid_idx->v.oidx, sizeof(sid_idx->v.oidx), 1, fp);
    }
}

static inline void write_export_sid_idx_table_pass_1(
        const FklCgExportSidIdxHashMap *t,
        FklValueTable *vt) {
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        fklTraverseSerializableValue(vt, sid_idx->k);
    }
}

static inline void write_compiler_macros_pass_1(const FklMacroHashMap *macros,
        FklValueTable *vt) {
    if (macros == NULL)
        return;
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        fklTraverseSerializableValue(vt, cur->k);
        for (const FklCgMacro *c = cur->v; c; c = c->next) {
            fklTraverseSerializableValue(vt, c->pattern);
            fklWriteByteCodelnt(FKL_VM_CO(c->bcl),
                    vt,
                    FKL_WRITE_CODE_PASS_FIRST,
                    NULL);
        }
    }
}

static inline void write_compiler_macros_pass_2(const FklMacroHashMap *macros,
        FklValueTable *vt,
        FILE *fp) {
    uint64_t count = 0;
    if (macros == NULL) {
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }

    count = macros->count;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        write_value_id(vt, 0, cur->k, fp);

        uint64_t count = 0;
        for (const FklCgMacro *c = cur->v; c; c = c->next)
            count++;
        fwrite(&count, sizeof(count), 1, fp);
        for (const FklCgMacro *c = cur->v; c; c = c->next) {
            write_value_id(vt, 0, c->pattern, fp);
            fwrite(&c->prototype_id, sizeof(c->prototype_id), 1, fp);
            fklWriteByteCodelnt(FKL_VM_CO(c->bcl),
                    vt,
                    FKL_WRITE_CODE_PASS_SECOND,
                    fp);
        }
    }
}

static inline void write_replacements_pass_1(const FklReplacementHashMap *ht,
        FklValueTable *vt) {
    if (ht == NULL)
        return;
    for (const FklReplacementHashMapNode *rep_list = ht->first; rep_list;
            rep_list = rep_list->next) {
        fklTraverseSerializableValue(vt, rep_list->k);
        fklTraverseSerializableValue(vt, rep_list->v);
    }
}

static inline void write_replacements_pass_2(const FklReplacementHashMap *ht,
        const FklValueTable *vt,
        FILE *fp) {
    if (ht == NULL) {
        uint32_t count = 0;
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }
    fwrite(&ht->count, sizeof(ht->count), 1, fp);
    for (const FklReplacementHashMapNode *rep_list = ht->first; rep_list;
            rep_list = rep_list->next) {
        write_value_id(vt, 0, rep_list->k, fp);
        write_value_id(vt, 0, rep_list->v, fp);
    }
}

static inline void write_codegen_script_lib_path(const char *rp,
        const char *main_dir,
        FILE *outfp) {
    char *relpath = fklRelpath(main_dir, rp);
    size_t count = 0;
    char **slices = fklSplit(relpath, FKL_PATH_SEPARATOR_STR, &count);

    for (size_t i = 0; i < count; i++) {
        fputs(slices[i], outfp);
        fputc('\0', outfp);
    }
    fputc('\0', outfp);
    fklZfree(relpath);
    fklZfree(slices);
}

static inline void write_codegen_dll_lib_path(const FklCgLib *lib,
        const char *main_dir,
        FILE *outfp) {
    char *relpath = fklRelpath(main_dir, lib->rp);
    size_t count = 0;
    char **slices = fklSplit(relpath, FKL_PATH_SEPARATOR_STR, &count);
    count--;

    for (size_t i = 0; i < count; i++) {
        fputs(slices[i], outfp);
        fputc('\0', outfp);
    }
    uint64_t len = strlen(slices[count]) - FKL_DLL_FILE_TYPE_STR_LEN;
    fwrite(slices[count], len, 1, outfp);
    fputc('\0', outfp);
    fputc('\0', outfp);
    fklZfree(relpath);
    fklZfree(slices);
}

static inline void write_codegen_script_lib_pass_1(const FklCgLib *lib,
        FklValueTable *vt) {
    fklWriteByteCodelnt(FKL_VM_CO(lib->bcl),
            vt,
            FKL_WRITE_CODE_PASS_FIRST,
            NULL);
    write_export_sid_idx_table_pass_1(&lib->exports, vt);
    write_compiler_macros_pass_1(lib->macros, vt);
    write_replacements_pass_1(lib->replacements, vt);
    write_named_prods_pass_1(lib->named_prod_groups, vt);
}

static inline void write_codegen_dll_lib_pass_1(const FklCgLib *lib,
        FklValueTable *vt) {
    write_export_sid_idx_table_pass_1(&lib->exports, vt);
}

static inline void write_codegen_dll_lib_pass_2(const FklCgLib *lib,
        const char *main_dir,
        const char *target_dir,
        const FklValueTable *vt,
        FILE *outfp) {
    write_codegen_dll_lib_path(lib, target_dir ? target_dir : main_dir, outfp);
    write_export_sid_idx_table_pass_2(&lib->exports, vt, outfp);
}

static inline void write_codegen_lib_pass_1(const FklCgLib *lib,
        FklValueTable *vt) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        write_codegen_script_lib_pass_1(lib, vt);
        break;
    case FKL_CODEGEN_LIB_DLL:
        write_codegen_dll_lib_pass_1(lib, vt);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void write_codegen_script_lib_pass_2(const FklCgLib *lib,
        const char *main_dir,
        FklValueTable *vt,
        FILE *outfp) {
    write_codegen_script_lib_path(lib->rp, main_dir, outfp);
    fwrite(&lib->prototypeId, sizeof(lib->prototypeId), 1, outfp);
    fklWriteByteCodelnt(FKL_VM_CO(lib->bcl),
            vt,
            FKL_WRITE_CODE_PASS_SECOND,
            outfp);
    fwrite(&lib->epc, sizeof(lib->epc), 1, outfp);
    write_export_sid_idx_table_pass_2(&lib->exports, vt, outfp);
    write_compiler_macros_pass_2(lib->macros, vt, outfp);
    write_replacements_pass_2(lib->replacements, vt, outfp);
    write_named_prods_pass_2(lib->named_prod_groups, vt, outfp);
}

static inline void write_codegen_lib_pass_2(const FklCgLib *lib,
        const char *main_dir,
        const char *target_dir,
        FklValueTable *vt,
        FILE *fp) {
    uint8_t type_byte = lib->type;
    fwrite(&type_byte, sizeof(type_byte), 1, fp);
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        write_codegen_script_lib_pass_2(lib, main_dir, vt, fp);
        break;
    case FKL_CODEGEN_LIB_DLL:
        write_codegen_dll_lib_pass_2(lib, main_dir, target_dir, vt, fp);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void write_lib_vector_passes(FILE *outfp,
        FklValueTable *value_table,
        const char *main_dir,
        const char *target_dir,
        FklWriteCodePass pass,
        FklVMvalueCgLibs *const libraries) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST: {
        const FklCgLib *base = fklVMvalueCgLibs(libraries);
        size_t count = fklVMvalueCgLibsCount(libraries);

        for (size_t i = 0; i < count; i++) {
            const FklCgLib *lib = &base[i];
            write_codegen_lib_pass_1(lib, value_table);
        }
    } break;
    case FKL_WRITE_CODE_PASS_SECOND: {
        const FklCgLib *base = fklVMvalueCgLibs(libraries);
        uint64_t num = fklVMvalueCgLibsCount(libraries);
        fwrite(&num, sizeof(uint64_t), 1, outfp);
        for (size_t i = 0; i < num; i++) {
            const FklCgLib *lib = &base[i];
            write_codegen_lib_pass_2(lib,
                    main_dir,
                    target_dir,
                    value_table,
                    outfp);
        }
    } break;
    }
}

static inline void write_lib_main_file_passes(FILE *outfp,
        FklValueTable *value_table,
        const char *main_dir,
        FklWriteCodePass pass,
        const FklVMvalueCgInfo *codegen,
        const FklByteCodelnt *bcl) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        fklWriteByteCodelnt(bcl, value_table, FKL_WRITE_CODE_PASS_FIRST, NULL);
        write_export_sid_idx_table_pass_1(&codegen->exports, value_table);
        write_compiler_macros_pass_1(codegen->export_macros, value_table);
        write_replacements_pass_1(codegen->export_replacement, value_table);
        write_named_prods_pass_1(codegen->export_prod_groups, value_table);
        // write_export_named_prods_pass_1(codegen->export_named_prod_groups,
        //         codegen->named_prod_groups,
        //         value_table);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_codegen_script_lib_path(codegen->realpath, main_dir, outfp);
        uint32_t protoId = 1;
        fwrite(&protoId, sizeof(protoId), 1, outfp);
        fklWriteByteCodelnt(bcl,
                value_table,
                FKL_WRITE_CODE_PASS_SECOND,
                outfp);
        fwrite(&codegen->epc, sizeof(codegen->epc), 1, outfp);
        write_export_sid_idx_table_pass_2(&codegen->exports,
                value_table,
                outfp);
        write_compiler_macros_pass_2(codegen->export_macros,
                value_table,
                outfp);
        write_replacements_pass_2(codegen->export_replacement,
                value_table,
                outfp);
        write_named_prods_pass_2(codegen->export_prod_groups,
                value_table,
                outfp);
        // write_export_named_prods_pass_2(codegen->export_named_prod_groups,
        //         codegen->named_prod_groups,
        //         value_table,
        //         outfp);
        break;
    }
}

static inline void write_pre_compile_passes(FILE *fp,
        FklValueTable *value_table,
        const char *target_dir,
        FklWriteCodePass pass,
        const FklWritePreCompileArgs *const args) {
    const FklCgCtx *const ctx = args->ctx;
    const char *main_dir = ctx->main_file_real_path_dir;

    fklWriteFuncPrototypes(ctx->pts, value_table, pass, fp);

    write_lib_vector_passes(fp,
            value_table,
            main_dir,
            target_dir,
            pass,
            ctx->libraries);

    write_lib_main_file_passes(fp,
            value_table,
            main_dir,
            pass,
            args->main_info,
            args->main_bcl);

    fklWriteFuncPrototypes(ctx->macro_pts, value_table, pass, fp);

    write_lib_vector_passes(fp,
            value_table,
            main_dir,
            target_dir,
            pass,
            ctx->macro_libraries);
}

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args) {
    FklValueTable value_table;
    fklInitValueTable(&value_table);

    write_pre_compile_passes(fp,
            &value_table,
            target_dir,
            FKL_WRITE_CODE_PASS_FIRST,
            args);

    fklWriteValueTable(&value_table, fp);

    write_pre_compile_passes(fp,
            &value_table,
            target_dir,
            FKL_WRITE_CODE_PASS_SECOND,
            args);

    fklUninitValueTable(&value_table);
}

int fklLoadPreCompile(FILE *fp,
        const char *rp,
        FklLoadPreCompileArgs *const args) {
    int err = 0;

    FklCgCtx *ctx = args->ctx;
    char *main_dir = fklGetDir(rp);
    main_dir = fklStrCat(main_dir, FKL_PATH_SEPARATOR_STR);

    FklLoadValueArgs load_values = { .vm = ctx->vm };

    fklLoadValueTable(fp, &load_values);

    FklVMvalueProtos *pts = fklCreateVMvalueProtos(ctx->vm, 0);

    fklLoadFuncPrototypes(fp, &load_values, pts);

    FklVMvalueCgLibs *libraries = load_lib_vector_and_main(fp, //
            main_dir,
            &load_values,
            args);
    if (libraries == NULL)
        goto error;

    FklVMvalueProtos *macro_pts = fklCreateVMvalueProtos(ctx->vm, 0);

    fklLoadFuncPrototypes(fp, &load_values, macro_pts);

    FklVMvalueCgLibs *macro_libraries = load_lib_vector(fp, //
            main_dir,
            &load_values,
            args);

    if (macro_libraries == NULL) {
    error:
        err = 1;
        goto exit;
    }

    increase_prototype_and_lib_id(libraries,
            &(IncCtx){
                .pts_count = args->pts->p.count,
                .lib_count = fklVMvalueCgLibsCount(args->libraries),
            });

    merge_prototypes(args->pts, pts);

    increase_macro_prototype_and_lib_id(libraries,
            &(IncCtx){
                .pts_count = args->macro_pts->p.count,
                .lib_count = fklVMvalueCgLibsCount(args->macro_libraries),
            });

    fklVMvalueCgLibsMerge(args->libraries, libraries);

    increase_prototype_and_lib_id(macro_libraries,
            &(IncCtx){
                .pts_count = args->macro_pts->p.count,
                .lib_count =fklVMvalueCgLibsCount( args->macro_libraries),
            });

    increase_macro_prototype_and_lib_id(macro_libraries,
            &(IncCtx){
                .pts_count = args->macro_pts->p.count,
                .lib_count = fklVMvalueCgLibsCount(args->macro_libraries),
            });

    merge_prototypes(args->macro_pts, macro_pts);

    fklVMvalueCgLibsMerge(args->macro_libraries, macro_libraries);

exit:
    load_values.count = 0;
    fklZfree(load_values.values);

    fklZfree(main_dir);
    return err;
}
