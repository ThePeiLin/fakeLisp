#ifndef FAKEVM_H
#define FAKEVM_H
#include"fakedef.h"
#include<pthread.h>

void runFakeVM(fakeVM*);
void printVMvalue(VMvalue*,VMpair*,FILE*,int8_t,int8_t);
void princVMvalue(VMvalue*,VMpair*,FILE*,int8_t);
void printProc(VMcode*,FILE*);
fakeVM* newFakeVM(ByteCode*,ByteCode*);
fakeVM* newTmpFakeVM(ByteCode*,ByteCode*);
fakeVM* newThreadVM(VMcode*,ByteCode*,filestack*,VMheap*,Dlls*);
void initGlobEnv(VMenv*,VMheap*);
int B_dummy(fakeVM*);
int B_push_nil(fakeVM*);
int B_push_pair(fakeVM*);
int B_push_int(fakeVM*);
int B_push_chr(fakeVM*);
int B_push_dbl(fakeVM*);
int B_push_str(fakeVM*);
int B_push_sym(fakeVM*);
int B_push_byte(fakeVM*);
int B_push_var(fakeVM*);
int B_push_car(fakeVM*);
int B_push_cdr(fakeVM*);
int B_push_top(fakeVM*);
int B_push_proc(fakeVM*);
int B_push_list_arg(fakeVM*);
int B_pop(fakeVM*);
int B_pop_var(fakeVM*);
int B_pop_rest_var(fakeVM*);
int B_pop_car(fakeVM*);
int B_pop_cdr(fakeVM*);
int B_pop_ref(fakeVM*);
int B_add(fakeVM*);
int B_sub(fakeVM*);
int B_mul(fakeVM*);
int B_div(fakeVM*);
int B_mod(fakeVM*);
int B_atom(fakeVM*);
int B_null(fakeVM*);
int B_init_proc(fakeVM*);
int B_call_proc(fakeVM*);
int B_end_proc(fakeVM*);
int B_set_bp(fakeVM*);
int B_invoke(fakeVM*);
int B_res_bp(fakeVM*);
int B_open(fakeVM*);
int B_close(fakeVM*);
int B_cast_to_chr(fakeVM*);
int B_cast_to_int(fakeVM*);
int B_cast_to_dbl(fakeVM*);
int B_cast_to_str(fakeVM*);
int B_cast_to_sym(fakeVM*);
int B_cast_to_byte(fakeVM*);
int B_type_of(fakeVM*);
int B_nth(fakeVM*);
int B_length(fakeVM*);
int B_appd(fakeVM*);
int B_str_cat(fakeVM*);
int B_byte_cat(fakeVM*);
int B_eq(fakeVM*);
int B_eqn(fakeVM*);
int B_equal(fakeVM*);
int B_gt(fakeVM*);
int B_ge(fakeVM*);
int B_lt(fakeVM*);
int B_le(fakeVM*);
int B_not(fakeVM*);
int B_jump_if_ture(fakeVM*);
int B_jump_if_false(fakeVM*);
int B_jump(fakeVM*);
int B_read(fakeVM*);
int B_readb(fakeVM*);
int B_write(fakeVM*);
int B_writeb(fakeVM*);
int B_princ(fakeVM*);
int B_go(fakeVM*);
int B_send(fakeVM*);
int B_accept(fakeVM*);
int B_getid(fakeVM*);
VMstack* newStack(int32_t);
filestack* newFileStack();
void freeFileStack(filestack*);
void freeVMstack(VMstack*);
void freeMessage(threadMessage*);
void stackRecycle(fakeVM*);
VMcode* newBuiltInProc(ByteCode*);
VMprocess* newFakeProcess(VMcode*,VMprocess*);
void printAllStack(VMstack*,FILE*);
int createNewThread(fakeVM*);
fakeVMStack* newThreadStack(int32_t);
threadMessage* newThreadMessage(VMvalue*,VMheap*);
threadMessage* newMessage(VMvalue*);
int sendMessage(threadMessage*,fakeVM*);
VMvalue* acceptMessage(fakeVM*);
VMprocess* hasSameProc(VMcode*,VMprocess*);
int isTheLastExpress(const VMprocess*,const VMprocess*);
void printEnv(VMenv*,FILE*);
VMheap* newVMheap();
void freeVMheap(VMheap*);
void freeAllVMs();
void joinAllThread();
void writeRef(VMvalue*,VMvalue*);
void GC_mark(fakeVM*);
void GC_markValue(VMvalue*);
void GC_markValueInStack(VMstack*);
void GC_markValueInEnv(VMenv*);
void GC_markValueInCallChain(VMprocess*);
void GC_markMessage(threadMessage*);
void GC_sweep(VMheap*);
void GC_compact(VMheap*);
#endif
