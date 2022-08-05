#ifndef FKL_AST_H
#define FKL_AST_H
#include"symbol.h"
#include<stdint.h>
#include"basicADT.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	FKL_TYPE_NIL=0,
	FKL_TYPE_I32,
	FKL_TYPE_SYM,
	FKL_TYPE_CHR,
	FKL_TYPE_F64,
	FKL_TYPE_I64,
	FKL_TYPE_BIG_INT,
	FKL_TYPE_STR,
	FKL_TYPE_VECTOR,
	FKL_TYPE_PAIR,
	FKL_TYPE_BOX,
	FKL_TYPE_BYTEVECTOR,
	FKL_TYPE_USERDATA,
	FKL_TYPE_PROC,
	FKL_TYPE_CONT,
	FKL_TYPE_CHAN,
	FKL_TYPE_FP,
	FKL_TYPE_DLL,
	FKL_TYPE_DLPROC,
	FKL_TYPE_ERR,
	FKL_TYPE_ENV,
	FKL_TYPE_HASHTABLE,
	FKL_TYPE_ATM,
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
	size_t size;
	FklAstCptr* base;
}FklAstVector;

typedef struct FklAstPair
{
	struct FklAstPair* prev;
	FklAstCptr car,cdr;
}FklAstPair;

typedef struct FklAstHashTable
{
	uint32_t type;
	uint64_t num;
	FklAstCptr items;
}FklAstHashTable;

typedef struct FklAstAtom
{
	FklAstPair* prev;
	FklValueType type;
	union
	{
		char chr;
		int32_t i32;
		int64_t i64;
		double f64;
		FklString* str;
		FklAstVector vec;
		FklBytevector* bvec;
		FklBigInt bigInt;
		FklAstCptr box;
		FklAstHashTable hash;
	}value;
}FklAstAtom;

//typedef struct FklAstValue
//{
//	FklValueType type;
//	uint32_t curline;
//	union
//	{
//		char* sym;
//		char chr;
//		int32_t i32;
//		int64_t i64;
//		double f64;
//		FklAstString str;
//		FklAstVector vec;
//		FklAstPair pair;
//		FklBigInt bigInt;
//		struct FklAstValue* box;
//	}value;
//}FklAstValue;

typedef struct FklInterpreter FklInterpreter;
typedef struct FklStringMatchPattern FklStringMatchPattern;
typedef struct FklCompEnv FklCompEnv;

void fklPrintCptr(const FklAstCptr*,FILE*);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(FklValueType type,FklAstPair*);
void fklFreeAtom(FklAstAtom*);

void fklMakeAstHashTable(FklAstHashTable* hash,uint32_t,uint64_t);
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
#ifdef __cplusplus
}
#endif
#endif
