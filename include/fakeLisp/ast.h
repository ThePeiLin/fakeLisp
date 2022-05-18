#ifndef FKL_AST_H
#define FKL_AST_H
#include"symbol.h"
#include<stdint.h>
#include"basicADT.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	FKL_NIL=0,
	FKL_I32,
	FKL_CHR,
	FKL_F64,
	FKL_I64,
	FKL_BIG_INT,
	FKL_SYM,
	FKL_STR,
	FKL_VECTOR,
	FKL_PROC,
	FKL_CONT,
	FKL_CHAN,
	FKL_FP,
	FKL_DLL,
	FKL_DLPROC,
	FKL_ERR,
	FKL_ENV,
	FKL_USERDATA,
	FKL_PAIR,
	FKL_ATM,
}FklValueType;

typedef struct
{
	struct FklAstPair* outer;
	uint32_t curline;
	FklValueType type;
	union
	{
		struct FklAstAtom* atom;
		struct FklAstPair* pair;
		void* all;
	}u;
}FklAstCptr;

typedef struct
{
	uint64_t size;
	char* str;
}FklAstString;

typedef struct
{
	size_t size;
	FklAstCptr* base;
}FklAstVector;

typedef struct FklAstPair
{
	struct FklAstPair* prev;
	FklAstCptr car,cdr;
}FklAstPair;

typedef struct FklAstAtom
{
	FklAstPair* prev;
	FklValueType type;
	union
	{
		char* sym;
		char chr;
		int32_t i32;
		int64_t i64;
		double f64;
		FklAstString str;
		FklAstVector vec;
		FklBigInt bigInt;
	} value;
}FklAstAtom;

typedef struct FklInterpreter FklInterpreter;
typedef struct FklStringMatchPattern FklStringMatchPattern;
typedef struct FklCompEnv FklCompEnv;

void fklPrintCptr(const FklAstCptr*,FILE*);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(FklValueType type,const char*,FklAstPair*);
void fklFreeAtom(FklAstAtom*);

void fklMakeAstVector(FklAstVector* vec,size_t size,const FklAstCptr* base);

int fklCptrcmp(const FklAstCptr*,const FklAstCptr*);
int fklDeleteCptr(FklAstCptr*);
int fklCopyCptr(FklAstCptr*,const FklAstCptr*);
void fklReplaceCptr(FklAstCptr*,const FklAstCptr*);
FklAstCptr* fklNextCptr(const FklAstCptr*);
FklAstCptr* fklPrevCptr(const FklAstCptr*);
FklAstCptr* fklGetLastCptr(const FklAstCptr*);
FklAstCptr* fklGetFirstCptr(const FklAstCptr*);
int fklIsListCptr(const FklAstCptr*);
unsigned int fklLengthListCptr(const FklAstCptr*);
FklAstCptr* fklGetASTPairCar(const FklAstCptr*);
FklAstCptr* fklGetASTPairCdr(const FklAstCptr*);
FklAstCptr* fklGetCptrCar(const FklAstCptr*);
FklAstCptr* fklGetCptrCdr(const FklAstCptr*);

FklAstCptr* fklCreateAstWithTokens(FklPtrStack* tokenStack,const char* filename,FklCompEnv*);
int fklAstStrcmp(const FklAstString* fir,const FklAstString* sec);
#ifdef __cplusplus
}
#endif
#endif
