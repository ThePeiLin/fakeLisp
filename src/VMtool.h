#ifndef VMTOOL_H
#define VMTOOL_H
#include"fakedef.h"
#include<pthread.h>
#include<stdint.h>

#define SET_RETURN(fn,v,stack) {\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(VMvalue**)realloc((stack)->values,sizeof(VMvalue*)*((stack)->size+64));\
		if((stack)->values==NULL)\
		{\
			fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
			perror((fn));\
			exit(1);\
		}\
		(stack)->size+=64;\
	}\
	(stack)->values[(stack)->tp]=(v);\
	(stack)->tp+=1;\
}

#define RAISE_BUILTIN_ERROR(ERRORTYPE,RUNNABLE,EXE) {\
	char* errorMessage=genErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	VMerror* err=newVMerror(builtInErrorType[(ERRORTYPE)],errorMessage);\
	free(errorMessage);\
	raiseVMerror(err,(EXE));\
	return;\
}

typedef struct Cirular_Ref_List
{
	VMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;

void writeVMvalue(VMvalue*,FILE*,CRL**);
void princVMvalue(VMvalue*,FILE*,CRL**);
VMenvNode* newVMenvNode(VMvalue*,int32_t);
VMenvNode* addVMenvNode(VMenvNode*,VMenv*);
VMenvNode* findVMenvNode(int32_t,VMenv*);
void freeVMenvNode(VMenvNode*);
VMproc* newVMproc(uint32_t scp,uint32_t cpc);
void increaseVMprocRefcount(VMproc*);
void decreaseVMprocRefcount(VMproc*);

VMvalue* copyVMvalue(VMvalue*,VMheap*);
VMvalue* newVMvalue(ValueType,void*,VMheap*,int);
VMvalue* newTrueValue(VMheap*);
VMvalue* newNilValue(VMheap*);
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
VMvalue* getVMpairCar(VMvalue*);
VMvalue* getVMpairCdr(VMvalue*);
void copyRef(VMvalue*,VMvalue*);
void writeRef(VMvalue*,VMvalue*);
void freeRef(VMvalue*);
int VMvaluecmp(VMvalue*,VMvalue*);
int subVMvaluecmp(VMvalue*,VMvalue*);
int numcmp(VMvalue*,VMvalue*);

VMenv* newVMenv(VMenv*);
void increaseVMenvRefcount(VMenv*);
void decreaseVMenvRefcount(VMenv*);

VMenv* castPreEnvToVMenv(PreEnv*,VMenv*,VMheap*,SymbolTable*);
VMpair* newVMpair(VMheap*);
void increaseVMpairRefcount(VMpair*);
void decreaseVMpairRefcount(VMpair*);

VMstr* newVMstr(const char*);
void increaseVMstrRefcount(VMstr*);
void decreaseVMstrRefcount(VMstr*);

VMvalue* castCptrVMvalue(AST_cptr*,VMheap*);
ByteString* newByteString(size_t,uint8_t*);
void increaseByteStringRefcount(ByteString*);
void decreaseByteStringRefcount(ByteString*);

ByteString* copyByteString(const ByteString*);
ByteString* newEmptyByteString();

VMChanl* newVMChanl(int32_t size);
void increaseVMChanlRefcount(VMChanl*);
void decreaseVMChanlRefcount(VMChanl*);

void freeVMChanl(VMChanl*);
VMChanl* copyVMChanl(VMChanl*,VMheap*);
volatile int32_t getNumVMChanl(volatile VMChanl*);

uint8_t* copyArry(size_t,uint8_t*);
uint8_t* createByteString(int32_t);
VMproc* copyVMproc(VMproc*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
VMstack* copyStack(VMstack*);
void freeVMproc(VMproc*);
void freeVMstr(VMstr*);
void freeVMenv(VMenv*);
void releaseSource(pthread_rwlock_t*);
void lockSource(pthread_rwlock_t*);
VMvalue* getArg(VMstack*);

VMcontinuation* newVMcontinuation(VMstack*,ComStack*);
void increaseVMcontRefcount(VMcontinuation*);
void decreaseVMcontRefcount(VMcontinuation*);
void freeVMcontinuation(VMcontinuation*);

VMfp* newVMfp(FILE*);
void increaseVMfpRefcount(VMfp*);
void decreaseVMfpRefcount(VMfp*);
void freeVMfp(VMfp*);

VMDll* newVMDll(const char*);
void increaseVMDllRefcount(VMDll*);
void decreaseVMDllRefcount(VMDll*);
void* getAddress(const char*,DllHandle);
void freeVMDll(VMDll*);

VMDlproc* newVMDlproc(DllFunc,VMDll*);
void increaseVMDlprocRefcount(VMDlproc*);
void decreaseVMDlprocRefcount(VMDlproc*);
void freeVMDlproc(VMDlproc*);

VMerror* newVMerror(const char* type,const char* message);
void increaseVMerrorRefcount(VMerror*);
void decreaseVMerrorRefcount(VMerror*);
void freeVMerror(VMerror*);

RecvT* newRecvT(FakeVM*);
void freeRecvT(RecvT*);

SendT* newSendT(VMvalue*);
void freeSendT(SendT*);

void chanlSend(SendT*,VMChanl*);
void chanlRecv(RecvT*,VMChanl*);

VMTryBlock* newVMTryBlock(const char* errSymbol,uint32_t tp,long int rtp);
void freeVMTryBlock(VMTryBlock* b);

VMerrorHandler* newVMerrorHandler(const char* type,VMproc* proc);
void freeVMerrorHandler(VMerrorHandler*);
int raiseVMerror(VMerror* err,FakeVM*);
VMrunnable* newVMrunnable(VMproc*);
char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe);
int32_t getSymbolIdInByteCode(const uint8_t*);
int resBp(VMstack*);
#endif
