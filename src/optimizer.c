#include<fakeLisp/optimizer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/common.h>
#include<stdlib.h>

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)
#define I64_L24_MASK (0xFFFFFF)

static inline void set_ins_uc(FklInstruction* ins,uint32_t k)
{
	ins->au=k&I24_L8_MASK;
	ins->bu=k>>FKL_BYTE_WIDTH;
}

static inline void set_ins_ux(FklInstruction* ins,uint32_t k)
{
	ins[0].bu=k&I32_L16_MASK;
	ins[1].op=FKL_OP_EXTRA_ARG;
	ins[1].bu=k>>FKL_I16_WIDTH;
}

static inline void set_ins_uxx(FklInstruction* ins,uint64_t k)
{
	set_ins_uc(&ins[0],k&I64_L24_MASK);
	ins[1].op=FKL_OP_EXTRA_ARG;
	set_ins_uc(&ins[1],(k>>FKL_I24_WIDTH)&I64_L24_MASK);
	ins[2].op=FKL_OP_EXTRA_ARG;
	ins[2].bu=(k>>(FKL_I24_WIDTH*2));
}

static inline int set_ins_with_signed_imm(FklInstruction* ins,FklOpcode op,int64_t k)
{
	int l=1;
	if(k>=INT16_MIN&&k<=INT16_MAX)
	{
		ins[0].op=op;
		ins[0].bi=k;
	}
	else if(k>=FKL_I24_MIN&&k<=FKL_I24_MAX)
	{
		ins[0].op=op+1;
		set_ins_uc(ins,k+FKL_I24_OFFSET);
	}
	else if(k>=INT32_MIN&&k<=INT32_MAX)
	{
		ins[0].op=op+2;
		set_ins_ux(ins,k);
		l=2;
	}
	else
	{
		ins[0].op=op+3;
		set_ins_uxx(ins,k);
		l=3;
	}
	return l;
}

static inline int set_ins_with_unsigned_imm(FklInstruction* ins,FklOpcode op,uint64_t k)
{
	int l=1;
	if(k<=UINT16_MAX)
	{
		ins[0].op=op;
		ins[0].bu=k;
	}
	else if(k<=FKL_U24_MAX)
	{
		ins[0].op=op+1;
		set_ins_uc(&ins[0],k);
	}
	else if(k<=UINT32_MAX)
	{
		ins[0].op=op+2;
		set_ins_ux(ins,k);
		l=2;
	}
	else
	{
		ins[0].op=op+3;
		set_ins_uxx(ins,k);
		l=3;
	}
	return l;
}

static inline int set_ins_with_2_unsigned_imm(FklInstruction* ins,FklOpcode op,uint64_t ux,uint64_t uy)
{
	int l=1;
	if(ux<=UINT8_MAX&&uy<=UINT16_MAX)
	{
		ins[0].op=op;
		ins[0].au=ux;
		ins[0].bu=uy;
	}
	else if(ux<=FKL_U24_MAX&&uy<=FKL_U24_MAX)
	{
		ins[0].op=op+1;
		ins[1].op=FKL_OP_EXTRA_ARG;
		set_ins_uc(&ins[0],ux);
		set_ins_uc(&ins[1],uy);
		l=2;
	}
	else if(ux<=UINT32_MAX&&uy<=UINT32_MAX)
	{
		ins[0].op=op+2;
		set_ins_uc(&ins[0],ux&I64_L24_MASK);
		ins[1].op=FKL_OP_EXTRA_ARG;
		ins[1].au=ux>>FKL_I24_WIDTH;
		ins[1].bu=uy&I32_L16_MASK;
		ins[2].op=FKL_OP_EXTRA_ARG;
		ins[2].bu=uy>>FKL_I16_WIDTH;
		l=3;
	}
	else
	{
		ins[0].op=op+3;
		set_ins_uc(&ins[0],ux&I64_L24_MASK);
		ins[1].op=FKL_OP_EXTRA_ARG;
		ins[1].au=ux>>FKL_I24_WIDTH;
		ins[1].bu=uy&I32_L16_MASK;

		ins[2].op=FKL_OP_EXTRA_ARG;
		set_ins_uc(&ins[2],(uy>>FKL_I16_WIDTH)&I64_L24_MASK);

		ins[3].op=FKL_OP_EXTRA_ARG;
		set_ins_uc(&ins[3],(uy>>(FKL_I16_WIDTH+FKL_I24_WIDTH))&I64_L24_MASK);
		l=4;
	}
	return l;
}

#undef I24_L8_MASK
#undef I32_L16_MASK
#undef I64_L24_MASK

static inline void push_ins_ln(FklByteCodeBuffer* buf,const FklInsLn* ins)
{
	if(buf->size==buf->capacity)
	{
		buf->capacity<<=1;
		FklInsLn* new_base=(FklInsLn*)fklRealloc(buf->base,buf->capacity*sizeof(FklInsLn));
		FKL_ASSERT(new_base);
		buf->base=new_base;
	}

	buf->base[buf->size++]=*ins;
}

static inline void set_ins_ln_to_ins(const FklInsLn* cur_ins_ln,FklInstruction* ins)
{
	const FklInstruction* cur_ins=&cur_ins_ln->ins;
	FklOpcode op=cur_ins->op;
	int ol=fklGetOpcodeModeLen(op);

	for(int i=0;i<ol;i++)
		ins[i]=cur_ins_ln[i].ins;
}

static inline FklByteCodeBuffer* recompute_jmp_target(FklByteCodeBuffer* a,FklByteCodeBuffer* b)
{
	int change=0;
	FklInstructionArg arg;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};

	do
	{
		b->size=0;

		for(uint64_t i=0;i<a->size;)
		{
			FklInsLn* cur=&a->base[i];
			if(cur->jmp_to)
			{
				set_ins_ln_to_ins(cur,ins);
				fklGetInsOpArg(ins,&arg);
				FKL_ASSERT(arg.ix);
				if(arg.ix>0)
				{
					abort();
				}
				else
				{
					abort();
				}
			}
			else
			{
				push_ins_ln(b,cur);
				i++;
			}
		}

		FklByteCodeBuffer* tmp=a;
		a=b;
		b=tmp;
	}while(change);

	return a;
}

#include<math.h>
static inline void print_basic_block(FklByteCodeBuffer* buf,FILE* fp)
{
	fputs("\n------\n",fp);
	FklInstructionArg ins_arg;
	uint64_t codeLen=buf->size;
	int numLen=codeLen?(int)(log10(codeLen)+1):1;
	for(uint64_t i=0;i<buf->size;i++)
	{
		FklInsLn* cur=&buf->base[i];
		if(cur->block_id)
			fprintf(fp,"\nblock_id: %u\n",cur->block_id);
		FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};
		int l=fklGetOpcodeModeLen(cur->ins.op);
		for(int i=0;i<l;i++)
			ins[i]=cur[i].ins;
		fklGetInsOpArg(ins,&ins_arg);
		fprintf(fp,"%-*ld:\t%-17s ",numLen,i,fklGetOpcodeName(ins->op));
		switch(fklGetOpcodeMode(ins->op))
		{
			case FKL_OP_MODE_IsA:
			case FKL_OP_MODE_IsB:
			case FKL_OP_MODE_IsC:
			case FKL_OP_MODE_IsBB:
			case FKL_OP_MODE_IsCCB:
				fprintf(fp,"%"FKL_PRT64D,ins_arg.ix);
				break;
			case FKL_OP_MODE_IuB:
			case FKL_OP_MODE_IuC:
			case FKL_OP_MODE_IuBB:
			case FKL_OP_MODE_IuCCB:
				fprintf(fp,"%"FKL_PRT64U,ins_arg.ux);
				break;
			case FKL_OP_MODE_IuAuB:
			case FKL_OP_MODE_IuCuC:
			case FKL_OP_MODE_IuCAuBB:
			case FKL_OP_MODE_IuCAuBCC:
				fprintf(fp,"%"FKL_PRT64U"\t%"FKL_PRT64U,ins_arg.ux,ins_arg.uy);
				break;
			case FKL_OP_MODE_I:
				break;

			case FKL_OP_MODE_IxAxB:
				fprintf(fp,"%#"FKL_PRT64x"\t%#"FKL_PRT64x,ins_arg.ux,ins_arg.uy);
				break;
		}
		putc('\n',fp);
		if(cur->jmp_to)
			fprintf(fp,"jmp_to: %u\n",cur->jmp_to);
	}
}

// FIXME: 修复重计算立即数后的条件和无条件跳转指令的目标问题
void fklRecomputeInsImm(FklByteCodelnt* bcl
		,void* ctx
		,FklRecomputeInsImmPredicate predicate
		,FklRecomputeInsImmFunc func)
{
	FklInstructionArg arg;

	FklInstruction cur[4]={FKL_INSTRUCTION_STATIC_INIT};
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};

	FklByteCodeBuffer buf;
	fklInitByteCodeBufferWith(&buf,bcl);
	fklByteCodeBufferScanAndSetBasicBlock(&buf);

	FklByteCodeBuffer new_buf;
	fklInitByteCodeBuffer(&new_buf,bcl->bc->len);

	FklInsLn tmp_ins_ln;

	FklInsLn* cur_ins_ln=buf.base;
	const FklInsLn* end=&cur_ins_ln[buf.size];
	while(cur_ins_ln<end)
	{
		FklInstruction* cur_ins=&cur_ins_ln->ins;
		tmp_ins_ln=*cur_ins_ln;
		FklOpcode op=cur_ins->op;
		if(predicate(op))
		{
			int ol=fklGetOpcodeModeLen(op);

			for(int i=0;i<ol;i++)
				cur[i]=cur_ins_ln[i].ins;

			FKL_ASSERT((op<FKL_OP_JMP_IF_TRUE||op>FKL_OP_JMP_XX)&&op!=FKL_OP_EXTRA_ARG);

			fklGetInsOpArg(cur,&arg);
			FklOpcodeMode mode=FKL_OP_MODE_I;
			if(func(ctx,&op,&mode,&arg))
			{
				fklUninitByteCodeBuffer(&buf);
				fklUninitByteCodeBuffer(&new_buf);
				return;
			}

			int nl=0;
			switch(mode)
			{
				case FKL_OP_MODE_IsA:
				case FKL_OP_MODE_IsB:
				case FKL_OP_MODE_IsC:
				case FKL_OP_MODE_IsBB:
				case FKL_OP_MODE_IsCCB:
					nl=set_ins_with_signed_imm(ins,op,arg.ix);
					break;
				case FKL_OP_MODE_IuB:
				case FKL_OP_MODE_IuC:
				case FKL_OP_MODE_IuBB:
				case FKL_OP_MODE_IuCCB:
					nl=set_ins_with_unsigned_imm(ins,op,arg.ux);
					break;
				case FKL_OP_MODE_IuAuB:
				case FKL_OP_MODE_IuCuC:
				case FKL_OP_MODE_IuCAuBB:
				case FKL_OP_MODE_IuCAuBCC:
					nl=set_ins_with_2_unsigned_imm(ins,op,arg.ux,arg.uy);
					break;

				case FKL_OP_MODE_I:
				case FKL_OP_MODE_IxAxB:
					abort();
			}

			tmp_ins_ln.ins=ins[0];
			push_ins_ln(&new_buf,&tmp_ins_ln);
			for(int i=1;i<nl;i++)
				fklByteCodeBufferPush(&new_buf,&ins[i],cur_ins_ln->line,cur_ins_ln->scope,cur_ins_ln->fid);

			cur_ins_ln+=ol;
		}
		else
		{
			push_ins_ln(&new_buf,&tmp_ins_ln);
			cur_ins_ln++;
		}
	}

	FklByteCodeBuffer* fin=recompute_jmp_target(&new_buf,&buf);
	fklSetByteCodelntWithBuf(bcl,fin);
	print_basic_block(&new_buf,stderr);
	fklUninitByteCodeBuffer(&buf);
	fklUninitByteCodeBuffer(&new_buf);
}

void fklInitByteCodeBuffer(FklByteCodeBuffer* buf,size_t capacity)
{
	FKL_ASSERT(capacity);
	buf->capacity=capacity;
	buf->size=0;
	buf->base=(FklInsLn*)malloc(capacity*sizeof(FklInsLn));
	FKL_ASSERT(buf->base);
}

FklByteCodeBuffer* fklCreateByteCodeBuffer(size_t capacity)
{
	FklByteCodeBuffer* buf=(FklByteCodeBuffer*)malloc(sizeof(FklByteCodeBuffer));
	FKL_ASSERT(buf);
	fklInitByteCodeBuffer(buf,capacity);
	return buf;
}

void fklInitByteCodeBufferWith(FklByteCodeBuffer* buf,const FklByteCodelnt* bcl)
{
	size_t len=bcl->bc->len;
	fklInitByteCodeBuffer(buf,len);
	buf->size=len;

	uint64_t pc=0;
	uint64_t ln_idx=0;
	uint64_t ln_idx_end=bcl->ls-1;
	const FklLineNumberTableItem* ln_item_base=bcl->l;
	const FklInstruction* ins_base=bcl->bc->code;
	FklInsLn* ins_ln_base=buf->base;
	for(;pc<len;pc++)
	{
		if(ln_idx<ln_idx_end&&pc>=ln_item_base[ln_idx+1].scp)
			ln_idx++;

		ins_ln_base[pc]=(FklInsLn)
		{
			.ins=ins_base[pc],
			.line=ln_item_base[ln_idx].line,
			.scope=ln_item_base[ln_idx].scope,
			.fid=ln_item_base[ln_idx].fid,
			.block_id=0,
			.jmp_to=0,
		};

	}
}

void fklByteCodeBufferPush(FklByteCodeBuffer* buf
		,const FklInstruction* ins
		,uint32_t line
		,uint32_t scope
		,FklSid_t fid)
{
	if(buf->size==buf->capacity)
	{
		buf->capacity<<=1;
		FklInsLn* new_base=(FklInsLn*)fklRealloc(buf->base,buf->capacity*sizeof(FklInsLn));
		FKL_ASSERT(new_base);
		buf->base=new_base;
	}

	buf->base[buf->size++]=(FklInsLn)
	{
		.ins=*ins,
		.line=line,
		.scope=scope,
		.fid=fid,
		.block_id=0,
		.jmp_to=0,
	};
}

void fklUninitByteCodeBuffer(FklByteCodeBuffer* buf)
{
	buf->capacity=0;
	buf->size=0;
	free(buf->base);
	buf->base=NULL;
}

void fklDestroyByteCodeBuffer(FklByteCodeBuffer* buf)
{
	fklUninitByteCodeBuffer(buf);
	free(buf);
}

void fklSetByteCodelntWithBuf(FklByteCodelnt* bcl,const FklByteCodeBuffer* buf)
{
	FKL_ASSERT(buf->size);
	FklLineNumberTableItem* ln=(FklLineNumberTableItem*)fklRealloc(bcl->l,buf->size*sizeof(FklLineNumberTableItem));
	FKL_ASSERT(ln);

	FklByteCode* bc=bcl->bc;
	bc->len=buf->size;
	FklInstruction* new_code=(FklInstruction*)fklRealloc(bc->code,bc->len*sizeof(FklInstruction));
	FKL_ASSERT(new_code);

	bc->code=new_code;
	bcl->l=ln;

	const FklInsLn* ins_ln_base=buf->base;
	uint64_t ln_idx=0;
	ln[ln_idx]=(FklLineNumberTableItem){.fid=ins_ln_base->fid,.scp=0,.line=ins_ln_base->line,.scope=ins_ln_base->scope};
	for(size_t i=0;i<buf->size;i++)
	{
		const FklInsLn* cur=&ins_ln_base[i];
		if(ln[ln_idx].scope!=cur->scope
				||ln[ln_idx].line!=cur->line
				||ln[ln_idx].fid!=cur->fid)
		{
			ln_idx++;
			ln[ln_idx]=(FklLineNumberTableItem){.fid=cur->fid,.scp=i,.line=cur->line,.scope=cur->scope};
		}
		new_code[i]=cur->ins;
	}

	uint32_t ls=ln_idx+1;

	bcl->ls=ls;
	FklLineNumberTableItem* nnl=(FklLineNumberTableItem*)fklRealloc(ln,ls*sizeof(FklLineNumberTableItem));
	FKL_ASSERT(nnl);
	bcl->l=nnl;
}

void fklByteCodeBufferScanAndSetBasicBlock(FklByteCodeBuffer* buf)
{
	FKL_ASSERT(buf->size);
	FklInstructionArg arg;
	buf->base[0].block_id=1;
	uint32_t next_block_id=2;
	FklInsLn* base=buf->base;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};

	for(uint64_t i=1;i<buf->size;)
	{
		FklInsLn* cur=&base[i];
		if(fklIsJmpIns(&cur->ins)
				||fklIsCondJmpIns(&cur->ins))
		{
			int l=fklGetOpcodeModeLen(cur->ins.op);
			for(int i=0;i<l;i++)
				ins[i]=cur[i].ins;
			fklGetInsOpArg(ins,&arg);
			if(!cur[l].block_id)
				cur[l].block_id=next_block_id++;
			uint64_t jmp_to=i+arg.ix+l;
			FklInsLn* jmp_to_ins=&base[jmp_to];
			if(!jmp_to_ins->block_id)
				jmp_to_ins->block_id=next_block_id++;
			cur->jmp_to=jmp_to_ins->block_id;
			i+=l;
		}
		else
			i++;
	}
}

