#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

typedef struct FklSharedObjNode
{
	FklDllHandle dll;
	struct FklSharedObjNode* next;
}FklSharedObjNode;

int fklRunVM(FklVM*);
FklVM* fklNewVM(FklByteCode*);
FklVM* fklNewTmpVM(FklByteCode*);
FklVM* fklNewThreadVM(FklVMproc*,VMheap*);
FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,VMheap* heap);
void fklInitGlobEnv(FklVMenv*,VMheap*);

FklVMstack* fklNewVMstack(int32_t);
void fklFreeVMstack(FklVMstack*);
void fklStackRecycle(FklVM*);
int fklCreateNewThread(FklVM*);
FklVMlist* fklNewThreadStack(int32_t);
FklVMrunnable* fklHasSameProc(uint32_t,FklPtrStack*);
int fklIsTheLastExpress(const FklVMrunnable*,const FklVMrunnable*,const FklVM* exe);
VMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FklVM*,VMcontinuation*);
void fklFreeVMheap(VMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInEnv(FklVMenv*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_sweep(VMheap*);
void fklGC_compact(VMheap*);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);
void fklFreeAllSharedObj(void);
#endif
