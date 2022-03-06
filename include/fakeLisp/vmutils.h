#ifndef VMTOOL_H
#define VMTOOL_H
#include"vm.h"
#include"compiler.h"
#include<stdint.h>
struct Cirular_Ref_List;
FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap*);
int fklSET_REF(FklVMvalue* P,FklVMvalue* V);
void fklPrin1VMvalue(FklVMvalue*,FILE*,struct Cirular_Ref_List**);
void fklPrincVMvalue(FklVMvalue*,FILE*,struct Cirular_Ref_List**);
FklVMenvNode* fklNewVMenvNode(FklVMvalue*,int32_t);
FklVMenvNode* fklAddVMenvNode(FklVMenvNode*,FklVMenv*);
FklVMenvNode* fklFindVMenvNode(FklSid_t,FklVMenv*);
void fklFreeVMenvNode(FklVMenvNode*);
FklVMproc* fklNewVMproc(uint32_t scp,uint32_t cpc);

FklVMvalue* fklCopyVMvalue(FklVMvalue*,FklVMheap*);
FklVMvalue* fklNewVMvalue(FklValueType,void*,FklVMheap*);
FklVMvalue* fklNewTrueValue(FklVMheap*);
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

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv*,FklVMenv*,FklVMheap*);
FklVMpair* fklNewVMpair(void);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,FklVMheap*);
FklVMbyts* fklNewVMbyts(size_t,uint8_t*);
void fklIncreaseVMbyts(FklVMbyts*);
void fklDecreaseVMbyts(FklVMbyts*);

FklVMbyts* fklCopyVMbyts(const FklVMbyts*);
FklVMbyts* fklNewEmptyVMbyts();
void fklVMbytsCat(FklVMbyts**,const FklVMbyts*);
int fklEqVMbyts(const FklVMbyts*,const FklVMbyts*);
FklVMchanl* fklNewVMChanl(int32_t size);

void fklFreeVMChanl(FklVMchanl*);
FklVMchanl* fklCopyVMChanl(FklVMchanl*,FklVMheap*);
int32_t fklGetNumVMChanl(FklVMchanl*);

uint8_t* fklCopyArry(size_t,uint8_t*);
FklVMproc* fklCopyVMproc(FklVMproc*,FklVMheap*);
FklVMenv* fklCopyVMenv(FklVMenv*,FklVMheap*);
FklVMstack* fklCopyStack(FklVMstack*);
void fklFreeVMproc(FklVMproc*);
void fklFreeVMenv(FklVMenv*);
void fklReleaseSource(pthread_rwlock_t*);
void fklLockSource(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklPtrStack*,FklPtrStack*);
void fklFreeVMcontinuation(VMcontinuation*);

void fklFreeVMfp(FILE*);

FklVMdllHandle* fklNewVMDll(const char*);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklFreeVMDll(FklVMdllHandle*);

FklVMdlproc* fklNewVMDlproc(FklVMdllFunc,FklVMvalue*);
void fklFreeVMDlproc(FklVMdlproc*);

FklVMflproc* fklNewVMFlproc(FklTypeId_t type,void* func);
void fklFreeVMFlproc(FklVMflproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);

FklVMrecv* fklNewRecvT(FklVM*);
void fklFreeRecvT(FklVMrecv*);

FklVMsend* fklNewSendT(FklVMvalue*);
void fklFreeSendT(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*);

FklVMtryBlock* fklNewVMTryBlock(FklSid_t,uint32_t tp,long int rtp);
void fklFreeVMTryBlock(FklVMtryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*);
char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);
int fklResBp(FklVMstack*);

FklVMmem* fklNewVMMem(FklTypeId_t typeId,uint8_t* mem);
#endif
