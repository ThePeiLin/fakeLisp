#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

int runFakeVM(FakeVM*);
FakeVM* newFakeVM(ByteCode*);
FakeVM* newTmpFakeVM(ByteCode*);
FakeVM* newThreadVM(VMproc*,VMheap*);
FakeVM* newThreadDlprocVM(VMrunnable* r,VMheap* heap);
void initGlobEnv(VMenv*,VMheap*);

VMstack* newVMstack(int32_t);
void freeVMstack(VMstack*);
void stackRecycle(FakeVM*);
int createNewThread(FakeVM*);
FakeVMlist* newThreadStack(int32_t);
VMrunnable* hasSameProc(uint32_t,ComStack*);
int isTheLastExpress(const VMrunnable*,const VMrunnable*,const FakeVM* exe);
VMheap* newVMheap();
void createCallChainWithContinuation(FakeVM*,VMcontinuation*);
void freeVMheap(VMheap*);
void freeAllVMs();
void deleteCallChain(FakeVM*);
void joinAllThread();
void cancelAllThread();
void GC_mark(FakeVM*);
void GC_markValue(VMvalue*);
void GC_markValueInStack(VMstack*);
void GC_markValueInEnv(VMenv*);
void GC_markValueInCallChain(ComStack*);
void GC_markMessage(QueueNode*);
void GC_markSendT(QueueNode*);
void GC_sweep(VMheap*);
void GC_compact(VMheap*);

void DBG_printVMenv(VMenv*,FILE*);
void DBG_printVMvalue(VMvalue*,FILE*);
void DBG_printVMstack(VMstack*,FILE*,int);
void DBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);
#endif
