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

static inline int set_ins_ln_to_ins(const FklInsLn* cur_ins_ln,FklInstruction* ins)
{
	const FklInstruction* cur_ins=&cur_ins_ln->ins;
	FklOpcode op=cur_ins->op;
	int ol=fklGetOpcodeModeLen(op);

	for(int i=0;i<ol;i++)
		ins[i]=cur_ins_ln[i].ins;
	return ol;
}

static inline int set_jmp_backward(FklOpcode op,int64_t len,FklInstruction* new_ins)
{
	len=-len-1;
	if(len>=FKL_I24_MIN)
		return set_ins_with_signed_imm(new_ins,op,len);
	len-=1;
	if(len>=INT32_MIN)
		return set_ins_with_signed_imm(new_ins,op,len);
	len-=1;
	return set_ins_with_signed_imm(new_ins,op,len);
}

static inline FklByteCodeBuffer* recompute_jmp_target(FklByteCodeBuffer* a,FklByteCodeBuffer* b)
{
	int change;
	FklInstructionArg arg;
	FklInsLn tmp_ins_ln;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};
	FklInstruction new_ins[4]={FKL_INSTRUCTION_STATIC_INIT};

	do
	{
		change=0;
		b->size=0;

		for(uint64_t i=0;i<a->size;)
		{
			FklInsLn* cur=&a->base[i];
			tmp_ins_ln=*cur;
			if(cur->jmp_to)
			{
				int ol=set_ins_ln_to_ins(cur,ins);
				fklGetInsOpArg(ins,&arg);
				if(fklIsPushProcIns(ins))
				{
					FKL_ASSERT(arg.uy);
					FklInsLn* jmp_to=&cur[ol];
					uint64_t end=a->size-i-ol;
					uint64_t jmp_offset=0;
					for(;jmp_offset<end&&jmp_to[jmp_offset].block_id!=cur->jmp_to;jmp_offset++);
					FKL_ASSERT(jmp_offset<end);

					FklOpcode op=FKL_OP_PUSH_PROC;

					int nl=set_ins_with_2_unsigned_imm(new_ins,op,arg.ux,jmp_offset);

					if(jmp_offset==(uint64_t)arg.uy&&ol==nl)
						goto no_change;
					change=1;

					tmp_ins_ln.ins=new_ins[0];
					push_ins_ln(b,&tmp_ins_ln);
					for(int i=1;i<nl;i++)
						fklByteCodeBufferPush(b,&new_ins[i],cur->line,cur->scope,cur->fid);
					goto done;
				}
				FKL_ASSERT(arg.ix);
				if(arg.ix>0)
				{
					FklInsLn* jmp_to=&cur[ol];
					uint64_t end=a->size-i-ol;
					uint64_t jmp_offset=0;
					for(;jmp_offset<end&&jmp_to[jmp_offset].block_id!=cur->jmp_to;jmp_offset++);
					FKL_ASSERT(jmp_offset<end);

					FklOpcode op=(cur->ins.op>=FKL_OP_JMP_IF_TRUE&&cur->ins.op<=FKL_OP_JMP_IF_TRUE_XX)
						?FKL_OP_JMP_IF_TRUE
						:(cur->ins.op>=FKL_OP_JMP_IF_FALSE&&cur->ins.op<=FKL_OP_JMP_IF_FALSE_XX)
						?FKL_OP_JMP_IF_FALSE
						:FKL_OP_JMP;

					int nl=set_ins_with_unsigned_imm(new_ins,op,jmp_offset);

					if(jmp_offset==(uint64_t)arg.ix&&ol==nl)
						goto no_change;
					change=1;

					tmp_ins_ln.ins=new_ins[0];
					push_ins_ln(b,&tmp_ins_ln);
					for(int i=1;i<nl;i++)
						fklByteCodeBufferPush(b,&new_ins[i],cur->line,cur->scope,cur->fid);
				}
				else
				{
					int64_t offset=1;
					for(;(int64_t)i>=offset&&cur[-offset].block_id!=cur->jmp_to;offset++);
					FKL_ASSERT((int64_t)i>=offset);

					FklOpcode op=(cur->ins.op>=FKL_OP_JMP_IF_TRUE&&cur->ins.op<=FKL_OP_JMP_IF_FALSE)
						?FKL_OP_JMP_IF_TRUE
						:(cur->ins.op>=FKL_OP_JMP_IF_FALSE&&cur->ins.op<=FKL_OP_JMP_IF_FALSE_XX)
						?FKL_OP_JMP_IF_FALSE
						:FKL_OP_JMP;

					int nl=set_jmp_backward(op,offset,new_ins);
					FklInstructionArg new_arg;
					fklGetInsOpArg(new_ins,&new_arg);

					if(new_arg.ix==arg.ix&&ol==nl)
						goto no_change;
					change=1;

					tmp_ins_ln.ins=new_ins[0];
					push_ins_ln(b,&tmp_ins_ln);
					for(int i=1;i<nl;i++)
						fklByteCodeBufferPush(b,&new_ins[i],cur->line,cur->scope,cur->fid);
				}
done:
				i+=ol;
			}
			else
			{
no_change:
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

#if 0
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
#endif

void fklRecomputeInsImm(FklByteCodelnt* bcl
		,void* ctx
		,FklRecomputeInsImmPredicate predicate
		,FklRecomputeInsImmFunc func)
{
	FklInstructionArg arg;

	FklInstruction cur[4]={FKL_INSTRUCTION_STATIC_INIT};
	FklInstruction new_ins[4]={FKL_INSTRUCTION_STATIC_INIT};

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

			FKL_ASSERT(op!=FKL_OP_EXTRA_ARG);

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
					nl=set_ins_with_signed_imm(new_ins,op,arg.ix);
					break;
				case FKL_OP_MODE_IuB:
				case FKL_OP_MODE_IuC:
				case FKL_OP_MODE_IuBB:
				case FKL_OP_MODE_IuCCB:
					nl=set_ins_with_unsigned_imm(new_ins,op,arg.ux);
					break;
				case FKL_OP_MODE_IuAuB:
				case FKL_OP_MODE_IuCuC:
				case FKL_OP_MODE_IuCAuBB:
				case FKL_OP_MODE_IuCAuBCC:
					nl=set_ins_with_2_unsigned_imm(new_ins,op,arg.ux,arg.uy);
					break;

				case FKL_OP_MODE_I:
				case FKL_OP_MODE_IxAxB:
					abort();
			}

			tmp_ins_ln.ins=new_ins[0];
			push_ins_ln(&new_buf,&tmp_ins_ln);
			for(int i=1;i<nl;i++)
				fklByteCodeBufferPush(&new_buf,&new_ins[i],cur_ins_ln->line,cur_ins_ln->scope,cur_ins_ln->fid);

			cur_ins_ln+=ol;
		}
		else
		{
			push_ins_ln(&new_buf,cur_ins_ln);
			cur_ins_ln++;
		}
	}

	FklByteCodeBuffer* fin=recompute_jmp_target(&new_buf,&buf);
	fklSetByteCodelntWithBuf(bcl,fin);
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

static inline void bytecode_buffer_reserve(FklByteCodeBuffer* buf,size_t target)
{
	if(buf->capacity>=target)
		return;
	size_t target_capacity=buf->capacity<<1;
	if(target_capacity<target)
		target_capacity=target;
	FklInsLn* new_base=(FklInsLn*)fklRealloc(buf->base,target_capacity*sizeof(FklInsLn));
	FKL_ASSERT(new_base);
	buf->base=new_base;
	buf->capacity=target_capacity;
}

void fklSetByteCodeBuffer(FklByteCodeBuffer* buf,const FklByteCodelnt* bcl)
{
	size_t len=bcl->bc->len;
	bytecode_buffer_reserve(buf,len);
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

FklByteCodeBuffer* fklCreateByteCodeBufferWith(const FklByteCodelnt* bcl)
{
	FklByteCodeBuffer* buf=(FklByteCodeBuffer*)malloc(sizeof(FklByteCodeBuffer));
	FKL_ASSERT(buf);
	fklInitByteCodeBufferWith(buf,bcl);
	return buf;
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

uint32_t fklByteCodeBufferScanAndSetBasicBlock(FklByteCodeBuffer* buf)
{
	FKL_ASSERT(buf->size);
	FklInstructionArg arg;
	buf->base[0].block_id=1;
	uint32_t next_block_id=1;
	FklInsLn* base=buf->base;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};

	for(uint64_t i=0;i<buf->size;)
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
				cur[l].block_id=++next_block_id;
			uint64_t jmp_to=i+arg.ix+l;
			FklInsLn* jmp_to_ins=&base[jmp_to];
			if(!jmp_to_ins->block_id)
				jmp_to_ins->block_id=++next_block_id;
			cur->jmp_to=jmp_to_ins->block_id;
			i+=l;
		}
		else if(fklIsPushProcIns(&cur->ins))
		{
			int l=fklGetOpcodeModeLen(cur->ins.op);
			for(int i=0;i<l;i++)
				ins[i]=cur[i].ins;
			fklGetInsOpArg(ins,&arg);
			if(!cur[l].block_id)
				cur[l].block_id=++next_block_id;
			uint64_t jmp_to=i+arg.uy+l;
			FklInsLn* jmp_to_ins=&base[jmp_to];
			if(!jmp_to_ins->block_id)
				jmp_to_ins->block_id=++next_block_id;
			cur->jmp_to=jmp_to_ins->block_id;
			i+=l;
		}
		else
			i++;
	}
	return next_block_id;
}

FklByteCodelnt* fklCreateByteCodelntFromBuf(const FklByteCodeBuffer* buf)
{
	FklByteCodelnt* bcl=fklCreateByteCodelnt(fklCreateByteCode(0));
	fklSetByteCodelntWithBuf(bcl,buf);
	return bcl;
}

static inline void scan_and_set_block_start(FklByteCodeBuffer* buf,uint64_t* block_start)
{
	for(uint64_t i=0;i<buf->size;i++)
		if(buf->base[i].block_id)
			block_start[buf->base[i].block_id-1]=i;
}

#define PEEPHOLE_SIZE (12)

typedef uint32_t (*PeepholeOptimizationPredicate)(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k);
typedef uint32_t (*PeepholeOptimizationOutput)(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k
		,FklInsLn* output);

struct PeepholeOptimizer
{
	PeepholeOptimizationPredicate predicate;
	PeepholeOptimizationOutput output;
};

static uint32_t not3_predicate(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k)
{
	if(k<3)
		return 0;
	if(peephole[0].ins.op==FKL_OP_NOT
			&&peephole[1].ins.op==FKL_OP_NOT
			&&peephole[2].ins.op==FKL_OP_NOT)
		return 3;
	return 0;
}

static uint32_t not3_output(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k
		,FklInsLn* output)
{
	output[0]=peephole[0];
	return 1;
}

static uint32_t inc_or_dec_loc_predicate(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k)
{
	if(k<3)
		return 0;
	uint32_t i=0;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};
	FklInstructionArg arg;
	i+=set_ins_ln_to_ins(&peephole[i],ins);
	uint32_t loc_idx;
	if(ins[0].op<FKL_OP_GET_LOC||ins[0].op>FKL_OP_GET_LOC_X)
		return 0;
	fklGetInsOpArg(ins,&arg);
	loc_idx=arg.ux;
	if(peephole[i].ins.op!=FKL_OP_INC&&peephole[i].ins.op!=FKL_OP_DEC)
		return 0;
	i++;
	i+=set_ins_ln_to_ins(&peephole[i],ins);
	if(ins[0].op<FKL_OP_PUT_LOC||ins[0].op>FKL_OP_PUT_LOC_X)
		return 0;
	fklGetInsOpArg(ins,&arg);
	if(loc_idx!=arg.ux)
		return 0;
	return i;
}

static uint32_t inc_or_dec_loc_output(const FklByteCodeBuffer* buf
		,const uint64_t* block_start
		,const FklInsLn* peephole
		,uint32_t k
		,FklInsLn* output)
{
	uint32_t i=0;
	FklInstruction ins[4]={FKL_INSTRUCTION_STATIC_INIT};
	FklInstructionArg arg;
	i+=set_ins_ln_to_ins(&peephole[i],ins);
	fklGetInsOpArg(ins,&arg);
	uint32_t loc_idx=arg.ux;
	FklOpcode op=peephole[i].ins.op==FKL_OP_INC?FKL_OP_INC_LOC:FKL_OP_DEC_LOC;
	uint32_t nl=set_ins_with_unsigned_imm(ins,op,loc_idx);

	for(uint32_t i=0;i<nl;i++)
	{
		output[i]=peephole[i];
		output[i].ins=ins[i];
	}

	return nl;
}

static const struct PeepholeOptimizer PeepholeOptimizers[]=
{
	{not3_predicate, not3_output, },
	{inc_or_dec_loc_predicate,inc_or_dec_loc_output,},
	{NULL,           NULL,        },
};

static inline int do_peephole_optimize(FklByteCodeBuffer* buf
		,const uint64_t* block_start)
{
	FklInsLn peephole[PEEPHOLE_SIZE];
	FklInsLn output[PEEPHOLE_SIZE];
	uint64_t valid_ins_idx[PEEPHOLE_SIZE];

	int change=0;

	for(uint64_t i=0;i<buf->size;)
	{
		uint64_t j=0;
		uint64_t k=0;

		for(;j+i<buf->size&&k<PEEPHOLE_SIZE;j++)
			if(buf->base[i+j].ins.op)
			{
				peephole[k]=buf->base[i+j];
				valid_ins_idx[k++]=j;
			}

		uint32_t offset=0;
		for(const struct PeepholeOptimizer* optimizer=&PeepholeOptimizers[0]
				;optimizer->predicate
				;optimizer++)
		{
			offset=optimizer->predicate(buf,block_start,peephole,k);
			if(offset)
			{
				FklInsLn* output_start=&buf->base[i];
				uint32_t output_len=optimizer->output(buf,block_start,peephole,k,output);
				uint32_t ii=0;
				for(;ii<output_len;ii++)
					output_start[ii]=output[ii];
				if(offset<k)
				{
					for(;ii<valid_ins_idx[offset];ii++)
						output_start[ii].ins.op=FKL_OP_DUMMY;
					for(i+=valid_ins_idx[offset];i<buf->size&&buf->base[i].ins.op==FKL_OP_EXTRA_ARG;i++);
				}
				else
				{
					for(;i+ii<buf->size;ii++)
						output_start[ii].ins.op=FKL_OP_DUMMY;
					i=buf->size;
				}
				change=1;
				goto done;
			}
		}
		
		for(i+=(valid_ins_idx[0]+1);i<buf->size&&buf->base[i].ins.op==FKL_OP_EXTRA_ARG;i++);
done:
		continue;
	}
	return change;
}

static inline FklByteCodeBuffer* remove_dummy_code(FklByteCodeBuffer* a,FklByteCodeBuffer* b)
{
	for(uint64_t i=0;i<a->size;i++)
	{
		FklInsLn* cur=&a->base[i];
		if(cur->ins.op)
			push_ins_ln(b,cur);
	}

	return recompute_jmp_target(b,a);
}

void fklPeepholeOptimize(FklByteCodelnt* bcl)
{
	FklByteCodeBuffer buf;
	fklInitByteCodeBufferWith(&buf,bcl);

	uint32_t block_num=fklByteCodeBufferScanAndSetBasicBlock(&buf);
	uint64_t* block_start=(uint64_t*)malloc(block_num*sizeof(uint64_t));
	FKL_ASSERT(block_start);
	scan_and_set_block_start(&buf,block_start);

	int change;
	do
	{
		change=do_peephole_optimize(&buf,block_start);
	}while(change);

	FklByteCodeBuffer new_buf;
	fklInitByteCodeBuffer(&new_buf,buf.size);

	FklByteCodeBuffer* fin=remove_dummy_code(&buf,&new_buf);

	fklSetByteCodelntWithBuf(bcl,fin);
	fklUninitByteCodeBuffer(&buf);
	fklUninitByteCodeBuffer(&new_buf);

	free(block_start);
}

