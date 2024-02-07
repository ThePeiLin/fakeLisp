#ifndef FKL_BDB_BDB_H
#define FKL_BDB_BDB_H

#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>
#include<replxx.h>

#ifdef __cplusplus
extern "C"{
#endif

enum
{
	DBG_INTERRUPTED=1,
	DBG_ERROR_OCCUR
};

typedef struct
{
	Replxx* replxx;
	FklStringBuffer buf;
	uint32_t offset;
	FklPtrStack symbolStack;
	FklUintStack lineStack;
	FklPtrStack stateStack;
}CmdReadCtx;

typedef struct Breakpoint
{
	uint64_t num;
	uint64_t count;
	struct Breakpoint* prev;
	struct Breakpoint* next;

	FklSid_t fid;
	uint32_t line;
	uint8_t compiled;
	uint8_t is_temporary;
	uint8_t is_deleted;
	uint8_t is_disabled;

	struct DebugCtx* ctx;
	FklInstruction* ins;
	FklInstruction origin_ins;

	FklVMvalue* cond_exp_obj;
	union
	{
		FklNastNode* cond_exp;
		FklVMvalue* proc;
	};
}Breakpoint;

typedef struct
{
	uint64_t num;
	Breakpoint* bp;
}BreakpointHashItem;

typedef struct DebugCtx
{
	CmdReadCtx read_ctx;
	FklCodegenEnv* glob_env;
	FklCodegenOuterCtx outer_ctx;

	FklHashTable file_sid_set;
	FklSymbolTable* st;

	int8_t exit;
	int8_t running;
	int8_t done;

	uint64_t curlist_ins_pc;
	FklVMvalue* curlist_bytecode;

	uint32_t curlist_line;
	uint32_t curline;
	FklSid_t curline_file;
	const FklString* curline_str;
	const FklPtrStack* curfile_lines;

	FklHashTable source_code_table;
	FklHashTable envs;

	FklPtrStack extra_mark_value;
	FklPtrStack code_objs;

	jmp_buf jmpb;
	jmp_buf jmpe;
	FklVM* reached_thread;

	FklPtrStack reached_thread_frames;
	uint32_t curframe_idx;
	uint32_t temp_proc_prototype_id;

	FklPtrStack threads;
	uint32_t curthread_idx;

	const Breakpoint* reached_breakpoint;


	FklVMgc* gc;

	uint64_t breakpoint_num;
	FklHashTable breakpoints;
	Breakpoint* deleted_breakpoints;
	FklUintStack unused_prototype_id_for_cond_bp;

	struct SteppingCtx
	{
		enum
		{
			STEP_OUT,
			STEP_INTO,
			STEP_OVER,
			STEP_UNTIL,
			SINGLE_INS,
		}stepping_mode;
		FklVM* vm;
		const FklLineNumberTableItem* ln;
		FklVMvalue* stepping_proc;
		FklVMframe* stepping_frame;

		FklSid_t cur_fid;
		uint64_t target_line;
	}stepping_ctx;
}DebugCtx;

typedef struct
{
	FklSid_t sid;
	FklPtrStack lines;
}SourceCodeHashItem;

typedef enum
{
	PUT_BP_AT_END_OF_FILE=1,
	PUT_BP_FILE_INVALID,
	PUT_BP_NOT_A_PROC,
}PutBreakpointErrorType;

typedef struct
{
	Breakpoint* bp;
	const FklLineNumberTableItem* ln;
	DebugCtx* ctx;
	int err;
}DbgInterruptArg;

typedef struct
{
	uint32_t id;
	FklCodegenEnv* env;
}EnvHashItem;

DebugCtx* createDebugCtx(FklVM* exe
		,const char* filename
		,FklVMvalue* argv);
void exitDebugCtx(DebugCtx*);
void destroyDebugCtx(DebugCtx*);
const FklString* getCurLineStr(DebugCtx* ctx,FklSid_t fid,uint32_t line);
const FklLineNumberTableItem* getCurLineNumberItemWithCp(const FklInstruction* cp,FklByteCodelnt* code);

void initBreakpointTable(FklHashTable*);
void initEnvTable(FklHashTable*);
void putEnv(DebugCtx* ctx,FklCodegenEnv* env);
FklCodegenEnv* getEnv(DebugCtx*,uint32_t id);

void toggleVMint3(FklVM*);

void markBreakpointCondExpObj(DebugCtx* ctx,FklVMgc* gc);

const SourceCodeHashItem* getSourceWithFid(DebugCtx* dctx,FklSid_t fid);
Breakpoint* createBreakpoint(uint64_t,FklSid_t,uint32_t,FklInstruction* ins,DebugCtx* ctx);
Breakpoint* putBreakpointWithFileAndLine(DebugCtx* ctx,FklSid_t fid,uint32_t line,PutBreakpointErrorType*);
Breakpoint* putBreakpointForProcedure(DebugCtx* ctx,FklSid_t name_sid);

typedef struct
{
	Breakpoint* bp;
}BreakpointWrapper;

FklVMvalue* createBreakpointWrapper(FklVM* exe,Breakpoint* bp);
int isBreakpointWrapper(FklVMvalue* v);
Breakpoint* getBreakpointFromWrapper(FklVMvalue* v);

const char* getPutBreakpointErrorInfo(PutBreakpointErrorType t);
Breakpoint* disBreakpoint(DebugCtx* ctx,uint64_t num);
Breakpoint* enableBreakpoint(DebugCtx* ctx,uint64_t num);
Breakpoint* delBreakpoint(DebugCtx* ctx,uint64_t num);
void clearBreakpoint(DebugCtx* ctx);

const FklLineNumberTableItem* getCurFrameLineNumber(const FklVMframe*);

void unsetStepping(DebugCtx*);
void setStepInto(DebugCtx*);
void setStepOver(DebugCtx*);
void setStepOut(DebugCtx*);
void setStepUntil(DebugCtx*,uint32_t line);
void setSingleStep(DebugCtx*);

void dbgInterrupt(FklVM*,DbgInterruptArg* arg);
FklVMinterruptResult dbgInterruptHandler(FklVM*
		,FklVMvalue* ev
		,FklVMvalue** pv
		,void* arg);
void setAllThreadReadyToExit(FklVM* head);
void waitAllThreadExit(FklVM* head);
void restartDebugging(DebugCtx* ctx);

FklVMvalue* compileConditionExpression(DebugCtx* ctx,FklVM*,FklNastNode* exp,FklVMframe* cur_frame,int* is_complie_unabled);
FklVMvalue* compileEvalExpression(DebugCtx* ctx,FklVM*,FklNastNode* v,FklVMframe* frame,int* is_complie_unabled);
FklVMvalue* callEvalProc(DebugCtx* ctx,FklVM*,FklVMvalue* proc,FklVMframe* frame);

void setReachedThread(DebugCtx* ctx,FklVM*);
void printBacktrace(DebugCtx* ctx,const FklString* prefix,FILE* fp);
void printCurFrame(DebugCtx* ctx,const FklString* prefix,FILE* fp);
FklVMframe* getCurrentFrame(DebugCtx* ctx);

FklVM* getCurThread(DebugCtx* ctx);
void switchCurThread(DebugCtx* ctx,uint32_t idx);
void listThreads(DebugCtx* ctx,const FklString* prefix,FILE* fp);
void printThreadAlreadyExited(DebugCtx* ctx,FILE* fp);
void printThreadCantEvaluate(DebugCtx* ctx,FILE* fp);
void printUnableToCompile(FILE* fp);

#ifdef __cplusplus
}
#endif
#endif
