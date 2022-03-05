#ifndef FKL_SRC_UTILS_H
#define FKL_SRC_UTILS_H
#include<stdint.h>
#include<stddef.h>
#include<stdio.h>
#define THRESHOLD_SIZE 64
#define NUM_OF_BUILT_IN_SYMBOL 54
#define MAX_STRING_SIZE 64
#define NUM_OF_BUILT_IN_ERROR_TYPE 28
#define STATIC_SYMBOL_INIT {0,NULL,NULL}
#define UNUSEDBITNUM 3
#define PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define TAG_MASK ((intptr_t)0x7)

#define INCREASE_ALL_SCP(l,ls,s) {int32_t i=0;\
	for(;i<(ls);i++)\
	(l)[i]->scp+=(s);\
}

#define FKL_MIN(a,b) (((a)<(b))?(a):(b))

#define FKL_ASSERT(exp,str,filepath,cl) \
{ \
	if(!(exp)) \
	{\
		fprintf(stderr,"In file \"%s\" line %d\n",(filepath),(cl));\
		perror((str));\
		exit(1);\
	}\
}

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

#define SET_RETURN(fn,v,stack) do{\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(FklVMvalue**)realloc((stack)->values,sizeof(FklVMvalue*)*((stack)->size+64));\
		FKL_ASSERT((stack)->values,fn,__FILE__,__LINE__);\
		if((stack)->values==NULL)\
		{\
			fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
			perror((fn));\
			exit(1);\
		}\
		(stack)->size+=64;\
	}\
	(stack)->values[(stack)->tp]=(v);\
	(stack)->tp+=1;\
}while(0)

#define RAISE_BUILTIN_ERROR(WHO,ERRORTYPE,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNewVMvalue(FKL_ERR,fklNewVMerror((WHO),builtInErrorType[(ERRORTYPE)],errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define RAISE_BUILTIN_INVALIDSYMBOL_ERROR(WHO,STR,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNewVMvalue(FKL_ERR,fklNewVMerror((WHO),"invalid-symbol",errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define VM_NIL ((FklVMptr)0x1)
#define VM_TRUE (MAKE_VM_I32(1))
#define VM_EOF ((FklVMptr)0x7fffffffa)
#define MAKE_VM_I32(I) ((FklVMptr)((((uintptr_t)(I))<<UNUSEDBITNUM)|FKL_I32_TAG))
#define MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<UNUSEDBITNUM)|FKL_CHR_TAG))
#define MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<UNUSEDBITNUM)|FKL_SYM_TAG))
#define MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|FKL_PTR_TAG))
#define MAKE_VM_REF(P) ((FklVMptr)(((uintptr_t)(P))|FKL_REF_TAG))
#define MAKE_VM_CHF(P,H) (fklNewVMvalue(FKL_CHF,P,H))
#define MAKE_VM_MEM(P,H) (fklNewVMvalue(FKL_MEM,P,H))
#define GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&TAG_MASK))
#define GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&PTR_MASK))
#define GET_I32(P) ((int32_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_CHR(P) ((char)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_SYM(P) ((FklSid_t)((uintptr_t)(P)>>UNUSEDBITNUM))
//#define fklSET_REF(p,v) do{FklVMvalue* P=p;FklVMvalue* V=v;if(IS_CHF(P)){VMmemref* pRef=(VMmemref*)GET_PTR(P);setVMmemref(pRef,V);free(pRef);} else *(FklVMvalue**)GET_PTR(P)=(V);}while(0)
#define IS_PTR(P) (GET_TAG(P)==FKL_PTR_TAG)
#define IS_PAIR(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PAIR)
#define IS_DBL(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DBL)
#define IS_STR(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_STR)
#define IS_BYTS(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_BYTS)
#define IS_CHAN(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CHAN)
#define IS_FP(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_FP)
#define IS_DLL(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLL)
#define IS_PRC(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PRC)
#define IS_DLPROC(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLPROC)
#define IS_FLPROC(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_FLPROC)
#define IS_ERR(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_ERR)
#define IS_CONT(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CONT)
#define IS_I32(P) (GET_TAG(P)==FKL_I32_TAG)
#define IS_CHR(P) (GET_TAG(P)==FKL_CHR_TAG)
#define IS_SYM(P) (GET_TAG(P)==FKL_SYM_TAG)
#define IS_REF(P) (GET_TAG(P)==FKL_REF_TAG)
#define IS_CHF(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CHF)
#define IS_MEM(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_MEM)
#define IS_I64(P) (GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_I64)
#define FREE_CHF(P) (free(GET_PTR(P)))

#define MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_NATIVE_TYPE_TAG))
#define MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_ARRAY_TYPE_TAG))
#define MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_PTR_TYPE_TAG))
#define MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_STRUCT_TYPE_TAG))
#define MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_UNION_TYPE_TAG))
#define MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_FUNC_TYPE_TAG))
#define GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&PTR_MASK))
#define GET_TYPES_TAG(P) ((FklDefTypeTag)(((uintptr_t)(P))&TAG_MASK))

int fklIsHexNum(const char*);
int fklIsOctNum(const char*);
int fklIsDouble(const char*);
int fklIsNum(const char*);
char* fklGetStringFromList(const char*);
char* fklGetStringAfterBackslash(const char*);
int fklPower(int,int);
char* fklCastEscapeCharater(const char*,char,size_t*);
void fklPrintRawString(const char*,FILE*);
void fklPrintRawChar(char,FILE*);
char* fklDoubleToString(double);
double fklStringToDouble(const char*);
char* fklIntToString(long);
int64_t fklStringToInt(const char*);
int32_t fklCountChar(const char*,char,int32_t);
int fklStringToChar(const char*);
uint8_t* fklCastStrByteStr(const char*);
uint8_t fklCastCharInt(char);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyStr(const char*);
void* fklCopyMemory(void*,size_t);
int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
void fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
char** fklSplit(char*,char*,int*);
char* fklRelpath(char*,char*);


void mergeSort(void* base
		,size_t num
		,size_t size
		,int (*cmpf)(const void*,const void*));

char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);
#endif
