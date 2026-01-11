#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/string_table.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"

typedef uint32_t LibCount;
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

static inline void write_value_create_op(enum ValueCreateOpcode o, FILE *fp) {
    uint8_t op = o;
    fwrite(&op, sizeof(op), 1, fp);
}

static inline void write_value_id(const FklValueTable *t,
        FklValueId v_id,
        const FklVMvalue *v,
        FILE *fp) {
    FklValueId u = fklValueTableGet(t, v);
    FKL_ASSERT((u || v == NULL) && (v_id == 0 || u <= v_id));
    fwrite(&u, sizeof(u), 1, fp);
}

static inline FklVMvalue *load_value_id(FILE *fp,
        const FklLoadValueArgs *values) {
    FklValueId id = 0;
    fread(&id, sizeof(id), 1, fp);
    if (id == 0)
        return NULL;

    FklVMvalue *r = values->values[id - 1];
    FKL_ASSERT(r);

    return r;
}

static inline void write_proto_id(const FklProtoTable *t,
        FklValueId v_id,
        const FklVMvalueProto *v,
        FILE *fp) {
    FklValueId u = fklProtoTableGet(t, v);
    FKL_ASSERT((u || v == NULL) && (v_id == 0 || u <= v_id));
    fwrite(&u, sizeof(u), 1, fp);
}

static inline FklVMvalueProto *load_proto_id(FILE *fp,
        const FklLoadProtoArgs *protos) {
    FklValueId id = 0;
    fread(&id, sizeof(id), 1, fp);
    if (id == 0)
        return NULL;

    FklVMvalueProto *r = protos->protos[id - 1];
    FKL_ASSERT(r);

    return r;
}

static inline void write_bigint(const FklVMvalueBigInt *bi, FILE *fp) {
    fwrite(&bi->num, sizeof(bi->num), 1, fp);
    fwrite(bi->digits, fklAbs(bi->num) * sizeof(*(bi->digits)), 1, fp);
}

static inline void write_value_create_instructions(const FklVMvalue *v,
        FklValueId value_id,
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

    FklValueId count = vt->next_id - 1;
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
        FklValueId self_id,
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

static inline void
load_symbol_def(FILE *fp, const FklLoadValueArgs *values, FklVarRefDef *def) {
    def->sid = load_value_id(fp, values);
    def->cidx = load_value_id(fp, values);
    def->is_local = load_value_id(fp, values);
}

typedef uint32_t TotalValCount;

static inline FklVMvalueProto *load_prototype(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos) {
    TotalValCount total_val_count = 0;
    fread(&total_val_count, sizeof(total_val_count), 1, fp);
    FklVMvalueProto *pt = fklCreateVMvalueProto(values->vm, total_val_count);

    fread(&pt->local_count, sizeof(pt->local_count), 1, fp);
    fread(&pt->ref_count, sizeof(pt->ref_count), 1, fp);
    fread(&pt->ref_offset, sizeof(pt->ref_offset), 1, fp);

    FklVarRefDef *refs = fklVMvalueProtoVarRefs(pt);
    for (uint32_t i = 0; i < pt->ref_count; ++i) {
        load_symbol_def(fp, values, &refs[i]);
    }

    pt->name = load_value_id(fp, values);
    pt->file = load_value_id(fp, values);

    fread(&pt->line, sizeof(pt->line), 1, fp);
    fread(&pt->konsts_count, sizeof(pt->konsts_count), 1, fp);
    fread(&pt->konsts_offset, sizeof(pt->konsts_offset), 1, fp);

    FklVMvalue **konsts = (FklVMvalue **)fklVMvalueProtoConsts(pt);
    for (uint32_t i = 0; i < pt->konsts_count; ++i) {
        konsts[i] = load_value_id(fp, values);
    }

    fread(&pt->child_proto_count, sizeof(pt->child_proto_count), 1, fp);
    fread(&pt->child_proto_offset, sizeof(pt->child_proto_offset), 1, fp);

    FklVMvalueProto **child_protos =
            (FklVMvalueProto **)fklVMvalueProtoChildren(pt);

    for (uint32_t i = 0; i < pt->child_proto_count; ++i) {
        child_protos[i] = load_proto_id(fp, protos);
    }

    return pt;
}

int fklLoadProtoTable(FILE *fp,
        const FklLoadValueArgs *values,
        FklLoadProtoArgs *args) {
    fread(&args->count, sizeof(args->count), 1, fp);

    FklVMvalueProto **protos = (FklVMvalueProto **)fklZcalloc(args->count,
            sizeof(FklVMvalueProto *));
    FKL_ASSERT(protos);
    args->protos = protos;

    for (FklValueId id = args->count; id > 0; --id) {
        args->protos[id - 1] = load_prototype(fp, values, args);
    }
    return 0;
}

static inline void write_symbol_def_pass_1(const FklVarRefDef *def,
        FklValueTable *vt) {
    fklTraverseSerializableValue(vt, def->sid);
    fklTraverseSerializableValue(vt, def->cidx);
    fklTraverseSerializableValue(vt, def->is_local);
}

static inline void write_prototype_pass_1(const FklVMvalueProto *pt,
        FklValueTable *vt,
        FklProtoTable *proto_table) {

    FklValueVector pending = { 0 };
    fklValueVectorInit(&pending, pt->child_proto_count);
    fklValueVectorPushBack2(&pending, FKL_VM_VAL(pt));

    while (!fklValueVectorIsEmpty(&pending)) {
        FklVMvalue *pt_v = *fklValueVectorPopBackNonNull(&pending);
        FklVMvalueProto *pt = fklVMvalueProto(pt_v);

        fklProtoTableAdd(proto_table, pt);

        const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(pt);
        for (uint32_t i = 0; i < pt->ref_count; i++) {
            write_symbol_def_pass_1(&refs[i], vt);
        }

        fklTraverseSerializableValue(vt, pt->name);
        fklTraverseSerializableValue(vt, pt->file);
        FklVMvalue *const *konsts = fklVMvalueProtoConsts(pt);
        for (uint32_t i = 0; i < pt->konsts_count; ++i) {
            fklTraverseSerializableValue(vt, konsts[i]);
        }

        FklVMvalueProto *const *child_proc_proto = fklVMvalueProtoChildren(pt);
        for (uint32_t i = 0; i < pt->child_proto_count; ++i) {
            fklValueVectorPushBack2(&pending, FKL_VM_VAL(child_proc_proto[i]));
        }
    }

    fklValueVectorUninit(&pending);
}

static inline void write_symbol_def_pass_2(const FklVarRefDef *def,
        const FklValueTable *vt,
        FILE *fp) {
    write_value_id(vt, 0, def->sid, fp);
    write_value_id(vt, 0, def->cidx, fp);
    write_value_id(vt, 0, def->is_local, fp);
}

static inline void write_prototype_pass_2(const FklVMvalueProto *pt,
        const FklValueTable *vt,
        const FklProtoTable *proto_table,
        FILE *fp) {
    TotalValCount total_val_count = pt->total_val_count;
    fwrite(&total_val_count, sizeof(total_val_count), 1, fp);
    fwrite(&pt->local_count, sizeof(pt->local_count), 1, fp);
    fwrite(&pt->ref_count, sizeof(pt->ref_count), 1, fp);
    fwrite(&pt->ref_offset, sizeof(pt->ref_offset), 1, fp);

    const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(pt);
    for (uint32_t i = 0; i < pt->ref_count; ++i)
        write_symbol_def_pass_2(&refs[i], vt, fp);
    write_value_id(vt, 0, pt->name, fp);
    write_value_id(vt, 0, pt->file, fp);
    fwrite(&pt->line, sizeof(pt->line), 1, fp);
    fwrite(&pt->konsts_count, sizeof(pt->konsts_count), 1, fp);
    fwrite(&pt->konsts_offset, sizeof(pt->konsts_offset), 1, fp);

    FklVMvalue *const *konsts = fklVMvalueProtoConsts(pt);
    for (uint32_t i = 0; i < pt->konsts_count; ++i) {
        write_value_id(vt, 0, konsts[i], fp);
    }

    fwrite(&pt->child_proto_count, sizeof(pt->child_proto_count), 1, fp);
    fwrite(&pt->child_proto_offset, sizeof(pt->child_proto_offset), 1, fp);

    FklVMvalueProto *const *child_proc_proto = fklVMvalueProtoChildren(pt);
    for (uint32_t i = 0; i < pt->child_proto_count; ++i) {
        write_proto_id(proto_table, 0, child_proc_proto[i], fp);
    }
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

void fklWriteProc(const FklVMvalueProc *proc,
        FklValueTable *vt,
        FklProtoTable *proto_table,
        FklWriteCodePass pass,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        write_prototype_pass_1(proc->proto, vt, proto_table);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_proto_id(proto_table, 0, proc->proto, fp);
        break;
    }
    fklWriteByteCodelnt(FKL_VM_CO(proc->bcl), vt, pass, fp);
}

FklVMvalueProc *fklLoadProc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos) {
    FklVM *vm = values->vm;
    FklVMvalueProto *pt = load_proto_id(fp, protos);
    FklVMvalue *bcl = fklCreateVMvalueCodeObj1(vm);

    int r = fklLoadByteCodelnt(fp, values, FKL_VM_CO(bcl));
    FKL_ASSERT(r == 0);

    FklVMvalue *proc = fklCreateVMvalueProc(vm, bcl, pt);
    fklInitMainProcRefs(vm, proc);

    return FKL_VM_PROC(proc);
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

static void write_vm_libs_pass_1(const FklVMvalueVec *l,
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    for (size_t i = 1; i < l->size; ++i) {
        const FklVMvalueLib *lib = fklVMvalueLib(l->base[i]);
        FKL_TODO();
        if (FKL_IS_PROC(lib->proc)) {
            fklWriteProc(FKL_VM_PROC(lib->proc),
                    vt,
                    proto_table,
                    FKL_WRITE_CODE_PASS_FIRST,
                    NULL);
        } else {
            fklTraverseSerializableValue(vt, lib->proc);
        }
    }
}

static void write_vm_libs_pass_2(const FklVMvalueVec *l,
        FklValueTable *vt,
        FklProtoTable *proto_table,
        FILE *fp) {

    LibCount count = l->size - 1;
    fwrite(&count, sizeof(count), 1, fp);
    for (size_t i = 1; i < l->size; ++i) {
        FKL_TODO();
        const FklVMvalueLib *lib = fklVMvalueLib(l->base[i]);
        FKL_ASSERT(FKL_IS_PROC(lib->proc) || FKL_IS_STR(lib->proc));
        uint8_t type_byte = FKL_IS_PROC(lib->proc) ? FKL_CODEGEN_LIB_SCRIPT
                                                   : FKL_CODEGEN_LIB_DLL;
        fwrite(&type_byte, sizeof(type_byte), 1, fp);
        switch (type_byte) {
        case FKL_CODEGEN_LIB_SCRIPT: {
            fwrite(&lib->epc, sizeof(lib->epc), 1, fp);

            FklVMvalueProc *proc = FKL_VM_PROC(lib->proc);
            fklWriteProc(proc, vt, proto_table, FKL_WRITE_CODE_PASS_SECOND, fp);
        } break;
        case FKL_CODEGEN_LIB_DLL: {
            write_value_id(vt, 0, lib->proc, fp);
        } break;
        }
    }
}

void fklWriteVMlibs(const FklVMvalueVec *l,
        FklValueTable *vt,
        FklProtoTable *proto_table,
        FklWriteCodePass pass,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        write_vm_libs_pass_1(l, vt, proto_table);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_vm_libs_pass_2(l, vt, proto_table, fp);
        break;
    }
}

FklVMvalueVec *fklLoadVMlibs(FILE *fp,
        FklVM *vm,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos) {
    FKL_TODO();
    LibCount count = 0;
    // fread(&count, sizeof(count), 1, fp);
    // fklVMvalueLibsReserve(l, l->count);
    //
    // for (uint64_t i = 1; i <= l->count; ++i) {
    //     FklVMlib *lib = &l->libs[i];
    //     memset(lib, 0, sizeof(*lib));
    //     uint8_t type_byte = 0;
    //     fread(&type_byte, sizeof(type_byte), 1, fp);
    //     switch ((FklCgLibType)type_byte) {
    //     case FKL_CODEGEN_LIB_SCRIPT: {
    //         fread(&lib->epc, sizeof(lib->epc), 1, fp);
    //         FklVMvalueProc *proc = fklLoadProc(fp, values, protos);
    //
    //         lib->proc = FKL_VM_VAL(proc);
    //     } break;
    //     case FKL_CODEGEN_LIB_DLL: {
    //         lib->proc = load_value_id(fp, values);
    //     } break;
    //     default:
    //         FKL_UNREACHABLE();
    //         break;
    //     }
    // }
}

static inline void write_code_file_passes(FILE *fp,
        FklValueTable *value_table,
        FklProtoTable *proto_table,
        FklWriteCodePass pass,
        const FklWriteCodeFileArgs *args) {
    FKL_TODO();
    // fklWriteProc(args->proc, value_table, proto_table, pass, fp);
    // fklWriteVMlibs(args->libs, value_table, proto_table, pass, fp);
}

static inline void write_prototype_table(const FklProtoTable *proto_table,
        const FklValueTable *value_table,
        FILE *fp) {
    FklValueId count = proto_table->vt.next_id - 1;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklValueIdHashMapNode *cur = proto_table->vt.ht.last; cur;
            cur = cur->prev) {
        const FklVMvalue *pt_v = cur->k;
        write_prototype_pass_2(fklVMvalueProto(pt_v),
                value_table,
                proto_table,
                fp);
    }
}

void fklWriteCodeFile(FILE *fp, const FklWriteCodeFileArgs *const args) {
    FklValueTable value_table = { 0 };
    fklInitValueTable(&value_table);

    FklProtoTable proto_table = { 0 };
    fklInitProtoTable(&proto_table);

    write_code_file_passes(fp,
            &value_table,
            &proto_table,
            FKL_WRITE_CODE_PASS_FIRST,
            args);

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, fp);

    write_code_file_passes(fp,
            &value_table,
            &proto_table,
            FKL_WRITE_CODE_PASS_SECOND,
            args);

    fklUninitProtoTable(&proto_table);
    fklUninitValueTable(&value_table);
}

int fklLoadCodeFile(FILE *fp, FklLoadCodeFileArgs *const args) {
    FklVM *vm = args->vm;
    FklLoadValueArgs values = { .vm = vm };
    FklLoadProtoArgs protos = { .vm = vm };

    int r = fklLoadValueTable(fp, &values);
    FKL_ASSERT(r == 0);
    (void)r;

    r = fklLoadProtoTable(fp, &values, &protos);
    FKL_ASSERT(r == 0);
    (void)r;

    FklVMvalueProc *main_func = fklLoadProc(fp, &values, &protos);

    args->main_func = main_func;

    FKL_TODO();
    // args->libs = fklLoadVMlibs(fp, vm, &values, &protos);

    values.count = 0;
    fklZfree(values.values);
    values.values = NULL;

    protos.count = 0;
    fklZfree(protos.protos);
    protos.protos = NULL;

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
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    if (fklIsCustomActionProd(prod)) {
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        fklWriteProc(FKL_VM_PROC(ctx->proc),
                vt,
                proto_table,
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
        FklProtoTable *proto_table,
        FILE *fp) {
    uint8_t type;
    if (fklIsCustomActionProd(prod)) {
        type = FKL_CODEGEN_PROD_CUSTOM;
        fwrite(&type, sizeof(type), 1, fp);
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        fklWriteProc(FKL_VM_PROC(ctx->proc),
                vt,
                proto_table,
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
        FklValueTable *vt,
        FklProtoTable *proto_table) {
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

            write_production_rule_action_pass_1(prod, vt, proto_table);
        }
    }
}

static inline void write_grammer_in_binary_pass_2(const FklGrammer *g,
        FklValueTable *vt,
        FklProtoTable *proto_table,
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

            write_production_rule_action_pass_2(prod, vt, proto_table, fp);
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
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    if (named_prod_groups == NULL)
        return;

    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
            list = list->next) {
        fklTraverseSerializableValue(vt, list->k);
        write_grammer_in_binary_pass_1(&list->v.g, vt, proto_table);
    }
}

static inline void write_named_prods_pass_2(
        const FklGraProdGroupHashMap *named_prod_groups,
        FklValueTable *vt,
        FklProtoTable *proto_table,
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
        write_grammer_in_binary_pass_2(&list->v.g, vt, proto_table, fp);
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
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
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
                syms);
        FklVMvalueCustomActCtx *actx = prod->ctx;
        FklVMvalueProc *proc = fklLoadProc(fp, values, protos);
        actx->proc = FKL_VM_VAL(proc);
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
        const FklLoadProtoArgs *const protos,
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
                    values,
                    protos);

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
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *protos) {
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
        load_grammer_in_binary(fp, ctx, values, protos, &group->g);
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
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
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
            FklVMvalueProc *proc = fklLoadProc(fp, values, protos);

            FklCgMacro *cur = fklCreateCgMacro(pattern, FKL_VM_VAL(proc), NULL);

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
        const FklLoadProtoArgs *const protos,
        FklCgLib *lib,
        FklLoadPreCompileArgs *const args) {
    FKL_TODO();
    // memset(lib, 0, sizeof(*lib));
    // lib->type = FKL_CODEGEN_LIB_SCRIPT;
    // lib->rp = load_script_lib_path(main_dir, fp);
    // lib->proc = FKL_VM_VAL(fklLoadProc(fp, values, protos));
    // fread(&lib->epc, sizeof(lib->epc), 1, fp);
    // load_export_sid_idx_table(fp, values, &lib->exports);
    // lib->macros = load_compiler_macros(args->ctx->vm, fp, values, protos);
    // lib->replacements = load_replacements(fp, values);
    // lib->named_prod_groups = load_named_prods(fp, args->ctx, values, protos);
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
    FKL_TODO();
    // memset(lib, 0, sizeof(*lib));
    // lib->type = FKL_CODEGEN_LIB_DLL;
    // lib->rp = load_dll_lib_path(main_dir, fp);
    // if (!lib->rp || !fklIsAccessibleRegFile(lib->rp)) {
    //     fklZfree(lib->rp);
    //     return 1;
    // }
    //
    // if (uv_dlopen(lib->rp, &lib->dll)) {
    //     args->error = fklZstrdup(uv_dlerror(&lib->dll));
    //     uv_dlclose(&lib->dll);
    //     fklZfree(lib->rp);
    //     return 1;
    // }
    // load_export_sid_idx_table(fp, values, &lib->exports);
    // return 0;
}

static inline int load_lib_from_pre_compile(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos,
        FklCgLib *lib,
        FklLoadPreCompileArgs *const args) {
    uint8_t type = 0;
    fread(&type, sizeof(type), 1, fp);
    lib->type = type;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        load_script_lib_from_pre_compile(fp,
                main_dir,
                values,
                protos,
                lib,
                args);
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
        const FklLoadProtoArgs *const protos,
        FklLoadPreCompileArgs *const args) {
    FKL_TODO();
    // uint64_t num = 0;
    // fread(&num, sizeof(num), 1, fp);
    // FklVMvalueCgLibs *libs = fklCreateVMvalueCgLibs1(args->ctx->vm, num);
    // for (size_t i = 0; i < num; i++) {
    //     FklCgLib lib = { 0 };
    //     if (load_lib_from_pre_compile(fp, main_dir, values, protos, &lib,
    //     args))
    //         return NULL;
    //     fklVMvalueCgLibsEmplaceBack(libs, &lib);
    // }
    // return libs;
}

static inline FklVMvalueCgLibs *load_lib_vector_and_main(FILE *fp,
        const char *main_dir,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos,
        FklLoadPreCompileArgs *const args) {
    FKL_TODO();
    // FklVMvalueCgLibs *l = load_lib_vector(fp, main_dir, values, protos,
    // args); if (l == NULL)
    //     return NULL;
    // FklCgLib main_lib = { 0 };
    // load_script_lib_from_pre_compile(fp,
    //         main_dir,
    //         values,
    //         protos,
    //         &main_lib,
    //         args);
    // fklVMvalueCgLibsEmplaceBack(l, &main_lib);
    // return l;
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
} IncCtx;

static int increase_bcl_lib_id_predicate(FklOpcode op) {
    return IS_LOAD_LIB_OP(op) || IS_LOAD_DLL_OP(op) || IS_EXPORT_TO_OP(op);
}

static int increase_bcl_lib_id_func(void *cctx,
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
    }
    return 0;
}

static inline void increase_bcl_lib_id(FklByteCodelnt *bcl,
        const IncCtx *args) {
    fklRecomputeInsImm(bcl,
            FKL_TYPE_CAST(void *, args),
            increase_bcl_lib_id_predicate,
            increase_bcl_lib_id_func);
}

static inline void increase_compiler_macros_lib_id(FklMacroHashMap *macros,
        const IncCtx *args) {
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        for (FklCgMacro *c = cur->v; c; c = c->next) {
            FklVMvalue *co = FKL_VM_PROC(c->proc)->bcl;
            increase_bcl_lib_id(FKL_VM_CO(co), args);
        }
    }
}

static inline void increase_lib_id(FklVMvalueCgLibs *libraries,
        const IncCtx *args) {
    FKL_TODO();
    // uint32_t top = fklVMvalueCgLibsCount(libraries);
    // FklCgLib *base = fklVMvalueCgLibs(libraries);
    // for (uint32_t i = 0; i < top; i++) {
    //     FklCgLib *lib = &base[i];
    //     if (lib->type == FKL_CODEGEN_LIB_SCRIPT) {
    //         FklVMvalue *co = FKL_VM_PROC(lib->proc)->bcl;
    //         increase_bcl_lib_id(FKL_VM_CO(co), args);
    //     }
    // }
}

static inline void increase_reader_macro_lib_id(
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
                    FklVMvalue *co = FKL_VM_PROC(ctx->proc)->bcl;
                    increase_bcl_lib_id(FKL_VM_CO(co), args);
                }
            }
        }
    }
}

static inline void increase_macro_lib_id(FklVMvalueCgLibs *libraries,
        const IncCtx *args) {
    FKL_TODO();
    // uint32_t top = fklVMvalueCgLibsCount(libraries);
    // FklCgLib *base = fklVMvalueCgLibs(libraries);
    // for (uint32_t i = 0; i < top; i++) {
    //     FklCgLib *lib = &base[i];
    //     if (lib->type == FKL_CODEGEN_LIB_SCRIPT) {
    //         increase_compiler_macros_lib_id(lib->macros, args);
    //         increase_reader_macro_lib_id(lib->named_prod_groups, args);
    //     }
    // }
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
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    if (macros == NULL)
        return;
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        fklTraverseSerializableValue(vt, cur->k);
        for (const FklCgMacro *c = cur->v; c; c = c->next) {
            fklTraverseSerializableValue(vt, c->pattern);
            fklWriteProc(FKL_VM_PROC(c->proc),
                    vt,
                    proto_table,
                    FKL_WRITE_CODE_PASS_FIRST,
                    NULL);
        }
    }
}

static inline void write_compiler_macros_pass_2(const FklMacroHashMap *macros,
        FklValueTable *vt,
        FklProtoTable *proto_table,
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
            fklWriteProc(FKL_VM_PROC(c->proc),
                    vt,
                    proto_table,
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
    FKL_TODO();
    // char *relpath = fklRelpath(main_dir, lib->rp);
    // size_t count = 0;
    // char **slices = fklSplit(relpath, FKL_PATH_SEPARATOR_STR, &count);
    // count--;
    //
    // for (size_t i = 0; i < count; i++) {
    //     fputs(slices[i], outfp);
    //     fputc('\0', outfp);
    // }
    // uint64_t len = strlen(slices[count]) - FKL_DLL_FILE_TYPE_STR_LEN;
    // fwrite(slices[count], len, 1, outfp);
    // fputc('\0', outfp);
    // fputc('\0', outfp);
    // fklZfree(relpath);
    // fklZfree(slices);
}

static inline void write_codegen_script_lib_pass_1(const FklCgLib *lib,
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    FKL_TODO();
    // fklWriteProc(FKL_VM_PROC(lib->proc),
    //         vt,
    //         proto_table,
    //         FKL_WRITE_CODE_PASS_FIRST,
    //         NULL);
    // write_export_sid_idx_table_pass_1(&lib->exports, vt);
    // write_compiler_macros_pass_1(lib->macros, vt, proto_table);
    // write_replacements_pass_1(lib->replacements, vt);
    // write_named_prods_pass_1(lib->named_prod_groups, vt, proto_table);
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
        FklValueTable *vt,
        FklProtoTable *proto_table) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        write_codegen_script_lib_pass_1(lib, vt, proto_table);
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
        FklProtoTable *proto_table,
        FILE *outfp) {
    FKL_TODO();
    // write_codegen_script_lib_path(lib->rp, main_dir, outfp);
    // fklWriteProc(FKL_VM_PROC(lib->proc),
    //         vt,
    //         proto_table,
    //         FKL_WRITE_CODE_PASS_SECOND,
    //         outfp);
    // fwrite(&lib->epc, sizeof(lib->epc), 1, outfp);
    // write_export_sid_idx_table_pass_2(&lib->exports, vt, outfp);
    // write_compiler_macros_pass_2(lib->macros, vt, proto_table, outfp);
    // write_replacements_pass_2(lib->replacements, vt, outfp);
    // write_named_prods_pass_2(lib->named_prod_groups, vt, proto_table, outfp);
}

static inline void write_codegen_lib_pass_2(const FklCgLib *lib,
        const char *main_dir,
        const char *target_dir,
        FklValueTable *vt,
        FklProtoTable *proto_table,
        FILE *fp) {
    uint8_t type_byte = lib->type;
    fwrite(&type_byte, sizeof(type_byte), 1, fp);
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        write_codegen_script_lib_pass_2(lib, main_dir, vt, proto_table, fp);
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
        FklProtoTable *proto_table,
        const char *main_dir,
        const char *target_dir,
        FklWriteCodePass pass,
        FklVMvalueCgLibs *const libraries) {
    FKL_TODO();
    // switch (pass) {
    // case FKL_WRITE_CODE_PASS_FIRST: {
    //     const FklCgLib *base = fklVMvalueCgLibs(libraries);
    //     size_t count = fklVMvalueCgLibsCount(libraries);
    //
    //     for (size_t i = 0; i < count; i++) {
    //         const FklCgLib *lib = &base[i];
    //         write_codegen_lib_pass_1(lib, value_table, proto_table);
    //     }
    // } break;
    // case FKL_WRITE_CODE_PASS_SECOND: {
    //     const FklCgLib *base = fklVMvalueCgLibs(libraries);
    //     uint64_t num = fklVMvalueCgLibsCount(libraries);
    //     fwrite(&num, sizeof(uint64_t), 1, outfp);
    //     for (size_t i = 0; i < num; i++) {
    //         const FklCgLib *lib = &base[i];
    //         write_codegen_lib_pass_2(lib,
    //                 main_dir,
    //                 target_dir,
    //                 value_table,
    //                 proto_table,
    //                 outfp);
    //     }
    // } break;
    // }
}

static inline void write_lib_main_file_passes(FILE *outfp,
        FklValueTable *value_table,
        FklProtoTable *proto_table,
        const char *main_dir,
        FklWriteCodePass pass,
        const FklVMvalueCgInfo *codegen,
        const FklVMvalueProc *proc) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        fklWriteProc(proc,
                value_table,
                proto_table,
                FKL_WRITE_CODE_PASS_FIRST,
                NULL);
        write_export_sid_idx_table_pass_1(&codegen->exports, value_table);
        write_compiler_macros_pass_1(codegen->export_macros,
                value_table,
                proto_table);
        write_replacements_pass_1(codegen->export_replacement, value_table);
        write_named_prods_pass_1(codegen->export_prod_groups,
                value_table,
                proto_table);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_codegen_script_lib_path(codegen->realpath, main_dir, outfp);
        fklWriteProc(proc,
                value_table,
                proto_table,
                FKL_WRITE_CODE_PASS_SECOND,
                outfp);

        fwrite(&codegen->epc, sizeof(codegen->epc), 1, outfp);
        write_export_sid_idx_table_pass_2(&codegen->exports,
                value_table,
                outfp);
        write_compiler_macros_pass_2(codegen->export_macros,
                value_table,
                proto_table,
                outfp);
        write_replacements_pass_2(codegen->export_replacement,
                value_table,
                outfp);
        write_named_prods_pass_2(codegen->export_prod_groups,
                value_table,
                proto_table,
                outfp);
        break;
    }
}

static inline void write_pre_compile_passes(FILE *fp,
        FklValueTable *value_table,
        FklProtoTable *proto_table,
        const char *target_dir,
        FklWriteCodePass pass,
        const FklWritePreCompileArgs *const args) {
    const FklCgCtx *const ctx = args->ctx;
    const char *main_dir = ctx->main_file_real_path_dir;

    write_lib_vector_passes(fp,
            value_table,
            proto_table,
            main_dir,
            target_dir,
            pass,
            ctx->libraries);

    write_lib_main_file_passes(fp,
            value_table,
            proto_table,
            main_dir,
            pass,
            args->main_info,
            args->main_proc);

    write_lib_vector_passes(fp,
            value_table,
            proto_table,
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

    FklProtoTable proto_table;
    fklInitProtoTable(&proto_table);

    write_pre_compile_passes(fp,
            &value_table,
            &proto_table,
            target_dir,
            FKL_WRITE_CODE_PASS_FIRST,
            args);

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, fp);

    write_pre_compile_passes(fp,
            &value_table,
            &proto_table,
            target_dir,
            FKL_WRITE_CODE_PASS_SECOND,
            args);

    fklUninitValueTable(&value_table);
    fklUninitProtoTable(&proto_table);
}

const FklCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args) {
    FKL_TODO();
    //     int err = 0;
    //
    //     FklCgCtx *ctx = args->ctx;
    //     char *main_dir = fklGetDir(rp);
    //     main_dir = fklStrCat(main_dir, FKL_PATH_SEPARATOR_STR);
    //
    //     FklLoadValueArgs values = { .vm = ctx->vm };
    //     FklLoadProtoArgs protos = { .vm = ctx->vm };
    //
    //     err = fklLoadValueTable(fp, &values);
    //     FKL_ASSERT(err == 0);
    //
    //     err = fklLoadProtoTable(fp, &values, &protos);
    //     FKL_ASSERT(err == 0);
    //
    //     FklVMvalueCgLibs *libraries = load_lib_vector_and_main(fp, //
    //             main_dir,
    //             &values,
    //             &protos,
    //             args);
    //     if (libraries == NULL)
    //         goto error;
    //
    //     FklVMvalueCgLibs *macro_libraries = load_lib_vector(fp, //
    //             main_dir,
    //             &values,
    //             &protos,
    //             args);
    //
    //     if (macro_libraries == NULL) {
    //     error:
    //         err = 1;
    //         goto exit;
    //     }
    //
    //     // increase_lib_id(libraries,
    //     //         &(IncCtx){
    //     //             .lib_count = fklVMvalueCgLibsCount(args->libraries),
    //     //         });
    //     //
    //     // increase_macro_lib_id(libraries,
    //     //         &(IncCtx){
    //     //             .lib_count =
    //     fklVMvalueCgLibsCount(args->macro_libraries),
    //     //         });
    //
    //     // fklVMvalueCgLibsMerge(args->libraries, libraries);
    //
    //     // increase_lib_id(macro_libraries,
    //     //         &(IncCtx){
    //     //             .lib_count =
    //     fklVMvalueCgLibsCount(args->macro_libraries),
    //     //         });
    //     //
    //     // increase_macro_lib_id(macro_libraries,
    //     //         &(IncCtx){
    //     //             .lib_count =
    //     fklVMvalueCgLibsCount(args->macro_libraries),
    //     //         });
    //
    //     // fklVMvalueCgLibsMerge(args->macro_libraries, macro_libraries);
    //
    // exit:
    //     values.count = 0;
    //     fklZfree(values.values);
    //     values.values = NULL;
    //
    //     protos.count = 0;
    //     fklZfree(protos.protos);
    //     protos.protos = NULL;
    //
    //     fklZfree(main_dir);
    //     return err;
}
