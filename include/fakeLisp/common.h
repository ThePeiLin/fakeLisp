#ifndef FKL_COMMON_H
#define FKL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
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

#ifdef __cplusplus
}
#endif

#endif
