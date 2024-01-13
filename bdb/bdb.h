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

typedef struct
{
	FklSid_t fid;
	uint64_t pc;
	uint32_t line;
}BreakpointHashKey;

typedef struct
{
	BreakpointHashKey key;
	uint64_t num;
	struct DebugCtx* ctx;
	FklInstruction* ins;
	FklInstruction origin_ins;

	union
	{
		FklNastNode* cond_exp;
		FklVMvalue* proc;
	};
	int compiled;
}BreakpointHashItem;

typedef struct DebugCtx
{
	CmdReadCtx read_ctx;
	FklCodegenOuterCtx outer_ctx;

	FklHashTable file_sid_set;
	FklSymbolTable* st;

	int end;
	int interrupted_by_debugger;

	uint32_t curlist_line;
	uint32_t curline;
	FklSid_t curline_file;
	const FklString* curline_str;
	const FklPtrStack* curfile_lines;

	FklHashTable source_code_table;
	FklPtrStack envs;

	FklPtrStack extra_mark_value;
	FklVMvalue* code_objs;

	jmp_buf jmpb;
	FklVM* reached_thread;

	FklPtrStack reached_thread_frames;
	uint32_t curframe_idx;
	uint32_t temp_proc_prototype_id;

	FklPtrStack threads;
	uint32_t curthread_idx;

	const BreakpointHashItem* reached_breakpoint;


	FklVMgc* gc;
	FklHashTable breakpoints;

	uint32_t breakpoint_num;

	struct SteppingCtx
	{
		enum
		{
			STEP_OUT,
			STEP_INTO,
			STEP_OVER,
			STEP_UNTIL,
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
	const BreakpointHashItem* bp;
	const FklLineNumberTableItem* ln;
	DebugCtx* ctx;
}DbgInterruptArg;

DebugCtx* createDebugCtx(FklVM* exe
		,const char* filename
		,FklVMvalue* argv);
const FklString* getCurLineStr(DebugCtx* ctx,FklSid_t fid,uint32_t line);
const FklLineNumberTableItem* getCurLineNumberItemWithCp(const FklInstruction* cp,FklByteCodelnt* code);

void initBreakpointTable(FklHashTable*);

void toggleVMint3(FklVM*);
BreakpointHashItem* putBreakpointWithFileAndLine(DebugCtx* ctx,FklSid_t fid,uint32_t line,PutBreakpointErrorType*);
BreakpointHashItem* putBreakpointForProcedure(DebugCtx* ctx,FklSid_t name_sid);

typedef struct
{
	const BreakpointHashItem* bp;
}BreakpointWrapper;

FklVMvalue* createBreakpointWrapper(FklVM* exe,const BreakpointHashItem* bp);
int isBreakpointWrapper(FklVMvalue* v);
const BreakpointHashItem* getBreakpointFromWrapper(FklVMvalue* v);

const char* getPutBreakpointErrorInfo(PutBreakpointErrorType t);
void delBreakpoint(DebugCtx* ctx,uint64_t num);
const FklLineNumberTableItem* getCurFrameLineNumber(const FklVMframe*);

void unsetStepping(DebugCtx*);
void setStepInto(DebugCtx*);
void setStepOver(DebugCtx*);
void setStepOut(DebugCtx*);
void setStepUntil(DebugCtx*,uint32_t line);

void dbgInterrupt(FklVM*,DbgInterruptArg* arg);
int dbgInterruptHandler(FklVMgc* gc
		,FklVM*
		,FklVMvalue* ev
		,void* arg);

FklVMvalue* getMainProc(DebugCtx* ctx);
FklVMvalue* compileExpression(DebugCtx* ctx,FklNastNode* v,FklVMframe* frame);
FklVMvalue* callEvalProc(DebugCtx* ctx,FklVMvalue* proc,FklVMframe* frame);

void setReachedThread(DebugCtx* ctx,FklVM*);
void printBacktrace(DebugCtx* ctx,const FklString* prefix,FILE* fp);
void printCurFrame(DebugCtx* ctx,const FklString* prefix,FILE* fp);
FklVMframe* getCurrentFrame(DebugCtx* ctx);

FklVM* getCurThread(DebugCtx* ctx);
void switchCurThread(DebugCtx* ctx,uint32_t idx);
void listThreads(DebugCtx* ctx,const FklString* prefix,FILE* fp);
void printThreadAlreadyExited(DebugCtx* ctx,FILE* fp);
void printThreadCantEvaluate(DebugCtx* ctx,FILE* fp);

#ifdef __cplusplus
}
#endif
#endif
