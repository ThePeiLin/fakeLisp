#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>
#define WRONGARG 1
#define STACKERROR 2
#define TOOMUCHARG 3
#define TOOFEWARG 4
#define CANTCREATETHREAD 5
#define THREADERROR 6
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
VMenv* newVMenv(int32_t,VMenv*);
VMenv* castPreEnvToVMenv(PreEnv*,int32_t,VMenv*,VMheap*);
VMpair* newVMpair(VMheap*);
VMstr* newVMstr(const char*);
VMvalue* castCptrVMvalue(const AST_cptr*,VMheap*);
ByteArry* newByteArry(size_t,uint8_t*);
ByteArry* copyByteArry(const ByteArry*);
ByteArry* newEmptyByteArry();
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
#endif
