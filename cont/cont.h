#include<fakeLisp/vm.h>

typedef struct Continuation
{
	FklVMstack* stack;
	FklVMframe* curr;
}Continuation;

typedef struct ContPublicData
{
	FklSid_t contSid;
}ContPublicData;

Continuation* createContinuation(uint32_t ap,FklVM*);

void destroyContinuation(Continuation* cont);

void createCallChainWithContinuation(FklVM*,Continuation*);
int isCont(FklVMvalue*);

FklVMudata* createContUd(Continuation* cc,FklVMvalue* rel,FklVMvalue* pd);

ContPublicData* createContPd(FklSid_t contSid);
