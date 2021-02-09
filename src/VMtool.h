#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

VMenvNode* newVMenvNode(VMvalue*,int32_t);
VMenvNode* addVMenvNode(VMenvNode*,VMenv*);
VMenvNode* findVMenvNode(int32_t,VMenv*);
void freeVMenvNode(VMenvNode*);
VMcode* newVMcode(ByteCode*);
VMvalue* copyValue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*,int);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue(VMheap*);
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getCar(VMvalue*);
VMvalue* getCdr(VMvalue*);
void copyRef(VMvalue*,VMvalue*);
void freeRef(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);
VMenv* newVMenv(VMenv*);
VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*,SymbolTable*);
VMpair* newVMpair(VMheap*);
VMstr* newVMstr(const char*);
VMvalue* castCptrVMvalue(const AST_cptr*,VMheap*);
ByteString* newByteArry(size_t,uint8_t*);
ByteString* copyByteArry(const ByteString*);
ByteString* newEmptyByteArry();
uint8_t* copyArry(size_t,uint8_t*);
uint8_t* createByteArry(int32_t);
VMcode* copyVMcode(VMcode*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
VMstack* copyStack(VMstack*);
void freeVMcode(VMcode*);
void freeVMstr(VMstr*);
void freeVMenv(VMenv*);
void releaseSource(pthread_rwlock_t*);
void lockSource(pthread_rwlock_t*);
VMvalue* getArg(VMstack*);
FILE* getFile(Filestack*,int32_t);

int32_t countCallChain(VMprocess*);
VMcontinuation* newVMcontinuation(VMstack*,VMprocess*);
void freeVMcontinuation(VMcontinuation*);
#endif
