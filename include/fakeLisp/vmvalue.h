#ifndef FKL_VMVALUE_H
#define FKL_VMVALUE_H
#include"vm.h"
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

FklVMproc* fklCopyVMproc(FklVMproc*,FklVMheap*);
void fklFreeVMproc(FklVMproc*);

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklPtrStack*,FklPtrStack*);
void fklFreeVMcontinuation(VMcontinuation*);

void fklFreeVMfp(FILE*);

FklVMdllHandle* fklNewVMdll(const char*);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklFreeVMdll(FklVMdllHandle*);

FklVMdlproc* fklNewVMdlproc(FklVMdllFunc,FklVMvalue*);
void fklFreeVMdlproc(FklVMdlproc*);

FklVMflproc* fklNewVMFlproc(FklTypeId_t type,void* func);
void fklFreeVMFlproc(FklVMflproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);


FklVMrecv* fklNewVMrecv(FklVM*);
void fklFreeVMrecv(FklVMrecv*);

FklVMsend* fklNewVMsend(FklVMvalue*);
void fklFreeVMsend(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*);


#endif
