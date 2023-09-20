#include<fakeLisp/bytecode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/symbol.h>
#include<string.h>
#include<stdlib.h>

FklByteCode* fklCreateByteCode(size_t len)
{
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->len=len;
	tmp->code=len?(FklInstruction*)malloc(len*sizeof(FklInstruction)):NULL;
	FKL_ASSERT(tmp->code||!len);
	return tmp;
}

FklByteCodelnt* fklCreateByteCodelnt(FklByteCode* bc)
{
	FklByteCodelnt* t=(FklByteCodelnt*)malloc(sizeof(FklByteCodelnt));
	FKL_ASSERT(t);
	t->ls=0;
	t->l=NULL;
	t->bc=bc;
	return t;
}

inline FklByteCodelnt* fklCreateSingleInsBclnt(FklInstruction ins
		,FklSid_t fid
		,uint64_t line)
{
	FklByteCode* bc=fklCreateByteCode(1);
	bc->code[0]=ins;
	FklByteCodelnt* r=fklCreateByteCodelnt(bc);
	r->ls=1;
	r->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*1);
	FKL_ASSERT(r->l);
	fklInitLineNumTabNode(&r->l[0],fid,0,bc->len,line);
	return r;
}

void fklDestroyByteCodelnt(FklByteCodelnt* t)
{
	fklDestroyByteCode(t->bc);
	if(t->l)
		free(t->l);
	free(t);
}

void fklDestroyByteCode(FklByteCode* obj)
{
	free(obj->code);
	free(obj);
}

void fklCodeConcat(FklByteCode* fir,const FklByteCode* sec)
{
	uint32_t len=fir->len;
	fir->len=sec->len+fir->len;
	fir->code=(FklInstruction*)fklRealloc(fir->code,sizeof(FklInstruction)*fir->len);
	FKL_ASSERT(fir->code||!fir->len);
	memcpy(&fir->code[len],sec->code,sizeof(FklInstruction)*sec->len);
}

void fklCodeReverseConcat(const FklByteCode* fir,FklByteCode* sec)
{
	uint32_t len=fir->len;
	FklInstruction* tmp=(FklInstruction*)malloc(sizeof(FklInstruction)*(fir->len+sec->len));
	FKL_ASSERT(tmp);
	memcpy(tmp,fir->code,sizeof(FklInstruction)*fir->len);
	memcpy(&tmp[len],sec->code,sizeof(FklInstruction)*sec->len);
	free(sec->code);
	sec->code=tmp;
	sec->len=fir->len+sec->len;
}

FklByteCode* fklCopyByteCode(const FklByteCode* obj)
{
	FklByteCode* tmp=fklCreateByteCode(obj->len);
	memcpy(tmp->code,obj->code,sizeof(FklInstruction)*obj->len);
	return tmp;
}

FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt* obj)
{
	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCopyByteCode(obj->bc));
	tmp->ls=obj->ls;
	tmp->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*obj->ls);
	FKL_ASSERT(tmp->l);
	for(size_t i=0;i<obj->ls;i++)
		tmp->l[i]=obj->l[i];
	return tmp;
}

typedef struct ByteCodePrintState
{
	uint32_t tc;
	uint64_t cp;
	uint64_t cpc;
}ByteCodePrintState;

static ByteCodePrintState* createByteCodePrintState(uint32_t tc,uint64_t cp,uint64_t cpc)
{
	ByteCodePrintState* t=(ByteCodePrintState*)malloc(sizeof(ByteCodePrintState));
	FKL_ASSERT(t);
	t->tc=tc;
	t->cp=cp;
	t->cpc=cpc;
	return t;
}

static inline uint32_t printSingleByteCode(const FklByteCode* tmpCode
		,uint64_t i
		,FILE* fp
		,ByteCodePrintState* cState
		,FklPtrStack* s
		,int* needBreak
		,FklSymbolTable* table
		,const char* format)
{
	uint32_t tab_count=cState->tc;
	uint32_t proc_len=0;
	fprintf(fp,format,i);
	putc(':',fp);
	for(uint32_t i=0;i<tab_count;i++)
		fputs("    ",fp);
	const FklInstruction* ins=&tmpCode->code[i];
	FklOpcode op=ins->op;
	fprintf(fp," %s",fklGetOpcodeName(op));
	int opcodeArgLen=fklGetOpcodeArgLen(op);
	if(opcodeArgLen)
		fputc(' ',fp);
	switch(opcodeArgLen)
	{
		case -1:
			{
				switch(op)
				{
					case FKL_OP_PUSH_PROC:
						{
							fprintf(fp,"%u ",ins->imm);
							fprintf(fp,"%lu",ins->imm_u64);
							fklPushPtrStack(createByteCodePrintState(cState->tc,i+ins->imm_u64,cState->cpc),s);
							fklPushPtrStack(createByteCodePrintState(tab_count+1,i+1,i+ins->imm_u64),s);
							proc_len=ins->imm_u64;
							*needBreak=1;
						}
						break;
					case FKL_OP_PUSH_STR:
						{
							const FklString* str=ins->str;
							fprintf(fp,"%lu "
									,str->size);
							fklPrintRawString(str,fp);
						}
						break;
					case FKL_OP_PUSH_BYTEVECTOR:
						{
							const FklBytevector* bvec=ins->bvec;
							fprintf(fp,"%lu ",bvec->size);
							fklPrintRawBytevector(bvec,fp);
						}
						break;
					case FKL_OP_PUSH_BIG_INT:
						{
							const FklBigInt* bi=ins->bi;
							fklPrintBigInt(bi,fp);
						}
						break;
					default:
						FKL_ASSERT(0);
						break;
				}
			}
			break;
		case 0:
			break;
		case 1:
			{
				if(op==FKL_OP_PUSH_CHAR)
					fklPrintRawChar(ins->chr,fp);
				else
					fprintf(fp,"%d",ins->imm_i8);
			}
			break;
		case 2:
			fprintf(fp,"%d",ins->imm_i16);
			break;
		case 4:
			fprintf(fp,"%d",ins->imm_i32);
			break;
		case 8:
			switch(op)
			{
				case FKL_OP_PUSH_F64:
					fprintf(fp,"%lf",ins->f64);
					break;
				case FKL_OP_PUSH_VECTOR:
				case FKL_OP_PUSH_HASHTABLE_EQ:
				case FKL_OP_PUSH_HASHTABLE_EQV:
				case FKL_OP_PUSH_HASHTABLE_EQUAL:
				case FKL_OP_PUSH_LIST:
					fprintf(fp,"%lu",ins->imm_u64);
					break;
				case FKL_OP_PUSH_I64:
				case FKL_OP_JMP:
				case FKL_OP_JMP_IF_FALSE:
				case FKL_OP_JMP_IF_TRUE:
					fprintf(fp,"%ld",ins->imm_i64);
					break;
				case FKL_OP_PUSH_SYM:
					fklPrintRawSymbol(fklGetSymbolWithId(ins->sid,table)->symbol,fp);
					break;
				case FKL_OP_IMPORT:
				case FKL_OP_CLOSE_REF:
					fprintf(fp,"%u %u",ins->imm,ins->imm_u32);
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
			break;
	}
	return proc_len+1;
}

#include<math.h>
void fklPrintByteCode(const FklByteCode* tmpCode
		,FILE* fp
		,FklSymbolTable* table)
{
	FklPtrStack s=FKL_STACK_INIT;
	fklInitPtrStack(&s,32,16);
	fklPushPtrStack(createByteCodePrintState(0,0,tmpCode->len),&s);
	uint64_t codeLen=tmpCode->len;
	int numLen=codeLen?(int)(log10(codeLen)+1):1;
	char format[FKL_MAX_STRING_SIZE]="";
	snprintf(format,FKL_MAX_STRING_SIZE,"%%-%dld",numLen);

	if(!fklIsPtrStackEmpty(&s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(&s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
		}
		free(cState);
	}
	while(!fklIsPtrStackEmpty(&s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(&s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
		}
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
		}
		free(cState);
	}
	fklUninitPtrStack(&s);
}

static uint64_t skipToCall(uint64_t index,const FklByteCode* bc)
{
	uint64_t r=0;
	const FklInstruction* ins=NULL;
	while(index+r<bc->len&&(ins=&bc->code[index+r])->op!=FKL_OP_CALL)
	{
		if(ins->op==FKL_OP_PUSH_PROC)
			r+=ins->imm_u64;
		else
			r++;
	}
	return r;
}

static inline int64_t get_next(uint64_t i,FklInstruction* code)
{
	if(code[i].op==FKL_OP_JMP)
		return code[i].imm_i64+1;
	return 1;
}

static int fklIsTheLastExpression(uint64_t index,FklByteCode* bc)
{
	uint64_t size=bc->len;
	FklInstruction* code=bc->code;
	for(uint64_t i=index;i<size;i+=get_next(i,code))
		if(code[i].op!=FKL_OP_JMP&&code[i].op!=FKL_OP_CLOSE_REF&&code[i].op!=FKL_OP_RET)
			return 0;
	return 1;
}

void fklScanAndSetTailCall(FklByteCode* bc)
{
	for(uint64_t i=skipToCall(0,bc);i<bc->len;i+=skipToCall(i,bc))
	{
		if(fklIsTheLastExpression(++i,bc))
			bc->code[i-1].op=FKL_OP_TAIL_CALL;
	}
}

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,FklSymbolTable* table)
{
	FklByteCode* tmpCode=obj->bc;
	FklPtrStack s=FKL_STACK_INIT;
	fklInitPtrStack(&s,32,16);
	fklPushPtrStack(createByteCodePrintState(0,0,tmpCode->len),&s);
	uint64_t j=0;
	FklSid_t fid=0;
	uint64_t line=0;

	uint64_t codeLen=tmpCode->len;
	int numLen=codeLen?(int)(log10(codeLen)+1):1;
	char format[FKL_MAX_STRING_SIZE]="";
	snprintf(format,FKL_MAX_STRING_SIZE,"%%-%dld",numLen);

	if(!fklIsPtrStackEmpty(&s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(&s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
			if(obj->l[j].scp+obj->l[j].cpc<i)
				j++;
			if(obj->l[j].fid!=fid||obj->l[j].line!=line)
			{
				fid=obj->l[j].fid;
				line=obj->l[j].line;
				if(fid)
				{
					fprintf(fp,"    ;");
					fklPrintString(fklGetSymbolWithId(obj->l[j].fid,table)->symbol,fp);
					fprintf(fp,":%u:%lu",obj->l[j].line,obj->l[j].cpc);
				}
				else
					fprintf(fp,"\t%u:%lu",obj->l[j].line,obj->l[j].cpc);
			}
		}
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
			if(obj->l[j].scp+obj->l[j].cpc<i)
				j++;
			if(obj->l[j].fid!=fid||obj->l[j].line!=line)
			{
				fid=obj->l[j].fid;
				line=obj->l[j].line;
				if(fid)
				{
					fprintf(fp,"    ;");
					fklPrintString(fklGetSymbolWithId(obj->l[j].fid,table)->symbol,fp);
					fprintf(fp,":%u:%lu",obj->l[j].line,obj->l[j].cpc);
				}
				else
					fprintf(fp,"\t%u:%lu",obj->l[j].line,obj->l[j].cpc);
			}
		}
		free(cState);
	}
	while(!fklIsPtrStackEmpty(&s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(&s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
			if(obj->l[j].scp+obj->l[j].cpc<i)
				j++;
			if(obj->l[j].fid!=fid||obj->l[j].line!=line)
			{
				fid=obj->l[j].fid;
				line=obj->l[j].line;
				if(fid)
				{
					fprintf(fp,"    ;");
					fklPrintString(fklGetSymbolWithId(obj->l[j].fid,table)->symbol,fp);
					fprintf(fp,":%u:%lu",obj->l[j].line,obj->l[j].cpc);
				}
				else
					fprintf(fp,"\t%u:%lu",obj->l[j].line,obj->l[j].cpc);
			}
		}
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,&s,&needBreak,table,format);
			if(obj->l[j].scp+obj->l[j].cpc<i)
				j++;
			if(obj->l[j].fid!=fid||obj->l[j].line!=line)
			{
				fid=obj->l[j].fid;
				line=obj->l[j].line;
				if(fid)
				{
					fprintf(fp,"    ;");
					fklPrintString(fklGetSymbolWithId(obj->l[j].fid,table)->symbol,fp);
					fprintf(fp,":%u:%lu",obj->l[j].line,obj->l[j].cpc);
				}
				else
					fprintf(fp,"\t%u:%lu",obj->l[j].line,obj->l[j].cpc);
			}
			if(needBreak)
				break;
		}
		free(cState);
	}
	fklUninitPtrStack(&s);
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt* o,uint64_t size)
{
	uint32_t i=0;
	for(;i<o->ls;i++)
		o->l[i].scp+=size;
}

#define FKL_INCREASE_ALL_SCP(l,ls,s) for(size_t i=0;i<(ls);i++)(l)[i].scp+=(s)
void fklCodeLntConcat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(s->bc->len)
	{
		if(!f->l)
		{
			f->ls=s->ls;
			f->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*s->ls);
			FKL_ASSERT(f->l);
			s->l[0].cpc+=f->bc->len;
			FKL_INCREASE_ALL_SCP(s->l+1,s->ls-1,f->bc->len);
			memcpy(f->l,s->l,(s->ls)*sizeof(FklLineNumberTableItem));
		}
		else
		{
			FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->len);
			if(f->l[f->ls-1].line==s->l[0].line&&f->l[f->ls-1].fid==s->l[0].fid)
			{
				f->l[f->ls-1].cpc+=s->l[0].cpc;
				f->l=(FklLineNumberTableItem*)fklRealloc(f->l,sizeof(FklLineNumberTableItem)*(f->ls+s->ls-1));
				FKL_ASSERT(f->l);
				memcpy(&f->l[f->ls],&s->l[1],(s->ls-1)*sizeof(FklLineNumberTableItem));
				f->ls+=s->ls-1;
			}
			else
			{
				f->l=(FklLineNumberTableItem*)fklRealloc(f->l,sizeof(FklLineNumberTableItem)*(f->ls+s->ls));
				FKL_ASSERT(f->l);
				memcpy(&f->l[f->ls],s->l,(s->ls)*sizeof(FklLineNumberTableItem));
				f->ls+=s->ls;
			}
		}
		fklCodeConcat(f->bc,s->bc);
	}
}

void fklCodeLntReverseConcat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(f->bc->len)
	{
		if(!s->l)
		{
			s->ls=f->ls;
			s->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*f->ls);
			FKL_ASSERT(s->l);
			f->l[f->ls-1].cpc+=s->bc->len;
			memcpy(s->l,f->l,(f->ls)*sizeof(FklLineNumberTableItem));
		}
		else
		{
			FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->len);
			if(f->l[f->ls-1].line==s->l[0].line&&f->l[f->ls-1].fid==s->l[0].fid)
			{
				FklLineNumberTableItem* l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*(f->ls+s->ls-1));
				FKL_ASSERT(l);
				f->l[f->ls-1].cpc+=s->l[0].cpc;
				memcpy(l,f->l,(f->ls)*sizeof(FklLineNumberTableItem));
				memcpy(&l[f->ls],&s->l[1],(s->ls-1)*sizeof(FklLineNumberTableItem));
				free(s->l);
				s->l=l;
				s->ls+=f->ls-1;
			}
			else
			{
				FklLineNumberTableItem* l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*(f->ls+s->ls));
				FKL_ASSERT(l);
				memcpy(l,f->l,(f->ls)*sizeof(FklLineNumberTableItem));
				memcpy(&l[f->ls],s->l,(s->ls)*sizeof(FklLineNumberTableItem));
				free(s->l);
				s->l=l;
				s->ls+=f->ls;
			}
		}
		fklCodeReverseConcat(f->bc,s->bc);
	}
}

void fklByteCodeInsertFront(FklInstruction ins,FklByteCode* bc)
{
	FklInstruction* code=(FklInstruction*)malloc(sizeof(FklInstruction)*(bc->len+1));
	FKL_ASSERT(code);
	memcpy(&code[1],bc->code,sizeof(FklInstruction)*bc->len);
	free(bc->code);
	code[0]=ins;
	bc->len++;
	bc->code=code;
}

void fklByteCodePushBack(FklByteCode* bc,FklInstruction ins)
{
	bc->code=(FklInstruction*)fklRealloc(bc->code,sizeof(FklInstruction)*(bc->len+1));
	FKL_ASSERT(bc->code);
	bc->code[bc->len]=ins;
	bc->len++;
}

void fklBclInsAppendToBcl(FklByteCodelnt* bcl
		,const FklInstruction* ins
		,FklSid_t fid
		,uint64_t line)
{
	if(!bcl->l)
	{
		bcl->ls=1;
		bcl->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*1);
		FKL_ASSERT(bcl->l);
		fklInitLineNumTabNode(&bcl->l[0],fid,0,1,line);
		fklByteCodePushBack(bcl->bc,*ins);
	}
	else
	{
		fklByteCodePushBack(bcl->bc,*ins);
		bcl->l[bcl->ls-1].cpc+=1;
	}
}

void fklInsBclAppendToBcl(const FklInstruction* ins
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line)
{
	if(!bcl->l)
	{
		bcl->ls=1;
		bcl->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*1);
		FKL_ASSERT(bcl->l);
		fklInitLineNumTabNode(&bcl->l[0],fid,0,1,line);
		fklByteCodePushBack(bcl->bc,*ins);
	}
	else
	{
		fklByteCodeInsertFront(*ins,bcl->bc);
		bcl->l[0].cpc+=1;
		FKL_INCREASE_ALL_SCP(bcl->l+1,bcl->ls-1,1);
	}
}

void fklInitLineNumTabNode(FklLineNumberTableItem* n
		,FklSid_t fid
		,uint64_t scp
		,uint64_t cpc
		,uint64_t line)
{
	n->fid=fid;
	n->scp=scp;
	n->cpc=cpc;
	n->line=line;
}

FklLineNumberTableItem* fklCreateLineNumTabNode(FklSid_t fid,uint64_t scp,uint64_t cpc,uint64_t line)
{
	FklLineNumberTableItem* t=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem));
	FKL_ASSERT(t);
	fklInitLineNumTabNode(t,fid,scp,cpc,line);
	return t;
}

FklLineNumberTableItem* fklCreateLineNumTabNodeWithFilename(const char* filename
		,uint64_t scp
		,uint64_t cpc
		,uint32_t line
		,FklSymbolTable* table)
{
	FklLineNumberTableItem* t=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem));
	FKL_ASSERT(t);
	t->fid=filename?fklAddSymbolCstr(filename,table)->id:0;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

inline FklLineNumberTableItem* fklFindLineNumTabNode(uint64_t cp,size_t ls,FklLineNumberTableItem* line_numbers)
{
	int64_t l=0;
	int64_t h=ls-1;
	int64_t mid=0;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		FklLineNumberTableItem* cur=&line_numbers[mid];
		if(cp<cur->scp)
			h=mid-1;
		else if(cp>=(cur->scp+cur->cpc))
			l=mid+1;
		else
			return cur;
	}
	return NULL;
}

void fklWriteLineNumberTable(FklLineNumberTableItem* line_numbers,uint32_t size,FILE* fp)
{
	fwrite(&size,sizeof(size),1,fp);
	for(uint32_t i=0;i<size;i++)
	{
		FklLineNumberTableItem* n=&line_numbers[i];
		fwrite(&n->fid,sizeof(n->fid),1,fp);
		fwrite(&n->scp,sizeof(n->scp),1,fp);
		fwrite(&n->cpc,sizeof(n->cpc),1,fp);
		fwrite(&n->line,sizeof(n->line),1,fp);
	}
}

void fklDBG_printByteCode(FklInstruction* code,uint64_t s,uint64_t c,FILE* fp)
{
	FklByteCode t={c,code+s};
	fklPrintByteCode(&t,fp,NULL);
}

inline int16_t fklGetI16FromByteCode(uint8_t* code)
{
	int16_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	return i;
}

inline int32_t fklGetI32FromByteCode(uint8_t* code)
{
	int32_t i;
	memcpy(&i,code,sizeof(i));
	return i;
}

inline uint32_t fklGetU32FromByteCode(uint8_t* code)
{
	uint32_t i;
	memcpy(&i,code,sizeof(i));
	return i;
}

inline int64_t fklGetI64FromByteCode(uint8_t* code)
{
	int64_t i;
	memcpy(&i,code,sizeof(i));
	return i;
}

inline uint64_t fklGetU64FromByteCode(uint8_t* code)
{
	uint64_t i;
	memcpy(&i,code,sizeof(i));
	return i;
}

inline double fklGetF64FromByteCode(uint8_t* code)
{
	double d;
	memcpy(&d,code,sizeof(d));
	return d;
}

inline FklSid_t fklGetSidFromByteCode(uint8_t* code)
{
	FklSid_t i;
	memcpy(&i,code,sizeof(i));
	return i;
}

inline void fklSetI8ToByteCode(uint8_t* code,int8_t i)
{
	code[0]=i;
}

inline void fklSetI16ToByteCode(uint8_t* code,int16_t i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetI32ToByteCode(uint8_t* code,int32_t i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetU32ToByteCode(uint8_t* code,uint32_t i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetI64ToByteCode(uint8_t* code,int64_t i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetU64ToByteCode(uint8_t* code,uint64_t i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetF64ToByteCode(uint8_t* code,double i)
{
	memcpy(code,&i,sizeof(i));
}

inline void fklSetSidToByteCode(uint8_t* code,FklSid_t i)
{
	memcpy(code,&i,sizeof(i));
}

#define NEW_PUSH_FIX_OBJ_BYTECODE(OPCODE,OBJ,FIELD) return (FklInstruction){.op=OPCODE,.FIELD=OBJ}

FklInstruction fklCreatePushI8Ins(int8_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I8,a,imm_i8);}
FklInstruction fklCreatePushI16Ins(int16_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I16,a,imm_i16);}
FklInstruction fklCreatePushI32Ins(int32_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I32,a,imm_i32);}
FklInstruction fklCreatePushI64Ins(int64_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I64,a,imm_i64);}
FklInstruction fklCreatePushSidIns(FklSid_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_SYM,a,sid);}
FklInstruction fklCreatePushCharIns(char a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_CHAR,a,chr);}
FklInstruction fklCreatePushF64Ins(double a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_F64,a,f64);}
FklInstruction fklCreatePushStrIns(const FklString* str)
{
	NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_STR,fklCopyString(str),str);
}

FklInstruction fklCreatePushBvecIns(const FklBytevector* bvec)
{
	NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_BYTEVECTOR,fklCopyBytevector(bvec),bvec);
}

FklInstruction fklCreatePushBigIntIns(const FklBigInt* bigInt)
{
	FklInstruction r;
	if(fklIsGtLtI64BigInt(bigInt))
	{
		r.op=FKL_OP_PUSH_BIG_INT;
		r.bi=fklCopyBigInt(bigInt);
	}
	else
	{
		r.op=FKL_OP_PUSH_I64_BIG;
		r.imm_i64=fklBigIntToI64(bigInt);
	}
	return r;
}

void fklLoadLineNumberTable(FILE* fp,FklLineNumberTableItem** plist,uint32_t* pnum)
{
	size_t size=0;
	fread(&size,sizeof(uint32_t),1,fp);
	FklLineNumberTableItem* list=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*size);
	FKL_ASSERT(list||!size);
	for(size_t i=0;i<size;i++)
	{
		FklSid_t fid=0;
		uint64_t scp=0;
		uint64_t cpc=0;
		uint32_t line=0;
		fread(&fid,sizeof(fid),1,fp);
		fread(&scp,sizeof(scp),1,fp);
		fread(&cpc,sizeof(cpc),1,fp);
		fread(&line,sizeof(line),1,fp);
		fklInitLineNumTabNode(&list[i],fid,scp,cpc,line);
	}
	*plist=list;
	*pnum=size;
}
