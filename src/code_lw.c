#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/string_table.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/value_table.h>
#include <fakeLisp/vm.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// write and load value table

typedef FklVMvalueCgLib FklCgLib;

typedef enum FklWriteCodePass {
    FKL_WRITE_CODE_PASS_FIRST = 0,
    FKL_WRITE_CODE_PASS_SECOND,
} FklWriteCodePass;

typedef struct {
    int is_writting_pre_compile;
    const FklLibTable *internal_lib_table;
    const FklLibTable *imported_by_macros;

    const FklVMvalueHash *map;

    FklValueTable *value_table;
    FklProtoTable *proto_table;
    FklLibTable *lib_table;
} WriteLibExtraArgs;

typedef struct {
    // in
    FklVM *const vm;

    // out
    FklValueId count;
    FklVMvalue **values;
} FklLoadValueArgs;

typedef uint64_t MacroCount;

FKL_NODISCARD
static int load_value_table(FILE *fp, FklLoadValueArgs *args);

typedef struct {
    // in
    FklVM *const vm;

    // out
    FklValueId count;
    FklVMvalueProto **protos;
} FklLoadProtoArgs;

FKL_NODISCARD
static int load_proto_table(FILE *fp,
        const FklLoadValueArgs *values,
        FklValueVector *ph_vec,
        FklLoadProtoArgs *args);

typedef struct {
    // in
    int is_loading_pre_compile;
    FklVM *const vm;
    const char *main_dir;
    FklPreCompileFixup *fixup;

    const FklCgCtx *cg_ctx;

    // out
    FklValueId count;
    FklVMvalueLib **libs;
} FklLoadLibArgs;

FKL_NODISCARD
static int load_lib_table(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        FklLoadLibArgs *args);

// write and load bytecodes

static void write_lnt(const FklLntItem *,
        uint32_t count,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *);

static void load_lnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklLntItem **plist,
        uint32_t *pnum);

static void write_bc(const FklByteCode *bc, FILE *fp);
static void load_bc(FklByteCode *bc, FILE *fp);

static void write_proc(const FklVMvalueProc *proc,
        FklWriteCodePass pass,
        const WriteLibExtraArgs *extra_args,
        FILE *fp);
static FklVMvalueProc *load_proc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos);

static void write_bc_lnt(const FklByteCodelnt *bcl,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp);

FKL_NODISCARD
static int
load_bc_lnt(FILE *fp, const FklLoadValueArgs *values, FklByteCodelnt *bcl);

typedef uint32_t TotalValCount;

typedef uint32_t LibIdx;

typedef uint8_t LibType;

#define PRIu_LIBIDX PRIu32

FKL_VM_DEF_UD_STRUCT(LibPlaceholder, { LibIdx idx; });

static FklVMudMetaTable const LibPlaceholderMt;

static FKL_ALWAYS_INLINE FKL_UNUSED int is_lib_placeholder(
        const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &LibPlaceholderMt;
}

static FKL_ALWAYS_INLINE LibPlaceholder *as_lib_placeholder(
        const FklVMvalue *v) {
    FKL_ASSERT(is_lib_placeholder(v));
    return FKL_TYPE_CAST(LibPlaceholder *, v);
}

static void
lib_placeholder_print(const FklVMvalue *ud, FklCodeBuilder *buf, FklVM *exe) {
    fklCodeBuilderFmt(buf,
            "#<lib-placeholder: %" PRIu_LIBIDX ">",
            as_lib_placeholder(ud)->idx);
}

static FklVMudMetaTable const LibPlaceholderMt = {
    .size = sizeof(LibPlaceholder),
    .princ = lib_placeholder_print,
    .prin1 = lib_placeholder_print,
};

static inline LibPlaceholder *
create_lib_placeholder(FklVM *vm, LibIdx idx, FklValueVector *ph_vec) {
    FKL_ASSERT(idx > 0);
    fklValueVectorReserve(ph_vec, idx);
    if (ph_vec->size < idx) {
        size_t clean_size = (idx - ph_vec->size) * sizeof(*ph_vec->base);
        memset(&ph_vec->base[ph_vec->size], 0, clean_size);
        ph_vec->size = idx;
    }

    FklVMvalue *v = ph_vec->base[idx - 1];
    if (v != NULL) {
        FKL_ASSERT(is_lib_placeholder(v));
        return (LibPlaceholder *)v;
    }

    v = fklCreateVMvalueUd(vm, &LibPlaceholderMt, NULL);
    LibPlaceholder *p = (LibPlaceholder *)v;
    p->idx = idx;

    ph_vec->base[idx - 1] = v;
    return p;
}

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

static inline void write_lib_id(const FklLibTable *t,
        FklValueId v_id,
        const FklVMvalueLib *v,
        FILE *fp) {
    FklValueId u = fklLibTableGet(t, v);
    FKL_ASSERT((u || v == NULL) && (v_id == 0 || u <= v_id));
    fwrite(&u, sizeof(u), 1, fp);
}

static inline FklValueId read_lib_id(FILE *fp) {
    FklValueId u = 0;
    fread(&u, sizeof(u), 1, fp);
    return u;
}

static inline FklVMvalueLib *get_lib_with_id(const FklLoadLibArgs *libs,
        LibIdx id) {
    FklVMvalueLib *r = libs->libs[id - 1];
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

static int load_value_table(FILE *fp, FklLoadValueArgs *args) {
    FklVM *vm = args->vm;
    fread(&args->count, sizeof(args->count), 1, fp);

    if (args->count == 0) {
        args->values = NULL;
        return 0;
    }

    size_t const total_size = args->count * sizeof(FklVMvalue *);
    FklVMvalue **values = (FklVMvalue **)fklZmalloc(total_size);
    FKL_ASSERT(values);
    memset(values, 0, total_size);

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

static inline FklVMvalueProto *load_prototype(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        FklValueVector *ph_vec) {
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

    fread(&pt->used_libraries_count, sizeof(pt->used_libraries_count), 1, fp);
    fread(&pt->used_libraries_offset, sizeof(pt->used_libraries_offset), 1, fp);

    FklVMvalue **libs = &pt->vals[pt->used_libraries_offset];
    for (uint32_t i = 0; i < pt->used_libraries_count; ++i) {
        FklValueId u = read_lib_id(fp);
        LibPlaceholder *p = create_lib_placeholder(values->vm, u, ph_vec);
        libs[i] = FKL_VM_VAL(p);
    }

    return pt;
}

static inline FklVMvalueLib *load_vm_lib(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        const FklLoadLibArgs *libs) {
    FklVMvalue *name = load_value_id(fp, values);
    TotalValCount val_count = 0;
    fread(&val_count, sizeof(val_count), 1, fp);

    FklVMvalue *names = fklCreateVMvalueVec(values->vm, val_count);
    for (size_t i = 0; i < val_count; ++i) {
        FKL_VM_VEC(names)->base[i] = load_value_id(fp, values);
    }

    FklVM *vm = values->vm;

    LibType mod_type = 0;
    fread(&mod_type, sizeof(mod_type), 1, fp);

    FklVMvalueLib *lib = NULL;
    if (mod_type != FKL_LIB_REF_EXTERNAL) {
        lib = fklCreateVMvalueLib(vm, name, FKL_VM_VEC(names));
    }

    switch ((FklLibRefType)mod_type) {
    default:
        FKL_UNREACHABLE();
        break;

    case FKL_LIB_REF_EXTERNAL: {
        FKL_ASSERT(libs->is_loading_pre_compile != 0);
        uint8_t is_imported_by_macro = 0;
        fread(&is_imported_by_macro, sizeof(is_imported_by_macro), 1, fp);

        FklFileType ft = FKL_FILE_NONE;
        FklVMvalue *rp = fklResolveLibPath(libs->vm, libs->main_dir, name, &ft);
        if (rp == NULL) {
            goto mod_not_imported;
        }

        printf("[DEBUG] external lib rp: %s\n", FKL_VM_SYM(rp)->str);

        const FklCgCtx *cg_ctx = libs->cg_ctx;
        FklVMvalueCgLibs *cg_libs = is_imported_by_macro
                                          ? cg_ctx->macro_libraries
                                          : cg_ctx->libraries;

        const FklCgLib *l = fklVMvalueCgLibsGet1(cg_libs, rp);
        if (l != NULL) {
            lib = l->lib;
            break;
        }

    mod_not_imported:
        lib = fklCreateVMvalueLib(vm, name, FKL_VM_VEC(names));

        if (libs->fixup == NULL) {
            break;
        }

        FklPreCompileFixup *fixup = libs->fixup;

        lib->proc = FKL_MAKE_VM_FIX(fixup->pendings.size);

        FklPcDep dep = {
            .is_imported_by_macro = is_imported_by_macro,
            .name = name,
            .rp = rp,
            .ft = ft,
        };

        fklPcDepVectorPushBack(&fixup->pendings, &dep);
    } break;

    case FKL_LIB_REF_SCRIPT_EMBEDDED: {
        FklVMvalueProc *proc = load_proc(fp, values, protos);
        lib->proc = FKL_VM_VAL(proc);
    } break;

    case FKL_LIB_REF_DLL_INTERNAL: {
        lib->proc = load_value_id(fp, values);
        FKL_ASSERT(FKL_IS_SYM(lib->proc));

        FklFileType ft = FKL_FILE_NONE;
        FklVMvalue *rp = fklResolveLibPath(vm, libs->main_dir, lib->proc, &ft);
        if (rp == NULL || ft != FKL_FILE_DLL) {
            if (libs->fixup == NULL)
                break;

            FklPreCompileFixup *fixup = libs->fixup;

            lib->proc = FKL_MAKE_VM_FIX(fixup->pendings.size);

            FklPcDep dep = {
                .is_imported_by_macro = 0,
                .name = name,
                .rp = rp,
                .ft = ft,
            };

            fklPcDepVectorPushBack(&fixup->pendings, &dep);
            break;
        }

        lib->proc = rp;
    } break;

    case FKL_LIB_REF_DLL_ABSOLUTE:
        lib->proc = load_value_id(fp, values);
        FKL_ASSERT(FKL_IS_SYM(lib->proc));
        break;
    }

    return lib;
}

static int load_proto_table(FILE *fp,
        const FklLoadValueArgs *values,
        FklValueVector *ph_vec,
        FklLoadProtoArgs *args) {
    fread(&args->count, sizeof(args->count), 1, fp);

    if (args->count == 0) {
        args->protos = NULL;
        return 0;
    }

    size_t const total_size = args->count * sizeof(FklVMvalueProto *);
    FklVMvalueProto **protos = (FklVMvalueProto **)fklZmalloc(total_size);
    FKL_ASSERT(protos);
    memset(protos, 0, total_size);

    args->protos = protos;
    for (FklValueId id = args->count; id > 0; --id) {
        args->protos[id - 1] = load_prototype(fp, values, args, ph_vec);
    }
    return 0;
}

static int load_lib_table(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        FklLoadLibArgs *args) {
    fread(&args->count, sizeof(args->count), 1, fp);
    if (args->count == 0) {
        args->libs = NULL;
        return 0;
    }

    size_t const total_size = args->count * sizeof(FklVMvalueLib *);
    FklVMvalueLib **libs = (FklVMvalueLib **)fklZmalloc(total_size);
    FKL_ASSERT(libs);
    memset(libs, 0, total_size);

    args->libs = libs;
    for (FklValueId id = args->count; id > 0; --id) {
        args->libs[id - 1] = load_vm_lib(fp, values, protos, args);
    }

    return 0;
}

static inline void write_symbol_def_pass_1(const FklVarRefDef *def,
        FklValueTable *vt) {
    fklTraverseSerializableValue(vt, def->sid);
    fklTraverseSerializableValue(vt, def->cidx);
    fklTraverseSerializableValue(vt, def->is_local);
}

static inline void write_vm_lib_pass_1(const FklVMvalueLib *l,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {

    FklValueTable *vt = extra_args->value_table;
    FklLibTable *lib_table = extra_args->lib_table;

    FklValueId id = fklLibTableGet(lib_table, l);
    if (id != 0)
        return;

    fklLibTableAdd(lib_table, l);

    FklVMvalue *proc_v = l->proc;

    fklValueTableAdd(vt, l->name);

    FklVMvalue *const *names = fklVMvalueLibNames(l);
    for (size_t i = 0; i < l->count; ++i) {
        fklValueTableAdd(vt, names[i]);
    }

    int is_writting_pre_compile = extra_args->is_writting_pre_compile;
    const FklLibTable *internal_lib_table = extra_args->internal_lib_table;

    if (is_writting_pre_compile && fklLibTableGet(internal_lib_table, l) == 0) {
        return;
    }

    if (FKL_IS_PROC(proc_v)) {
        FklVMvalueProc *proc = FKL_VM_PROC(proc_v);
        fklValueVectorPushBack2(pending, FKL_VM_VAL(proc->proto));
        write_bc_lnt(FKL_VM_CO(proc->bcl), vt, FKL_WRITE_CODE_PASS_FIRST, NULL);
    } else if (FKL_IS_SYM(proc_v)) {
        if (!is_writting_pre_compile)
            fklValueTableAdd(vt, proc_v);
    } else if (!fklIsVMvalueDll(proc_v)) {
        FKL_UNREACHABLE();
    }
}

static inline void write_prototype_pass_1(const FklVMvalueProto *pt,
        const WriteLibExtraArgs *extra_args) {

    FklValueTable *vt = extra_args->value_table;
    FklProtoTable *proto_table = extra_args->proto_table;

    FklValueVector pending = { 0 };
    fklValueVectorInit(&pending, pt->child_proto_count);
    fklValueVectorPushBack2(&pending, FKL_VM_VAL(pt));

    while (!fklValueVectorIsEmpty(&pending)) {
        FklVMvalue *pt_v = *fklValueVectorPopBackNonNull(&pending);
        FklVMvalueProto *pt = fklVMvalueProto(pt_v);

        FklValueId id = fklProtoTableGet(proto_table, pt);
        if (id != 0)
            continue;

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

        FklVMvalueLib *const *libs = fklVMvalueProtoUsedLibs(pt);
        for (uint32_t i = 0; i < pt->used_libraries_count; ++i) {
            FklVMvalueLib *const l = libs[i];
            write_vm_lib_pass_1(l, &pending, extra_args);
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
        const FklLibTable *lib_table,
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

    fwrite(&pt->used_libraries_count, sizeof(pt->used_libraries_count), 1, fp);
    fwrite(&pt->used_libraries_offset,
            sizeof(pt->used_libraries_offset),
            1,
            fp);

    FklVMvalueLib *const *libs = fklVMvalueProtoUsedLibs(pt);
    for (uint32_t i = 0; i < pt->used_libraries_count; ++i) {
        write_lib_id(lib_table, 0, libs[i], fp);
    }
}

static void write_lnt(const FklLntItem *items,
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
            const FklLntItem *n = &items[i];
            write_value_id(vt, 0, n->fid, fp);
            fwrite(&n->scp, sizeof(n->scp), 1, fp);
            fwrite(&n->line, sizeof(n->line), 1, fp);
        }
        break;
    }
}

static void load_lnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklLntItem **plist,
        uint32_t *pnum) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    FklLntItem *list;
    if (count == 0)
        list = NULL;
    else {
        list = (FklLntItem *)fklZmalloc(count * sizeof(FklLntItem));
        FKL_ASSERT(list);
        for (uint32_t i = 0; i < count; i++) {
            FklLntItem *item = &list[i];
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

static void write_bc(const FklByteCode *bc, FILE *outfp) {
    uint64_t len = bc->len;
    fwrite(&len, sizeof(bc->len), 1, outfp);
    size_t r = fwrite(bc->code, sizeof(bc->code[0]) * len, 1, outfp);
    FKL_ASSERT(r == 1);
    (void)r;
    // XXX: need refactor
#if 0
    const FklIns *end = bc->code + len;
    const FklIns *code = bc->code;
    while (code < end) {
        uint8_t op = FKL_INS_OP(code);
        fwrite(&op, sizeof(op), 1, outfp);
        int ins_len = fklGetOpcodeModeLen(op);
        FklOpcodeMode mode = fklGetOpcodeMode(op);
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
#endif
}

static void write_proc(const FklVMvalueProc *proc,
        FklWriteCodePass pass,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        write_prototype_pass_1(proc->proto, extra_args);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_proto_id(extra_args->proto_table, 0, proc->proto, fp);
        break;
    }
    write_bc_lnt(FKL_VM_CO(proc->bcl), extra_args->value_table, pass, fp);
}

static FklVMvalueProc *load_proc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos) {
    FklVM *vm = values->vm;
    FklVMvalueProto *pt = load_proto_id(fp, protos);
    FklVMvalue *bcl = fklCreateVMvalueCodeObj1(vm);

    int r = load_bc_lnt(fp, values, FKL_VM_CO(bcl));
    FKL_ASSERT(r == 0);
    (void)r;

    FklVMvalue *proc = fklCreateVMvalueProc(vm, bcl, pt);
    fklInitMainProcRefs(vm, proc);

    return FKL_VM_PROC(proc);
}

static void write_bc_lnt(const FklByteCodelnt *bcl,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp) {
    write_lnt(bcl->l, bcl->ls, vt, pass, fp);
    if (pass == FKL_WRITE_CODE_PASS_SECOND)
        write_bc(&bcl->bc, fp);
}

static void load_bc(FklByteCode *tmp, FILE *fp) {
    uint64_t len = 0;
    fread(&len, sizeof(uint64_t), 1, fp);
    fklInitByteCode(tmp, len);
    size_t r = fread(tmp->code, sizeof(tmp->code[0]) * len, 1, fp);
    FKL_ASSERT(r == 1);
    (void)r;

// XXX: need refactor
#if 0
    const FklIns *end = tmp->code + len;
    FklIns *code = tmp->code;
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
#endif
}

static int
load_bc_lnt(FILE *fp, const FklLoadValueArgs *values, FklByteCodelnt *bcl) {
    load_lnt(fp, values, &bcl->l, &bcl->ls);
    load_bc(&bcl->bc, fp);
    return 0;
}

static inline void write_prototype_table(const FklProtoTable *proto_table,
        const FklValueTable *value_table,
        const FklLibTable *lib_table,
        FILE *fp) {
    FklValueId count = proto_table->vt.next_id - 1;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklValueIdHashMapNode *cur = proto_table->vt.ht.last; cur;
            cur = cur->prev) {
        const FklVMvalue *pt_v = cur->k;
        write_prototype_pass_2(fklVMvalueProto(pt_v),
                value_table,
                proto_table,
                lib_table,
                fp);
    }
}

static inline LibType get_writting_lib_type(LibType t,
        const FklVMvalueLib *l,
        const WriteLibExtraArgs *extra_args) {
    int is_writting_pre_compile = extra_args->is_writting_pre_compile;

    if (is_writting_pre_compile) {
        const FklLibTable *intern = extra_args->internal_lib_table;
        FKL_ASSERT(intern);
        LibIdx id = fklLibTableGet(intern, l);

        if (id == 0) {
            t = FKL_LIB_REF_EXTERNAL;
        }

        return t;
    }

    switch ((FklLibRefType)t) {
    default:
        FKL_UNREACHABLE();
        break;
    case FKL_LIB_REF_SCRIPT_EMBEDDED:
        break;

    case FKL_LIB_REF_DLL_INTERNAL:
        t = FKL_LIB_REF_DLL_ABSOLUTE;
        break;
    }

    return t;
}

static inline void write_vm_lib_pass_2(const FklVMvalueLib *lib,
        const FklLibTable *lib_table,
        const FklValueTable *value_table,
        const FklProtoTable *proto_table,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FKL_ASSERT(FKL_IS_PROC(lib->proc)   //
               || FKL_IS_SYM(lib->proc) //
               || fklIsVMvalueDll(lib->proc));

    LibType type_byte = FKL_IS_PROC(lib->proc) ? FKL_LIB_REF_SCRIPT_EMBEDDED
                                               : FKL_LIB_REF_DLL_INTERNAL;

    type_byte = get_writting_lib_type(type_byte, lib, extra_args);

    write_value_id(value_table, 0, lib->name, fp);

    if (type_byte == FKL_LIB_REF_EXTERNAL) {
        static const char *lib_ref_type_names[] = {
            [FKL_LIB_REF_EXTERNAL] = "external",
            [FKL_LIB_REF_SCRIPT_EMBEDDED] = "script-embedded",
            [FKL_LIB_REF_DLL_ABSOLUTE] = "dll-absolute",
            [FKL_LIB_REF_DLL_INTERNAL] = "dll-internal",
        };
        printf("[DEBUG] writting \"%s\" lib: %s\n",
                lib_ref_type_names[type_byte],
                FKL_VM_SYM(lib->name)->str);
    }

    TotalValCount val_count = lib->count;
    fwrite(&val_count, sizeof(val_count), 1, fp);

    FklVMvalue *const *names = fklVMvalueLibNames(lib);
    for (size_t i = 0; i < val_count; ++i) {
        write_value_id(value_table, 0, names[i], fp);
    }

    fwrite(&type_byte, sizeof(type_byte), 1, fp);
    switch ((FklLibRefType)type_byte) {
    default:
        FKL_UNREACHABLE();
        break;

    case FKL_LIB_REF_SCRIPT_EMBEDDED: {
        FklVMvalueProc *proc = FKL_VM_PROC(lib->proc);
        write_proc(proc, FKL_WRITE_CODE_PASS_SECOND, extra_args, fp);
    } break;

    case FKL_LIB_REF_DLL_INTERNAL: {
        FklVMvalue *proc = lib->proc;
        if (FKL_IS_SYM(proc) || fklIsVMvalueDll(proc)) {
            write_value_id(value_table, 0, lib->name, fp);
        } else {
            FKL_UNREACHABLE();
        }
    } break;

    case FKL_LIB_REF_DLL_ABSOLUTE: {
        FklVMvalue *rp = lib->proc;
        FKL_ASSERT(FKL_IS_SYM(lib->proc));
        write_value_id(value_table, 0, rp, fp);
    } break;

    case FKL_LIB_REF_EXTERNAL: {
        LibIdx idx = fklLibTableGet(extra_args->imported_by_macros, lib);
        uint8_t is_imported_by_macro = idx != 0;
        fwrite(&is_imported_by_macro, sizeof(is_imported_by_macro), 1, fp);
        printf("[DEBUG] is imported by macros: %d\n", is_imported_by_macro);
    } break;
    }
}

static inline void write_lib_table(const FklLibTable *lib_table,
        const FklValueTable *value_table,
        const FklProtoTable *proto_table,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FklValueId count = lib_table->vt.next_id - 1;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklValueIdHashMapNode *cur = lib_table->vt.ht.last; cur;
            cur = cur->prev) {
        const FklVMvalue *lib_v = cur->k;
        write_vm_lib_pass_2(fklVMvalueLib(lib_v),
                lib_table,
                value_table,
                proto_table,
                extra_args,
                fp);
    }
}

void fklWriteCodeFile(FILE *fp, const FklVMvalueProc *const main_func) {
    FklValueTable value_table = { 0 };
    fklInitValueTable(&value_table);

    FklProtoTable proto_table = { 0 };
    fklInitProtoTable(&proto_table);

    FklLibTable lib_table = { 0 };
    fklInitLibTable(&lib_table);

    WriteLibExtraArgs extra_args = {
        .is_writting_pre_compile = 0,

        .value_table = &value_table,
        .proto_table = &proto_table,
        .lib_table = &lib_table,
    };

    write_proc(main_func, FKL_WRITE_CODE_PASS_FIRST, &extra_args, NULL);

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, &lib_table, fp);

    write_lib_table(&lib_table, &value_table, &proto_table, &extra_args, fp);

    write_proc(main_func, FKL_WRITE_CODE_PASS_SECOND, &extra_args, fp);

    fklUninitLibTable(&lib_table);
    fklUninitProtoTable(&proto_table);
    fklUninitValueTable(&value_table);
}

static int fixup_proto_lib_refs(const FklLoadProtoArgs *protos,
        const FklLoadLibArgs *args) {
    FklVMvalueProto *const *cur = protos->protos;
    FklVMvalueProto *const *const end = cur + protos->count;
    for (; cur < end; ++cur) {
        FklVMvalueProto *c = *cur;
        LibIdx count = c->used_libraries_count;
        FklVMvalue **libs = &c->vals[c->used_libraries_offset];
        for (LibIdx i = 0; i < count; ++i) {
            LibIdx idx = as_lib_placeholder(libs[i])->idx;
            libs[i] = FKL_VM_VAL(get_lib_with_id(args, idx));
        }
    }
    return 0;
}

FklVMvalueProc *fklLoadCodeFile(FILE *fp,
        FklVM *vm,
        const char *main_dir,
        FklLibTable *lib_table) {
    FklLoadValueArgs values = { .vm = vm };
    FklLoadProtoArgs protos = { .vm = vm };
    FklLoadLibArgs libs = {
        .is_loading_pre_compile = 0,
        .vm = vm,
        .main_dir = main_dir,
    };

    int r = load_value_table(fp, &values);
    (void)r;
    FKL_ASSERT(r == 0);

    FklValueVector ph_vec = { 0 };
    fklValueVectorInit(&ph_vec, 8);

    r = load_proto_table(fp, &values, &ph_vec, &protos);
    FKL_ASSERT(r == 0);

    fklValueVectorUninit(&ph_vec);

    r = load_lib_table(fp, &values, &protos, &libs);
    FKL_ASSERT(r == 0);

    fixup_proto_lib_refs(&protos, &libs);

    FklVMvalueProc *main_func = load_proc(fp, &values, &protos);

    values.count = 0;
    fklZfree(values.values);
    values.values = NULL;

    protos.count = 0;
    fklZfree(protos.protos);
    protos.protos = NULL;

    if (lib_table != NULL) {
        for (FklValueId i = 0; i < libs.count; ++i)
            fklLibTableAdd(lib_table, libs.libs[i]);
    }

    libs.count = 0;
    fklZfree(libs.libs);
    libs.libs = NULL;

    return main_func;
}

// write pre compile file

static inline void write_prod_action_pass_1(const FklVMvalue *ac,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    if (fklIsVMvalueCustomActCtx(ac)) {
        FklVMvalueCustomActCtx *ctx = fklVMvalueCustomActCtx(ac);

        uint64_t len = ctx->actual_len;
        fklTraverseSerializableValue(vt, ctx->doller_s);
        fklTraverseSerializableValue(vt, ctx->line_s);
        for (size_t i = 0; i < len; ++i) {
            fklTraverseSerializableValue(vt, ctx->dollers[i]);
        }

        write_proc(FKL_VM_PROC(ctx->proc),
                FKL_WRITE_CODE_PASS_FIRST,
                extra_args,
                NULL);
    } else if (fklIsVMvalueSimpleActCtx(ac)) {
        FklVMvalueSimpleActCtx *ctx = fklVMvalueSimpleActCtx(ac);
        fklTraverseSerializableValue(vt, ctx->vec);
    } else {
        // is replace or builtin prod
        fklTraverseSerializableValue(vt, ac);
    }
}

static inline void write_prod_action_pass_2(const FklVMvalue *ac,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    uint8_t type;

    FklValueTable *vt = extra_args->value_table;
    if (fklIsVMvalueCustomActCtx(ac)) {
        type = FKL_CG_PROD_ACT_CTX_TYPE_CUSTOM;
        fwrite(&type, sizeof(type), 1, fp);
        FklVMvalueCustomActCtx *ctx = fklVMvalueCustomActCtx(ac);

        MacroCount len = ctx->actual_len;
        fwrite(&len, sizeof(len), 1, fp);
        write_value_id(vt, 0, ctx->doller_s, fp);
        write_value_id(vt, 0, ctx->line_s, fp);
        for (size_t i = 0; i < len; ++i) {
            write_value_id(vt, 0, ctx->dollers[i], fp);
        }

        write_proc(FKL_VM_PROC(ctx->proc),
                FKL_WRITE_CODE_PASS_SECOND,
                extra_args,
                fp);
    } else if (fklIsVMvalueSimpleActCtx(ac)) {
        type = FKL_CG_PROD_ACT_CTX_TYPE_SIMPLE;
        fwrite(&type, sizeof(type), 1, fp);
        FklVMvalueSimpleActCtx *ctx = fklVMvalueSimpleActCtx(ac);
        write_value_id(vt, 0, ctx->vec, fp);
    } else {
        type = FKL_CG_PROD_ACT_CTX_TYPE_OTHER;
        fwrite(&type, sizeof(type), 1, fp);
        write_value_id(vt, 0, ac, fp);
    }
}

static inline void write_rmacro_prod_pass_1(const FklVMvalueCgRmacroProd *prod,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    fklTraverseSerializableValue(vt, prod->left);
    fklTraverseSerializableValue(vt, prod->action_type);
    write_prod_action_pass_1(prod->action, extra_args);

    for (size_t i = 0; i < prod->len; ++i) {
        const FklCgRmacroGraSym *sym = &prod->syms[i];
        fklTraverseSerializableValue(vt, sym->v);
    }
}

static inline void write_rmacro_cmd_pass_1(const FklCgRmacroCmd *cmd,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    FklVMvalue *v = cmd->args;
    switch (cmd->op) {
    case FKL_CG_RMACRO_NONE:
        FKL_UNREACHABLE();
        break;
    case FKL_CG_RMACRO_ADD_PROD:
    case FKL_CG_RMACRO_ADD_IGNORE:
        write_rmacro_prod_pass_1(fklVMvalueCgRmacroProd(v), extra_args);
        break;
    case FKL_CG_RMACRO_ADD_DELIM:
        fklTraverseSerializableValue(vt, v);
        break;
    }
}

static inline void write_rmacro_pass_1(const FklVMvalueCgRmacro *g,
        const WriteLibExtraArgs *extra_args) {
    for (uint64_t i = 0; i < g->len; ++i) {
        const FklCgRmacroCmd *cmd = &g->cmds[i];
        write_rmacro_cmd_pass_1(cmd, extra_args);
    }
}

static inline void write_rmacro_prod_pass_2(const FklVMvalueCgRmacroProd *prod,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FklValueTable *vt = extra_args->value_table;
    write_value_id(vt, 0, prod->left, fp);
    write_value_id(vt, 0, prod->action_type, fp);
    write_prod_action_pass_2(prod->action, extra_args, fp);

    uint8_t add_extra = prod->add_extra;
    fwrite(&add_extra, sizeof(add_extra), 1, fp);

    MacroCount len = prod->len;
    fwrite(&len, sizeof(len), 1, fp);

    for (size_t i = 0; i < len; ++i) {
        const FklCgRmacroGraSym *sym = &prod->syms[i];
        FklVMvalue *v = sym->v;
        uint8_t type = sym->type;

        fwrite(&type, sizeof(type), 1, fp);
        write_value_id(vt, 0, v, fp);
    }
}

static inline void write_rmacro_cmd_pass_2(const FklCgRmacroCmd *cmd,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FklValueTable *vt = extra_args->value_table;
    FklVMvalue *v = cmd->args;
    uint8_t op = cmd->op;
    fwrite(&op, sizeof(op), 1, fp);
    switch (cmd->op) {
    case FKL_CG_RMACRO_NONE:
        FKL_UNREACHABLE();
    case FKL_CG_RMACRO_ADD_PROD:
    case FKL_CG_RMACRO_ADD_IGNORE:
        write_rmacro_prod_pass_2(fklVMvalueCgRmacroProd(v), extra_args, fp);
        break;
    case FKL_CG_RMACRO_ADD_DELIM:
        write_value_id(vt, 0, v, fp);
        break;
    }
}

static inline void write_rmacro_pass_2(const FklVMvalueCgRmacro *g,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    MacroCount count = g->len;
    fwrite(&count, sizeof(count), 1, fp);
    for (size_t i = 0; i < count; ++i) {
        const FklCgRmacroCmd *cmd = &g->cmds[i];
        write_rmacro_cmd_pass_2(cmd, extra_args, fp);
    }
}

static inline void write_rmacros_pass_1(
        const FklVMvalueCgRmacroHashMap *rmacros,
        const WriteLibExtraArgs *extra_args) {
    if (rmacros == NULL)
        return;

    FklValueTable *vt = extra_args->value_table;
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        fklTraverseSerializableValue(vt, cur->k);
        FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(cur->v);
        write_rmacro_pass_1(rmacro, extra_args);
    }
}

static inline void write_rmacros_pass_2(
        const FklVMvalueCgRmacroHashMap *rmacros,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FklValueTable *vt = extra_args->value_table;
    MacroCount count = rmacros->ht.count;
    fwrite(&count, sizeof(count), 1, fp);
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        write_value_id(vt, 0, cur->k, fp);
        FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(cur->v);
        write_rmacro_pass_2(rmacro, extra_args, fp);
    }
}

static inline FklVMvalue *load_prod_action(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    uint8_t type = 255;
    fread(&type, sizeof(type), 1, fp);
    FKL_ASSERT(type != 255);
    FklVMvalue *v = NULL;
    switch ((FklCgProdActCtxType)type) {
    case FKL_CG_PROD_ACT_CTX_TYPE_OTHER:
        v = load_value_id(fp, values);
        break;
    case FKL_CG_PROD_ACT_CTX_TYPE_SIMPLE: {
        FklVMvalue *act = load_value_id(fp, values);
        FklVMvalueSimpleActCtx *s = fklCreateVMvalueSimpleActCtx1(ctx, act);
        FKL_ASSERT(s != NULL);
        v = FKL_VM_VAL(s);
        break;
    }
    case FKL_CG_PROD_ACT_CTX_TYPE_CUSTOM: {
        MacroCount len = 0;
        fread(&len, sizeof(len), 1, fp);
        FklVMvalueCustomActCtx *c = fklCreateVMvalueCustomActCtx(ctx->vm, len);

        c->doller_s = load_value_id(fp, values);
        c->line_s = load_value_id(fp, values);
        for (size_t i = 0; i < len; ++i) {
            c->dollers[i] = load_value_id(fp, values);
        }

        FklVMvalueProc *proc = load_proc(fp, values, protos);
        c->proc = FKL_VM_VAL(proc);

        v = FKL_VM_VAL(c);
        break;
    }
    }
    return v;
}

static inline FklVMvalueCgRmacroProd *load_rmacro_prod(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    FklVMvalue *left = load_value_id(fp, values);
    FklVMvalue *action_type = load_value_id(fp, values);
    FklVMvalue *action = load_prod_action(fp, ctx, values, protos);

    uint8_t add_extra = 0;
    fread(&add_extra, sizeof(add_extra), 1, fp);

    MacroCount len = 0;
    fread(&len, sizeof(len), 1, fp);
    FklVMvalueCgRmacroProd *p = fklCreateVMvalueCgRmacroProd(ctx->vm,
            left,
            action_type,
            action,
            add_extra,
            len);

    for (size_t i = 0; i < len; ++i) {
        uint8_t type = 0;
        fread(&type, sizeof(type), 1, fp);
        FklVMvalue *v = load_value_id(fp, values);

        FklCgRmacroGraSym *sym = &p->syms[i];
        sym->type = type;
        sym->v = v;
    }

    return p;
}

static inline void load_rmacro_cmd(FklCgRmacroCmd *cmd,
        FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    uint8_t op = FKL_CG_RMACRO_NONE;
    fread(&op, sizeof(op), 1, fp);
    cmd->op = op;
    switch (cmd->op) {
    case FKL_CG_RMACRO_NONE:
        FKL_UNREACHABLE();
    case FKL_CG_RMACRO_ADD_PROD:
    case FKL_CG_RMACRO_ADD_IGNORE: {
        FklVMvalueCgRmacroProd *prod;
        prod = load_rmacro_prod(fp, ctx, values, protos);
        cmd->args = FKL_VM_VAL(prod);
        break;
    }
    case FKL_CG_RMACRO_ADD_DELIM:
        cmd->args = load_value_id(fp, values);
        break;
    }
}

static inline FklVMvalueCgRmacro *load_rmacro(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    MacroCount count = 0;
    fread(&count, sizeof(count), 1, fp);
    FklVMvalueCgRmacro *g = fklCreateVMvalueCgRmacro(ctx->vm, count);
    for (size_t i = 0; i < count; ++i) {
        FklCgRmacroCmd *cmd = &g->cmds[i];
        load_rmacro_cmd(cmd, fp, ctx, values, protos);
    }

    return g;
}

static inline FklVMvalueCgRmacroHashMap *load_rmacros(FILE *fp,
        FklCgCtx *ctx,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *protos) {
    FklVMvalueCgRmacroHashMap *ht = fklCreateVMvalueCgRmacroHashMap(ctx);
    MacroCount count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (; count > 0; --count) {
        FklVMvalue *name = load_value_id(fp, values);
        FklVMvalueCgRmacro *rmacro = load_rmacro(fp, ctx, values, protos);
        FklValueHashMapElm *e = fklCgRmacroHashMapRef1(ht, name);
        e->v = FKL_VM_VAL(rmacro);
    }

    return ht;
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

static inline void load_macros_with_same_header(FklCgCtx *ctx,
        FklVMvalue **pr,
        FILE *fp,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    MacroCount count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint64_t j = 0; j < count; ++j) {
        FklVMvalue *pat = load_value_id(fp, values);
        FklVMvalue *proc = FKL_VM_VAL(load_proc(fp, values, protos));
        FklVMvalueCgMacro *cur = fklCreateVMvalueCgMacro(ctx, pat, proc);
        FklVMvalue *pair = fklCreateVMvaluePair(ctx->vm, FKL_VM_VAL(cur), *pr);

        *pr = pair;
        pr = &FKL_VM_CDR(pair);
    }
}

static inline FklVMvalueCgMacroHashMap *load_compiler_macros(FklCgCtx *ctx,
        FILE *fp,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos) {
    FklVMvalueCgMacroHashMap *macro_table = fklCreateVMvalueCgMacroHashMap(ctx);
    MacroCount count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint64_t i = 0; i < count; ++i) {
        FklVMvalue *head = load_value_id(fp, values);
        FklValueHashMapElm *macros = fklCgMacroHashMapRef1(macro_table, head);
        FklVMvalue **pr = &macros->v;

        load_macros_with_same_header(ctx, pr, fp, values, protos);
    }

    return macro_table;
}

static inline FklVMvalueCgRplHashMap *load_replacements(FklCgCtx *ctx,
        FILE *fp,
        const FklLoadValueArgs *const values) {
    FklVMvalueCgRplHashMap *ht = fklCreateVMvalueCgRplHashMap(ctx);
    MacroCount count = 0;
    fread(&count, sizeof(count), 1, fp);
    for (uint32_t i = 0; i < count; i++) {
        FklVMvalue *id = load_value_id(fp, values);
        FklVMvalue *v = load_value_id(fp, values);

        FklVMvalueCgRpl *rpl = fklCreateVMvalueCgRpl(ctx, v);
        fklCgRplHashMapSet(ht, id, rpl);
    }
    return ht;
}

static inline void load_script_lib_from_pre_compile(FILE *fp,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos,
        FklCgLib *cg_lib,
        FklLoadPreCompileArgs *const args) {
    FklVMvalue *name = load_value_id(fp, values);
    load_export_sid_idx_table(fp, values, &cg_lib->exports);
    cg_lib->macros = load_compiler_macros(args->ctx, fp, values, protos);
    cg_lib->replacements = load_replacements(args->ctx, fp, values);
    cg_lib->rmacros = load_rmacros(fp, args->ctx, values, protos);

    FklVMvalueProc *proc = load_proc(fp, values, protos);
    FklVMvalueVec *names = fklCreateCgNamesVec(values->vm, &cg_lib->exports);
    FklVMvalueLib *lib = fklCreateVMvalueLib(values->vm, name, names);
    lib->proc = FKL_VM_VAL(proc);
    cg_lib->lib = lib;
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

static inline void write_compiler_macros_pass_1(
        const FklVMvalueCgMacroHashMap *macros,
        const WriteLibExtraArgs *extra_args) {

    FklValueTable *vt = extra_args->value_table;
    // TODO: may be incomplete
    if (macros == NULL)
        return;
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        fklTraverseSerializableValue(vt, cur->k);
        for (const FklVMvalue *c = cur->v; FKL_IS_PAIR(c); c = FKL_VM_CDR(c)) {
            FklVMvalueCgMacro *macro = fklVMvalueCgMacro(FKL_VM_CAR(c));
            fklTraverseSerializableValue(vt, macro->pattern);

            write_proc(FKL_VM_PROC(macro->proc),
                    FKL_WRITE_CODE_PASS_FIRST,
                    extra_args,
                    NULL);
        }
    }
}

static inline void write_compiler_macros_pass_2(
        const FklVMvalueCgMacroHashMap *macros,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    // TODO: may be incomplete
    MacroCount count = 0;
    if (macros == NULL) {
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }

    FklValueTable *vt = extra_args->value_table;
    count = macros->ht.count;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        write_value_id(vt, 0, cur->k, fp);

        MacroCount count = fklVMlistLength(cur->v);
        fwrite(&count, sizeof(count), 1, fp);

        for (const FklVMvalue *cur_pair = cur->v; FKL_IS_PAIR(cur_pair);
                cur_pair = FKL_VM_CDR(cur_pair)) {
            FklVMvalueCgMacro *macro = fklVMvalueCgMacro(FKL_VM_CAR(cur_pair));
            write_value_id(vt, 0, macro->pattern, fp);
            write_proc(FKL_VM_PROC(macro->proc),
                    FKL_WRITE_CODE_PASS_SECOND,
                    extra_args,
                    fp);
        }
    }
}

static inline void write_replacements_pass_1(const FklVMvalueCgRplHashMap *ht,
        FklValueTable *vt) {
    if (ht == NULL)
        return;
    for (const FklValueHashMapNode *rep_list = ht->ht.first; rep_list;
            rep_list = rep_list->next) {
        FklVMvalueCgRpl *rpl = fklVMvalueCgRpl(rep_list->v);
        fklTraverseSerializableValue(vt, rep_list->k);
        fklTraverseSerializableValue(vt, rpl->value);
    }
}

static inline void write_replacements_pass_2(const FklVMvalueCgRplHashMap *ht,
        const FklValueTable *vt,
        FILE *fp) {
    MacroCount count = 0;
    if (ht == NULL) {
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }
    count = ht->ht.count;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklValueHashMapNode *rep_list = ht->ht.first; rep_list;
            rep_list = rep_list->next) {
        FklVMvalueCgRpl *rpl = fklVMvalueCgRpl(rep_list->v);
        write_value_id(vt, 0, rep_list->k, fp);
        write_value_id(vt, 0, rpl->value, fp);
    }
}

static inline void write_lib_main_file_passes(FILE *outfp,
        const char *main_dir,
        FklWriteCodePass pass,
        const FklVMvalueCgInfo *info,
        const FklVMvalueProc *proc,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *value_table = extra_args->value_table;

    switch (pass) {
    case FKL_WRITE_CODE_PASS_FIRST:
        fklValueTableAdd(value_table, info->fid);
        write_export_sid_idx_table_pass_1(&info->exports, value_table);
        write_compiler_macros_pass_1(info->export_macros, extra_args);
        write_replacements_pass_1(info->export_replacement, value_table);
        write_rmacros_pass_1(info->export_rmacros, extra_args);
        write_proc(proc, FKL_WRITE_CODE_PASS_FIRST, extra_args, NULL);
        break;
    case FKL_WRITE_CODE_PASS_SECOND:
        write_value_id(value_table, 0, info->fid, outfp);
        write_export_sid_idx_table_pass_2(&info->exports, value_table, outfp);
        write_compiler_macros_pass_2(info->export_macros, extra_args, outfp);
        write_replacements_pass_2(info->export_replacement, value_table, outfp);
        write_rmacros_pass_2(info->export_rmacros, extra_args, outfp);

        write_proc(proc, FKL_WRITE_CODE_PASS_SECOND, extra_args, outfp);

        break;
    }
}

static inline void write_pre_compile_passes(FILE *fp,
        const char *target_dir,
        FklWriteCodePass pass,
        const FklWritePreCompileArgs *const args,
        const WriteLibExtraArgs *extra_args) {
    const FklCgCtx *const ctx = args->ctx;
    const char *main_dir = ctx->main_file_real_path_dir;

    write_lib_main_file_passes(fp,
            main_dir,
            pass,
            args->main_info,
            args->main_proc,
            extra_args);
}

static FKL_ALWAYS_INLINE int is_internal_module(const char *main_dir,
        const FklCgLib *l) {
    const char *rp = fklCgLibRp(l);
    return fklStrStartWith(rp, main_dir);
}

static void collect_internal_modules(const FklVMvalueCgInfo *info,
        FklLibTable *intern) {
    const char *main_dir = info->dir;

    for (const FklValueHashMapNode *c = info->libraries->ht.first; c != NULL;
            c = c->next) {
        const FklCgLib *l = fklVMvalueCgLib(c->v);
        const char *rp = fklCgLibRp(l);
        if (is_internal_module(main_dir, l)) {
            fklLibTableAdd(intern, l->lib);
        }

        printf("[DEBUG] lib rp: %s, is internal module: %d\n",
                rp,
                is_internal_module(main_dir, l));
    }
}

static void collect_libs_imported_by_macros(const FklCgCtx *ctx,
        FklLibTable *out) {
    const char *main_dir = ctx->main_file_real_path_dir;
    for (const FklValueHashMapNode *c = ctx->macro_libraries->ht.first;
            c != NULL;
            c = c->next) {
        const FklCgLib *l = fklVMvalueCgLib(c->v);
        if (!is_internal_module(main_dir, l)) {
            fklLibTableAdd(out, l->lib);
        }
    }
}

static void collect_replacements(FklVMvalueCgRplHashMap *rpls,
        FklValueTable *vt) {
    for (const FklValueHashMapNode *c = rpls->ht.first; c != NULL;
            c = c->next) {
        FklVMvalue *rpl = c->v;
        FKL_ASSERT(fklIsVMvalueCgRpl(rpl));
        fklValueTableAdd(vt, rpl);
    }
}

static void collect_cmacros(FklVMvalueCgMacroHashMap *cmacros,
        FklValueTable *vt) {
    for (const FklValueHashMapNode *c = cmacros->ht.first; c != NULL;
            c = c->next) {
        FklVMvalue *macros = c->v;
        for (const FklVMvalue *cur_pair = macros; FKL_IS_PAIR(cur_pair);
                cur_pair = FKL_VM_CDR(cur_pair)) {
            FklVMvalue *m = FKL_VM_CAR(cur_pair);
            FKL_ASSERT(fklIsVMvalueCgMacro(m));
            fklValueTableAdd(vt, m);
        }
    }
}

static void collect_rmacros(FklVMvalueCgRmacroHashMap *rmacros,
        FklValueTable *vt) {
    for (const FklValueHashMapNode *c = rmacros->ht.first; c != NULL;
            c = c->next) {
        FklVMvalue *rmacro = c->v;
        FKL_ASSERT(fklIsVMvalueCgRmacro(rmacro));
        fklValueTableAdd(vt, rmacro);
    }
}

static void collect_external_macros1(const char *main_dir,
        const FklVMvalueCgLibs *libs,
        FklValueTable *t) {
    for (const FklValueHashMapNode *c = libs->ht.first; c != NULL;
            c = c->next) {
        const FklCgLib *l = fklVMvalueCgLib(c->v);
        if (is_internal_module(main_dir, l))
            continue;
        if (!FKL_IS_PROC(l->lib->proc))
            continue;

        collect_replacements(l->replacements, t);
        collect_cmacros(l->macros, t);
        collect_rmacros(l->rmacros, t);
    }
}

static void collect_external_macros(const FklCgCtx *ctx, FklValueTable *t) {
    collect_external_macros1(ctx->main_file_real_path_dir,
            ctx->macro_libraries,
            t);
    collect_external_macros1(ctx->main_file_real_path_dir, ctx->libraries, t);
}

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args) {
    const FklVMvalueCgInfo *info = args->main_info;

    FklValueTable value_table;
    fklInitValueTable(&value_table);

    FklProtoTable proto_table;
    fklInitProtoTable(&proto_table);

    FklLibTable lib_table;
    fklInitLibTable(&lib_table);

    FklLibTable internal_lib_table;
    fklInitLibTable(&internal_lib_table);

    FklValueTable macro_table;
    fklInitValueTable(&macro_table);

    FklLibTable libs_imported_by_macros;
    fklInitLibTable(&libs_imported_by_macros);

    collect_internal_modules(info, &internal_lib_table);
    collect_external_macros(args->ctx, &macro_table);
    collect_libs_imported_by_macros(args->ctx, &libs_imported_by_macros);

    WriteLibExtraArgs extra_args = {
        .is_writting_pre_compile = 1,
        .internal_lib_table = &internal_lib_table,
        .imported_by_macros = &libs_imported_by_macros,

        .value_table = &value_table,
        .proto_table = &proto_table,
        .lib_table = &lib_table,
    };

    write_pre_compile_passes(fp,
            target_dir,
            FKL_WRITE_CODE_PASS_FIRST,
            args,
            &extra_args);

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, &lib_table, fp);

    write_lib_table(&lib_table, &value_table, &proto_table, &extra_args, fp);

    write_pre_compile_passes(fp,
            target_dir,
            FKL_WRITE_CODE_PASS_SECOND,
            args,
            &extra_args);

    fklUninitValueTable(&value_table);
    fklUninitValueTable(&macro_table);

    fklUninitProtoTable(&proto_table);
    fklUninitLibTable(&lib_table);
    fklUninitLibTable(&internal_lib_table);
    fklUninitLibTable(&libs_imported_by_macros);
}

FKL_NODISCARD
static FKL_ALWAYS_INLINE FklVMvalue *has_unimportable_mod(
        const FklPreCompileFixup *fixup) {
	if(fixup == NULL)
		return NULL;
    const FklPcDepVector *pendings = &fixup->pendings;
    for (size_t i = 0; i < pendings->size; ++i) {
        const FklPcDep *dep = &pendings->base[i];
        if (dep->ft == FKL_FILE_NONE) {
            return dep->name;
        }
    }

    return NULL;
}

const FklCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args) {
    int err = 0;

    FklCgCtx *ctx = args->ctx;

    FklLoadValueArgs values = { .vm = ctx->vm };
    FklLoadProtoArgs protos = { .vm = ctx->vm };
    char *main_dir = fklDupDir(rp);
    FklLoadLibArgs libs = {
        .is_loading_pre_compile = 1,
        .vm = ctx->vm,
        .cg_ctx = ctx,
        .main_dir = main_dir,
        .fixup = args->fixup,
    };

    err = load_value_table(fp, &values);
    (void)err;
    FKL_ASSERT(err == 0);

    FklValueVector ph_vec = { 0 };
    fklValueVectorInit(&ph_vec, 8);

    err = load_proto_table(fp, &values, &ph_vec, &protos);
    FKL_ASSERT(err == 0);

    fklValueVectorUninit(&ph_vec);

    err = load_lib_table(fp, &values, &protos, &libs);
    FKL_ASSERT(err == 0);

    fixup_proto_lib_refs(&protos, &libs);

    FklCgLib *lib = fklVMvalueCgLibsAdd(args->ctx, args->libraries, rp);
    load_script_lib_from_pre_compile(fp, &values, &protos, lib, args);

    if (args->fixup) {
        for (size_t i = 0; i < protos.count; ++i) {
            FklVMvalue *v = FKL_VM_VAL(protos.protos[i]);
            fklValueVectorPushBack2(&args->fixup->protos, v);
        }
    }

    values.count = 0;
    fklZfree(values.values);
    values.values = NULL;

    protos.count = 0;
    fklZfree(protos.protos);
    protos.protos = NULL;

    if (args->lib_table != NULL) {
        for (FklValueId i = 0; i < libs.count; ++i)
            fklLibTableAdd(args->lib_table, libs.libs[i]);
    }

    libs.count = 0;
    fklZfree(libs.libs);
    libs.libs = NULL;

    fklZfree(main_dir);
    lib->lib->name = fklCgRealpathToModuleName(ctx, rp);

    FklVMvalue *mod_name = has_unimportable_mod(args->fixup);
    if (mod_name != NULL) {
        args->error_fmt = "failed to import %S for %S";
        args->error_obj = mod_name;
    }

    return lib;
}

void fklPreCompileFixupInit(FklPreCompileFixup *fixup) {
    fklPcDepVectorInit(&fixup->pendings, 8);
    fklValueVectorInit(&fixup->protos, 8);
}

void fklPreCompileFixupUninit(FklPreCompileFixup *fixup) {
    fklPcDepVectorUninit(&fixup->pendings);
    fklValueVectorUninit(&fixup->protos);
}

static FKL_ALWAYS_INLINE void fixup_proto_external_libs(FklVMvalueProto *p,
        const FklPreCompileFixup *fixup,
        const FklValueVector *lib_vec) {
    LibIdx count = p->used_libraries_count;
    FklVMvalue **libs = &p->vals[p->used_libraries_offset];
    for (LibIdx j = 0; j < count; ++j) {
        FklVMvalueLib *old_l = fklVMvalueLib(libs[j]);
        FklVMvalue *idx_v = old_l->proc;
        if (!FKL_IS_FIX(idx_v))
            continue;
        int64_t idx = FKL_GET_FIX(idx_v);
        FKL_ASSERT(idx >= 0 && idx < (int64_t)lib_vec->size);
        FklVMvalueCgLib *new_cg_l = fklVMvalueCgLib(lib_vec->base[idx]);
        libs[j] = FKL_VM_VAL(new_cg_l->lib);
    }
}

int fklPreCompileFixup(const FklPreCompileFixup *fixup, const FklCgCtx *ctx) {
    const FklPcDepVector *dep_vec = &fixup->pendings;
    FklValueVector lib_vec = { 0 };
    fklValueVectorInit(&lib_vec, fixup->pendings.size);

    for (size_t i = 0; i < dep_vec->size; ++i) {
        const FklPcDep *dep = &dep_vec->base[i];
        FklVMvalue *rp = dep->rp;
        FklVMvalueCgLibs *libs = dep->is_imported_by_macro
                                       ? ctx->macro_libraries
                                       : ctx->libraries;
        FklCgLib *l = fklVMvalueCgLibsGet1(libs, rp);
        if (l == NULL) {
            fklValueVectorUninit(&lib_vec);
            return -1;
        }

        fklValueVectorPushBack2(&lib_vec, FKL_VM_VAL(l));
    }

    const FklValueVector *protos = &fixup->protos;
    for (size_t i = 0; i < protos->size; ++i) {
        FklVMvalueProto *p = fklVMvalueProto(protos->base[i]);
        fixup_proto_external_libs(p, fixup, &lib_vec);
    }

    fklValueVectorUninit(&lib_vec);

    return 0;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fixup_print, "fix-up");

static void fixup_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(ud);
    FklPreCompileFixup *fixup = &f->f;

    FklPcDepVector *dep_vec = &fixup->pendings;
    for (size_t i = 0; i < dep_vec->size; ++i) {
        const FklPcDep *dep = &dep_vec->base[i];
        fklVMgcToGray(dep->name, gc);
        fklVMgcToGray(dep->rp, gc);
    }

    FklValueVector *proto_vec = &fixup->protos;
    for (size_t i = 0; i < proto_vec->size; ++i) {
        fklVMgcToGray(proto_vec->base[i], gc);
    }
}

static int fixup_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(ud);
    fklPreCompileFixupUninit(&f->f);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const FixupMt = {
    .size = sizeof(FklVMvaluePcFixup),
    .princ = fixup_print,
    .prin1 = fixup_print,
    .atomic = fixup_atomic,
    .finalize = fixup_finalize,
};

FklVMvaluePcFixup *fklCreateVMvaluePcFixup(FklVM *vm) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FixupMt, NULL);
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(v);
    fklPreCompileFixupInit(&f->f);
    return f;
}

int fklIsVMvaluePcFixup(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &FixupMt;
}
