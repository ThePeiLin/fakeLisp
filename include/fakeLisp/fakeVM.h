#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

typedef struct FklSharedObjNode
{
	DllHandle dll;
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
FklVMrunnable* fklHasSameProc(uint32_t,FklComStack*);
int fklIsTheLastExpress(const FklVMrunnable*,const FklVMrunnable*,const FklVM* exe);
VMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FklVM*,VMcontinuation*);
void fklFreeVMheap(VMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void fklGC_mark(FklVM*);
void fklfklGC_markValue(FklVMvalue*);
void fklfklfklGC_markValueInStack(FklVMstack*);
void fklfklfklGC_markValueInEnv(FklVMenv*);
void fklfklfklGC_markValueInCallChain(FklComStack*);
void fklfklGC_markMessage(FklQueueNode*);
void fklfklGC_markSendT(FklQueueNode*);
void fklGC_sweep(VMheap*);
void fklGC_compact(VMheap*);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);
void fklDBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);
void fklFreeAllSharedObj(void);
#endif
