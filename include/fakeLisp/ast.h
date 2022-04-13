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
	FKL_I8,
	FKL_F64,
	FKL_I64,
	FKL_SYM,
	FKL_STR,
	FKL_BYTS,
	FKL_VECTOR,
	FKL_PROC,
	FKL_CONT,
	FKL_CHAN,
	FKL_FP,
	FKL_DLL,
	FKL_DLPROC,
	FKL_FLPROC,
	FKL_ERR,
	FKL_MEM,
	FKL_CHF,
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
	uint8_t* str;
}FklByteString;


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
		char* str;
		char i8;
		int32_t i32;
		int64_t i64;
		double f64;
		FklByteString byts;
	} value;
}FklAstAtom;

typedef struct FklInterpreter FklInterpreter;
typedef struct FklStringMatchPattern FklStringMatchPattern;
typedef struct FklCompEnv FklCompEnv;
FklAstCptr* fklCreateTree(const char*,FklInterpreter*,FklStringMatchPattern*);
FklAstCptr* fklBaseCreateTree(const char*,FklInterpreter*);

void fklPrintCptr(const FklAstCptr*,FILE*);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(int type,const char*,FklAstPair*);
void fklFreeAtom(FklAstAtom*);

int fklEqByteString(const FklByteString*,const FklByteString*);

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
