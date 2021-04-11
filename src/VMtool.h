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
VMcode* newVMcode(ByteCode*,int32_t);
VMvalue* copyVMvalue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*,int);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue(VMheap*);
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getCar(VMvalue*);
VMvalue* getCdr(VMvalue*);
void copyRef(VMvalue*,VMvalue*);
void writeRef(VMvalue*,VMvalue*);
void freeRef(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);
VMenv* newVMenv(VMenv*);
void increaseRefcount(VMenv*);
void decreaseRefcount(VMenv*);
VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*,SymbolTable*);
VMpair* newVMpair(VMheap*);
VMstr* newVMstr(const char*);
VMvalue* castCptrVMvalue(const AST_cptr*,VMheap*);
ByteString* newByteString(size_t,uint8_t*);
ByteString* copyByteArry(const ByteString*);
ByteString* newEmptyByteArry();

Chanl* newChanl(int32_t size);
void freeChanl(Chanl*);
Chanl* copyChanl(Chanl*,VMheap*);
void freeMessage(ThreadMessage*);

uint8_t* copyArry(size_t,uint8_t*);
uint8_t* createByteString(int32_t);
VMcode* copyVMcode(VMcode*,VMheap*);
VMcode* copyVMcodeWithoutEnv(VMcode*);
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
void freeVMcontinuation(VMcontinuation*);

VMfp* newVMfp(FILE*);
void freeVMfp(VMfp*);
#endif
