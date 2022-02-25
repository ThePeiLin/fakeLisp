#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

#define UNUSEDBITNUM 3
#define PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define TAG_MASK ((intptr_t)0x7)

#define SET_RETURN(fn,v,stack) do{\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(FklVMvalue**)realloc((stack)->values,sizeof(FklVMvalue*)*((stack)->size+64));\
		FAKE_ASSERT((stack)->values,fn,__FILE__,__LINE__);\
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
	FklVMvalue* err=fklNewVMvalue(ERR,fklNewVMerror((WHO),builtInErrorType[(ERRORTYPE)],errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define RAISE_BUILTIN_INVALIDSYMBOL_ERROR(WHO,STR,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNewVMvalue(ERR,fklNewVMerror((WHO),"invalid-symbol",errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define VM_NIL ((FklVMptr)0x1)
#define VM_TRUE (MAKE_VM_I32(1))
#define VM_EOF ((FklVMptr)0x7fffffffa)
#define MAKE_VM_I32(I) ((FklVMptr)((((uintptr_t)(I))<<UNUSEDBITNUM)|FKL_I32_TAG))
#define MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<UNUSEDBITNUM)|CHR_TAG))
#define MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<UNUSEDBITNUM)|SYM_TAG))
#define MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|PTR_TAG))
#define MAKE_VM_REF(P) ((FklVMptr)(((uintptr_t)(P))|REF_TAG))
#define MAKE_VM_CHF(P,H) (fklNewVMvalue(CHF,P,H))
#define MAKE_VM_MEM(P,H) (fklNewVMvalue(MEM,P,H))
#define GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&TAG_MASK))
#define GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&PTR_MASK))
#define GET_I32(P) ((int32_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_CHR(P) ((char)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_SYM(P) ((FklSid_t)((uintptr_t)(P)>>UNUSEDBITNUM))
//#define fklSET_REF(p,v) do{FklVMvalue* P=p;FklVMvalue* V=v;if(IS_CHF(P)){VMMemref* pRef=(VMMemref*)GET_PTR(P);setVMMemref(pRef,V);free(pRef);} else *(FklVMvalue**)GET_PTR(P)=(V);}while(0)
#define IS_PTR(P) (GET_TAG(P)==PTR_TAG)
#define IS_PAIR(P) (GET_TAG(P)==PTR_TAG&&(P)->type==PAIR)
#define IS_DBL(P) (GET_TAG(P)==PTR_TAG&&(P)->type==DBL)
#define IS_STR(P) (GET_TAG(P)==PTR_TAG&&(P)->type==STR)
#define IS_BYTS(P) (GET_TAG(P)==PTR_TAG&&(P)->type==BYTS)
#define IS_CHAN(P) (GET_TAG(P)==PTR_TAG&&(P)->type==CHAN)
#define IS_FP(P) (GET_TAG(P)==PTR_TAG&&(P)->type==FP)
#define IS_DLL(P) (GET_TAG(P)==PTR_TAG&&(P)->type==DLL)
#define IS_PRC(P) (GET_TAG(P)==PTR_TAG&&(P)->type==PRC)
#define IS_DLPROC(P) (GET_TAG(P)==PTR_TAG&&(P)->type==DLPROC)
#define IS_FLPROC(P) (GET_TAG(P)==PTR_TAG&&(P)->type==FLPROC)
#define IS_ERR(P) (GET_TAG(P)==PTR_TAG&&(P)->type==ERR)
#define IS_CONT(P) (GET_TAG(P)==PTR_TAG&&(P)->type==CONT)
#define IS_I32(P) (GET_TAG(P)==FKL_I32_TAG)
#define IS_CHR(P) (GET_TAG(P)==CHR_TAG)
#define IS_SYM(P) (GET_TAG(P)==SYM_TAG)
#define IS_REF(P) (GET_TAG(P)==REF_TAG)
#define IS_CHF(P) (GET_TAG(P)==PTR_TAG&&(P)->type==CHF)
#define IS_MEM(P) (GET_TAG(P)==PTR_TAG&&(P)->type==MEM)
#define IS_I64(P) (GET_TAG(P)==PTR_TAG&&(P)->type==FKL_I64)
#define FREE_CHF(P) (free(GET_PTR(P)))

#define MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|NATIVE_TYPE_TAG))
#define MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|ARRAY_TYPE_TAG))
#define MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|PTR_TYPE_TAG))
#define MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|STRUCT_TYPE_TAG))
#define MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|UNION_TYPE_TAG))
#define MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FUNC_TYPE_TAG))
#define GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&PTR_MASK))
#define GET_TYPES_TAG(P) ((FklDefTypeTag)(((uintptr_t)(P))&TAG_MASK))

typedef struct Cirular_Ref_List
{
	FklVMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;

FklVMvalue* fklGET_VAL(FklVMvalue* P,VMheap*);
int fklSET_REF(FklVMvalue* P,FklVMvalue* V);
void fklPrin1VMvalue(FklVMvalue*,FILE*,CRL**);
void fklPrincVMvalue(FklVMvalue*,FILE*,CRL**);
FklVMenvNode* fklNewVMenvNode(FklVMvalue*,int32_t);
FklVMenvNode* fklAddVMenvNode(FklVMenvNode*,FklVMenv*);
FklVMenvNode* fklFindVMenvNode(FklSid_t,FklVMenv*);
void fklFreeVMenvNode(FklVMenvNode*);
FklVMproc* fklNewVMproc(uint32_t scp,uint32_t cpc);

FklVMvalue* fklCopyVMvalue(FklVMvalue*,VMheap*);
FklVMvalue* fklNewVMvalue(FklValueType,void*,VMheap*);
FklVMvalue* fklNewTrueValue(VMheap*);
FklVMvalue* fklNewNilValue();
FklVMvalue* fklGetTopValue(FklVMstack*);
FklVMvalue* fklGetValue(FklVMstack*,int32_t);
FklVMvalue* fklGetVMpairCar(FklVMvalue*);
FklVMvalue* fklGetVMpairCdr(FklVMvalue*);
int fklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int subfklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int fklNumcmp(FklVMvalue*,FklVMvalue*);

FklVMenv* fklNewVMenv(FklVMenv*);
void fklIncreaseVMenvRefcount(FklVMenv*);
void fklDecreaseVMenvRefcount(FklVMenv*);

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv*,FklVMenv*,VMheap*);
FklVMpair* fklNewVMpair(void);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,VMheap*);
FklVMByts* fklNewVMByts(size_t,uint8_t*);
void fklIncreaseVMByts(FklVMByts*);
void fklDecreaseVMByts(FklVMByts*);

FklVMByts* fklCopyVMByts(const FklVMByts*);
FklVMByts* fklNewEmptyVMByts();
void fklVMBytsCat(FklVMByts**,const FklVMByts*);
int fklEqVMByts(const FklVMByts*,const FklVMByts*);
FklVMChanl* fklNewVMChanl(int32_t size);

void fklFreeVMChanl(FklVMChanl*);
FklVMChanl* fklCopyVMChanl(FklVMChanl*,VMheap*);
int32_t fklGetNumVMChanl(FklVMChanl*);

uint8_t* fklCopyArry(size_t,uint8_t*);
FklVMproc* fklCopyVMproc(FklVMproc*,VMheap*);
FklVMenv* fklCopyVMenv(FklVMenv*,VMheap*);
FklVMstack* fklCopyStack(FklVMstack*);
void fklFreeVMproc(FklVMproc*);
void fklFreeVMenv(FklVMenv*);
void fklReleaseSource(pthread_rwlock_t*);
void fklLockSource(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklComStack*,FklComStack*);
void fklFreeVMcontinuation(VMcontinuation*);

void fklFreeVMfp(FILE*);

DllHandle* fklNewVMDll(const char*);
void* fklGetAddress(const char*,DllHandle);
void fklFreeVMDll(DllHandle*);

FklVMDlproc* fklNewVMDlproc(FklDllFunc,FklVMvalue*);
void fklFreeVMDlproc(FklVMDlproc*);

FklVMFlproc* fklNewVMFlproc(FklTypeId_t type,void* func);
void fklFreeVMFlproc(FklVMFlproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);

FklRecvT* fklNewRecvT(FklVM*);
void fklFreeRecvT(FklRecvT*);

FklSendT* fklNewSendT(FklVMvalue*);
void fklFreeSendT(FklSendT*);

void fklChanlSend(FklSendT*,FklVMChanl*);
void fklChanlRecv(FklRecvT*,FklVMChanl*);

FklVMTryBlock* fklNewVMTryBlock(FklSid_t,uint32_t tp,long int rtp);
void fklFreeVMTryBlock(FklVMTryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*);
char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);
int fklResBp(FklVMstack*);

FklTypeId_t fklGenDefTypes(FklAstCptr*,FklVMDefTypes* otherTypes,FklSid_t* typeName);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklVMDefTypes* otherTypes);
FklVMDefTypesNode* fklFindVMDefTypesNode(FklSid_t typeId,FklVMDefTypes* otherTypes);
int fklAddDefTypes(FklVMDefTypes*,FklSid_t typeName,FklTypeId_t);

FklTypeId_t fklNewVMNativeType(FklSid_t,size_t,size_t);
void fklFreeVMNativeType(FklVMNativeType*);

FklTypeId_t fklNewVMArrayType(FklTypeId_t,size_t);
void fklFreeVMArrayType(FklVMArrayType*);

FklTypeId_t fklNewVMPtrType(FklTypeId_t);
void fklFreeVMPtrType(FklVMPtrType*);

FklTypeId_t fklNewVMStructType(const char*,uint32_t,FklSid_t[],FklTypeId_t []);
void fklFreeVMStructType(FklVMStructType*);

FklTypeId_t fklNewVMUnionType(const char* structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[]);
void fklFreeVMUnionType(FklVMUnionType*);

FklTypeId_t fklNewVMFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[]);
FklTypeId_t fklFindSameFuncType(FklTypeId_t,uint32_t anum,FklTypeId_t atypes[]);
void fklFreeVMFuncType(FklVMFuncType*);

size_t fklGetVMTypeSize(FklVMTypeUnion t);
size_t fklGetVMTypeAlign(FklVMTypeUnion t);
size_t fklGetVMTypeSizeWithTypeId(FklTypeId_t t);
FklVMTypeUnion fklGetVMTypeUnion(FklTypeId_t);

FklVMMem* fklNewVMMem(FklTypeId_t typeId,uint8_t* mem);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklVMDefTypes* otherTypes);
void fklInitNativeDefTypes(FklVMDefTypes* otherTypes);
void fklWriteTypeList(FILE* fp);
void fklLoadTypeList(FILE* fp);
void fklFreeGlobTypeList(void);

int fklIsNativeTypeId(FklTypeId_t);
int fklIsArrayTypeId(FklTypeId_t);
int fklIsPtrTypeId(FklTypeId_t);
int fklIsStructTypeId(FklTypeId_t);
int fklIsUnionTypeId(FklTypeId_t);
int fklIsFunctionTypeId(FklTypeId_t);
#endif
