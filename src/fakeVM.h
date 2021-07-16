#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

int runFakeVM(FakeVM*);
FakeVM* newFakeVM(ByteCode*);
FakeVM* newTmpFakeVM(ByteCode*);
FakeVM* newThreadVM(VMproc*,VMheap*);
FakeVM* newThreadDlprocVM(VMrunnable* r,VMheap* heap);
void initGlobEnv(VMenv*,VMheap*);

void B_dummy(FakeVM*);
void B_push_nil(FakeVM*);
void B_push_pair(FakeVM*);
void B_push_int(FakeVM*);
void B_push_chr(FakeVM*);
void B_push_dbl(FakeVM*);
void B_push_str(FakeVM*);
void B_push_sym(FakeVM*);
void B_push_byte(FakeVM*);
void B_push_var(FakeVM*);
void B_push_env_var(FakeVM*);
void B_push_top(FakeVM*);
void B_push_proc(FakeVM*);
void B_push_list_arg(FakeVM*);
void B_pop(FakeVM*);
void B_pop_var(FakeVM*);
void B_pop_arg(FakeVM*);
void B_pop_rest_arg(FakeVM*);
void B_pop_car(FakeVM*);
void B_pop_cdr(FakeVM*);
void B_pop_ref(FakeVM*);
void B_pop_env(FakeVM*);
void B_swap(FakeVM*);
void B_pack_cc(FakeVM*);
void B_set_tp(FakeVM*);
void B_set_bp(FakeVM*);
void B_invoke(FakeVM*);
void B_res_tp(FakeVM*);
void B_pop_tp(FakeVM*);
void B_res_bp(FakeVM*);
void B_append(FakeVM*);
void B_jmp_if_true(FakeVM*);
void B_jmp_if_false(FakeVM*);
void B_jmp(FakeVM*);
void B_push_try(FakeVM*);
void B_pop_try(FakeVM*);

VMstack* newVMstack(int32_t);
void freeVMstack(VMstack*);
void stackRecycle(FakeVM*);
int createNewThread(FakeVM*);
FakeVMlist* newThreadStack(int32_t);
VMrunnable* hasSameProc(VMproc*,ComStack*);
int isTheLastExpress(const VMrunnable*,const VMrunnable*,const FakeVM* exe);
VMheap* newVMheap();
void createCallChainWithContinuation(FakeVM*,VMcontinuation*);
void freeVMheap(VMheap*);
void freeAllVMs();
void deleteCallChain(FakeVM*);
void joinAllThread();
void cancelAllThread();
void GC_mark(FakeVM*);
void GC_markValue(VMvalue*);
void GC_markValueInStack(VMstack*);
void GC_markValueInEnv(VMenv*);
void GC_markValueInCallChain(ComStack*);
void GC_markMessage(QueueNode*);
void GC_markSendT(QueueNode*);
void GC_sweep(VMheap*);
void GC_compact(VMheap*);

void DBG_printVMenv(VMenv*,FILE*);
void DBG_printVMvalue(VMvalue*,FILE*);
void DBG_printVMstack(VMstack*,FILE*,int);
void DBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);
#endif
