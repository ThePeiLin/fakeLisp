#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

#define SET_RETURN(fn,v,stack) {\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(VMvalue**)realloc((stack)->values,sizeof(VMvalue*)*((stack)->size+64));\
		if((stack)->values==NULL)\
		errors((fn),__FILE__,__LINE__);\
		(stack)->size+=64;\
	}\
	(stack)->values[(stack)->tp]=(v);\
	(stack)->tp+=1;\
}

VMenvNode* newVMenvNode(VMvalue*,int32_t);
VMenvNode* addVMenvNode(VMenvNode*,VMenv*);
VMenvNode* findVMenvNode(int32_t,VMenv*);
void freeVMenvNode(VMenvNode*);
VMcode* newVMcode(const char* code,int32_t size,uint32_t);
void increaseVMcodeRefcount(VMcode*);
void decreaseVMcodeRefcount(VMcode*);

VMvalue* copyVMvalue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*,int);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue(VMheap*);
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getVMpairCar(VMvalue*);
VMvalue* getVMpairCdr(VMvalue*);
void copyRef(VMvalue*,VMvalue*);
void writeRef(VMvalue*,VMvalue*);
void freeRef(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);

VMenv* newVMenv(VMenv*);
void increaseVMenvRefcount(VMenv*);
void decreaseVMenvRefcount(VMenv*);

VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*,SymbolTable*);
VMpair* newVMpair(VMheap*);
void increaseVMpairRefcount(VMpair*);
void decreaseVMpairRefcount(VMpair*);

VMstr* newVMstr(const char*);
void increaseVMstrRefcount(VMstr*);
void decreaseVMstrRefcount(VMstr*);

VMvalue* castCptrVMvalue(AST_cptr*,VMheap*);
ByteString* newByteString(size_t,uint8_t*);
void increaseByteStringRefcount(ByteString*);
void decreaseByteStringRefcount(ByteString*);

ByteString* copyByteArry(const ByteString*);
ByteString* newEmptyByteArry();

Chanl* newChanl(int32_t size);
void increaseChanlRefcount(Chanl*);
void decreaseChanlRefcount(Chanl*);

void freeChanl(Chanl*);
Chanl* copyChanl(Chanl*,VMheap*);
volatile int32_t getNumChanl(volatile Chanl*);

uint8_t* copyArry(size_t,uint8_t*);
uint8_t* createByteString(int32_t);
VMcode* copyVMcode(VMcode*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
VMstack* copyStack(VMstack*);
void freeVMcode(VMcode*);
void freeVMstr(VMstr*);
void freeVMenv(VMenv*);
void releaseSource(pthread_rwlock_t*);
void lockSource(pthread_rwlock_t*);
VMvalue* getArg(VMstack*);

int32_t countCallChain(VMprocess*);

VMcontinuation* newVMcontinuation(VMstack*,VMprocess*);
void increaseVMcontRefcount(VMcontinuation*);
void decreaseVMcontRefcount(VMcontinuation*);
void freeVMcontinuation(VMcontinuation*);

VMfp* newVMfp(FILE*);
void increaseVMfpRefcount(VMfp*);
void decreaseVMfpRefcount(VMfp*);
void freeVMfp(VMfp*);

VMDll* newVMDll(const char*);
void increaseVMDllRefcount(VMDll*);
void decreaseVMDllRefcount(VMDll*);
void* getAddress(const char*,DllHandle);
void freeVMDll(VMDll*);

VMDlproc* newVMDlproc(DllFunc,VMDll*);
void increaseVMDlprocRefcount(VMDlproc*);
void decreaseVMDlprocRefcount(VMDlproc*);
void freeVMDlproc(VMDlproc*);
#endif
