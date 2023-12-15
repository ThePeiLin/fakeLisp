#ifndef FKL_COMMON_H
#define FKL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define FKL_PRT64D "lld"
#define FKL_PRT64U "llu"
#else
#define FKL_PRT64D "ld"
#define FKL_PRT64U "lu"
#endif

#define FKL_DOUBLE_FMT "%.17g"

#ifdef __cplusplus
}
#endif

#endif
