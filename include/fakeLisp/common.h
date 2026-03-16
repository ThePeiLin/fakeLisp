#ifndef FKL_COMMON_H
#define FKL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <limits.h>
#include <stdint.h>

typedef intptr_t ssize_t;

#define FKL_MASK1(T, S, P) (((((T)1) << (S)) - 1) << (P))
#define FKL_MASK0(T, S, P) (~FKL_MASK1(T, S, P))

static_assert(FKL_MASK1(uint32_t, 24, 4) == 0x0FFFFFF0);
static_assert(FKL_MASK0(uint32_t, 24, 4) == 0xF000000F);

#define FKL_BYTE_WIDTH (CHAR_BIT)
#define FKL_I16_WIDTH (FKL_BYTE_WIDTH * 2)
#define FKL_I24_WIDTH (FKL_BYTE_WIDTH * 3)
#define FKL_PATH_MAX (4096)
#define FKL_DOUBLE_FMT "%.17g"
#define FKL_I24_OFFSET (0x800000)
#define FKL_U24_MAX (0xFFFFFF)
#define FKL_I24_MAX (0x7FFFFF)
#define FKL_I24_MIN (-0x800000)

#define FKL_FIX_INT_MAX (1152921504606846975)
#define FKL_FIX_INT_MIN (-1152921504606846975 - 1)
#define FKL_FIX_INT_OFFSET (0x1000000000000000)

#if (-1 >> 1) == -1
#define FKL_IS_ARITHMETIC_RIGHT_SHIFT (1)
#else
#define FKL_IS_ARITHMETIC_RIGHT_SHIFT (0)
#endif

#ifdef _WIN32
#define FKL_PATH_SEPARATOR ('\\')
#define FKL_PATH_SEPARATOR_STR ("\\")
#define FKL_PATH_UPPER_DIR "..\\"
#define FKL_PATH_SEPARATOR_STR_LEN (1)

#define FKL_DLL_FILE_TYPE (".dll")
#define FKL_DLL_FILE_TYPE_STR_LEN (4)

#else

#define FKL_PATH_SEPARATOR ('/')
#define FKL_PATH_SEPARATOR_STR ("/")
#define FKL_PATH_UPPER_DIR "../"
#define FKL_PATH_SEPARATOR_STR_LEN (1)
#define FKL_DLL_FILE_TYPE (".so")
#define FKL_DLL_FILE_TYPE_STR_LEN (3)
#endif

#if defined(__GNUC__) || defined(__clang__)

#define FKL_FMT_ATTR(A, B) __attribute__((format(printf, A, B)))

#define FKL_DEPRECATED __attribute__((deprecated))
#define FKL_ALWAYS_INLINE inline __attribute__((always_inline))
#define FKL_UNREACHABLE_() __builtin_unreachable()
#define FKL_NODISCARD __attribute__((warn_unused_result))

#elif defined(_MSC_VER)

#define FKL_FMT_ATTR(A, B)
#define FKL_DEPRECATED __declspec(deprecated)
#define FKL_ALWAYS_INLINE __forceinline
#define FKL_UNREACHABLE_() __assume(0)
#define FKL_NODISCARD

#else

#define FKL_FMT_ATTR(A, B)
#define FKL_UNREACHABLE_()
#define FKL_DEPRECATED
#define FKL_ALWAYS_INLINE inline
#define FKL_NODISCARD

#endif

        FKL_DEPRECATED static inline int fklDeprecatedFunc(void) {
    return 0;
}

#define FKL_PACKAGE_MAIN_FILE ("main.fkl")
#define FKL_PRE_COMPILE_PACKAGE_MAIN_FILE ("main.fklp")

#define FKL_SCRIPT_FILE_EXTENSION (".fkl")
#define FKL_BYTECODE_FILE_EXTENSION (".fklc")
#define FKL_PRE_COMPILE_FILE_EXTENSION (".fklp")

#define FKL_BYTECODE_FKL_SUFFIX_STR ("c")
#define FKL_PRE_COMPILE_FKL_SUFFIX_STR ("p")

#define FKL_BYTECODE_FKL_SUFFIX ('c')
#define FKL_PRE_COMPILE_FKL_SUFFIX ('p')

#define FKL_MAX_STRING_SIZE (64)
#define FKL_STATIC_SYMBOL_INIT { 0, NULL, NULL }
#define FKL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FKL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define FKL_ASSERT(exp) assert(exp)

#ifdef NDEBUG
#define FKL_UNREACHABLE()                                                      \
    do {                                                                       \
        FKL_UNREACHABLE_();                                                    \
        abort();                                                               \
    } while (0)
#else
#define FKL_UNREACHABLE()                                                      \
    do {                                                                       \
        fprintf(stderr, "[%s: %d] unreachable!\n", __REL_FILE__, __LINE__);    \
        abort();                                                               \
    } while (0)
#endif

#define FKL_UNTEST()                                                           \
    do {                                                                       \
        fprintf(stderr,                                                        \
                "[%s: %d] %s is untested!\n",                                  \
                __REL_FILE__,                                                  \
                __LINE__,                                                      \
                __func__);                                                     \
    } while (0)

#define FKL_TODO()                                                             \
    do {                                                                       \
        fprintf(stderr,                                                        \
                "[%s: %d] %s is incomplete!\n",                                \
                __REL_FILE__,                                                  \
                __LINE__,                                                      \
                __func__);                                                     \
        abort();                                                               \
    } while (0)

#define FKL_TYPE_CAST(T, V) ((T)(V))

#define FKL_CONTAINER_OF(ptr, type, member)                                    \
    FKL_TYPE_CAST(type *, FKL_TYPE_CAST(char *, ptr) - offsetof(type, member))

#define FKL_ESCAPE_CHARS ("ABTNVFRS")
#define FKL_ESCAPE_CHARS_TO ("\a\b\t\n\v\f\r\x20")

#ifdef __cplusplus
}
#endif

#endif
