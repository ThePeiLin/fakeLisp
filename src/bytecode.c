#include<fakeLisp/bytecode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/symbol.h>
#include<string.h>
#include<stdlib.h>

FklByteCode* fklCreateByteCode(size_t size)
{
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(size*sizeof(uint8_t));
	FKL_ASSERT(tmp->code);
	return tmp;
}

FklByteCode* fklCreateByteCodeAndInit(size_t size,const uint8_t* ptr)
{
	FklByteCode* tmp=(FklByteCode*)malloc(sizeof(FklByteCode));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->code=(uint8_t*)malloc(size*sizeof(uint8_t));
	FKL_ASSERT(tmp->code);
	memcpy(tmp->code,ptr,size);
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

void fklDestroyLineNumTabNodeArray(FklLineNumTabNode** a,size_t num)
{
	for(size_t i=0;i<num;i++)
		fklDestroyLineNumTabNode(a[i]);
	free(a);
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

void fklCodeCat(FklByteCode* fir,const FklByteCode* sec)
{
	int32_t size=fir->size;
	fir->size=sec->size+fir->size;
	fir->code=(uint8_t*)realloc(fir->code,sizeof(uint8_t)*fir->size);
	FKL_ASSERT(fir->code||!fir->size);
	memcpy(fir->code+size,sec->code,sec->size);
}

void fklReCodeCat(const FklByteCode* fir,FklByteCode* sec)
{
	int32_t size=fir->size;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*(fir->size+sec->size));
	FKL_ASSERT(tmp);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

FklByteCode* fklCopyByteCode(const FklByteCode* obj)
{
	FklByteCode* tmp=fklCreateByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt* obj)
{
	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCopyByteCode(obj->bc));
	tmp->ls=obj->ls;
	tmp->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*obj->ls);
	FKL_ASSERT(tmp->l);
	int32_t i=0;
	for(;i<obj->ls;i++)
		tmp->l[i]=obj->l[i];
	return tmp;
}

typedef enum{BP_NONE,BP_ERROR_HANLER} BPtype;

typedef struct ByteCodePrintState
{
	BPtype type;
	uint32_t tc;
	uint64_t cp;
	uint64_t cpc;
}ByteCodePrintState;

static ByteCodePrintState* createByteCodePrintState(BPtype type,uint32_t tc,uint64_t cp,uint64_t cpc)
{
	ByteCodePrintState* t=(ByteCodePrintState*)malloc(sizeof(ByteCodePrintState));
	FKL_ASSERT(t);
	t->type=type;
	t->tc=tc;
	t->cp=cp;
	t->cpc=cpc;
	return t;
}
static inline uint64_t printErrorHandlerHead(const FklByteCode* tmpCode,uint64_t i,FILE* fp,FklSymbolTable* table)
{
	uint64_t r=0;
	uint32_t errTypeNum=fklGetU32FromByteCode(tmpCode->code+i);
	i+=sizeof(uint32_t);
	r+=sizeof(uint32_t);
	fputc('(',fp);
	for(uint32_t k=0;k<errTypeNum;k++)
	{
		FklSid_t type=fklGetSidFromByteCode(tmpCode->code+i);
		fklPrintString(fklGetSymbolWithId(type,table)->symbol,fp);
		i+=sizeof(FklSid_t);
		r+=sizeof(FklSid_t);
		if(k+1<errTypeNum)
			fputc(' ',fp);
	}
	fputc(')',fp);
	uint64_t pCpc=fklGetU64FromByteCode(tmpCode->code+i);
	fprintf(fp," %ld\n",pCpc);
	i+=sizeof(uint64_t);
	r+=sizeof(uint64_t);
	return r;
}

static inline uint32_t printSingleByteCode(const FklByteCode* tmpCode
		,uint64_t i
		,FILE* fp
		,ByteCodePrintState* cState
		,FklPtrStack* s
		,int* needBreak
		,FklSymbolTable* table)
{
	uint32_t tc=cState->tc;
	uint32_t r=0;
	if(cState->type==BP_ERROR_HANLER)
	{
		fprintf(fp,"EH:");
		for(uint32_t i=0;i<tc-1;i++)
			fputc('\t',fp);
		r+=printErrorHandlerHead(tmpCode,i,fp,table);
		i+=r;
		cState->type=BP_NONE;
	}
	fprintf(fp,"%ld:",i);
	for(uint32_t i=0;i<tc;i++)
		fputc('\t',fp);
	fprintf(fp," %s",fklGetOpcodeName((FklOpcode)(tmpCode->code[i])));
	int opcodeArgLen=fklGetOpcodeArgLen((FklOpcode)(tmpCode->code[i]));
	if(opcodeArgLen)
		fputc(' ',fp);
	switch(opcodeArgLen)
	{
		case -1:
			{
				FklOpcode op=tmpCode->code[i];
				switch(op)
				{
					case FKL_OP_POP_VAR:
						fprintf(fp,"%d ",fklGetU32FromByteCode(tmpCode->code+i+sizeof(char)));
						fklPrintString(fklGetSymbolWithId(fklGetSidFromByteCode(tmpCode->code+i+sizeof(char)+sizeof(int32_t)),table)->symbol,fp);
						r+=sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t);
						break;
					case FKL_OP_PUSH_PROC:
						{
							uint64_t ncpc=fklGetU64FromByteCode(tmpCode->code+i+sizeof(char));
							fprintf(fp,"%lu",ncpc);
							fklPushPtrStack(createByteCodePrintState(cState->type,cState->tc,i+sizeof(char)+sizeof(uint64_t)+ncpc,cState->cpc),s);
							fklPushPtrStack(createByteCodePrintState(BP_NONE,tc+1,i+sizeof(char)+sizeof(uint64_t),i+sizeof(char)+sizeof(uint64_t)+ncpc),s);
							r+=sizeof(char)+sizeof(uint64_t);
							*needBreak=1;
						}
						break;
					case FKL_OP_PUSH_TRY:
						{
							FklSid_t errSymId=fklGetSidFromByteCode(tmpCode->code+(++i));
							fklPrintString(fklGetSymbolWithId(errSymId,table)->symbol,fp);
							i+=sizeof(FklSid_t);
							uint32_t handlerNum=fklGetU32FromByteCode(tmpCode->code+i);
							fprintf(fp," %d",handlerNum);
							i+=sizeof(uint32_t);
							int j=0;
							r+=sizeof(FklSid_t)+sizeof(uint32_t);
							FklPtrStack* tmpStack=fklCreatePtrStack(32,16);
							for(;j<handlerNum;j++)
							{
								uint64_t ti=i;
								uint32_t errTypeNum=fklGetU32FromByteCode(tmpCode->code+i);
								i+=sizeof(uint32_t);
								r+=sizeof(uint32_t);
								for(uint32_t k=0;k<errTypeNum;k++)
								{
									i+=sizeof(FklSid_t);
									r+=sizeof(FklSid_t);
								}
								uint64_t pCpc=fklGetU64FromByteCode(tmpCode->code+i);
								i+=sizeof(uint64_t);
								r+=sizeof(uint64_t);
								i+=pCpc;
								r+=pCpc;
								fklPushPtrStack(createByteCodePrintState(BP_ERROR_HANLER,cState->tc+1,ti,i),tmpStack);
							}
							fklPushPtrStack(createByteCodePrintState(cState->type,cState->tc,i,cState->cpc),s);
							while(!fklIsPtrStackEmpty(tmpStack))
								fklPushPtrStack(fklPopPtrStack(tmpStack),s);
							fklDestroyPtrStack(tmpStack);
							*needBreak=1;
						}
						break;
					case FKL_OP_PUSH_STR:
						{
							fprintf(fp,"%lu "
									,fklGetU64FromByteCode(tmpCode->code+i+sizeof(char)));
							fklPrintRawCharBuf((char*)tmpCode->code+i+sizeof(char)+sizeof(uint64_t)
									,'"'
									,fklGetU64FromByteCode(tmpCode->code+i+sizeof(char))
									,fp);
							r+=sizeof(char)
								+sizeof(uint64_t)
								+fklGetU64FromByteCode(tmpCode->code+i+sizeof(char));
						}
						break;
					case FKL_OP_PUSH_BYTEVECTOR:
						{
							fprintf(fp,"%lu "
									,fklGetU64FromByteCode(tmpCode->code+i+sizeof(char)));
							fklPrintRawByteBuf(tmpCode->code+i+sizeof(char)+sizeof(uint64_t),fklGetU64FromByteCode(tmpCode->code+i+sizeof(char)),fp);
							r+=sizeof(char)
								+sizeof(uint64_t)
								+fklGetU64FromByteCode(tmpCode->code+i+sizeof(char));
						}
						break;
					case FKL_OP_PUSH_BIG_INT:
						{
							uint64_t num=fklGetU64FromByteCode(tmpCode->code+i+sizeof(char));
							FklBigInt* bi=fklCreateBigIntFromMem(tmpCode->code+i+sizeof(char)+sizeof(num),num);
							fklPrintBigInt(bi,fp);
							r+=sizeof(char)+sizeof(bi->num)+sizeof(uint8_t)*num;
							fklDestroyBigInt(bi);
						}
						break;
					default:
						FKL_ASSERT(0);
						break;
				}
			}
			break;
		case 0:
			r+=sizeof(char);
			break;
		case 1:
			fklPrintRawChar(tmpCode->code[i+1],fp);
			r+=sizeof(char)+sizeof(char);
			break;
		case 4:
			if(tmpCode->code[i]==FKL_OP_PUSH_SYM)
				fklPrintString(fklGetSymbolWithId(fklGetSidFromByteCode(tmpCode->code+i+sizeof(char)),table)->symbol,fp);
			else
				fprintf(fp,"%d"
						,fklGetI32FromByteCode(tmpCode->code+i+sizeof(char)));
			r+=sizeof(char)+sizeof(int32_t);
			break;
		case 8:
			switch(tmpCode->code[i])
			{
				case FKL_OP_PUSH_F64:
					fprintf(fp,"%lf"
							,fklGetF64FromByteCode(tmpCode->code+i+sizeof(char)));
					break;
				case FKL_OP_IMPORT:
				case FKL_OP_PUSH_VECTOR:
				case FKL_OP_PUSH_HASHTABLE_EQ:
				case FKL_OP_PUSH_HASHTABLE_EQV:
				case FKL_OP_PUSH_HASHTABLE_EQUAL:
					fprintf(fp,"%lu"
							,fklGetU64FromByteCode(tmpCode->code+i+sizeof(char)));
					break;
				case FKL_OP_PUSH_I64:
				case FKL_OP_JMP:
				case FKL_OP_JMP_IF_FALSE:
				case FKL_OP_JMP_IF_TRUE:
					fprintf(fp,"%ld"
							,fklGetI64FromByteCode(tmpCode->code+i+sizeof(char)));
					break;
				case FKL_OP_POP_ARG:
				case FKL_OP_PUSH_VAR:
				case FKL_OP_PUSH_SYM:
				case FKL_OP_POP_REST_ARG:
					fklPrintRawSymbol(fklGetSymbolWithId(fklGetSidFromByteCode(tmpCode->code+i+sizeof(char)),table)->symbol,fp);
					break;
			}
			r+=sizeof(char)+sizeof(int64_t);
			break;
	}
	return r;
}

void fklPrintByteCode(const FklByteCode* tmpCode,FILE* fp,FklSymbolTable* table)
{
	FklPtrStack* s=fklCreatePtrStack(32,16);
	fklPushPtrStack(createByteCodePrintState(BP_NONE,0,0,tmpCode->size),s);
	if(!table)
		table=fklGetGlobSymbolTable();
	if(!fklIsPtrStackEmpty(s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
		}
		free(cState);
	}
	while(!fklIsPtrStackEmpty(s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
		}
		while(i<cpc&&!needBreak)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
		}
		free(cState);
	}
	fklDestroyPtrStack(s);
}

static uint64_t skipToCall(uint64_t index,const FklByteCode* bc)
{
	uint64_t r=0;
	while(index+r<bc->size&&bc->code[index+r]!=FKL_OP_CALL)
	{
		switch(fklGetOpcodeArgLen((FklOpcode)(bc->code[index+r])))
		{
			case -1:
				{
					FklOpcode op=bc->code[index+r];
					switch(op)
					{
						case FKL_OP_PUSH_STR:
						case FKL_OP_PUSH_BIG_INT:
						case FKL_OP_PUSH_BYTEVECTOR:
							r+=sizeof(char)
								+sizeof(uint64_t)
								+fklGetU64FromByteCode(bc->code+index+r+sizeof(char));
							break;
						case FKL_OP_PUSH_PROC:
							r+=sizeof(char)
								+sizeof(uint64_t)
								+fklGetU64FromByteCode(bc->code+index+r+sizeof(char));
							break;
						case FKL_OP_POP_VAR:
							r+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
							break;
						case FKL_OP_PUSH_TRY:
							{
								r+=sizeof(char);
								r+=sizeof(FklSid_t);
								uint32_t handlerNum=fklGetU32FromByteCode(bc->code+index+r);
								r+=sizeof(uint32_t);
								for(uint32_t j=0;j<handlerNum;j++)
								{
									uint32_t errTypeNum=fklGetU64FromByteCode(bc->code+index+r);
									r+=sizeof(FklSid_t)*errTypeNum;
									uint64_t pCpc=fklGetU64FromByteCode(bc->code+index+r);
									r+=sizeof(uint64_t);
									r+=pCpc;
								}
							}
							break;
						default:
							FKL_ASSERT(0);
							break;
					}
				}
				break;
			case 0:
				r+=sizeof(char);
				break;
			case 1:
				r+=sizeof(char)+sizeof(char);
				break;
			case 4:
				r+=sizeof(char)+sizeof(int32_t);
				break;
			case 8:
				r+=sizeof(char)+sizeof(int64_t);
				break;
		}
	}
	return r;
}

static int fklIsTheLastExpression(uint64_t index,FklByteCode* bc)
{
	uint64_t size=bc->size;
	uint8_t* code=bc->code;
	for(uint64_t i=index;i<size;i+=(code[i]==FKL_OP_JMP)?fklGetI64FromByteCode(code+i+sizeof(char))+sizeof(char)+sizeof(int64_t):1)
		if(code[i]!=FKL_OP_POP_TP
				&&code[i]!=FKL_OP_POP_TRY
				&&code[i]!=FKL_OP_JMP
				&&code[i]!=FKL_OP_POP_R_ENV)
			return 0;
	return 1;
}

void fklScanAndSetTailCall(FklByteCode* bc)
{
	for(uint64_t i=skipToCall(0,bc);i<bc->size;i+=skipToCall(i,bc))
	{
		if(fklIsTheLastExpression(++i,bc))
			bc->code[i-1]=FKL_OP_TAIL_CALL;
	}
}

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,FklSymbolTable* table)
{
	FklByteCode* tmpCode=obj->bc;
	FklPtrStack* s=fklCreatePtrStack(32,16);
	fklPushPtrStack(createByteCodePrintState(BP_NONE,0,0,tmpCode->size),s);
	if(!table)
		table=fklGetGlobSymbolTable();
	uint64_t j=0;
	FklSid_t fid=0;
	uint64_t line=0;
	if(!fklIsPtrStackEmpty(s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
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
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
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
	while(!fklIsPtrStackEmpty(s))
	{
		ByteCodePrintState* cState=fklPopPtrStack(s);
		uint64_t i=cState->cp;
		uint64_t cpc=cState->cpc;
		int needBreak=0;
		if(i<cpc)
		{
			putc('\n',fp);
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
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
			i+=printSingleByteCode(tmpCode,i,fp,cState,s,&needBreak,table);
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
	fklDestroyPtrStack(s);
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt* o,uint64_t size)
{
	uint32_t i=0;
	for(;i<o->ls;i++)
		o->l[i].scp+=size;
}

void fklCodeLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(s->bc->size)
	{
		if(!f->l)
		{
			f->ls=s->ls;
			f->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*s->ls);
			FKL_ASSERT(f->l);
			s->l[0].cpc+=f->bc->size;
			FKL_INCREASE_ALL_SCP(s->l+1,s->ls-1,f->bc->size);
			memcpy(f->l,s->l,(s->ls)*sizeof(FklLineNumTabNode));
		}
		else
		{
			FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
			if(f->l[f->ls-1].line==s->l[0].line&&f->l[f->ls-1].fid==s->l[0].fid)
			{
				f->l[f->ls-1].cpc+=s->l[0].cpc;
				f->l=(FklLineNumTabNode*)realloc(f->l,sizeof(FklLineNumTabNode)*(f->ls+s->ls-1));
				FKL_ASSERT(f->l);
				memcpy(&f->l[f->ls],&s->l[1],(s->ls-1)*sizeof(FklLineNumTabNode));
				f->ls+=s->ls-1;
			}
			else
			{
				f->l=(FklLineNumTabNode*)realloc(f->l,sizeof(FklLineNumTabNode)*(f->ls+s->ls));
				FKL_ASSERT(f->l);
				memcpy(&f->l[f->ls],s->l,(s->ls)*sizeof(FklLineNumTabNode));
				f->ls+=s->ls;
			}
		}
		fklCodeCat(f->bc,s->bc);
	}
}

//void fklCodelntCopyCat(FklByteCodelnt* f,const FklByteCodelnt* s)
//{
//	if(s->bc->size)
//	{
//		if(s->ls)
//		{
//			if(!f->l)
//			{
//				f->ls=s->ls;
//				f->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s->ls);
//				FKL_ASSERT(f->l);
//				uint32_t i=0;
//				for(;i<f->ls;i++)
//				{
//					FklLineNumTabNode* t=s->l[i];
//					f->l[i]=fklCreateLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
//				}
//				f->l[0]->cpc+=f->bc->size;
//				FKL_INCREASE_ALL_SCP(f->l+1,f->ls-1,f->bc->size);
//			}
//			else
//			{
//				uint32_t i=0;
//				FklLineNumTabNode** tl=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s->ls);
//				FKL_ASSERT(tl);
//				for(;i<s->ls;i++)
//				{
//					FklLineNumTabNode* t=s->l[i];
//					tl[i]=fklCreateLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
//				}
//				FKL_INCREASE_ALL_SCP(tl,s->ls,f->bc->size);
//				if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
//				{
//					f->l[f->ls-1]->cpc+=s->l[0]->cpc;
//					f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls-1));
//					FKL_ASSERT(f->l);
//					memcpy(f->l+f->ls,tl+1,(s->ls-1)*sizeof(FklLineNumTabNode*));
//					f->ls+=s->ls-1;
//					fklDestroyLineNumTabNode(tl[0]);
//				}
//				else
//				{
//					f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls));
//					FKL_ASSERT(f->l);
//					memcpy(f->l+f->ls,tl,(s->ls)*sizeof(FklLineNumTabNode*));
//					f->ls+=s->ls;
//				}
//				free(tl);
//			}
//		}
//		fklCodeCat(f->bc,s->bc);
//	}
//}

void fklReCodeLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(f->bc->size)
	{
		if(!s->l)
		{
			s->ls=f->ls;
			s->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*f->ls);
			FKL_ASSERT(s->l);
			f->l[f->ls-1].cpc+=s->bc->size;
			memcpy(s->l,f->l,(f->ls)*sizeof(FklLineNumTabNode));
		}
		else
		{
			FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
			if(f->l[f->ls-1].line==s->l[0].line&&f->l[f->ls-1].fid==s->l[0].fid)
			{
				FklLineNumTabNode* l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*(f->ls+s->ls-1));
				FKL_ASSERT(l);
				f->l[f->ls-1].cpc+=s->l[0].cpc;
				memcpy(l,f->l,(f->ls)*sizeof(FklLineNumTabNode));
				memcpy(&l[f->ls],&s->l[1],(s->ls-1)*sizeof(FklLineNumTabNode));
				free(s->l);
				s->l=l;
				s->ls+=f->ls-1;
			}
			else
			{
				FklLineNumTabNode* l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*(f->ls+s->ls));
				FKL_ASSERT(l);
				memcpy(l,f->l,(f->ls)*sizeof(FklLineNumTabNode));
				memcpy(&l[f->ls],s->l,(s->ls)*sizeof(FklLineNumTabNode));
				free(s->l);
				s->l=l;
				s->ls+=f->ls;
			}
		}
		fklReCodeCat(f->bc,s->bc);
	}
}

//FklByteCodeLabel* fklCreateByteCodeLable(int32_t place,const char* label)
//{
//	FklByteCodeLabel* tmp=(FklByteCodeLabel*)malloc(sizeof(FklByteCodeLabel));
//	FKL_ASSERT(tmp);
//	tmp->place=place;
//	tmp->label=fklCopyStr(label);
//	return tmp;
//}
//
//FklByteCodeLabel* fklFindByteCodeLabel(const char* label,FklPtrStack* s)
//{
//	int32_t l=0;
//	int32_t h=s->top-1;
//	int32_t mid;
//	while(l<=h)
//	{
//		mid=l+(h-l)/2;
//		int cmp=strcmp(((FklByteCodeLabel*)s->base[mid])->label,label);
//		if(cmp>0)
//			h=mid-1;
//		else if(cmp<0)
//			l=mid+1;
//		else
//			return (FklByteCodeLabel*)s->base[mid];
//	}
//	return NULL;
//}
//
//
//void fklDestroyByteCodeLabel(FklByteCodeLabel* obj)
//{
//	free(obj->label);
//	free(obj);
//}

FklLineNumberTable* fklCreateLineNumTable()
{
	FklLineNumberTable* t=(FklLineNumberTable*)malloc(sizeof(FklLineNumberTable));
	FKL_ASSERT(t);
	t->num=0;
	t->list=NULL;
	return t;
}

void fklInitLineNumTabNode(FklLineNumTabNode* n
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

FklLineNumTabNode* fklCreateLineNumTabNode(FklSid_t fid,uint64_t scp,uint64_t cpc,uint64_t line)
{
	FklLineNumTabNode* t=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode));
	FKL_ASSERT(t);
	fklInitLineNumTabNode(t,fid,scp,cpc,line);
	return t;
}

FklLineNumTabNode* fklCreateLineNumTabNodeWithFilename(const char* filename
		,uint64_t scp
		,uint64_t cpc
		,uint32_t line)
{
	FklLineNumTabNode* t=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode));
	FKL_ASSERT(t);
	t->fid=filename?fklAddSymbolToGlobCstr(filename)->id:0;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

void fklDestroyLineNumTabNode(FklLineNumTabNode* n)
{
	free(n);
}

void fklDestroyLineNumberTable(FklLineNumberTable* t)
{
	free(t->list);
	free(t);
}

FklLineNumTabNode* fklFindLineNumTabNode(uint64_t cp,size_t ls,FklLineNumTabNode* l)
{
	for(uint32_t i=0;i<ls;i++)
	{
		if(l[i].scp<=cp&&cp<(l[i].scp+l[i].cpc))
			return &l[i];
	}
	return &l[ls-1];
}

void fklLntCat(FklLineNumberTable* t,uint64_t bs,FklLineNumTabNode* l2,uint64_t s2)
{
	if(!t->list)
	{
		t->num=s2;
		t->list=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*s2);
		FKL_ASSERT(t->list);
		l2[0].cpc+=bs;
		FKL_INCREASE_ALL_SCP(l2+1,s2-1,bs);
		memcpy(t->list,l2,(s2)*sizeof(FklLineNumTabNode));
	}
	else
	{
		FKL_INCREASE_ALL_SCP(l2,s2,bs);
		if(t->list[t->num-1].line==l2[0].line&&t->list[t->num-1].fid==l2[0].fid)
		{
			t->list[t->num-1].cpc+=l2[0].cpc;
			t->list=(FklLineNumTabNode*)realloc(t->list,sizeof(FklLineNumTabNode)*(t->num+s2-1));
			FKL_ASSERT(t->list);
			memcpy(&t->list[t->num],l2+1,(s2-1)*sizeof(FklLineNumTabNode));
			t->num+=s2-1;
		}
		else
		{
			t->list=(FklLineNumTabNode*)realloc(t->list,sizeof(FklLineNumTabNode)*(t->num+s2));
			FKL_ASSERT(t->list);
			memcpy(&t->list[t->num],l2,(s2)*sizeof(FklLineNumTabNode));
			t->num+=s2;
		}
	}
}

void fklWriteLineNumberTable(FklLineNumberTable* lnt,FILE* fp)
{
	uint32_t size=lnt->num;
	fwrite(&size,sizeof(size),1,fp);
	uint32_t i=0;
	for(;i<size;i++)
	{
		FklLineNumTabNode* n=&lnt->list[i];
		fwrite(&n->fid,sizeof(n->fid),1,fp);
		fwrite(&n->scp,sizeof(n->scp),1,fp);
		fwrite(&n->cpc,sizeof(n->cpc),1,fp);
		fwrite(&n->line,sizeof(n->line),1,fp);
	}
}

void fklDBG_printByteCode(uint8_t* code,uint64_t s,uint64_t c,FILE* fp)
{
	FklByteCode t={c,code+s};
	fklPrintByteCode(&t,fp,NULL);
}

inline int32_t fklGetI32FromByteCode(uint8_t* code)
{
	int32_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	return i;
}

inline uint32_t fklGetU32FromByteCode(uint8_t* code)
{
	uint32_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	return i;
}

inline int64_t fklGetI64FromByteCode(uint8_t* code)
{
	int64_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

inline uint64_t fklGetU64FromByteCode(uint8_t* code)
{
	uint64_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

inline double fklGetF64FromByteCode(uint8_t* code)
{
	double d=0;
	((uint8_t*)&d)[0]=code[0];
	((uint8_t*)&d)[1]=code[1];
	((uint8_t*)&d)[2]=code[2];
	((uint8_t*)&d)[3]=code[3];
	((uint8_t*)&d)[4]=code[4];
	((uint8_t*)&d)[5]=code[5];
	((uint8_t*)&d)[6]=code[6];
	((uint8_t*)&d)[7]=code[7];
	return d;
}

inline FklSid_t fklGetSidFromByteCode(uint8_t* code)
{
	FklSid_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

inline void fklSetI32ToByteCode(uint8_t* code,int32_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
}

inline void fklSetU32ToByteCode(uint8_t* code,uint32_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
}

inline void fklSetI64ToByteCode(uint8_t* code,int64_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
}

inline void fklSetU64ToByteCode(uint8_t* code,uint64_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
}

inline void fklSetF64ToByteCode(uint8_t* code,double i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
}

inline void fklSetSidToByteCode(uint8_t* code,FklSid_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
}

static void fklSetCharToByteCode(uint8_t* code,char c)
{
	code[0]=(uint8_t)c;
}

#define NEW_PUSH_FIX_OBJ_BYTECODE(OPCODE,OBJ,OBJ_SETTER) FklByteCode* t=fklCreateByteCode(sizeof(char)+sizeof(OBJ));\
	t->code[0]=(OPCODE);\
	(OBJ_SETTER)(t->code+sizeof(char),(OBJ));\
	return t

FklByteCode* fklCreatePushI32ByteCode(int32_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I32,a,fklSetI32ToByteCode);}
FklByteCode* fklCreatePushI64ByteCode(int64_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_I64,a,fklSetI64ToByteCode);}
FklByteCode* fklCreatePushSidByteCode(FklSid_t a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_SYM,a,fklSetSidToByteCode);}
FklByteCode* fklCreatePushCharByteCode(char a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_CHAR,a,fklSetCharToByteCode);}
FklByteCode* fklCreatePushF64ByteCode(double a) {NEW_PUSH_FIX_OBJ_BYTECODE(FKL_OP_PUSH_F64,a,fklSetF64ToByteCode);}
FklByteCode* fklCreatePushStrByteCode(const FklString* str)
{
	FklByteCode* tmp=fklCreateByteCode(sizeof(char)+sizeof(str->size)+str->size);
	tmp->code[0]=FKL_OP_PUSH_STR;
	fklSetU64ToByteCode(tmp->code+sizeof(char),str->size);
	memcpy(tmp->code+sizeof(char)+sizeof(str->size)
			,str->str
			,str->size);
	return tmp;
}

FklByteCode* fklCreatePushBvecByteCode(const FklBytevector* bvec)
{
	FklByteCode* tmp=fklCreateByteCode(sizeof(char)+sizeof(bvec->size)+bvec->size);
	tmp->code[0]=FKL_OP_PUSH_BYTEVECTOR;
	fklSetU64ToByteCode(tmp->code+sizeof(char),bvec->size);
	memcpy(tmp->code+sizeof(char)+sizeof(bvec->size)
			,bvec->ptr
			,bvec->size);
	return tmp;
}

FklByteCode* fklCreatePushBigIntByteCode(const FklBigInt* bigInt)
{
	FklByteCode* tmp=fklCreateByteCode(sizeof(char)+sizeof(char)+sizeof(bigInt->size)+bigInt->num);
	tmp->code[0]=FKL_OP_PUSH_BIG_INT;
	fklSetU64ToByteCode(tmp->code+sizeof(char),bigInt->num+1);
	tmp->code[sizeof(char)+sizeof(bigInt->num)]=bigInt->neg;
	memcpy(tmp->code+sizeof(char)+sizeof(bigInt->num)+sizeof(char)
			,bigInt->digits
			,bigInt->num);
	return tmp;
}

FklByteCode* fklCreatePushNilByteCode(void)
{
	FklByteCode* r=fklCreateByteCode(sizeof(char));
	r->code[0]=FKL_OP_PUSH_NIL;
	return r;
}
