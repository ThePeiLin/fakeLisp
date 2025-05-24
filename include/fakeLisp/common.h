#ifndef FKL_COMMON_H
#define FKL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#ifdef _WIN32
#define FKL_PRT64D "lld"
#define FKL_PRT64U "llu"
#define FKL_PRT64x "llx"
#define FKL_PRT64X "llX"
#else
#define FKL_PRT64D "ld"
#define FKL_PRT64U "lu"
#define FKL_PRT64x "lx"
#define FKL_PRT64X "lX"
#endif

#define FKL_BYTE_WIDTH (8)
#define FKL_I16_WIDTH (FKL_BYTE_WIDTH * 2)
#define FKL_I24_WIDTH (FKL_BYTE_WIDTH * 3)
#define FKL_PATH_MAX (4096)
#define FKL_DOUBLE_FMT "%.17g"
#define FKL_I24_OFFSET (0x800000)
#define FKL_U24_MAX (0xFFFFFF)
#define FKL_I24_MAX (0x7FFFFF)
#define FKL_I24_MIN (-0x800000)

#ifdef _WIN32
#define FKL_PATH_SEPARATOR ('\\')
#define FKL_PATH_SEPARATOR_STR ("\\")
#define FKL_PATH_SEPARATOR_STR_LEN (1)

#define FKL_DLL_FILE_TYPE (".dll")
#define FKL_DLL_FILE_TYPE_STR_LEN (4)

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#else
#define FKL_PATH_SEPARATOR ('/')
#define FKL_PATH_SEPARATOR_STR ("/")
#define FKL_PATH_SEPARATOR_STR_LEN (1)
#define FKL_DLL_FILE_TYPE (".so")
#define FKL_DLL_FILE_TYPE_STR_LEN (3)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FKL_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER_)
#define FKL_DEPRECATED __declspec(deprecated)
#else
#define FKL_DEPRECATED
#endif

FKL_DEPRECATED static inline int fklDeprecatedFunc(void) { return 0; }

#define FKL_PACKAGE_MAIN_FILE ("main.fkl")
#define FKL_PRE_COMPILE_PACKAGE_MAIN_FILE ("main.fklp")

#define FKL_SCRIPT_FILE_EXTENSION (".fkl")
#define FKL_BYTECODE_FILE_EXTENSION (".fklc")
#define FKL_PRE_COMPILE_FILE_EXTENSION (".fklp")

#define FKL_BYTECODE_FKL_SUFFIX_STR ("c")
#define FKL_PRE_COMPILE_FKL_SUFFIX_STR ("p")

#define FKL_BYTECODE_FKL_SUFFIX ('c')
#define FKL_PRE_COMPILE_FKL_SUFFIX ('p')

#define FKL_DEFAULT_INC (32)
#define FKL_MAX_STRING_SIZE (64)
#define FKL_STATIC_SYMBOL_INIT {0, NULL, NULL}
#define FKL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FKL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define FKL_ASSERT(exp) assert(exp)
#define FKL_REMOVE_CONST(T, V) ((T *)(V))
#define FKL_TYPE_CAST(T, V) ((T)(V))

#define FKL_ESCAPE_CHARS ("ABTNVFRS")
#define FKL_ESCAPE_CHARS_TO ("\a\b\t\n\v\f\r\x20")

#ifdef __cplusplus
}
#endif

#endif
