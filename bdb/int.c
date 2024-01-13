#include"bdb.h"

static void interrupt_queue_work_cb(FklVM* vm,void* a)
{
	DbgInterruptArg* arg=(DbgInterruptArg*)a;
	DebugCtx* ctx=arg->ctx;
	setReachedThread(ctx,vm);
	if(arg->bp)
	{
		BreakpointHashItem* bp=arg->bp;
		getCurLineStr(ctx,bp->key.fid,bp->key.line);
		ctx->reached_breakpoint=bp;
		if(bp->cond_exp_obj)
		{
			if(!bp->compiled)
			{
				bp->compiled=1;
				FklVMvalue* proc=compileConditionExpression(ctx,bp->cond_exp,vm->top_frame);;
				bp->cond_exp=NULL;
				bp->proc=proc;
			}
			if(bp->proc)
			{
				FklVMvalue* value=callConditionProc(ctx,bp->proc,vm->top_frame);
				if(value==FKL_VM_NIL)
					return;
			}
		}
		longjmp(ctx->jmpb,DBG_INTERRUPTED);
	}
	else
	{
		getCurLineStr(ctx,arg->ln->fid,arg->ln->line);
		longjmp(ctx->jmpb,DBG_INTERRUPTED);
	}
}

void dbgInterrupt(FklVM* exe,DbgInterruptArg* arg)
{
	fklQueueWorkInIdleThread(exe,interrupt_queue_work_cb,arg);
}

static inline FklVMframe* find_same_proc_frame(FklVMframe* f,FklVMvalue* proc)
{
	for(;f;f=f->prev)
	{
		if(f->type==FKL_FRAME_COMPOUND
				&&f->c.proc==proc)
			return f;
	}
	return NULL;
}

int dbgInterruptHandler(FklVMgc* gc
		,FklVM* exe
		,FklVMvalue* int_val
		,void* arg)
{
	FKL_ASSERT(int_val);

	DebugCtx* ctx=(DebugCtx*)arg;
	if(isBreakpointWrapper(int_val))
	{
		BreakpointHashItem* bp=getBreakpointFromWrapper(int_val);
		DbgInterruptArg arg=
		{
			.bp=bp,
			.ctx=ctx,
		};
		dbgInterrupt(exe,&arg);
	}
	else if(int_val==FKL_VM_NIL)
	{
		FklVMframe* f=exe->top_frame;
		switch(ctx->stepping_ctx.stepping_mode)
		{
			case STEP_UNTIL:
				{
					for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
					if(f)
					{
						const FklInstruction* ins=f->c.pc;
						FklByteCodelnt* code_obj=FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj);
						const FklLineNumberTableItem* next_ins_ln=getCurLineNumberItemWithCp(ins,code_obj);
						if(next_ins_ln->fid==ctx->stepping_ctx.cur_fid
								&&next_ins_ln->line>=ctx->stepping_ctx.target_line)
						{
							DbgInterruptArg arg=
							{
								.ln=next_ins_ln,
								.ctx=ctx,
							};
							unsetStepping(ctx);
							dbgInterrupt(exe,&arg);
						}
					}
				}
				break;
			case STEP_OUT:
				{
					if(f&&f->type==FKL_FRAME_COMPOUND)
					{
						const FklInstruction* ins=f->c.pc;
						FklByteCodelnt* code_obj=FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj);
						if(ins->op==FKL_OP_RET
								&&f==ctx->stepping_ctx.stepping_frame
								&&f->c.proc==ctx->stepping_ctx.stepping_proc)
						{
							DbgInterruptArg arg=
							{
								.ln=getCurLineNumberItemWithCp(ins,code_obj),
								.ctx=ctx,
							};
							unsetStepping(ctx);
							dbgInterrupt(exe,&arg);
						}
					}
				}
				break;
			case STEP_INTO:
				{
					const FklLineNumberTableItem* ln=ctx->stepping_ctx.ln;
					for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
					if(f)
					{
						const FklInstruction* ins=f->c.pc;
						FklByteCodelnt* code_obj=FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj);
						const FklLineNumberTableItem* next_ins_ln=getCurLineNumberItemWithCp(ins,code_obj);
						if(next_ins_ln->line!=ln->line||
								next_ins_ln->fid!=ln->fid)
						{
							DbgInterruptArg arg=
							{
								.ln=next_ins_ln,
								.ctx=ctx,
							};
							unsetStepping(ctx);
							dbgInterrupt(exe,&arg);
						}
					}
				}
				break;
			case STEP_OVER:
				{
					const FklLineNumberTableItem* ln=ctx->stepping_ctx.ln;
					for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
					if(f)
					{
						const FklInstruction* ins=f->c.pc;
						FklByteCodelnt* code_obj=FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj);
						const FklLineNumberTableItem* next_ins_ln=getCurLineNumberItemWithCp(ins,code_obj);

						FklVMframe* same_proc_frame=find_same_proc_frame(f,ctx->stepping_ctx.stepping_proc);
						if(same_proc_frame==NULL)
						{
							ctx->stepping_ctx.stepping_proc=f->c.proc;
							DbgInterruptArg arg=
							{
								.ln=next_ins_ln,
								.ctx=ctx,
							};
							unsetStepping(ctx);
							dbgInterrupt(exe,&arg);
						}
						else if(same_proc_frame==f)
						{
							if(next_ins_ln->line!=ln->line||
									next_ins_ln->fid!=ln->fid)
							{
								DbgInterruptArg arg=
								{
									.ln=next_ins_ln,
									.ctx=ctx,
								};
								unsetStepping(ctx);
								dbgInterrupt(exe,&arg);
							}
						}
					}
				}
				break;
		}
	}

	else if(FKL_IS_ERR(int_val))
	{
		if(exe->is_single_thread)
			return 0;
		else
			abort();
	}
	else
		return 1;
	return 0;
}

void setStepInto(DebugCtx* ctx)
{
	FklVM* exe=ctx->reached_thread;
	if(exe&&exe->top_frame)
	{
		ctx->stepping_ctx.vm=exe;
		ctx->stepping_ctx.stepping_mode=STEP_INTO;
		FklVMframe* f=exe->top_frame;
		for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
		if(f)
		{
			const FklInstruction* ins=f->c.pc;
			ctx->stepping_ctx.ln=getCurLineNumberItemWithCp(ins,FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj));
		}
		exe->trapping=1;
	}
}

void setStepOver(DebugCtx* ctx)
{
	FklVM* exe=ctx->reached_thread;
	if(exe&&exe->top_frame)
	{
		ctx->stepping_ctx.vm=exe;
		ctx->stepping_ctx.stepping_mode=STEP_OVER;
		FklVMframe* f=exe->top_frame;
		for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
		if(f)
		{
			const FklInstruction* ins=f->c.pc;
			ctx->stepping_ctx.ln=getCurLineNumberItemWithCp(ins,FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj));
			ctx->stepping_ctx.stepping_proc=f->c.proc;
		}
		exe->trapping=1;
	}
}

void setStepOut(DebugCtx* ctx)
{
	FklVM* exe=ctx->reached_thread;
	if(exe&&exe->top_frame)
	{
		ctx->stepping_ctx.vm=exe;
		ctx->stepping_ctx.stepping_mode=STEP_OUT;
		FklVMframe* f=exe->top_frame;
		for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
		if(f)
		{
			const FklInstruction* ins=f->c.pc;
			ctx->stepping_ctx.ln=getCurLineNumberItemWithCp(ins,FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj));
			ctx->stepping_ctx.stepping_proc=f->c.proc;
			ctx->stepping_ctx.stepping_frame=f;
		}
		exe->trapping=1;
	}
}

void setStepUntil(DebugCtx* ctx,uint32_t target_line)
{
	FklVM* exe=ctx->reached_thread;
	if(exe&&exe->top_frame)
	{
		ctx->stepping_ctx.vm=exe;
		ctx->stepping_ctx.stepping_mode=STEP_UNTIL;
		ctx->stepping_ctx.cur_fid=ctx->curline_file;
		ctx->stepping_ctx.target_line=target_line;
		exe->trapping=1;
	}
}

void unsetStepping(DebugCtx* ctx)
{
	struct SteppingCtx* sctx=&ctx->stepping_ctx;
	sctx->ln=NULL;
	sctx->stepping_proc=NULL;
	if(sctx->vm)
	{
		sctx->vm->trapping=0;
		sctx->vm=NULL;
	}
}

