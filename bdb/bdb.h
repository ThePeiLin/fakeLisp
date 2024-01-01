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
	DBG_REACH_BP=1,
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
	FklCodegenInfo main_info;

    FklSymbolTable* st;
	int end;
	uint32_t curline;
	FklSid_t curline_file;
	const FklString* curline_str;
	const FklPtrStack* curfile_lines;

	FklHashTable source_code_table;
	FklPtrStack envs;
	FklPtrStack codegen_infos;
	FklPtrStack code_objs;

	jmp_buf jmpb;
	FklVM* cur_thread;
	FklVMgc* gc;
	FklHashTable break_points;

	uv_mutex_t reach_breakpoint_lock;
	struct ReachBreakPoints
	{
		struct ReachBreakPoints* next;
		FklVM* vm;
		uv_cond_t cond;
	}* head;
	struct ReachBreakPoints** tail;

}DebugCtx;

typedef struct
{
	FklSid_t sid;
	FklPtrStack lines;
}SourceCodeHashItem;

DebugCtx* createDebugCtx(FklVM* exe
		,const char* filename
		,FklVMvalue* argv);
const FklString* GetCurLineStr(DebugCtx* ctx,FklSid_t fid,uint32_t line);

#ifdef __cplusplus
}
#endif
#endif
