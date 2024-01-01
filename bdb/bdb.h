#ifndef FKL_BDB_BDB_H
#define FKL_BDB_BDB_H

#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>
#include<replxx.h>

#ifdef __cplusplus
extern "C"{
#endif

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
	uv_thread_t idler_thread_id;
	CmdReadCtx read_ctx;
	FklCodegenOuterCtx outer_ctx;
	FklCodegenInfo main_info;

	int end;
	uint32_t curline;

	const char* cur_file_realpath;
	const char* curline_str;

	FklHashTable source_code_table;
	FklPtrStack envs;
	FklPtrStack codegen_infos;
	FklPtrStack code_objs;

	FklVM* main_thread;
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

DebugCtx* createDebugCtx(FklVM* exe
		,const char* filename
		,FklVMvalue* argv);
#ifdef __cplusplus
}
#endif
#endif
