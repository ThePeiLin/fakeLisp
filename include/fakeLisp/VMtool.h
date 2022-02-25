#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

extern const char*  builtInErrorType[NUMOFBUILTINERRORTYPE];

#define SET_RETURN(fn,v,stack) do{\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(VMvalue**)realloc((stack)->values,sizeof(VMvalue*)*((stack)->size+64));\
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
	VMvalue* err=fklNewVMvalue(ERR,fklNewVMerror((WHO),builtInErrorType[(ERRORTYPE)],errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define RAISE_BUILTIN_INVALIDSYMBOL_ERROR(WHO,STR,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(RUNNABLE),(EXE));\
	VMvalue* err=fklNewVMvalue(ERR,fklNewVMerror((WHO),"invalid-symbol",errorMessage),(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define VM_NIL ((VMptr)0x1)
#define VM_TRUE (MAKE_VM_IN32(1))
#define VM_EOF ((VMptr)0x7fffffffa)
#define MAKE_VM_IN32(I) ((VMptr)((((uintptr_t)(I))<<UNUSEDBITNUM)|IN32_TAG))
#define MAKE_VM_CHR(C) ((VMptr)((((uintptr_t)(C))<<UNUSEDBITNUM)|CHR_TAG))
#define MAKE_VM_SYM(S) ((VMptr)((((uintptr_t)(S))<<UNUSEDBITNUM)|SYM_TAG))
#define MAKE_VM_PTR(P) ((VMptr)(((uintptr_t)(P))|PTR_TAG))
#define MAKE_VM_REF(P) ((VMptr)(((uintptr_t)(P))|REF_TAG))
#define MAKE_VM_CHF(P,H) (fklNewVMvalue(CHF,P,H))
#define MAKE_VM_MEM(P,H) (fklNewVMvalue(MEM,P,H))
#define GET_TAG(P) ((VMptrTag)(((uintptr_t)(P))&TAG_MASK))
#define GET_PTR(P) ((VMptr)(((uintptr_t)(P))&PTR_MASK))
#define GET_IN32(P) ((int32_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_CHR(P) ((char)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_SYM(P) ((Sid_t)((uintptr_t)(P)>>UNUSEDBITNUM))
//#define fklSET_REF(p,v) do{VMvalue* P=p;VMvalue* V=v;if(IS_CHF(P)){VMMemref* pRef=(VMMemref*)GET_PTR(P);setVMMemref(pRef,V);free(pRef);} else *(VMvalue**)GET_PTR(P)=(V);}while(0)
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
#define IS_IN32(P) (GET_TAG(P)==IN32_TAG)
#define IS_CHR(P) (GET_TAG(P)==CHR_TAG)
#define IS_SYM(P) (GET_TAG(P)==SYM_TAG)
#define IS_REF(P) (GET_TAG(P)==REF_TAG)
#define IS_CHF(P) (GET_TAG(P)==PTR_TAG&&(P)->type==CHF)
#define IS_MEM(P) (GET_TAG(P)==PTR_TAG&&(P)->type==MEM)
#define IS_IN64(P) (GET_TAG(P)==PTR_TAG&&(P)->type==IN64)
#define FREE_CHF(P) (free(GET_PTR(P)))

#define MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|NATIVE_TYPE_TAG))
#define MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|ARRAY_TYPE_TAG))
#define MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|PTR_TYPE_TAG))
#define MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|STRUCT_TYPE_TAG))
#define MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|UNION_TYPE_TAG))
#define MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FUNC_TYPE_TAG))
#define GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&PTR_MASK))
#define GET_TYPES_TAG(P) ((DefTypeTag)(((uintptr_t)(P))&TAG_MASK))

typedef struct Cirular_Ref_List
{
	VMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;

VMvalue* fklGET_VAL(VMvalue* P,VMheap*);
int fklSET_REF(VMvalue* P,VMvalue* V);
void fklPrin1VMvalue(VMvalue*,FILE*,CRL**);
void fklPrincVMvalue(VMvalue*,FILE*,CRL**);
VMenvNode* fklNewVMenvNode(VMvalue*,int32_t);
VMenvNode* fklAddVMenvNode(VMenvNode*,VMenv*);
VMenvNode* fklFindVMenvNode(Sid_t,VMenv*);
void fklFreeVMenvNode(VMenvNode*);
VMproc* fklNewVMproc(uint32_t scp,uint32_t cpc);

VMvalue* fklCopyVMvalue(VMvalue*,VMheap*);
VMvalue* fklNewVMvalue(ValueType,void*,VMheap*);
VMvalue* fklNewTrueValue(VMheap*);
VMvalue* fklNewNilValue();
VMvalue* fklGetTopValue(VMstack*);
VMvalue* fklGetValue(VMstack*,int32_t);
VMvalue* fklGetVMpairCar(VMvalue*);
VMvalue* fklGetVMpairCdr(VMvalue*);
int fklVMvaluecmp(VMvalue*,VMvalue*);
int subfklVMvaluecmp(VMvalue*,VMvalue*);
int fklNumcmp(VMvalue*,VMvalue*);

VMenv* fklNewVMenv(VMenv*);
void fklIncreaseVMenvRefcount(VMenv*);
void fklDecreaseVMenvRefcount(VMenv*);

VMenv* fklCastPreEnvToVMenv(PreEnv*,VMenv*,VMheap*);
VMpair* fklNewVMpair(void);

VMvalue* fklCastCptrVMvalue(FklAstCptr*,VMheap*);
VMByts* fklNewVMByts(size_t,uint8_t*);
void fklIncreaseVMByts(VMByts*);
void fklDecreaseVMByts(VMByts*);

VMByts* fklCopyVMByts(const VMByts*);
VMByts* fklNewEmptyVMByts();
void fklVMBytsCat(VMByts**,const VMByts*);
int fklEqVMByts(const VMByts*,const VMByts*);
VMChanl* fklNewVMChanl(int32_t size);

void fklFreeVMChanl(VMChanl*);
VMChanl* fklCopyVMChanl(VMChanl*,VMheap*);
int32_t fklGetNumVMChanl(VMChanl*);

uint8_t* fklCopyArry(size_t,uint8_t*);
VMproc* fklCopyVMproc(VMproc*,VMheap*);
VMenv* fklCopyVMenv(VMenv*,VMheap*);
VMstack* fklCopyStack(VMstack*);
void fklFreeVMproc(VMproc*);
void fklFreeVMenv(VMenv*);
void fklReleaseSource(pthread_rwlock_t*);
void fklLockSource(pthread_rwlock_t*);
VMvalue* fklPopVMstack(VMstack*);

VMcontinuation* fklNewVMcontinuation(VMstack*,ComStack*,ComStack*);
void fklFreeVMcontinuation(VMcontinuation*);

void fklFreeVMfp(FILE*);

DllHandle* fklNewVMDll(const char*);
void* fklGetAddress(const char*,DllHandle);
void fklFreeVMDll(DllHandle*);

VMDlproc* fklNewVMDlproc(DllFunc,VMvalue*);
void fklFreeVMDlproc(VMDlproc*);

VMFlproc* fklNewVMFlproc(TypeId_t type,void* func);
void fklFreeVMFlproc(VMFlproc*);

VMerror* fklNewVMerror(const char* who,const char* type,const char* message);
VMerror* fklNewVMerrorWithSid(const char* who,Sid_t type,const char* message);
void fklFreeVMerror(VMerror*);

RecvT* fklNewRecvT(FklVM*);
void fklFreeRecvT(RecvT*);

SendT* fklNewSendT(VMvalue*);
void fklFreeSendT(SendT*);

void fklChanlSend(SendT*,VMChanl*);
void fklChanlRecv(RecvT*,VMChanl*);

VMTryBlock* fklNewVMTryBlock(Sid_t,uint32_t tp,long int rtp);
void fklFreeVMTryBlock(VMTryBlock* b);

VMerrorHandler* fklNewVMerrorHandler(Sid_t type,uint32_t scp,uint32_t cpc);
void fklFreeVMerrorHandler(VMerrorHandler*);
int fklRaiseVMerror(VMvalue* err,FklVM*);
VMrunnable* fklNewVMrunnable(VMproc*);
char* fklGenErrorMessage(unsigned int type,VMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(const char* str,VMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);
int fklResBp(VMstack*);

TypeId_t fklGenDefTypes(FklAstCptr*,VMDefTypes* otherTypes,Sid_t* typeName);
TypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,VMDefTypes* otherTypes);
VMDefTypesNode* fklFindVMDefTypesNode(Sid_t typeId,VMDefTypes* otherTypes);
int fklAddDefTypes(VMDefTypes*,Sid_t typeName,TypeId_t);

TypeId_t fklNewVMNativeType(Sid_t,size_t,size_t);
void fklFreeVMNativeType(VMNativeType*);

TypeId_t fklNewVMArrayType(TypeId_t,size_t);
void fklFreeVMArrayType(VMArrayType*);

TypeId_t fklNewVMPtrType(TypeId_t);
void fklFreeVMPtrType(VMPtrType*);

TypeId_t fklNewVMStructType(const char*,uint32_t,Sid_t[],TypeId_t []);
void fklFreeVMStructType(VMStructType*);

TypeId_t fklNewVMUnionType(const char* structName,uint32_t num,Sid_t symbols[],TypeId_t memberTypes[]);
void fklFreeVMUnionType(VMUnionType*);

TypeId_t fklNewVMFuncType(TypeId_t rtype,uint32_t anum,TypeId_t atypes[]);
TypeId_t fklFindSameFuncType(TypeId_t,uint32_t anum,TypeId_t atypes[]);
void fklFreeVMFuncType(VMFuncType*);

size_t fklGetVMTypeSize(VMTypeUnion t);
size_t fklGetVMTypeAlign(VMTypeUnion t);
size_t fklGetVMTypeSizeWithTypeId(TypeId_t t);
VMTypeUnion fklGetVMTypeUnion(TypeId_t);

VMMem* fklNewVMMem(TypeId_t typeId,uint8_t* mem);
TypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,VMDefTypes* otherTypes);
void fklInitNativeDefTypes(VMDefTypes* otherTypes);
void fklWriteTypeList(FILE* fp);
void fklLoadTypeList(FILE* fp);
void fklFreeGlobTypeList(void);

int fklIsNativeTypeId(TypeId_t);
int fklIsArrayTypeId(TypeId_t);
int fklIsPtrTypeId(TypeId_t);
int fklIsStructTypeId(TypeId_t);
int fklIsUnionTypeId(TypeId_t);
int fklIsFunctionTypeId(TypeId_t);
#endif
