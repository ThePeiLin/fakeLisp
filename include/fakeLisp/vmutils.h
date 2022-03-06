#ifndef VMTOOL_H
#define VMTOOL_H
#include"vm.h"
#include"compiler.h"
#include<stdint.h>
FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);
FklVMenv* fklCastPreEnvToVMenv(FklPreEnv*,FklVMenv*,FklVMheap*);
FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline);

uint8_t* fklCopyArry(size_t,uint8_t*);
FklVMstack* fklCopyStack(FklVMstack*);
void fklReleaseSource(pthread_rwlock_t*);
void fklLockSource(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);
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

VMcontinuation* fklNewVMcontinuation(FklVMstack* stack,FklPtrStack* rstack,FklPtrStack* tstack);
void fklFreeVMcontinuation(VMcontinuation* cont);
#endif
