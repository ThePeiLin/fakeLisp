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
	char* errorMessage=genErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	VMvalue* err=newVMvalue(ERR,newVMerror((WHO),builtInErrorType[(ERRORTYPE)],errorMessage),(EXE)->heap);\
	free(errorMessage);\
	raiseVMerror(err,(EXE));\
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
#define MAKE_VM_CHF(P) ((VMptr)(((uintptr_t)(P))|CHF_TAG))
#define MAKE_VM_MEM(P) ((VMptr)(((uintptr_t)(P))|MEM_TAG))
#define GET_TAG(P) ((VMptrTag)(((uintptr_t)(P))&TAG_MASK))
#define GET_PTR(P) ((VMptr)(((uintptr_t)(P))&PTR_MASK))
#define GET_IN32(P) ((int32_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_CHR(P) ((char)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_SYM(P) ((Sid_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define SET_REF(p,v) do{VMvalue* P=p;VMvalue* V=v;if(IS_CHF(P)){VMMemref* pRef=(VMMemref*)GET_PTR(P);setVMMemref(pRef,V);free(pRef);} else *(VMvalue**)GET_PTR(P)=(V);}while(0)
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
#define IS_ERR(P) (GET_TAG(P)==PTR_TAG&&(P)->type==ERR)
#define IS_CONT(P) (GET_TAG(P)==PTR_TAG&&(P)->type==CONT)
#define IS_IN32(P) (GET_TAG(P)==IN32_TAG)
#define IS_CHR(P) (GET_TAG(P)==CHR_TAG)
#define IS_SYM(P) (GET_TAG(P)==SYM_TAG)
#define IS_REF(P) (GET_TAG(P)==REF_TAG)
#define IS_CHF(P) (GET_TAG(P)==CHF_TAG)
#define FREE_CHF(P) (free(GET_PTR(P)))

#define MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|NATIVE_TYPE_TAG))
#define MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|ARRAY_TYPE_TAG))
#define MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|PTR_TYPE_TAG))
#define MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|STRUCT_TYPE_TAG))
#define MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|UNION_TYPE_TAG))
#define MAKE_BITS_TYPE(P) ((void*)(((uintptr_t)(P))|BITS_TYPE_TAG))
#define MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FUNC_TYPE_TAG))
#define GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&PTR_MASK))
#define GET_TYPES_TAG(P) ((DefTypeTag)(((uintptr_t)(P))&TAG_MASK))

typedef struct Cirular_Ref_List
{
	VMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;

VMvalue* GET_VAL(VMvalue* P);
void writeVMvalue(VMvalue*,FILE*,CRL**);
void princVMvalue(VMvalue*,FILE*,CRL**);
VMenvNode* newVMenvNode(VMvalue*,int32_t);
VMenvNode* addVMenvNode(VMenvNode*,VMenv*);
VMenvNode* findVMenvNode(Sid_t,VMenv*);
void freeVMenvNode(VMenvNode*);
VMproc* newVMproc(uint32_t scp,uint32_t cpc);

VMvalue* copyVMvalue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue();
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getVMpairCar(VMvalue*);
VMvalue* getVMpairCdr(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);

VMenv* newVMenv(VMenv*);
void increaseVMenvRefcount(VMenv*);
void decreaseVMenvRefcount(VMenv*);

VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*);
VMpair* newVMpair(void);

VMvalue* castCptrVMvalue(AST_cptr*,VMheap*);
VMByts* newVMByts(size_t,uint8_t*);
void increaseVMByts(VMByts*);
void decreaseVMByts(VMByts*);

VMByts* copyVMByts(const VMByts*);
VMByts* newEmptyVMByts();
void VMBytsCat(VMByts**,const VMByts*);
int eqVMByts(const VMByts*,const VMByts*);
VMChanl* newVMChanl(int32_t size);

void freeVMChanl(VMChanl*);
VMChanl* copyVMChanl(VMChanl*,VMheap*);
int32_t getNumVMChanl(VMChanl*);

uint8_t* copyArry(size_t,uint8_t*);
VMproc* copyVMproc(VMproc*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
VMstack* copyStack(VMstack*);
void freeVMproc(VMproc*);
void freeVMenv(VMenv*);
void releaseSource(pthread_rwlock_t*);
void lockSource(pthread_rwlock_t*);
VMvalue* popVMstack(VMstack*);

VMcontinuation* newVMcontinuation(VMstack*,ComStack*,ComStack*);
void freeVMcontinuation(VMcontinuation*);

void freeVMfp(FILE*);

DllHandle* newVMDll(const char*);
void* getAddress(const char*,DllHandle);
void freeVMDll(DllHandle*);

VMDlproc* newVMDlproc(DllFunc,VMvalue*);
void freeVMDlproc(VMDlproc*);

VMerror* newVMerror(const char* who,const char* type,const char* message);
VMerror* newVMerrorWithSid(const char* who,Sid_t type,const char* message);
void freeVMerror(VMerror*);

RecvT* newRecvT(FakeVM*);
void freeRecvT(RecvT*);

SendT* newSendT(VMvalue*);
void freeSendT(SendT*);

void chanlSend(SendT*,VMChanl*);
void chanlRecv(RecvT*,VMChanl*);

VMTryBlock* newVMTryBlock(Sid_t,uint32_t tp,long int rtp);
void freeVMTryBlock(VMTryBlock* b);

VMerrorHandler* newVMerrorHandler(Sid_t type,uint32_t scp,uint32_t cpc);
void freeVMerrorHandler(VMerrorHandler*);
int raiseVMerror(VMvalue* err,FakeVM*);
VMrunnable* newVMrunnable(VMproc*);
char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe);
int32_t getSymbolIdInByteCode(const uint8_t*);
int resBp(VMstack*);
VMvalue* newVMMemref(VMvalue* from,uint8_t* obj,size_t size);
int setVMMemref(VMMemref* pRef,VMvalue* obj);

VMTypeUnion genDefTypes(AST_cptr*,VMDefTypes* otherTypes,Sid_t* typeName);
VMTypeUnion findVMDefTypesNode(Sid_t typeId,VMDefTypes* otherTypes);
int addDefTypes(VMDefTypes*,Sid_t typeName,VMTypeUnion);

VMNativeType* newVMNativeType(Sid_t,size_t);
void freeVMNativeType(VMNativeType*);

VMArrayType* newVMArrayType(VMTypeUnion,size_t);
void freeVMArrayType(VMArrayType*);

VMStructType* newVMStructType(Sid_t,uint32_t,Sid_t[],VMTypeUnion []);
void freeVMStructType(VMStructType*);
size_t getVMTypeSize(VMTypeUnion t);

VMMem* newVMMem(Sid_t typeId,uint8_t* mem);
VMTypeUnion genDefTypesUnion(AST_cptr* objCptr,VMDefTypes* otherTypes);
#endif
