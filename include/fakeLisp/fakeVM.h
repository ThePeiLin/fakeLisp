#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

typedef struct FklSharedObjNode
{
	DllHandle dll;
	struct FklSharedObjNode* next;
}FklSharedObjNode;

int fklRunFakeVM(FakeVM*);
FakeVM* fklNewFakeVM(ByteCode*);
FakeVM* fklNewTmpFakeVM(ByteCode*);
FakeVM* fklNewThreadVM(VMproc*,VMheap*);
FakeVM* fklNewThreadDlprocVM(VMrunnable* r,VMheap* heap);
void fklInitGlobEnv(VMenv*,VMheap*);

VMstack* fklNewVMstack(int32_t);
void fklFreeVMstack(VMstack*);
void fklStackRecycle(FakeVM*);
int fklCreateNewThread(FakeVM*);
FakeVMlist* fklNewThreadStack(int32_t);
VMrunnable* fklHasSameProc(uint32_t,ComStack*);
int fklIsTheLastExpress(const VMrunnable*,const VMrunnable*,const FakeVM* exe);
VMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FakeVM*,VMcontinuation*);
void fklFreeVMheap(VMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FakeVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void fklGC_mark(FakeVM*);
void fklfklGC_markValue(VMvalue*);
void fklfklfklGC_markValueInStack(VMstack*);
void fklfklfklGC_markValueInEnv(VMenv*);
void fklfklfklGC_markValueInCallChain(ComStack*);
void fklfklGC_markMessage(QueueNode*);
void fklfklGC_markSendT(QueueNode*);
void fklGC_sweep(VMheap*);
void fklGC_compact(VMheap*);

void fklDBG_printVMenv(VMenv*,FILE*);
void fklDBG_printVMvalue(VMvalue*,FILE*);
void fklDBG_printVMstack(VMstack*,FILE*,int);
void fklDBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);
#endif
