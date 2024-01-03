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
	DBG_REACH_BREAKPOINT=1,
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
	CmdReadCtx read_ctx;
	FklCodegenOuterCtx outer_ctx;

	FklHashTable file_sid_set;
    FklSymbolTable* st;
	int is_reach_breakpoint;
	int end;
	uint32_t curline;
	FklSid_t curline_file;
	const FklString* curline_str;
	const FklPtrStack* curfile_lines;

	FklHashTable source_code_table;
	FklPtrStack envs;

	FklVMvalue* code_objs;

	jmp_buf jmpb;
	FklVM* cur_thread;
	FklVMgc* gc;
	FklHashTable break_points;

}DebugCtx;

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
	DebugCtx* ctx;
	FklInstruction* ins;
	FklInstruction origin_ins;
}BreakpointHashItem;

typedef struct
{
	FklSid_t sid;
	FklPtrStack lines;
}SourceCodeHashItem;

typedef enum
{
	PUT_BP_AT_END_OF_FILE=1,
	PUT_BP_FILE_INVALID,
	PUT_BP_IN_BLANK_OR_COMMENT,
	PUT_BP_NOT_A_PROC,
}PutBreakpointErrorType;

DebugCtx* createDebugCtx(FklVM* exe
		,const char* filename
		,FklVMvalue* argv);
const FklString* getCurLineStr(DebugCtx* ctx,FklSid_t fid,uint32_t line);

void initBreakpointTable(FklHashTable*);

BreakpointHashItem* putBreakpointWithFileAndLine(DebugCtx* ctx,FklSid_t fid,uint32_t line,PutBreakpointErrorType*);
BreakpointHashItem* putBreakpointForProcedure(DebugCtx* ctx,FklSid_t name_sid);
const char* getPutBreakpointErrorInfo(PutBreakpointErrorType t);
void delBreakpoint(DebugCtx* ctx,uint64_t num);
const FklLineNumberTableItem* getCurFrameLineNumber(const FklVMframe*);

#ifdef __cplusplus
}
#endif
#endif
