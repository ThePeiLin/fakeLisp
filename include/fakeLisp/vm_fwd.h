#ifndef FKL_VM_FWD_H
#define FKL_VM_FWD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklVMgc FklVMgc;
typedef struct FklVM FklVM;
typedef struct FklVMvalue FklVMvalue;
typedef struct FklVMvalueLib FklVMvalueLib;

#define FKL_VM_TYPE_X                                                          \
    X(F64 = 0, F64)                                                            \
    X(BIGINT, BIGINT)                                                          \
    X(STR, STR)                                                                \
    X(SYM, SYM)                                                                \
    X(BYTEVECTOR, BYTEVECTOR)                                                  \
    X(PAIR, PAIR)                                                              \
    X(VECTOR, VECTOR)                                                          \
    X(BOX, BOX)                                                                \
    X(HASHTABLE, HASHTABLE)                                                    \
    X(PROC, PROC)                                                              \
    X(CPROC, CPROC)                                                            \
    X(VAR_REF, VAR_REF)                                                        \
    X(USERDATA, USERDATA)

typedef enum {
#define X(A, B) FKL_TYPE_##A,
    FKL_VM_TYPE_X
#undef X
} FklValueType;
#define FKL_VM_VALUE_GC_TYPE_NUM (FKL_TYPE_USERDATA + 1)

typedef enum {
    FKL_MARK_W = 0,
    FKL_MARK_G,
    FKL_MARK_B,
} FklVMvalueMark;

#define FKL_VM_VALUE_COMMON_HEADER                                             \
    alignas(8) struct FklVMvalue *next_;                                       \
    struct FklVMvalue *gray_next_;                                             \
    FklVMvalueMark volatile mark_ : 32;                                        \
    FklValueType type_ : 32

#define FKL_VM_UD_COMMON_HEADER                                                \
    struct FklVMvalue *dll_;                                                   \
    const struct FklVMudMetaTable *mt_

#define FKL_VM_DEF_UD_STRUCT(NAME, ...)                                        \
    typedef struct NAME {                                                      \
        FKL_VM_VALUE_COMMON_HEADER;                                            \
        alignas(struct {                                                       \
            FKL_VM_UD_COMMON_HEADER;                                           \
            void *data[];                                                      \
        }) struct {                                                            \
            FKL_VM_UD_COMMON_HEADER;                                           \
            struct __VA_ARGS__;                                                \
        };                                                                     \
    } NAME

#ifdef __cplusplus
}
#endif

#endif
