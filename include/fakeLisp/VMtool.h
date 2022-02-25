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

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklComStack*,FklComStack*);
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

FklTypeId_t fklGenDefTypes(FklAstCptr*,FklVMDefTypes* otherTypes,FklSid_t* typeName);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklVMDefTypes* otherTypes);
FklVMDefTypesNode* fklFindVMDefTypesNode(FklSid_t typeId,FklVMDefTypes* otherTypes);
int fklAddDefTypes(FklVMDefTypes*,FklSid_t typeName,FklTypeId_t);

FklTypeId_t fklNewVMNativeType(FklSid_t,size_t,size_t);
void fklFreeVMNativeType(FklVMNativeType*);

FklTypeId_t fklNewVMArrayType(FklTypeId_t,size_t);
void fklFreeVMArrayType(FklVMArrayType*);

FklTypeId_t fklNewVMPtrType(FklTypeId_t);
void fklFreeVMPtrType(FklVMPtrType*);

FklTypeId_t fklNewVMStructType(const char*,uint32_t,FklSid_t[],FklTypeId_t []);
void fklFreeVMStructType(FklVMStructType*);

FklTypeId_t fklNewVMUnionType(const char* structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[]);
void fklFreeVMUnionType(FklVMUnionType*);

FklTypeId_t fklNewVMFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[]);
FklTypeId_t fklFindSameFuncType(FklTypeId_t,uint32_t anum,FklTypeId_t atypes[]);
void fklFreeVMFuncType(FklVMFuncType*);

size_t fklGetVMTypeSize(FklVMTypeUnion t);
size_t fklGetVMTypeAlign(FklVMTypeUnion t);
size_t fklGetVMTypeSizeWithTypeId(FklTypeId_t t);
FklVMTypeUnion fklGetVMTypeUnion(FklTypeId_t);

FklVMMem* fklNewVMMem(FklTypeId_t typeId,uint8_t* mem);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklVMDefTypes* otherTypes);
void fklInitNativeDefTypes(FklVMDefTypes* otherTypes);
void fklWriteTypeList(FILE* fp);
void fklLoadTypeList(FILE* fp);
void fklFreeGlobTypeList(void);

int fklIsNativeTypeId(FklTypeId_t);
int fklIsArrayTypeId(FklTypeId_t);
int fklIsPtrTypeId(FklTypeId_t);
int fklIsStructTypeId(FklTypeId_t);
int fklIsUnionTypeId(FklTypeId_t);
int fklIsFunctionTypeId(FklTypeId_t);
#endif
