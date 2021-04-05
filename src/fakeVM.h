#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

int runFakeVM(FakeVM*);
void printVMvalue(VMvalue*,VMpair*,FILE*,int8_t,int8_t);
void princVMvalue(VMvalue*,VMpair*,FILE*,int8_t);
void printProc(VMcode*,FILE*);
FakeVM* newFakeVM(ByteCode*,ByteCode*);
FakeVM* newTmpFakeVM(ByteCode*,ByteCode*);
FakeVM* newThreadVM(VMcode*,ByteCode*,VMheap*,Dlls*);
void initGlobEnv(VMenv*,VMheap*,SymbolTable*);
int B_dummy(FakeVM*);
int B_push_nil(FakeVM*);
int B_push_pair(FakeVM*);
int B_push_int(FakeVM*);
int B_push_chr(FakeVM*);
int B_push_dbl(FakeVM*);
int B_push_str(FakeVM*);
int B_push_sym(FakeVM*);
int B_push_byte(FakeVM*);
int B_push_chan(FakeVM*);
int B_push_var(FakeVM*);
int B_push_car(FakeVM*);
int B_push_cdr(FakeVM*);
int B_push_top(FakeVM*);
int B_push_proc(FakeVM*);
int B_push_mod_proc(FakeVM*);
int B_push_list_arg(FakeVM*);
int B_pop_var(FakeVM*);
int B_pop_rest_var(FakeVM*);
int B_pop_car(FakeVM*);
int B_pop_cdr(FakeVM*);
int B_pop_ref(FakeVM*);
int B_pack_cc(FakeVM*);
int B_add(FakeVM*);
int B_sub(FakeVM*);
int B_mul(FakeVM*);
int B_div(FakeVM*);
int B_rem(FakeVM*);
int B_atom(FakeVM*);
int B_null(FakeVM*);
int B_call_proc(FakeVM*);
int B_set_tp(FakeVM*);
int B_set_bp(FakeVM*);
int B_invoke(FakeVM*);
int B_res_tp(FakeVM*);
int B_pop_tp(FakeVM*);
int B_res_bp(FakeVM*);
int B_open(FakeVM*);
int B_cast_to_chr(FakeVM*);
int B_cast_to_int(FakeVM*);
int B_cast_to_dbl(FakeVM*);
int B_cast_to_str(FakeVM*);
int B_cast_to_sym(FakeVM*);
int B_cast_to_byte(FakeVM*);
int B_type(FakeVM*);
int B_nth(FakeVM*);
int B_length(FakeVM*);
int B_appd(FakeVM*);
int B_eq(FakeVM*);
int B_eqn(FakeVM*);
int B_equal(FakeVM*);
int B_gt(FakeVM*);
int B_ge(FakeVM*);
int B_lt(FakeVM*);
int B_le(FakeVM*);
int B_not(FakeVM*);
int B_jump_if_ture(FakeVM*);
int B_jump_if_false(FakeVM*);
int B_jump(FakeVM*);
int B_read(FakeVM*);
int B_getb(FakeVM*);
int B_write(FakeVM*);
int B_putb(FakeVM*);
int B_princ(FakeVM*);
int B_go(FakeVM*);
int B_chanl(FakeVM*);
int B_send(FakeVM*);
int B_recv(FakeVM*);
VMstack* newStack(int32_t);
void freeVMstack(VMstack*);
void stackRecycle(FakeVM*);
VMcode* newBuiltInProc(ByteCode*);
VMprocess* newFakeProcess(VMcode*,VMprocess*);
void printAllStack(VMstack*,FILE*,int);
int createNewThread(FakeVM*);
FakeVMlist* newThreadStack(int32_t);
ThreadMessage* newThreadMessage(VMvalue*,VMheap*);
ThreadMessage* newMessage(VMvalue*);
int sendMessage(ThreadMessage*,FakeVM*);
VMprocess* hasSameProc(VMcode*,VMprocess*);
int isTheLastExpress(const VMprocess*,const VMprocess*);
void printEnv(VMenv*,FILE*);
VMheap* newVMheap();
void createCallChainWithContinuation(FakeVM*,VMcontinuation*);
void freeVMheap(VMheap*);
void freeAllVMs();
void deleteCallChain(FakeVM*);
void joinAllThread();
void GC_mark(FakeVM*);
void GC_markValue(VMvalue*);
void GC_markValueInStack(VMstack*);
void GC_markValueInEnv(VMenv*);
void GC_markValueInCallChain(VMprocess*);
void GC_markMessage(ThreadMessage*);
void GC_sweep(VMheap*);
void GC_compact(VMheap*);

void fprintValue(VMvalue*,FILE*);
#endif
