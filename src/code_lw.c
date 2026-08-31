#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/string_table.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/value_table.h>
#include <fakeLisp/vm.h>

#include <fakeLisp/ins_helper.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// write and load value table

typedef FklVMvalueCgLib FklCgLib;
typedef FklVMvalueReExportCmds ReExportCmds;
typedef FklPreCompileFixup Fixup;
typedef FklVMvalueCgRmacroHashMap Rmacros;
typedef FklVMvalueCgMacroHashMap Macros;

typedef enum ReExportImportArg0Type {
    IMPORT_ARG0_FIX = 0,
    IMPORT_ARG0_LIB,
} ReExportImportArg0Type;

typedef struct {
    int is_writting_pre_compile;
    const FklLibTable *internal_lib_table;
    const FklLibTable *imported_by_macros;
    const FklValueTable *external_macros;

    const FklVMvalueHash *map;

    FklValueTable *value_table;
    FklProtoTable *proto_table;
    FklLibTable *lib_table;
    FklProcTable *proc_table;

    FklReExportCmdVector *re_exports;
    FklLibTable *meaningless_libs;

    FklRelocVector *relocations;
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
    FklPair *protos;
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
    Fixup *fixup;

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

static void
write_lnt(const FklLntItem *, uint32_t count, FklValueTable *vt, FILE *);

static void load_lnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklLntItem **plist,
        uint32_t *pnum);

static void write_bc(const FklByteCode *bc, FILE *fp);
static void load_bc(FklByteCode *bc, FILE *fp);

static void write_proc(const FklVMvalueProc *proc,
        const WriteLibExtraArgs *extra_args,
        FILE *fp);
static FklVMvalueProc *load_proc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos);

static void
write_bc_lnt(const FklByteCodelnt *bcl, FklValueTable *vt, FILE *fp);

FKL_NODISCARD
static int
load_bc_lnt(FILE *fp, const FklLoadValueArgs *values, FklByteCodelnt *bcl);

typedef uint32_t TotalValCount;

typedef uint32_t LibIdx;

typedef uint8_t LibType;

#define PRIu_LIBIDX PRIu32

FKL_VM_DEF_UD_STRUCT(LibPlaceholder, { LibIdx idx; });

static const alignas(8) FklVMvalueType LibPlaceholderType;

static FKL_ALWAYS_INLINE FKL_UNUSED int is_lib_placeholder(
        const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->tp_->token == &LibPlaceholderType.mt;
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

alignas(8) static const FklVMvalueType LibPlaceholderType =
        FKL_VM_TYPE_STATIC_INIT(LibPlaceholderType,
                {
                    .name = "lib-placeholder",
                    .size = sizeof(LibPlaceholder),
                    .princ = lib_placeholder_print,
                    .prin1 = lib_placeholder_print,
                });

static inline FklVMvalue *create_lib_placeholder(FklVM *vm, LibIdx idx) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &LibPlaceholderType);
    LibPlaceholder *p = (LibPlaceholder *)v;
    p->idx = idx;
    return v;
}

static inline LibPlaceholder *
create_lib_placeholder1(FklVM *vm, LibIdx idx, FklValueVector *ph_vec) {
    FKL_ASSERT(idx > 0);
    fklValueVectorResize2(ph_vec, idx, NULL);

    FklVMvalue *v = ph_vec->base[idx - 1];
    if (v != NULL) {
        FKL_ASSERT(is_lib_placeholder(v));
        return (LibPlaceholder *)v;
    }

    v = create_lib_placeholder(vm, idx);

    ph_vec->base[idx - 1] = v;
    return (LibPlaceholder *)v;
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
    MAKE_KEYWORD,
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

static inline FklPair *load_proto_id(FILE *fp, const FklLoadProtoArgs *protos) {
    FklValueId id = 0;
    fread(&id, sizeof(id), 1, fp);
    if (id == 0)
        return NULL;

    FklPair *r = &protos->protos[id - 1];
    FKL_ASSERT(r->car);

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
    } else if (FKL_IS_KEYWORD(v)) {
        write_value_create_op(MAKE_KEYWORD, fp);
        fklWriteString(FKL_VM_KEYWORD(v), fp);
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

    case MAKE_KEYWORD: {
        FklString *s = fklLoadString(fp);
        FklVMvalue *r = fklVMaddKeyword(vm, s);
        fklZfree(s);
        return r;
    }

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
        FklVMvalue *p = load_proto_id(fp, protos)->car;
        child_protos[i] = fklVMvalueProto(p);
    }

    fread(&pt->used_libraries_count, sizeof(pt->used_libraries_count), 1, fp);
    fread(&pt->used_libraries_offset, sizeof(pt->used_libraries_offset), 1, fp);

    FklVMvalue **libs = &pt->vals[pt->used_libraries_offset];
    for (uint32_t i = 0; i < pt->used_libraries_count; ++i) {
        FklValueId u = read_lib_id(fp);
        LibPlaceholder *p = create_lib_placeholder1(values->vm, u, ph_vec);
        libs[i] = FKL_VM_VAL(p);
    }

    return pt;
}

static FklCodeBuilder g_gdb_code_builder;
static FklCodeBuilder *const g_build = &g_gdb_code_builder;

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

    FklVMvalueLib *lib = fklCreateVMvalueLib(vm, name, FKL_VM_VEC(names));

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

        fklCodeBuilderLine(g_build,
                "[DEBUG] external lib rp: %s",
                FKL_VM_SYM(rp)->str);

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
        if (libs->fixup == NULL) {
            break;
        }

        Fixup *fixup = libs->fixup;

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

            Fixup *fixup = libs->fixup;

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

        fklCodeBuilderLine(g_build,
                "[DEBUG] dll internal: %s, rp %s",
                FKL_VM_SYM(lib->proc)->str,
                FKL_VM_SYM(rp)->str);

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

    size_t const total_size = args->count * sizeof(FklPair);
    FklPair *protos = (FklPair *)fklZmalloc(total_size);
    FKL_ASSERT(protos);
    memset(protos, 0, total_size);

    args->protos = protos;
    for (FklValueId id = args->count; id > 0; --id) {
        FklVMvalueProto *p = load_prototype(fp, values, args, ph_vec);
        args->protos[id - 1].car = FKL_VM_VAL(p);
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

static inline void traverse_symbol_def(const FklVarRefDef *def,
        FklValueTable *vt) {
    fklTraverseSerializableValue(vt, def->sid);
    fklTraverseSerializableValue(vt, def->cidx);
    fklTraverseSerializableValue(vt, def->is_local);
}

static inline void
write_symbol_def(const FklVarRefDef *def, const FklValueTable *vt, FILE *fp) {
    write_value_id(vt, 0, def->sid, fp);
    write_value_id(vt, 0, def->cidx, fp);
    write_value_id(vt, 0, def->is_local, fp);
}

static inline void write_prototype(const FklVMvalueProto *pt,
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
        write_symbol_def(&refs[i], vt, fp);
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
        FILE *fp) {
    fwrite(&count, sizeof(count), 1, fp);
    for (uint32_t i = 0; i < count; i++) {
        const FklLntItem *n = &items[i];
        write_value_id(vt, 0, n->fid, fp);
        fwrite(&n->scp, sizeof(n->scp), 1, fp);
        fwrite(&n->line, sizeof(n->line), 1, fp);
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
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    write_proto_id(extra_args->proto_table, 0, proc->proto, fp);
    write_bc_lnt(FKL_VM_CO(proc->bcl), extra_args->value_table, fp);
}

static FklVMvalueProc *load_proc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos) {
    FklVM *vm = values->vm;
    FklPair *p = load_proto_id(fp, protos);
    FklVMvalue *bcl = fklCreateVMvalueCodeObj1(vm);

    int r = load_bc_lnt(fp, values, FKL_VM_CO(bcl));
    FKL_ASSERT(r == 0);
    (void)r;

    FklVMvalueProto *pt = fklVMvalueProto(p->car);
    FklVMvalue *proc = fklCreateVMvalueProc(vm, bcl, pt);
    fklInitMainProcRefs(vm, proc);

    p->cdr = proc;
    return FKL_VM_PROC(proc);
}

static void
write_bc_lnt(const FklByteCodelnt *bcl, FklValueTable *vt, FILE *fp) {
    write_lnt(bcl->l, bcl->ls, vt, fp);
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
        write_prototype(fklVMvalueProto(pt_v),
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

static inline void write_vm_lib(const FklVMvalueLib *lib,
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
        fklCodeBuilderLine(g_build,
                "[DEBUG] writting \"%s\" lib: %s",
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
        write_proc(proc, extra_args, fp);
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
        fklCodeBuilderLine(g_build,
                "[DEBUG] is imported by macros: %d",
                is_imported_by_macro);
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
        write_vm_lib(fklVMvalueLib(lib_v),
                lib_table,
                value_table,
                proto_table,
                extra_args,
                fp);
    }
}

typedef int (*TraverseCb)(FklVM *vm,
        FklVMvalue *v,
        FklValueVector *const pending,
        void *args);

static int traverse_obj(FklVM *vm, TraverseCb cb, FklVMvalue *v, void *args) {
    int ret = 0;
    FklValueVector pendings = { 0 };
    fklValueVectorInit(&pendings, 8);
    fklValueVectorPushBack2(&pendings, v);
    while (!fklValueVectorIsEmpty(&pendings)) {
        FklVMvalue *v = *fklValueVectorPopBackNonNull(&pendings);
        ret = cb(vm, v, &pendings, args);
        if (ret != 0)
            break;
    }
    fklValueVectorUninit(&pendings);
    return ret;
}

static void traverse_compiler_macros(FklVMvalueCgMacroHashMap *macros,
        FklValueVector *const pending,
        const WriteLibExtraArgs *extra_args) {
    if (macros == NULL)
        return;

    FklValueTable *vt = extra_args->value_table;

    const FklValueTable *external_macros = extra_args->external_macros;

    for (FklValueHashMapNode *cur = macros->ht.first; cur;) {
        FklValueHashMapNode *next = cur->next;

        FklVMvalue **p_cdr = &cur->v;
        while (FKL_IS_PAIR(*p_cdr)) {
            const FklVMvalue *p = *p_cdr;

            FklVMvalue *macro_v = FKL_VM_CAR(p);
            if (fklValueTableGet(external_macros, macro_v) != 0) {
                *p_cdr = FKL_VM_CDR(p);
            } else {
                FklVMvalueCgMacro *macro = fklVMvalueCgMacro(macro_v);
                fklTraverseSerializableValue(vt, macro->pattern);

                fklValueVectorPushBack2(pending, macro->proc);
                p_cdr = &FKL_VM_CDR(p);
            }
        }

        if (cur->v == FKL_VM_NIL) {
            FklVMvalue *k = cur->k;
            fklCgMacroHashMapDel(macros, k);
        } else {
            fklTraverseSerializableValue(vt, cur->k);
        }

        cur = next;
    }
}

static inline void traverse_prod_action(const FklVMvalue *ac,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    if (fklIsVMvalueCustomActCtx(ac)) {
        FklVMvalueCustomActCtx *ctx = fklVMvalueCustomActCtx(ac);

        uint64_t len = ctx->actual_len;
        for (size_t i = 0; i < len; ++i) {
            fklTraverseSerializableValue(vt, ctx->dollars[i]);
        }

        fklValueVectorPushBack2(pending, ctx->proc);
    } else if (fklIsVMvalueSimpleActCtx(ac)) {
        FklVMvalueSimpleActCtx *ctx = fklVMvalueSimpleActCtx(ac);
        fklTraverseSerializableValue(vt, ctx->vec);
    } else {
        // is replace or builtin prod
        fklTraverseSerializableValue(vt, ac);
    }
}

static inline void traverse_rmacro_prod(const FklVMvalueCgRmacroProd *prod,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    fklTraverseSerializableValue(vt, prod->left);
    fklTraverseSerializableValue(vt, prod->action_type);
    traverse_prod_action(prod->action, pending, extra_args);

    for (size_t i = 0; i < prod->len; ++i) {
        const FklCgRmacroGraSym *sym = &prod->syms[i];
        fklTraverseSerializableValue(vt, sym->v);
    }
}

static inline void traverse_rmacro(const FklVMvalueCgRmacro *g,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    for (uint64_t i = 0; i < g->len; ++i) {
        const FklCgRmacroCmd *cmd = &g->cmds[i];
        FklVMvalue *v = cmd->args;
        switch (cmd->op) {
        case FKL_CG_RMACRO_NONE:
            FKL_UNREACHABLE();
            break;
        case FKL_CG_RMACRO_ADD_PROD:
        case FKL_CG_RMACRO_ADD_IGNORE: {
            FklVMvalueCgRmacroProd *p = fklVMvalueCgRmacroProd(v);
            traverse_rmacro_prod(p, pending, extra_args);
        } break;

        case FKL_CG_RMACRO_ADD_DELIM:
            fklTraverseSerializableValue(vt, v);
            break;
        }
    }
}

static inline void traverse_rmacros(FklVMvalueCgRmacroHashMap *rmacros,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {
    if (rmacros == NULL)
        return;

    FklValueTable *vt = extra_args->value_table;
    const FklValueTable *external_macros = extra_args->external_macros;
    for (const FklValueHashMapNode *cur = rmacros->ht.first; cur;) {
        const FklValueHashMapNode *next = cur->next;
        if (fklValueTableGet(external_macros, cur->v) != 0) {
            FklVMvalue *k = cur->k;
            fklCgRmacroHashMapDel(rmacros, k);
        } else {
            fklTraverseSerializableValue(vt, cur->k);
            FklVMvalueCgRmacro *g = fklVMvalueCgRmacro(cur->v);
            traverse_rmacro(g, pending, extra_args);
        }
        cur = next;
    }
}

static inline void traverse_bc_lnt(const FklByteCodelnt *bcl,
        FklValueTable *vt) {
    const FklLntItem *items = bcl->l;
    uint32_t count = bcl->ls;
    for (uint32_t i = 0; i < count; i++) {
        fklTraverseSerializableValue(vt, items[i].fid);
    }
}

static inline void traverse_prototype(const FklVMvalueProto *pt,
        FklValueVector *pending,
        const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;
    FklProtoTable *proto_table = extra_args->proto_table;

    FklValueId id = fklProtoTableGet(proto_table, pt);
    if (id != 0)
        return;

    fklProtoTableAdd(proto_table, pt);

    const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(pt);
    for (uint32_t i = 0; i < pt->ref_count; i++) {
        traverse_symbol_def(&refs[i], vt);
    }

    fklTraverseSerializableValue(vt, pt->name);
    fklTraverseSerializableValue(vt, pt->file);
    FklVMvalue *const *konsts = fklVMvalueProtoConsts(pt);
    for (uint32_t i = 0; i < pt->konsts_count; ++i) {
        fklTraverseSerializableValue(vt, konsts[i]);
    }

    FklVMvalueProto *const *child_proc_proto = fklVMvalueProtoChildren(pt);
    for (uint32_t i = 0; i < pt->child_proto_count; ++i) {
        fklValueVectorPushBack2(pending, FKL_VM_VAL(child_proc_proto[i]));
    }

    FklVMvalueLib *const *libs = fklVMvalueProtoUsedLibs(pt);
    for (uint32_t i = 0; i < pt->used_libraries_count; ++i) {
        FklVMvalueLib *const l = libs[i];
        fklValueVectorPushBack2(pending, FKL_VM_VAL(l));
    }
}

static inline void traverse_vm_lib(const FklVMvalueLib *l,
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
        fklValueVectorPushBack2(pending, FKL_VM_VAL(proc));
    } else if (FKL_IS_SYM(proc_v)) {
        if (!is_writting_pre_compile)
            fklValueTableAdd(vt, proc_v);
    } else if (!fklIsVMvalueDll(proc_v)) {
        FKL_UNREACHABLE();
    }
}

static inline void traverse_export_sid_idx_table(
        const FklCgExportSidIdxHashMap *t,
        FklValueTable *vt) {
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        fklTraverseSerializableValue(vt, sid_idx->k);
    }
}

static inline void traverse_replacements(FklVMvalueCgRplHashMap *rpls,
        const WriteLibExtraArgs *extra_args) {
    if (rpls == NULL)
        return;

    FklValueTable *vt = extra_args->value_table;
    const FklValueTable *external_macros = extra_args->external_macros;
    for (const FklValueHashMapNode *rep_list = rpls->ht.first; rep_list;) {
        const FklValueHashMapNode *next = rep_list->next;
        if (fklValueTableGet(external_macros, rep_list->v) != 0) {
            FklVMvalue *k = rep_list->k;
            fklCgRplHashMapDel(rpls, k);
        } else {
            FklVMvalueCgRpl *rpl = fklVMvalueCgRpl(rep_list->v);
            fklTraverseSerializableValue(vt, rep_list->k);
            fklTraverseSerializableValue(vt, rpl->value);
        }

        rep_list = next;
    }
}

static int traverse_writing_cb(FklVM *vm,
        FklVMvalue *v,
        FklValueVector *const pending,
        void *param) {
    const WriteLibExtraArgs *args = (const WriteLibExtraArgs *)param;

    FklValueTable *vt = args->value_table;

    if (fklIsVMvalueCgInfo(v)) {
        const FklVMvalueCgInfo *info = fklVMvalueCgInfo(v);
        fklValueTableAdd(vt, info->fid);

        traverse_export_sid_idx_table(&info->exports, vt);
        traverse_compiler_macros(info->export_macros, pending, args);
        traverse_replacements(info->export_replacement, args);
        traverse_rmacros(info->export_rmacros, pending, args);
    } else if (FKL_IS_PROC(v)) {
        FklVMvalueProc *proc = FKL_VM_PROC(v);
        if (args->proc_table != NULL) {
            FklValueId id = fklProcTableGet(args->proc_table, proc);
            if (id != 0)
                return 0;
            fklProcTableAdd(args->proc_table, proc);
        }

        fklValueVectorPushBack2(pending, FKL_VM_VAL(proc->proto));
        traverse_bc_lnt(FKL_VM_CO(proc->bcl), vt);
    } else if (fklIsVMvalueProto(v)) {
        traverse_prototype(fklVMvalueProto(v), pending, args);
    } else if (fklIsVMvalueLib(v)) {
        traverse_vm_lib(fklVMvalueLib(v), pending, args);
    } else {
        FKL_UNREACHABLE();
    }

    return 0;
}

static int traverse_writing_obj(FklVM *vm,
        const FklVMvalue *v,
        const WriteLibExtraArgs *args) {
    FKL_ASSERT(args->value_table != NULL);
    FKL_ASSERT(args->proto_table != NULL);
    FKL_ASSERT(args->lib_table != NULL);

    return traverse_obj(vm, traverse_writing_cb, FKL_VM_VAL(v), (void *)args);
}

void fklWriteCodeFile(FklVM *vm,
        FILE *fp,
        const FklVMvalueProc *const main_func) {
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

    traverse_writing_obj(vm, FKL_VM_VAL(main_func), &extra_args);

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, &lib_table, fp);

    write_lib_table(&lib_table, &value_table, &proto_table, &extra_args, fp);

    write_proc(main_func, &extra_args, fp);

    fklUninitLibTable(&lib_table);
    fklUninitProtoTable(&proto_table);
    fklUninitValueTable(&value_table);
}

static int fixup_proto_lib_refs(const FklLoadProtoArgs *protos,
        const FklLoadLibArgs *args) {
    FklPair const *cur = protos->protos;
    FklPair const *const end = cur + protos->count;
    for (; cur < end; ++cur) {
        FklVMvalueProto *c = fklVMvalueProto(cur->car);
        LibIdx count = c->used_libraries_count;
        FklVMvalue **libs = &c->vals[c->used_libraries_offset];
        for (LibIdx i = 0; i < count; ++i) {
            LibIdx idx = as_lib_placeholder(libs[i])->idx;
            libs[i] = FKL_VM_VAL(get_lib_with_id(args, idx));
        }
    }
    return 0;
}

static int fixup_re_export_cmds_lib_refs(ReExportCmds *cmds,
        const FklLoadLibArgs *args) {
    MacroCount len = cmds->count;
    LibIdx idx = 0;
    for (MacroCount i = 0; i < len; ++i) {
        FklReExportCmd *cmd = &cmds->cmds[i];
        switch (cmd->op) {
        case FKL_RE_EXPORT_OP_PUSH_CTX:
        case FKL_RE_EXPORT_OP_POP_CTX:
        case FKL_RE_EXPORT_OP_POP:
        case FKL_RE_EXPORT_OP_PUSH_LIB:
            break;
        case FKL_RE_EXPORT_OP_IMPORT:
            if (!is_lib_placeholder(cmd->arg0))
                break;
            idx = as_lib_placeholder(cmd->arg0)->idx;
            cmd->arg0 = FKL_VM_VAL(get_lib_with_id(args, idx));
            break;
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

static inline void write_prod_action(const FklVMvalue *ac,
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
        for (size_t i = 0; i < len; ++i) {
            write_value_id(vt, 0, ctx->dollars[i], fp);
        }

        write_proc(FKL_VM_PROC(ctx->proc), extra_args, fp);
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

static inline void write_rmacro_prod(const FklVMvalueCgRmacroProd *prod,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    FklValueTable *vt = extra_args->value_table;
    write_value_id(vt, 0, prod->left, fp);
    write_value_id(vt, 0, prod->action_type, fp);
    write_prod_action(prod->action, extra_args, fp);

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

static inline void write_rmacro_cmd(const FklCgRmacroCmd *cmd,
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
        write_rmacro_prod(fklVMvalueCgRmacroProd(v), extra_args, fp);
        break;
    case FKL_CG_RMACRO_ADD_DELIM:
        write_value_id(vt, 0, v, fp);
        break;
    }
}

static inline void write_rmacro(const FklVMvalueCgRmacro *g,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    MacroCount count = g->len;
    fwrite(&count, sizeof(count), 1, fp);
    for (size_t i = 0; i < count; ++i) {
        const FklCgRmacroCmd *cmd = &g->cmds[i];
        write_rmacro_cmd(cmd, extra_args, fp);
    }
}

static inline void write_rmacros(const FklVMvalueCgRmacroHashMap *rmacros,
        const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    MacroCount count = 0;
    if (rmacros == NULL) {
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }

    FklValueTable *vt = extra_args->value_table;
    count = rmacros->ht.count;
    fwrite(&count, sizeof(count), 1, fp);
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        write_value_id(vt, 0, cur->k, fp);
        FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(cur->v);
        write_rmacro(rmacro, extra_args, fp);
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

        for (size_t i = 0; i < len; ++i) {
            c->dollars[i] = load_value_id(fp, values);
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
    FklVM *vm = ctx->vm;
    FklVMvalueCgRmacroHashMap *ht = fklCreateVMvalueCgRmacroHashMap(vm);
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
    FklValueId count = 0;
    fread(&count, sizeof(count), 1, fp);
    uint32_t num = count;
    for (uint32_t i = 0; i < num; i++) {
        FklCgExportIdx idxs = { 0 };
        FklVMvalue *sid = load_value_id(fp, values);
        fread(&idxs.idx, sizeof(idxs.idx), 1, fp);
        fread(&idxs.flags, sizeof(idxs.flags), 1, fp);
        fklCgExportSidIdxHashMapPut(t, &sid, &idxs);
    }
}

static void load_re_export_cmd(FklReExportCmd *cmd,
        FILE *fp,
        const FklLoadValueArgs *values,
        FklValueVector *ph_vec) {
    uint8_t op = 0;
    fread(&op, sizeof(op), 1, fp);
    cmd->op = op;
    LibPlaceholder *ph = NULL;
    LibIdx id = 0;
    switch ((FklReExportOp)op) {
    case FKL_RE_EXPORT_OP_PUSH_CTX:
    case FKL_RE_EXPORT_OP_POP_CTX:
    case FKL_RE_EXPORT_OP_POP:
        break;

    case FKL_RE_EXPORT_OP_PUSH_LIB:
        cmd->arg0 = load_value_id(fp, values);
        break;

    case FKL_RE_EXPORT_OP_IMPORT: {
        uint8_t type = cmd->type;
        fread(&type, sizeof(type), 1, fp);

        cmd->type = type;

        uint8_t value_type = 0;
        fread(&value_type, sizeof(value_type), 1, fp);

        switch ((ReExportImportArg0Type)value_type) {
        case IMPORT_ARG0_FIX:
            cmd->arg0 = load_value_id(fp, values);
            break;

        case IMPORT_ARG0_LIB:
            id = read_lib_id(fp);
            ph = create_lib_placeholder1(values->vm, id, ph_vec);
            cmd->arg0 = FKL_VM_VAL(ph);
            break;
        }

        cmd->arg1 = load_value_id(fp, values);

        break;
    }

    default:
        FKL_UNREACHABLE();
        break;
    }
}

static ReExportCmds *load_re_export_cmds(FILE *fp,
        const FklLoadValueArgs *values,
        FklValueVector *ph_vec) {
    MacroCount len = 0;
    fread(&len, sizeof(len), 1, fp);
    ReExportCmds *cmds = fklCreateVMvalueReExportCmds(values->vm, len);
    for (MacroCount i = 0; i < len; ++i) {
        FklReExportCmd *cmd = &cmds->cmds[i];
        load_re_export_cmd(cmd, fp, values, ph_vec);
    }

    return cmds;
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
    FklVM *vm = ctx->vm;
    FklVMvalueCgMacroHashMap *macro_table = fklCreateVMvalueCgMacroHashMap(vm);
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
    FklVM *vm = ctx->vm;
    FklVMvalueCgRplHashMap *ht = fklCreateVMvalueCgRplHashMap(vm);
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

static void load_relocations(FILE *fp,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos,
        FklLoadPreCompileArgs *const args,
        FklValueVector *ph_vec) {
    MacroCount len = 0;
    fread(&len, sizeof(len), 1, fp);
    FklPreCompileFixup *fixup = args->fixup;
    FklRelocVector *reloc_vec = fixup == NULL ? NULL : &fixup->relocations;

    LibPlaceholder *ph = NULL;
    for (MacroCount i = 0; i < len; ++i) {
        FklReloc reloc = { 0 };

        FklValueId lib_id = read_lib_id(fp);
        FklPair *p = load_proto_id(fp, protos);
        FklVMvalue *s = load_value_id(fp, values);
        fread(&reloc.ins, sizeof(reloc.ins), 1, fp);
        fread(&reloc.used, sizeof(reloc.used), 1, fp);

        FKL_ASSERT(FKL_IS_SYM(s));
        FKL_ASSERT(p != NULL);
        FKL_ASSERT(p->cdr != NULL);

        ph = create_lib_placeholder1(values->vm, lib_id, ph_vec);

        reloc.lib = FKL_VM_VAL(ph);
        reloc.proc = FKL_VM_PROC(p->cdr);
        reloc.sym = s;

        if (reloc_vec == NULL)
            continue;

        fklRelocVectorPushBack(reloc_vec, &reloc);
    }
}

static inline void load_pre_compile(FILE *fp,
        const FklLoadValueArgs *const values,
        const FklLoadProtoArgs *const protos,
        FklCgLib *cg_lib,
        FklLoadPreCompileArgs *const args,
        FklValueVector *ph_vec) {
    FklVMvalue *name = load_value_id(fp, values);
    load_export_sid_idx_table(fp, values, &cg_lib->exports);
    cg_lib->re_exports = FKL_VM_VAL(load_re_export_cmds(fp, values, ph_vec));
    cg_lib->macros = load_compiler_macros(args->ctx, fp, values, protos);
    cg_lib->replacements = load_replacements(args->ctx, fp, values);
    cg_lib->rmacros = load_rmacros(fp, args->ctx, values, protos);

    FklVMvalueProc *proc = load_proc(fp, values, protos);
    FklVMvalueVec *names = fklCreateCgNamesVec(values->vm, &cg_lib->exports);
    FklVMvalueLib *lib = fklCreateVMvalueLib(values->vm, name, names);
    lib->proc = FKL_VM_VAL(proc);
    cg_lib->lib = lib;

    load_relocations(fp, values, protos, args, ph_vec);
}

static inline void write_export_sid_idx_table(const FklCgExportSidIdxHashMap *t,
        const FklValueTable *vt,
        FILE *fp) {
    FklValueId count = t->count;
    fwrite(&count, sizeof(count), 1, fp);
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        write_value_id(vt, 0, sid_idx->k, fp);
        fwrite(&sid_idx->v.idx, sizeof(sid_idx->v.idx), 1, fp);
        fwrite(&sid_idx->v.flags, sizeof(sid_idx->v.flags), 1, fp);
        if (sid_idx->v.f.not_owned) {
            fklCodeBuilderLine(g_build,
                    "[DEBUG] variable %s is not owned",
                    FKL_VM_SYM(sid_idx->k)->str);
        } else {
            fklCodeBuilderLine(g_build,
                    "[DEBUG] variable %s is owned",
                    FKL_VM_SYM(sid_idx->k)->str);
        }
    }
}

static inline void write_compiler_macros(const FklVMvalueCgMacroHashMap *macros,
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
            write_proc(FKL_VM_PROC(macro->proc), extra_args, fp);
        }
    }
}

static inline void write_replacements(const FklVMvalueCgRplHashMap *ht,
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

static const char *const import_type_name[] = {
    [FKL_CG_IMPORT_NONE] = "none",
    [FKL_CG_IMPORT_COMMON] = "common",
    [FKL_CG_IMPORT_PREFIX] = "prefix",
    [FKL_CG_IMPORT_ONLY] = "only",
    [FKL_CG_IMPORT_ALIAS] = "alias",
    [FKL_CG_IMPORT_EXCEPT] = "except",
};

static inline void dbg_print_re_export(const FklVMvalueCgReExport *re_export) {
    fklCodeBuilderLine(g_build,
            "[DEBUG] re-export lib: %s, type: %s",
            FKL_VM_SYM(re_export->lib->lib->name)->str,
            import_type_name[re_export->type]);
    switch (re_export->type) {
    case FKL_CG_IMPORT_NONE:
        FKL_UNREACHABLE();
        break;
    case FKL_CG_IMPORT_COMMON:
        break;

    case FKL_CG_IMPORT_PREFIX:
    case FKL_CG_IMPORT_ONLY:
    case FKL_CG_IMPORT_ALIAS:
    case FKL_CG_IMPORT_EXCEPT:
        fklCodeBuilderLineStart(g_build, "        args: ");
        fklPrin1VMvalue2(re_export->args, g_build, NULL);
        fklCodeBuilderLineEnd(g_build, "");
        break;
    }
}

typedef int (*ReExportVisitor)(FklVMvalue *v,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited,
        void *args);

static void traverse_re_export_chain_impl(FklVMvalue *re,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited,
        ReExportVisitor visitor,
        void *args) {
    FKL_ASSERT(fklIsVMvalueCgReExport(re) || fklIsVMvalueReExportCmds(re));

    FklValueVector stack1 = { 0 };
    fklValueVectorInit(&stack1, 8);

    FklValueVector stack2 = { 0 };
    fklValueVectorInit(&stack2, 8);

    fklValueVectorPushBack2(&stack1, FKL_VM_VAL(re));

    const FklLibTable *internal_lib_table = extra_args->internal_lib_table;
    while (!fklValueVectorIsEmpty(&stack1)) {
        FklVMvalue *v = *fklValueVectorPopBackNonNull(&stack1);
        fklValueVectorPushBack2(&stack2, v);

        if (!fklIsVMvalueCgReExport(v))
            continue;

        const FklVMvalueCgReExport *re = fklVMvalueCgReExport(v);

        const FklCgLib *cg_lib = re->lib;
        if (fklLibTableGet(internal_lib_table, cg_lib->lib) == 0)
            continue;
        if (fklLibTableGet(visited, cg_lib->lib) != 0)
            continue;

        fklLibTableAdd(visited, cg_lib->lib);

        fklValueVectorPushBack2(&stack1, FKL_VM_VAL(cg_lib->lib));
        for (FklVMvalue *cur = cg_lib->re_exports; FKL_IS_PAIR(cur);
                cur = FKL_VM_CDR(cur)) {
            FklVMvalue *v = FKL_VM_CAR(cur);
            fklValueVectorPushBack2(&stack1, v);
        }
        fklValueVectorPushBack2(&stack1, FKL_VM_VAL(cg_lib));
    }

    while (!fklValueVectorIsEmpty(&stack2)) {
        FklVMvalue *v = *fklValueVectorPopBackNonNull(&stack2);
        int err = visitor(v, extra_args, visited, args);
        if (err != 0)
            break;
    }

    fklValueVectorUninit(&stack1);
    fklValueVectorUninit(&stack2);
}

static void traverse_re_export_chain(FklVMvalue *re_exports,
        const WriteLibExtraArgs *extra_args,
        ReExportVisitor visitor,
        void *args) {
    FklLibTable visited = { 0 };
    fklInitLibTable(&visited);

    if (re_exports != FKL_VM_NIL && !FKL_IS_PAIR(re_exports)) {
        FKL_ASSERT(fklIsVMvalueReExportCmds(re_exports));
        traverse_re_export_chain_impl(re_exports,
                extra_args,
                &visited,
                visitor,
                args);
    }

    FklVMvalue *cur = re_exports;
    for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
        FklVMvalue *v = FKL_VM_CAR(cur);
        traverse_re_export_chain_impl(v, extra_args, &visited, visitor, args);
    }

    FKL_ASSERT(cur == FKL_VM_NIL);

    fklUninitLibTable(&visited);
}

static int dbg_print_re_export_chain_cb(FklVMvalue *v,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited,
        void *args) {
    if (fklIsVMvalueLib(v)) {
        const FklVMvalueLib *l = fklVMvalueLib(v);
        fklCodeBuilderLine(g_build,
                "[DEBUG] push, make lib: %s",
                FKL_VM_SYM(l->name)->str);
    } else if (fklIsVMvalueCgReExport(v)) {
        const FklVMvalueCgReExport *re = fklVMvalueCgReExport(v);
        dbg_print_re_export(re);
    } else if (fklIsVMvalueReExportCmds(v)) {
        fklCodeBuilderLine(g_build, "[DEBUG] expanding cmds");
    } else if (fklIsVMvalueCgLib(v)) {
        fklCodeBuilderLine(g_build, "[DEBUG] pop");
    } else {
        FKL_UNREACHABLE();
    }

    return 0;
}

static void dbg_print_re_export_chain_cmd_vector(const FklReExportCmd *cmds,
        size_t count) {
    static const char *cmd_op_name[] = {
        [FKL_RE_EXPORT_OP_PUSH_LIB] = "push",
        [FKL_RE_EXPORT_OP_POP] = "pop",
        [FKL_RE_EXPORT_OP_IMPORT] = "import",
        [FKL_RE_EXPORT_OP_PUSH_CTX] = "push-ctx",
        [FKL_RE_EXPORT_OP_POP_CTX] = "pop-ctx",
    };

    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === re-export cmds ===\033[0m\033[31m");
    for (uint64_t i = 0; i < count; ++i) {
        const FklReExportCmd *cmd = &cmds[i];

        fklCodeBuilderLineStart(g_build,
                "[DEBUG] %-4" PRIu64 ": %s, ",
                i,
                cmd_op_name[cmd->op]);
        switch (cmd->op) {
        case FKL_RE_EXPORT_OP_PUSH_LIB:
            fklPrin1VMvalue2(cmd->arg0, g_build, NULL);
            break;

        case FKL_RE_EXPORT_OP_POP:
        case FKL_RE_EXPORT_OP_PUSH_CTX:
        case FKL_RE_EXPORT_OP_POP_CTX:
            break;

        case FKL_RE_EXPORT_OP_IMPORT:
            fklCodeBuilderFmt(g_build, "%s, ", import_type_name[cmd->type]);
            fklPrin1VMvalue2(cmd->arg0, g_build, NULL);
            if (cmd->arg1 != NULL) {
                fklCodeBuilderPuts(g_build, ", ");
                fklPrin1VMvalue2(cmd->arg1, g_build, NULL);
            }
            break;
        }

        fklCodeBuilderLineEnd(g_build, "");
    }

    fklCodeBuilderFmt(g_build, "\033[0m");
}

static void dbg_print_re_export_chain_cmds(FklVMvalue *re_exports,
        const WriteLibExtraArgs *extra_args) {
    traverse_re_export_chain(re_exports,
            extra_args,
            dbg_print_re_export_chain_cb,
            NULL);
}

static void dbg_print_re_export_chain_cmds_impl(const FklCgLib *lib,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited) {
    const FklLibTable *internal_lib_table = extra_args->internal_lib_table;

    if (fklLibTableGet(internal_lib_table, lib->lib) == 0)
        return;
    if (fklLibTableGet(visited, lib->lib) != 0)
        return;

    fklLibTableAdd(visited, lib->lib);

    for (FklVMvalue *cur = lib->re_exports; FKL_IS_PAIR(cur);
            cur = FKL_VM_CDR(cur)) {
        const FklVMvalueCgReExport *re_export =
                fklVMvalueCgReExport(FKL_VM_CAR(cur));

        dbg_print_re_export(re_export);

        fklCodeBuilderIndent(g_build);
        dbg_print_re_export_chain_cmds_impl(re_export->lib,
                extra_args,
                visited);
        fklCodeBuilderUnindent(g_build);
    }
}

static void dbg_print_re_export_chain_tree(FklVMvalue *re_exports,
        const WriteLibExtraArgs *extra_args) {
    FklLibTable visited = { 0 };
    fklInitLibTable(&visited);

    for (FklVMvalue *cur = re_exports; FKL_IS_PAIR(cur);
            cur = FKL_VM_CDR(cur)) {
        const FklVMvalueCgReExport *re_export =
                fklVMvalueCgReExport(FKL_VM_CAR(cur));
        dbg_print_re_export(re_export);

        fklCodeBuilderIndent(g_build);
        dbg_print_re_export_chain_cmds_impl(re_export->lib,
                extra_args,
                &visited);
        fklCodeBuilderUnindent(g_build);
    }

    fklUninitLibTable(&visited);
}

static void dbg_print_re_export_chain_of_main(const FklVMvalueCgInfo *info,
        const WriteLibExtraArgs *extra_args) {
    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === re-export chain of main.fkl ===\033[0m");
    fklCodeBuilderLine(g_build, "\033[36m[DEBUG] === re-export tree ===");

    dbg_print_re_export_chain_tree(info->re_exports, extra_args);

    fklCodeBuilderLine(g_build, "\033[33m[DEBUG] === re-export cmds ===");

    dbg_print_re_export_chain_cmds(info->re_exports, extra_args);

    fklCodeBuilderFmt(g_build, "\033[0m");
}

static void traverse_re_export_cmds(const WriteLibExtraArgs *extra_args) {
    FklValueTable *vt = extra_args->value_table;

    const FklReExportCmdVector *cmd_vec = extra_args->re_exports;
    for (size_t i = 0; i < cmd_vec->size; ++i) {
        const FklReExportCmd *cmd = &cmd_vec->base[i];
        switch (cmd->op) {
        case FKL_RE_EXPORT_OP_POP:
        case FKL_RE_EXPORT_OP_PUSH_CTX:
        case FKL_RE_EXPORT_OP_POP_CTX:
            break;

        case FKL_RE_EXPORT_OP_PUSH_LIB:
            fklTraverseSerializableValue(vt, cmd->arg0);
            break;

        case FKL_RE_EXPORT_OP_IMPORT:
            FKL_ASSERT(FKL_IS_FIX(cmd->arg0) || fklIsVMvalueCgLib(cmd->arg0));
            if (FKL_IS_FIX(cmd->arg0)) {
                fklTraverseSerializableValue(vt, cmd->arg0);
            }
            fklTraverseSerializableValue(vt, cmd->arg1);
            break;
        }
    }

    return;
}

static void dbg_print_all_re_export_chains(const FklCgCtx *ctx,
        const WriteLibExtraArgs *extra_args) {
    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === re-export chain of libraries ===\033[0m\033[31m");
    for (const FklValueHashMapNode *cur = ctx->libraries->ht.first; cur;
            cur = cur->next) {
        const FklCgLib *l = fklVMvalueCgLib(cur->v);
        fklCodeBuilderLine(g_build,
                "\033[32m[DEBUG] re-export chain of lib: %s is \033[41;30m%s\033[0;0m",
                FKL_VM_SYM(l->lib->name)->str,
                l->re_exports == FKL_VM_NIL ? "nil" : "not nil");
        if (l->re_exports == FKL_VM_NIL) {
            continue;
        }

        fklCodeBuilderIndent(g_build);
        fklCodeBuilderLine(g_build, "\033[36m[DEBUG] === re-export tree ===");

        dbg_print_re_export_chain_tree(l->re_exports, extra_args);

        fklCodeBuilderLine(g_build, "\033[33m[DEBUG] === re-export cmds ===");

        dbg_print_re_export_chain_cmds(l->re_exports, extra_args);

        fklCodeBuilderUnindent(g_build);
        fklCodeBuilderFmt(g_build, "\033[0m");
    }
    if (ctx->libraries->ht.first == NULL) {
        fklCodeBuilderLine(g_build, "\033[41;30m[DEBUG] <empty>\033[0m");
    }

    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === re-export chain of macro libraries ===\033[0m\033[31m");
    for (const FklValueHashMapNode *cur = ctx->macro_libraries->ht.first; cur;
            cur = cur->next) {
        const FklCgLib *l = fklVMvalueCgLib(cur->v);
        fklCodeBuilderLine(g_build,
                "\033[32m[DEBUG] re-export chain of lib: %s is \033[41;30m%s\033[0;0m",
                FKL_VM_SYM(l->lib->name)->str,
                l->re_exports == FKL_VM_NIL ? "nil" : "not nil");
        if (l->re_exports == FKL_VM_NIL) {
            continue;
        }

        fklCodeBuilderIndent(g_build);
        fklCodeBuilderLine(g_build, "\033[36m[DEBUG] === re-export tree ===");

        dbg_print_re_export_chain_tree(l->re_exports, extra_args);

        fklCodeBuilderLine(g_build, "\033[33m[DEBUG] === re-export cmds ===");

        dbg_print_re_export_chain_cmds(l->re_exports, extra_args);
        fklCodeBuilderUnindent(g_build);

        fklCodeBuilderFmt(g_build, "\033[0m");
    }
    if (ctx->macro_libraries->ht.first == NULL) {
        fklCodeBuilderLine(g_build, "\033[41;30m[DEBUG] <empty>\033[0m");
    }

    fklCodeBuilderLine(g_build, "");
}

static void write_re_export_cmds(const WriteLibExtraArgs *extra_args,
        FILE *fp) {
    const FklReExportCmdVector *cmd_vec = extra_args->re_exports;
    const FklValueTable *vt = extra_args->value_table;
    const FklLibTable *lib_table = extra_args->lib_table;

    MacroCount len = cmd_vec->size;
    fwrite(&len, sizeof(len), 1, fp);
    FklVMvalueLib *lib = NULL;
    for (MacroCount i = 0; i < len; ++i) {
        const FklReExportCmd *cmd = &cmd_vec->base[i];
        uint8_t op = cmd->op;
        fwrite(&op, sizeof(op), 1, fp);
        switch ((FklReExportOp)op) {
        case FKL_RE_EXPORT_OP_PUSH_CTX:
        case FKL_RE_EXPORT_OP_POP_CTX:
        case FKL_RE_EXPORT_OP_POP:
            break;

        case FKL_RE_EXPORT_OP_PUSH_LIB:
            write_value_id(vt, 0, cmd->arg0, fp);
            break;

        case FKL_RE_EXPORT_OP_IMPORT: {
            uint8_t type = cmd->type;
            fwrite(&type, sizeof(type), 1, fp);
            FKL_ASSERT(FKL_IS_FIX(cmd->arg0) || fklIsVMvalueCgLib(cmd->arg0));

            uint8_t value_type = FKL_IS_FIX(cmd->arg0) //
                                       ? IMPORT_ARG0_FIX
                                       : IMPORT_ARG0_LIB;
            fwrite(&value_type, sizeof(value_type), 1, fp);
            switch ((ReExportImportArg0Type)value_type) {
            case IMPORT_ARG0_FIX:
                write_value_id(vt, 0, cmd->arg0, fp);
                break;

            case IMPORT_ARG0_LIB:
                lib = fklVMvalueCgLib(cmd->arg0)->lib;
                write_lib_id(lib_table, 0, lib, fp);
                break;
            }

            write_value_id(vt, 0, cmd->arg1, fp);
            break;
        }

        default:
            FKL_UNREACHABLE();
            break;
        }
    }

    return;
}

static void write_relocations(const WriteLibExtraArgs *extra_args, FILE *fp) {
    const FklRelocVector *reloc_vec = extra_args->relocations;
    const FklLibTable *libs = extra_args->lib_table;
    const FklValueTable *values = extra_args->value_table;
    const FklProtoTable *protos = extra_args->proto_table;

    MacroCount len = reloc_vec->size;
    fwrite(&len, sizeof(len), 1, fp);

    for (MacroCount i = 0; i < len; ++i) {
        const FklReloc *rel = &reloc_vec->base[i];
        write_lib_id(libs, 0, fklVMvalueLib(rel->lib), fp);
        write_proto_id(protos, 0, rel->proc->proto, fp);
        write_value_id(values, 0, rel->sym, fp);
        fwrite(&rel->ins, sizeof(rel->ins), 1, fp);
        fwrite(&rel->used, sizeof(rel->used), 1, fp);
    }
}

static int traverse_pre_compile(FklVM *vm,
        const FklWritePreCompileArgs *args,
        const WriteLibExtraArgs *extra_args) {
    traverse_re_export_cmds(extra_args);
    int r = traverse_writing_obj(vm, FKL_VM_VAL(args->main_info), extra_args);
    if (r != 0)
        return r;
    return traverse_writing_obj(vm, FKL_VM_VAL(args->main_proc), extra_args);
}

static inline void write_pre_compile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args,
        const WriteLibExtraArgs *extra_args) {
    const FklVMvalueCgInfo *info = args->main_info;
    const FklVMvalueProc *proc = args->main_proc;

    FklValueTable *value_table = extra_args->value_table;

    write_value_id(value_table, 0, info->fid, fp);
    write_export_sid_idx_table(&info->exports, value_table, fp);
    write_re_export_cmds(extra_args, fp);
    write_compiler_macros(info->export_macros, extra_args, fp);
    write_replacements(info->export_replacement, value_table, fp);
    write_rmacros(info->export_rmacros, extra_args, fp);
    write_proc(proc, extra_args, fp);
    write_relocations(extra_args, fp);
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

static inline void append_re_export_cmds(FklReExportCmdVector *cmd_vec,
        FklVMvalueReExportCmds *cmds) {
    for (size_t i = 0; i < cmds->count; ++i) {
        const FklReExportCmd *cmd = &cmds->cmds[i];
        fklReExportCmdVectorPushBack(cmd_vec, cmd);
    }
}

static int collect_re_export_chain_cmds_cb(FklVMvalue *v,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited,
        void *args) {
    const FklLibTable *internal_libs = extra_args->internal_lib_table;
    const FklLibTable *meaningless_libs = extra_args->meaningless_libs;
    FklLibTable *libs = args;

    FklReExportCmdVector *cmd_vec = extra_args->re_exports;
    if (fklIsVMvalueLib(v)) {
        const FklVMvalueLib *l = fklVMvalueLib(v);
        FklValueId id = fklLibTableGet(internal_libs, l);
        FKL_ASSERT(id != 0);
        if (fklLibTableGet(meaningless_libs, l) != 0)
            return 0;

        id = fklLibTableAdd(libs, l);
        FklVMvalue *arg0 = FKL_MAKE_VM_FIX(id);

        FklReExportCmd cmd = {
            .op = FKL_RE_EXPORT_OP_PUSH_LIB,
            .arg0 = arg0,
        };

        fklReExportCmdVectorPushBack(cmd_vec, &cmd);
    } else if (fklIsVMvalueCgReExport(v)) {
        const FklVMvalueCgReExport *re = fklVMvalueCgReExport(v);

        const FklCgLib *cg_lib = re->lib;
        const FklVMvalueLib *l = cg_lib->lib;
        if (fklLibTableGet(meaningless_libs, l) != 0)
            return 0;

        FklValueId id = fklLibTableGet(libs, l);
        FklVMvalue *arg0 = id == 0 ? FKL_VM_VAL(cg_lib) : FKL_MAKE_VM_FIX(id);

        FklReExportCmd cmd = {
            .op = FKL_RE_EXPORT_OP_IMPORT,
            .type = re->type,
            .arg0 = arg0,
            .arg1 = re->args,
        };

        fklReExportCmdVectorPushBack(cmd_vec, &cmd);

    } else if (fklIsVMvalueReExportCmds(v)) {
        FklReExportCmd cmd = { 0 };
        cmd.op = FKL_RE_EXPORT_OP_PUSH_CTX,
        fklReExportCmdVectorPushBack(cmd_vec, &cmd);

        FklVMvalueReExportCmds *re = fklVMvalueReExportCmds(v);
        append_re_export_cmds(cmd_vec, re);

        cmd.op = FKL_RE_EXPORT_OP_POP_CTX;
        fklReExportCmdVectorPushBack(cmd_vec, &cmd);
    } else if (fklIsVMvalueCgLib(v)) {
        const FklVMvalueLib *l = fklVMvalueCgLib(v)->lib;
        FklValueId id = fklLibTableGet(internal_libs, l);
        FKL_ASSERT(id != 0);

        if (fklLibTableGet(meaningless_libs, l) != 0)
            return 0;

        FklReExportCmd cmd = {
            .op = FKL_RE_EXPORT_OP_POP,
        };
        fklReExportCmdVectorPushBack(cmd_vec, &cmd);
    } else {
        FKL_UNREACHABLE();
    }

    return 0;
}

static void collect_re_export_chain_cmds(const FklVMvalueCgInfo *info,
        const WriteLibExtraArgs *extra_args) {
    FklLibTable libs = { 0 };
    fklInitLibTable(&libs);
    traverse_re_export_chain(info->re_exports,
            extra_args,
            collect_re_export_chain_cmds_cb,
            &libs);
    fklUninitLibTable(&libs);
}

static int collect_meaningless_libs_cb(FklVMvalue *v,
        const WriteLibExtraArgs *extra_args,
        FklLibTable *visited,
        void *args) {
    FklLibTable *meaningless_libs = extra_args->meaningless_libs;
    if (fklIsVMvalueCgLib(v)) {
        const FklCgLib *l = fklVMvalueCgLib(v);

        FklValueId id = fklLibTableGet(visited, l->lib);
        if (id == 0)
            return 0;
        FklVMvalue *cur = l->re_exports;
        for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
            FklVMvalue *v = FKL_VM_CAR(cur);
            FklVMvalueCgReExport *re = fklVMvalueCgReExport(v);

            FklVMvalueLib *l = re->lib->lib;
            FklValueId id = fklLibTableGet(meaningless_libs, l);
            if (id == 0)
                return 0;
        }

        FKL_ASSERT(cur == FKL_VM_NIL);
        fklLibTableAdd(meaningless_libs, l->lib);
    }

    return 0;
}

static void dbg_print_all_meaningless_libs(const FklVMvalueCgInfo *info,
        const WriteLibExtraArgs *extra_args) {
    const FklLibTable *meaningless_libs = extra_args->meaningless_libs;
    const FklValueIdHashMapNode *cur = meaningless_libs->vt.ht.first;
    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === meaningless libs ===\033[0m");
    for (; cur; cur = cur->next) {
        FklVMvalueLib *l = fklVMvalueLib(cur->k);
        fklCodeBuilderLine(g_build,
                "[DEBUG] meaningless lib: %s",
                FKL_VM_SYM(l->name)->str);
    }

    fklCodeBuilderFmt(g_build, "\033[0m");
}

static void collect_meaningless_libs(const FklVMvalueCgInfo *info,
        const WriteLibExtraArgs *extra_args) {
    traverse_re_export_chain(info->re_exports,
            extra_args,
            collect_meaningless_libs_cb,
            NULL);
}

typedef struct {
    FklVMvalueProto *proto;
    const FklIns *start;
    const FklIns *end;
} RelocScan;

// FklRelocScanVector
#define FKL_VECTOR_ELM_TYPE RelocScan
#define FKL_VECTOR_ELM_TYPE_NAME RelocScan
#include <fakeLisp/cont/vector.h>

static void collect_import_in_range(const FklVMvalueProc *proc,
        const RelocScan *scan,
        const FklIns *spc,
        FklRelocScanVector *pendings,
        const FklWritePreCompileArgs *const args,
        const WriteLibExtraArgs *const extra_args) {

    FKL_ASSERT(proc != NULL);
    FklRelocVector *reloc_vec = extra_args->relocations;
    FKL_ASSERT(reloc_vec != NULL);

    const FklCgCtx *const ctx = args->ctx;
    FklVMvalueCgEnvWeakMap *weak_map = ctx->proto_env_map;
    FKL_ASSERT(weak_map);

    FklVMvalueProto *const proto = scan->proto;
    FklVMvalueCgEnv *const env = fklVMvalueCgEnvWeakMapGet(weak_map, proto);
    const FklIns *cur = scan->start;
    const FklIns *const end = scan->end;

    FKL_ASSERT(spc <= cur);

    FklVMvalueProto *next_proto = NULL;
    FklVMvalueLib *cur_lib = NULL;
    uint32_t cur_lib_idx = 0;

    RelocScan next = { 0 };

    FklReloc reloc = { 0 };

    const uint8_t *p_used = NULL;

    while (cur < end) {
        FklIns ins = *(cur++);
        switch (OP(ins)) {
        default:
            break;
        case FKL_OP_LOAD_PROTO:
            FKL_ASSERT(next_proto == NULL);
            next_proto = fklVMvalueProtoChildren(proto)[uC(ins)];
            break;
        case FKL_OP_MAKE_PROC:
            memset(&next, 0, sizeof(next));

            next.proto = next_proto;
            next.start = cur;
            next.end = cur + uC(ins);
            fklRelocScanVectorPushBack(pendings, &next);

            next_proto = NULL;
            cur = next.end;
            break;

        case FKL_OP_LOAD_LIB:
            cur_lib_idx = uC(ins);
            cur_lib = fklVMvalueProtoUsedLibs(proto)[cur_lib_idx];
            if (fklLibTableGet(extra_args->internal_lib_table, cur_lib) != 0) {
                cur_lib = NULL;
            }

            break;
        case FKL_OP_IMPORT:
            if (cur_lib == NULL)
                break;

            memset(&reloc, 0, sizeof(reloc));

            p_used = fklGetImportedSymbolUsed(env, cur_lib_idx, uC(ins));
            FKL_ASSERT(p_used != NULL);
            reloc.lib = FKL_VM_VAL(cur_lib);
            reloc.sym = fklVMvalueLibNames(cur_lib)[uC(ins)];
            reloc.proc = proc;
            reloc.ins = cur - spc - 1;
            reloc.used = (*p_used == FKL_IMPORT_SYMBOL_USED);

            FKL_ASSERT(reloc.sym != NULL);
            FKL_ASSERT(FKL_IS_SYM(reloc.sym));

            fklCodeBuilderLine(g_build,
                    "[DEBUG] proc id: %" PRIu32 ", proto id: %" PRIu32
                    " , lib: %s, sym: %s, ins_offset: %" PRIu32
                    " , idx: %" PRIu32 ", used: %d",
                    fklProtoTableGet(extra_args->proto_table, proc->proto),
                    fklProtoTableGet(extra_args->proto_table, proto),
                    FKL_VM_SYM(cur_lib->name)->str,
                    FKL_VM_SYM(reloc.sym)->str,
                    reloc.ins,
                    uC(ins),
                    reloc.used);

            fklRelocVectorPushBack(reloc_vec, &reloc);
            break;
        }
    }
}

static void collect_relocations_impl(const FklVMvalueProc *proc,
        const FklWritePreCompileArgs *const args,
        const WriteLibExtraArgs *const extra_args) {
    FklRelocScanVector pendings = { 0 };
    fklRelocScanVectorInit(&pendings, 8);

    const FklByteCodelnt *bcl = FKL_VM_CO(proc->bcl);
    const FklIns *spc = bcl->bc.code;
    const RelocScan first = {
        .proto = proc->proto,
        .start = spc,
        .end = spc + bcl->bc.len,
    };

    fklRelocScanVectorPushBack(&pendings, &first);
    while (!fklRelocScanVectorIsEmpty(&pendings)) {
        const RelocScan cur = *fklRelocScanVectorPopBackNonNull(&pendings);
        collect_import_in_range(proc, &cur, spc, &pendings, args, extra_args);
    }

    fklRelocScanVectorUninit(&pendings);
}

static void collect_relocations(const FklWritePreCompileArgs *const args,
        const WriteLibExtraArgs *const extra_args) {
    fklCodeBuilderLine(g_build,
            "\033[41;30m[DEBUG] === relocations ===\033[0m\033[31m");

    FklValueIdHashMapNode *cur = fklProcTableFirst(extra_args->proc_table);
    for (; cur != NULL; cur = cur->next) {
        const FklVMvalueProc *v = FKL_VM_PROC(cur->k);
        collect_relocations_impl(v, args, extra_args);
    }

    fklCodeBuilderLine(g_build, "\033[0m");
}

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args) {
    fklInitCodeBuilderFp(&g_gdb_code_builder, stdout, NULL);
    const FklCgCtx *ctx = args->ctx;

    const FklVMvalueCgInfo *info = args->main_info;

    FklValueTable value_table;
    fklInitValueTable(&value_table);

    FklProtoTable proto_table;
    fklInitProtoTable(&proto_table);

    FklLibTable lib_table;
    fklInitLibTable(&lib_table);

    FklLibTable internal_lib_table;
    fklInitLibTable(&internal_lib_table);

    FklValueTable external_macro_table;
    fklInitValueTable(&external_macro_table);

    FklLibTable libs_imported_by_macros;
    fklInitLibTable(&libs_imported_by_macros);

    FklLibTable meaningless_libs;
    fklInitLibTable(&meaningless_libs);

    FklProcTable proc_table;
    fklInitProcTable(&proc_table);

    collect_internal_modules(info, &internal_lib_table);
    collect_external_macros(args->ctx, &external_macro_table);
    collect_libs_imported_by_macros(args->ctx, &libs_imported_by_macros);

    FklReExportCmdVector re_exports = { 0 };
    fklReExportCmdVectorInit(&re_exports, 8);

    FklRelocVector relocations = { 0 };
    fklRelocVectorInit(&relocations, 8);

    WriteLibExtraArgs extra_args = {
        .is_writting_pre_compile = 1,
        .internal_lib_table = &internal_lib_table,
        .imported_by_macros = &libs_imported_by_macros,
        .external_macros = &external_macro_table,

        .value_table = &value_table,
        .proto_table = &proto_table,
        .lib_table = &lib_table,
        .proc_table = &proc_table,
        .re_exports = &re_exports,

        .meaningless_libs = &meaningless_libs,
        .relocations = &relocations,
    };

    fklCodeBuilderLine(g_build, "");

    dbg_print_all_re_export_chains(args->ctx, &extra_args);

    dbg_print_re_export_chain_of_main(info, &extra_args);

    collect_meaningless_libs(info, &extra_args);

    dbg_print_all_meaningless_libs(info, &extra_args);

    collect_re_export_chain_cmds(info, &extra_args);

    traverse_pre_compile(ctx->vm, args, &extra_args);

    collect_relocations(args, &extra_args);

    dbg_print_re_export_chain_cmd_vector(re_exports.base, re_exports.size);
    fklCodeBuilderLine(g_build, "");

    fklWriteValueTable(&value_table, fp);

    write_prototype_table(&proto_table, &value_table, &lib_table, fp);

    write_lib_table(&lib_table, &value_table, &proto_table, &extra_args, fp);

    write_pre_compile(fp, target_dir, args, &extra_args);

    memset(&extra_args, 0, sizeof(extra_args));

    fklReExportCmdVectorUninit(&re_exports);
    fklRelocVectorUninit(&relocations);

    fklUninitValueTable(&value_table);
    fklUninitValueTable(&external_macro_table);

    fklUninitProtoTable(&proto_table);
    fklUninitLibTable(&lib_table);
    fklUninitProcTable(&proc_table);

    fklUninitLibTable(&internal_lib_table);
    fklUninitLibTable(&libs_imported_by_macros);
    fklUninitLibTable(&meaningless_libs);
}

FKL_NODISCARD
static FKL_ALWAYS_INLINE FklVMvalue *has_unimportable_mod(const Fixup *fixup) {
    if (fixup == NULL)
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

FklCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args) {
    fklInitCodeBuilderFp(&g_gdb_code_builder, stdout, NULL);

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

    err = load_lib_table(fp, &values, &protos, &libs);
    FKL_ASSERT(err == 0);

    fixup_proto_lib_refs(&protos, &libs);

    FklVMvalue *rp_s = fklVMaddSymbolCstr(ctx->vm, rp);
    FklCgLib *lib = fklCreateVMvalueCgLib(ctx->vm, rp_s);
    load_pre_compile(fp, &values, &protos, lib, args, &ph_vec);

    ReExportCmds *cmds = fklVMvalueReExportCmds(lib->re_exports);
    fixup_re_export_cmds_lib_refs(cmds, &libs);

    fklValueVectorUninit(&ph_vec);

    if (args->fixup) {
        for (size_t i = 0; i < protos.count; ++i) {
            FklVMvalue *v = FKL_VM_VAL(protos.protos[i].car);
            fklValueVectorPushBack2(&args->fixup->protos, v);
        }
        args->fixup->lib = lib;

        FklRelocVector *reloc_vec = &args->fixup->relocations;
        for (size_t i = 0; i < reloc_vec->size; ++i) {
            FklReloc *reloc = &reloc_vec->base[i];
            FKL_ASSERT(is_lib_placeholder(reloc->lib));

            LibPlaceholder *ph = as_lib_placeholder(reloc->lib);
            LibIdx idx = ph->idx;
            reloc->lib = FKL_VM_VAL(get_lib_with_id(&libs, idx));
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
        lib = NULL;
        args->error_fmt = "failed to import %S for %S";
        args->error_obj = mod_name;
    }

    return lib;
}

void fklPreCompileFixupInit(Fixup *fixup) {
    fklPcDepVectorInit(&fixup->pendings, 8);
    fklValueVectorInit(&fixup->protos, 8);

    fklRelocVectorInit(&fixup->relocations, 8);
}

void fklPreCompileFixupUninit(Fixup *fixup) {
    fklPcDepVectorUninit(&fixup->pendings);
    fklValueVectorUninit(&fixup->protos);

    fklRelocVectorUninit(&fixup->relocations);
}

static inline void fixup_proto_external_libs(FklVMvalueProto *p,
        const Fixup *fixup,
        const FklValueVector *lib_vec) {
    LibIdx count = p->used_libraries_count;
    FklVMvalue **libs = &p->vals[p->used_libraries_offset];
    for (LibIdx j = 0; j < count; ++j) {
        FKL_ASSERT(fklIsVMvalueLib(libs[j]));
        FklVMvalue *idx_v = fklVMvalueLib(libs[j])->proc;
        if (!FKL_IS_FIX(idx_v))
            continue;

        LibIdx idx = FKL_GET_FIX(idx_v);
        FKL_ASSERT(idx < lib_vec->size);
        FklVMvalueCgLib *cg_lib = fklVMvalueCgLib(lib_vec->base[idx]);
        libs[j] = FKL_VM_VAL(cg_lib->lib);
    }
}

static inline void fixup_re_export_cmds_external_libs(ReExportCmds *cmds,
        const Fixup *fixup,
        const FklValueVector *lib_vec) {
    MacroCount len = cmds->count;
    LibIdx idx = 0;
    for (MacroCount i = 0; i < len; ++i) {
        FklReExportCmd *cmd = &cmds->cmds[i];
        switch (cmd->op) {
        case FKL_RE_EXPORT_OP_PUSH_CTX:
        case FKL_RE_EXPORT_OP_POP_CTX:
        case FKL_RE_EXPORT_OP_POP:
        case FKL_RE_EXPORT_OP_PUSH_LIB:
            break;
        case FKL_RE_EXPORT_OP_IMPORT:
            if (!fklIsVMvalueLib(cmd->arg0))
                break;
            idx = FKL_GET_FIX(fklVMvalueLib(cmd->arg0)->proc);
            FklVMvalueCgLib *cg_lib = fklVMvalueCgLib(lib_vec->base[idx]);
            cmd->arg0 = FKL_VM_VAL(cg_lib);
            break;
        }
    }

    dbg_print_re_export_chain_cmd_vector(cmds->cmds, len);
}

static inline void fixup_relocations_external_libs(const Fixup *fixup,
        const FklValueVector *lib_vec) {
    const FklRelocVector *reloc_vec = &fixup->relocations;
    for (size_t i = 0; i < reloc_vec->size; ++i) {
        FklReloc *reloc = &reloc_vec->base[i];

        FKL_ASSERT(fklIsVMvalueLib(reloc->lib));

        FklVMvalue *idx_v = fklVMvalueLib(reloc->lib)->proc;
        FKL_ASSERT(FKL_IS_FIX(idx_v));

        LibIdx idx = FKL_GET_FIX(idx_v);
        FklVMvalueCgLib *cg_lib = fklVMvalueCgLib(lib_vec->base[idx]);

        reloc->lib = FKL_VM_VAL(cg_lib);
    }
}

static inline FklCgLib *create_new_cg_lib1(FklVM *vm,
        FklValueVector *cg_lib_vec,
        uintmax_t start_idx,
        int64_t idx) {
    FKL_ASSERT(idx > 0);
    uintmax_t const target = start_idx + idx;
    fklValueVectorResize2(cg_lib_vec, target, NULL);
    FklVMvalue **pv = &cg_lib_vec->base[target - 1];

    FklCgLib *l = fklCreateVMvalueCgLib(vm, FKL_MAKE_VM_FIX(idx));
    *pv = FKL_VM_VAL(l);
    fklCgExportSidIdxHashMapInit(&l->exports);
    l->macros = fklCreateVMvalueCgMacroHashMap(vm);
    l->replacements = fklCreateVMvalueCgRplHashMap(vm);
    l->rmacros = fklCreateVMvalueCgRmacroHashMap(vm);
    return l;
}

static inline FklCgLib *create_new_cg_lib(FklVM *vm) {
    FklCgLib *l = fklCreateVMvalueCgLib(vm, FKL_MAKE_VM_FIX(0));
    fklCgExportSidIdxHashMapInit(&l->exports);
    l->macros = fklCreateVMvalueCgMacroHashMap(vm);
    l->replacements = fklCreateVMvalueCgRplHashMap(vm);
    l->rmacros = fklCreateVMvalueCgRmacroHashMap(vm);
    return l;
}

static inline FklVMvalue *
get_cg_lib(const FklValueVector *cg_lib_vec, uintmax_t start_idx, int64_t idx) {
    uintmax_t const target = start_idx + idx;
    return cg_lib_vec->base[target - 1];
}

static void filter_not_owned_and_unused_symbol(FklVMvalueCgLib *lib,
        FklVMvalueCgLib *last_lib) {
    const FklCgExportSidIdxHashMapNode *cur = lib->exports.first;
    while (cur) {
        const FklCgExportSidIdxHashMapNode *next = cur->next;

        if (fklCgExportSidIdxHashMapGet2(&last_lib->exports, cur->k) == NULL
                && cur->v.f.not_owned) {
            fklCgExportSidIdxHashMapDel2(&lib->exports, cur->k);
        }

        cur = next;
    }
}

static void filter_new_exports(FklCgExportSidIdxHashMap *const new_exports,
        FklVMvalueCgLib *lib,
        FklVMvalueCgLib *last_lib) {
    const FklCgExportSidIdxHashMapNode *cur = last_lib->exports.first;
    while (cur) {
        const FklCgExportSidIdxHashMapNode *next = cur->next;

        if (fklCgExportSidIdxHashMapGet2(&lib->exports, cur->k) == NULL) {
            FklCgExportIdx *i = fklCgExportAdd(new_exports, cur->k, 0);
            *i = cur->v;
        }

        cur = next;
    }
}

static void dbg_print_export_symbols(const FklCgExportSidIdxHashMap *exports,
        const char *label) {
    const FklCgExportSidIdxHashMapNode *cur = exports->first;
    fklCodeBuilderLine(g_build,
            "\033[43;30m[DEBUG] === re-export %s ===\033[0m\033[33m",
            label);
    while (cur) {
        const FklCgExportSidIdxHashMapNode *next = cur->next;
        const FklVMvalueLib *from_lib = cur->v.from_lib;
        const char *name = FKL_VM_SYM(cur->k)->str;

        fklCodeBuilderLine(g_build,
                "[DEBUG] %s, from %s, idx %" PRIu32 "",
                name,
                from_lib == NULL ? "(nil)" : FKL_VM_SYM(from_lib->name)->str,
                cur->v.from_idx);
        cur = next;
    }
    fklCodeBuilderFmt(g_build, "\033[0m");
}

FKL_NODISCARD
static int execute_re_export_cmds(ReExportCmds *cmds,
        const FklCgCtx *ctx,
        const Fixup *fixup,
        const FklValueVector *lib_vec,
        FklValueVector *const missing_imports,
        FklVMvalue **new_exports_v) {
    FklValueVector stack = { 0 };
    fklValueVectorInit(&stack, lib_vec->capacity);

    FklValueVector cg_lib_vec = { 0 };
    fklValueVectorInit(&cg_lib_vec, lib_vec->capacity);

    FklUintVector idx_stack = { 0 };
    fklUintVectorInit(&idx_stack, 0);

    FklVM *vm = ctx->vm;

    FklCgLib *last_lib = create_new_cg_lib(vm);
    FklCgLib *cur_lib = last_lib;

    uintmax_t start_idx = 0;

    int has_error = 0;
    for (size_t i = 0; i < cmds->count; ++i) {
        const FklReExportCmd *cmd = &cmds->cmds[i];
        switch (cmd->op) {
        case FKL_RE_EXPORT_OP_PUSH_CTX:
            fklUintVectorPushBack2(&idx_stack, start_idx);
            start_idx = cg_lib_vec.size;
            break;
        case FKL_RE_EXPORT_OP_POP_CTX:
            FKL_ASSERT(!fklUintVectorIsEmpty(&idx_stack));
            start_idx = *fklUintVectorPopBackNonNull(&idx_stack);
            break;

        case FKL_RE_EXPORT_OP_POP:
            FKL_ASSERT(!fklValueVectorIsEmpty(&stack));
            cur_lib = fklVMvalueCgLib(*fklValueVectorPopBackNonNull(&stack));
            break;

        case FKL_RE_EXPORT_OP_PUSH_LIB: {
            FklVMvalue *idx_v = cmd->arg0;
            int64_t idx = FKL_GET_FIX(idx_v);
            FKL_ASSERT(idx > 0);
            fklValueVectorPushBack2(&stack, FKL_VM_VAL(cur_lib));
            cur_lib = create_new_cg_lib1(vm, &cg_lib_vec, start_idx, idx);
            break;
        }

        case FKL_RE_EXPORT_OP_IMPORT: {
            FklVMvalue *arg0 = cmd->arg0;
            FklValueVector missings = { 0 };

            fklValueVectorInit(&missings, 0);
            if (FKL_IS_FIX(arg0)) {
                int64_t idx = FKL_GET_FIX(cmd->arg0);
                FKL_ASSERT(idx > 0);
                arg0 = get_cg_lib(&cg_lib_vec, start_idx, idx);
            }

            FKL_ASSERT(fklIsVMvalueCgLib(arg0));

            FklCgImportArgs args = {
                .type = cmd->type,
                .no_replace = 0,
                .is_from_external_lib = !FKL_IS_FIX(cmd->arg0),
                .args = cmd->arg1,

                .replacements = { cur_lib->replacements },
                .macros = { cur_lib->macros },
                .rmacros = { cur_lib->rmacros },

                .missing_syms = &missings,
                .exports = &cur_lib->exports,
            };

            FklVMvalueCgLib *cg_lib = fklVMvalueCgLib(arg0);

            int r = fklCgImport(vm, cg_lib, &args);

            has_error |= (r != 0);
            if (r != 0 && missing_imports != NULL) {
                fklValueVectorPushBack2(missing_imports, arg0);
                FKL_ASSERT(missings.size != 0);
                for (size_t i = 0; i < missings.size; ++i) {
                    fklValueVectorPushBack2(missing_imports, missings.base[i]);
                }
            }

            fklValueVectorUninit(&missings);

            if (r == 0 || (r != 0 && missing_imports != NULL))
                break;

            return r;
        }
        }
    }

    int r = 0;
    if (has_error) {
        r = -1;
        goto exit;
    }

    FKL_ASSERT(cur_lib == last_lib);

    FklVMvalueCgLib *lib = fixup->lib;
    FklCgImportArgs args = {
        .type = FKL_CG_IMPORT_COMMON,
        .no_replace = 1,

        .replacements = { lib->replacements },
        .macros = { lib->macros },
        .rmacros = { lib->rmacros },
    };

    r = fklCgImport(vm, cur_lib, &args);
    if (r == 0) {
        filter_not_owned_and_unused_symbol(lib, last_lib);
        FklCgExportSidIdxHashMap new_exports = { 0 };
        fklCgExportSidIdxHashMapInit(&new_exports);

        filter_new_exports(&new_exports, lib, last_lib);
        if (new_exports.count != 0) {
            FklVMvalue *v = fklCreateVMvalueVec(vm, 1 + new_exports.count * 2);
            FklVMvalueVec *vv = FKL_VM_VEC(v);
            vv->base[0] = FKL_MAKE_VM_FIX(lib->exports.count);

            size_t i = 1;
            FklCgExportSidIdxHashMapNode *cur = new_exports.first;
            while (i < vv->size && cur != NULL) {
                fklCgExportAdd(&lib->exports, cur->k, 1); // not owned
                FKL_ASSERT(cur->v.from_lib != NULL);      // let it crash

                vv->base[i] = FKL_VM_VAL(cur->v.from_lib);
                vv->base[i + 1] = FKL_MAKE_VM_FIX(cur->v.from_idx);
                cur = cur->next;
                i += 2;
            }

            *new_exports_v = v;
        } else {
            *new_exports_v = FKL_VM_NIL;
        }

        dbg_print_export_symbols(&new_exports, "new exports");

        fklCgExportSidIdxHashMapUninit(&new_exports);
    }

exit:
    fklUintVectorUninit(&idx_stack);
    fklValueVectorUninit(&cg_lib_vec);
    fklValueVectorUninit(&stack);
    return r;
}

FKL_NODISCARD
static int apply_relocations(const Fixup *fixup,
        FklValueVector *missing_imports) {
    const FklRelocVector *reloc_vec = &fixup->relocations;

    int r = 0;
    for (size_t i = 0; i < reloc_vec->size; ++i) {
        const FklReloc *reloc = &reloc_vec->base[i];
        const FklVMvalueProc *proc = reloc->proc;
        const FklVMvalueCgLib *cg_lib = fklVMvalueCgLib(reloc->lib);
        const FklVMvalueLib *lib = cg_lib->lib;

        const FklByteCodelnt *bcl = FKL_VM_CO(proc->bcl);

        FklIns ins = bcl->bc.code[reloc->ins];

        uint32_t name_idx = uC(ins);

        FklVMvalue *const *names = fklVMvalueLibNames(lib);

        if (name_idx < lib->count && names[name_idx] == reloc->sym)
            continue;

        const FklCgExportSidIdxHashMap *exports = &cg_lib->exports;
        FklVMvalue *name = reloc->sym;
        const FklCgExportIdx *l = fklCgExportSidIdxHashMapGet2(exports, name);

        if (l != NULL) {
            uint32_t value_idx = l->idx;
            bcl->bc.code[reloc->ins] = set_uC(ins, value_idx);
            continue;
        }

        if (reloc->used) {
            r = -1;
            if (missing_imports) {
                fklValueVectorPushBack2(missing_imports, FKL_VM_VAL(cg_lib));
                fklValueVectorPushBack2(missing_imports, reloc->sym);
                continue;
            }
            break;
        } else {
            // 把 import 给 patch 成 push-nil
            // 后续我们实现运行时替换模块时，保留 relocations
            // 替换后用 relocations 把 import 给 patch 回来
            // 直接覆盖原来的立即数，不应该依赖原 import 指令的立即数
            bcl->bc.code[reloc->ins] = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
            continue;
        }
    }

    return r;
}

static void replace_from_lib(const FklCgExportSidIdxHashMap *t,
        FklVMvalueLib *ol,
        FklVMvalueLib *nl) {
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        if (sid_idx->v.from_lib == ol)
            sid_idx->v.from_lib = nl;
    }
}

static inline const FklIns *scan_export_more(const FklByteCode *bc) {
    const FklIns *cur = &bc->code[bc->len];
    const FklIns *const start = bc->code;
    while (cur > start) {
        --cur;
        if (OP(*cur) == FKL_OP_EXPORT_MORE) {
            return cur;
        }
    }
    return NULL;
}

int fklPreCompileFixup(const Fixup *fixup,
        const FklCgCtx *ctx,
        FklValueVector *const missing_import) {
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
            if (missing_import != NULL) {
                fklValueVectorPushBack2(missing_import, rp);
            }
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

    FklVMvalueCgLib *cg_lib = fixup->lib;
    ReExportCmds *cmds = fklVMvalueReExportCmds(cg_lib->re_exports);
    fixup_re_export_cmds_external_libs(cmds, fixup, &lib_vec);
    fixup_relocations_external_libs(fixup, &lib_vec);

    FklVMvalue *new_exports = FKL_VM_NIL;
    int r = 0;
    r = execute_re_export_cmds(cmds,
            ctx,
            fixup,
            &lib_vec,
            missing_import,
            &new_exports);
    if (r != 0)
        goto exit;

    r = apply_relocations(fixup, missing_import);
    if (r != 0)
        goto exit;

    if (new_exports == FKL_VM_NIL)
        goto exit;

    FklVM *vm = ctx->vm;
    FklVMvalueLib *old_lib = cg_lib->lib;
    FklVMvalueProc *proc = FKL_VM_PROC(old_lib->proc);
    FklVMvalueProto *pt = proc->proto;

    const FklIns *i = scan_export_more(&FKL_VM_CO(proc->bcl)->bc);
    if (i == NULL)
        goto exit;

    FKL_ASSERT(pt->konsts_count > 0);

    FklVMvalueVec *names = fklCreateCgNamesVec(vm, &cg_lib->exports);
    FklVMvalueLib *new_lib = fklCreateVMvalueLib(vm, old_lib->name, names);
    replace_from_lib(&cg_lib->exports, old_lib, new_lib);

    new_lib->proc = FKL_VM_VAL(proc);
    cg_lib->lib = new_lib;

    FklVMvalue **const values = (FklVMvalue **)fklVMvalueProtoConsts(pt);
    FKL_ASSERT(pt->konsts_count);

    values[uC(*i)] = new_exports;

exit:
    fklValueVectorUninit(&lib_vec);

    return r;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fixup_print, "fixup");

static void fixup_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(ud);
    Fixup *fixup = &f->f;

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

    FklRelocVector *reloc_vec = &fixup->relocations;
    for (size_t i = 0; i < reloc_vec->size; ++i) {
        const FklReloc *reloc = &reloc_vec->base[i];
        fklVMgcToGray(reloc->lib, gc);
        fklVMgcToGray(FKL_VM_VAL(reloc->proc), gc);
        fklVMgcToGray(reloc->sym, gc);
    }

    fklVMgcToGray(FKL_VM_VAL(fixup->lib), gc);
}

static FklVMudFinalizeResult fixup_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(ud);
    fklPreCompileFixupUninit(&f->f);
    return FKL_VM_UD_FINALIZE_NOW;
}

alignas(8) static const FklVMvalueType FixupType = FKL_VM_TYPE_STATIC_INIT(
        FixupType,
        {
            .name = "fixup",
            .size = sizeof(FklVMvaluePcFixup),
            .princ = fixup_print,
            .prin1 = fixup_print,
            .atomic = fixup_atomic,
            .finalize = fixup_finalize,
        });

FklVMvaluePcFixup *fklCreateVMvaluePcFixup(FklVM *vm) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FixupType);
    FklVMvaluePcFixup *f = fklVMvaluePcFixup(v);
    fklPreCompileFixupInit(&f->f);
    return f;
}

int fklIsVMvaluePcFixup(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &FixupType.mt;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(re_export_print, "re-export");

static void re_export_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueReExportCmds *re = fklVMvalueReExportCmds(ud);

    for (uint64_t i = 0; i < re->count; ++i) {
        const FklReExportCmd *cmd = &re->cmds[i];
        fklVMgcToGray(cmd->arg0, gc);
        fklVMgcToGray(cmd->arg1, gc);
    }
}

alignas(8) static const FklVMvalueType ReExportType = FKL_VM_TYPE_STATIC_INIT(
        ReExportType,
        {
            .name = "re-export",
            .size = sizeof(FklVMvalueReExportCmds),
            .princ = re_export_print,
            .prin1 = re_export_print,
            .atomic = re_export_atomic,
        });

int fklIsVMvalueReExportCmds(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &ReExportType.mt;
}

FklVMvalueReExportCmds *fklCreateVMvalueReExportCmds(FklVM *vm,
        uint64_t count) {
    size_t extra_size = count * sizeof(FklReExportCmd);

    FklVMvalue *v = fklCreateVMvalueUd2(vm, &ReExportType, extra_size);
    FklVMvalueReExportCmds *re_export = (FklVMvalueReExportCmds *)v;
    re_export->count = count;
    return re_export;
}
