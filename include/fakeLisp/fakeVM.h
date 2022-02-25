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
FklVM* fklNewVM(ByteCode*);
FklVM* fklNewTmpVM(ByteCode*);
FklVM* fklNewThreadVM(VMproc*,VMheap*);
FklVM* fklNewThreadDlprocVM(VMrunnable* r,VMheap* heap);
void fklInitGlobEnv(VMenv*,VMheap*);

VMstack* fklNewVMstack(int32_t);
void fklFreeVMstack(VMstack*);
void fklStackRecycle(FklVM*);
int fklCreateNewThread(FklVM*);
FklVMlist* fklNewThreadStack(int32_t);
VMrunnable* fklHasSameProc(uint32_t,ComStack*);
int fklIsTheLastExpress(const VMrunnable*,const VMrunnable*,const FklVM* exe);
VMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FklVM*,VMcontinuation*);
void fklFreeVMheap(VMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void fklGC_mark(FklVM*);
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
void fklFreeAllSharedObj(void);
#endif
