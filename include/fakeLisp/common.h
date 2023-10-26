#ifndef FKL_COMMON_H
#define FKL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define PRT64D "lld"
#define PRT64U "llu"
#else
#define PRT64D "ld"
#define PRT64U "lu"
#endif

#ifdef __cplusplus
}
#endif

#endif