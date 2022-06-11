#ifndef FKL_FKLC_SYM
#define FKL_FKLC_SYM
#include<fakeLisp/symbol.h>
#ifdef __cplusplus
extern "C"{
#endif

void fklcInitFsym(FklSymbolTable*);
FklSymbolTable* fklcGetOuterSymbolTable(void);
void fklcUninitFsym(void);
FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t);
#ifdef __cplusplus
}
#endif
#endif
