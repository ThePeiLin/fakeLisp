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
#define GET_TAG(P) ((VMptrTag)(((uintptr_t)(P))&TAG_MASK))
#define GET_PTR(P) ((VMptr)(((uintptr_t)(P))&PTR_MASK))
#define GET_IN32(P) ((int32_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_CHR(P) ((char)((uintptr_t)(P)>>UNUSEDBITNUM))
#define GET_SYM(P) ((Sid_t)((uintptr_t)(P)>>UNUSEDBITNUM))
#define SET_REF(p,v) do{VMvalue* P=p;VMvalue* V=v;if(IS_CHF(P)){*((VMChref*)GET_PTR(P))->obj=GET_CHR(V);free(GET_PTR(P));} else *(VMvalue**)GET_PTR(P)=(V);}while(0)
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
#define IS_IN32(P) (GET_TAG(P)==IN32_TAG)
#define IS_CHR(P) (GET_TAG(P)==CHR_TAG)
#define IS_SYM(P) (GET_TAG(P)==SYM_TAG)
#define IS_REF(P) (GET_TAG(P)==REF_TAG)
#define IS_CHF(P) (GET_TAG(P)==CHF_TAG)
#define FREE_CHF(P) (free(GET_PTR(P)))

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
void increaseVMprocRefcount(VMproc*);
void decreaseVMprocRefcount(VMproc*);

VMvalue* copyVMvalue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue();
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getVMpairCar(VMvalue*);
VMvalue* getVMpairCdr(VMvalue*);
//void copyRef(VMvalue*,VMvalue*);
//void writeRef(VMvalue*,VMvalue*);
//void freeRef(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);

VMenv* newVMenv(VMenv*);
void increaseVMenvRefcount(VMenv*);
void decreaseVMenvRefcount(VMenv*);

VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*);
VMpair* newVMpair(VMheap*);
void increaseVMpairRefcount(VMpair*);
void decreaseVMpairRefcount(VMpair*);

//VMstr* newVMstr(const char*);
//void increaseVMstrRefcount(VMstr*);
//void decreaseVMstrRefcount(VMstr*);
//VMstr* copyRefVMstr(char* str);

VMvalue* castCptrVMvalue(AST_cptr*,VMheap*);
VMByts* newVMByts(size_t,uint8_t*);
void increaseVMByts(VMByts*);
void decreaseVMByts(VMByts*);

VMByts* copyVMByts(const VMByts*);
VMByts* newEmptyVMByts();
VMByts* copyRefVMByts(size_t,uint8_t*);
void VMBytsCat(VMByts*,const VMByts*);
int eqVMByts(const VMByts*,const VMByts*);
VMChanl* newVMChanl(int32_t size);
void increaseVMChanlRefcount(VMChanl*);
void decreaseVMChanlRefcount(VMChanl*);

void freeVMChanl(VMChanl*);
VMChanl* copyVMChanl(VMChanl*,VMheap*);
int32_t getNumVMChanl(VMChanl*);

uint8_t* copyArry(size_t,uint8_t*);
VMproc* copyVMproc(VMproc*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
VMstack* copyStack(VMstack*);
void freeVMproc(VMproc*);
//void freeVMstr(VMstr*);
void freeVMenv(VMenv*);
void releaseSource(pthread_rwlock_t*);
void lockSource(pthread_rwlock_t*);
VMvalue* popVMstack(VMstack*);

VMcontinuation* newVMcontinuation(VMstack*,ComStack*);
void increaseVMcontRefcount(VMcontinuation*);
void decreaseVMcontRefcount(VMcontinuation*);
void freeVMcontinuation(VMcontinuation*);

//VMfp* newVMfp(FILE*);
//void increaseVMfpRefcount(VMfp*);
//void decreaseVMfpRefcount(VMfp*);
void freeVMfp(FILE*);

DllHandle* newVMDll(const char*);
//void increaseVMDllRefcount(VMDll*);
//void decreaseVMDllRefcount(VMDll*);
void* getAddress(const char*,DllHandle);
void freeVMDll(DllHandle*);

VMDlproc* newVMDlproc(DllFunc,VMvalue*);
void increaseVMDlprocRefcount(VMDlproc*);
void decreaseVMDlprocRefcount(VMDlproc*);
void freeVMDlproc(VMDlproc*);

VMerror* newVMerror(const char* who,const char* type,const char* message);
VMerror* newVMerrorWithSid(const char* who,Sid_t type,const char* message);
void increaseVMerrorRefcount(VMerror*);
void decreaseVMerrorRefcount(VMerror*);
void freeVMerror(VMerror*);

RecvT* newRecvT(FakeVM*);
void freeRecvT(RecvT*);

SendT* newSendT(VMvalue*);
void freeSendT(SendT*);

void chanlSend(SendT*,VMChanl*);
void chanlRecv(RecvT*,VMChanl*);

VMTryBlock* newVMTryBlock(Sid_t,uint32_t tp,long int rtp);
void freeVMTryBlock(VMTryBlock* b);

VMerrorHandler* newVMerrorHandler(Sid_t type,VMproc* proc);
void freeVMerrorHandler(VMerrorHandler*);
int raiseVMerror(VMvalue* err,FakeVM*);
VMrunnable* newVMrunnable(VMproc*);
char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe);
int32_t getSymbolIdInByteCode(const uint8_t*);
int resBp(VMstack*);
VMvalue* newVMChref(VMvalue* from,char* obj);
#endif
