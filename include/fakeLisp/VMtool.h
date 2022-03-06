#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

typedef struct Cirular_Ref_List
{
	FklVMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;

FklVMvalue* fklGET_VAL(FklVMvalue* P,VMheap*);
int fklSET_REF(FklVMvalue* P,FklVMvalue* V);
void fklPrin1VMvalue(FklVMvalue*,FILE*,CRL**);
void fklPrincVMvalue(FklVMvalue*,FILE*,CRL**);
FklVMenvNode* fklNewVMenvNode(FklVMvalue*,int32_t);
FklVMenvNode* fklAddVMenvNode(FklVMenvNode*,FklVMenv*);
FklVMenvNode* fklFindVMenvNode(FklSid_t,FklVMenv*);
void fklFreeVMenvNode(FklVMenvNode*);
FklVMproc* fklNewVMproc(uint32_t scp,uint32_t cpc);

FklVMvalue* fklCopyVMvalue(FklVMvalue*,VMheap*);
FklVMvalue* fklNewVMvalue(FklValueType,void*,VMheap*);
FklVMvalue* fklNewTrueValue(VMheap*);
FklVMvalue* fklNewNilValue();
FklVMvalue* fklGetTopValue(FklVMstack*);
FklVMvalue* fklGetValue(FklVMstack*,int32_t);
FklVMvalue* fklGetVMpairCar(FklVMvalue*);
FklVMvalue* fklGetVMpairCdr(FklVMvalue*);
int fklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int subfklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int fklNumcmp(FklVMvalue*,FklVMvalue*);

FklVMenv* fklNewVMenv(FklVMenv*);
void fklIncreaseVMenvRefcount(FklVMenv*);
void fklDecreaseVMenvRefcount(FklVMenv*);

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv*,FklVMenv*,VMheap*);
FklVMpair* fklNewVMpair(void);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,VMheap*);
FklVMByts* fklNewVMByts(size_t,uint8_t*);
void fklIncreaseVMByts(FklVMByts*);
void fklDecreaseVMByts(FklVMByts*);

FklVMByts* fklCopyVMByts(const FklVMByts*);
FklVMByts* fklNewEmptyVMByts();
void fklVMBytsCat(FklVMByts**,const FklVMByts*);
int fklEqVMByts(const FklVMByts*,const FklVMByts*);
FklVMChanl* fklNewVMChanl(int32_t size);

void fklFreeVMChanl(FklVMChanl*);
FklVMChanl* fklCopyVMChanl(FklVMChanl*,VMheap*);
int32_t fklGetNumVMChanl(FklVMChanl*);

uint8_t* fklCopyArry(size_t,uint8_t*);
FklVMproc* fklCopyVMproc(FklVMproc*,VMheap*);
FklVMenv* fklCopyVMenv(FklVMenv*,VMheap*);
FklVMstack* fklCopyStack(FklVMstack*);
void fklFreeVMproc(FklVMproc*);
void fklFreeVMenv(FklVMenv*);
void fklReleaseSource(pthread_rwlock_t*);
void fklLockSource(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklPtrStack*,FklPtrStack*);
void fklFreeVMcontinuation(VMcontinuation*);

void fklFreeVMfp(FILE*);

FklDllHandle* fklNewVMDll(const char*);
void* fklGetAddress(const char*,FklDllHandle);
void fklFreeVMDll(FklDllHandle*);

FklVMDlproc* fklNewVMDlproc(FklDllFunc,FklVMvalue*);
void fklFreeVMDlproc(FklVMDlproc*);

FklVMFlproc* fklNewVMFlproc(FklTypeId_t type,void* func);
void fklFreeVMFlproc(FklVMFlproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);

FklRecvT* fklNewRecvT(FklVM*);
void fklFreeRecvT(FklRecvT*);

FklSendT* fklNewSendT(FklVMvalue*);
void fklFreeSendT(FklSendT*);

void fklChanlSend(FklSendT*,FklVMChanl*);
void fklChanlRecv(FklRecvT*,FklVMChanl*);

FklVMTryBlock* fklNewVMTryBlock(FklSid_t,uint32_t tp,long int rtp);
void fklFreeVMTryBlock(FklVMTryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*);
char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);
int fklResBp(FklVMstack*);

FklVMMem* fklNewVMMem(FklTypeId_t typeId,uint8_t* mem);
#endif
