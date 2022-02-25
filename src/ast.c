#include<fakeLisp/ast.h>
#include<fakeLisp/common.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/VMtool.h>
#include<fakeLisp/fakeVM.h>
#include<string.h>
#include<ctype.h>

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

static void addToTail(FklAstCptr*,const FklAstCptr*);
static void addToList(FklAstCptr*,const FklAstCptr*);

static FklVMenv* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,VMheap* heap)
{
	FklVMenv* vEnv=fklNewVMenv(NULL);
	fklInitGlobEnv(vEnv,heap);
	FklByteCodelnt* tmpByteCode=cEnv->proc;
	FklVM* tmpVM=fklNewTmpVM(NULL);
	FklVMproc* tmpVMproc=fklNewVMproc(0,tmpByteCode->bc->size);
	FklVMrunnable* mainrunnable=fklNewVMrunnable(tmpVMproc);
	mainrunnable->localenv=vEnv;
	tmpVM->code=tmpByteCode->bc->code;
	tmpVM->size=tmpByteCode->bc->size;
	fklPushComStack(mainrunnable,tmpVM->rstack);
	tmpVMproc->prevEnv=NULL;
	tmpVM->lnt=fklNewLineNumTable();
	tmpVM->lnt->num=tmpByteCode->ls;
	tmpVM->lnt->list=tmpByteCode->l;
	fklFreeVMheap(tmpVM->heap);
	tmpVM->heap=heap;
	fklIncreaseVMenvRefcount(vEnv);
	int i=fklRunVM(tmpVM);
	if(i==1)
	{
		free(tmpVM->lnt);
		fklDeleteCallChain(tmpVM);
		fklFreeVMenv(vEnv);
		fklFreeVMstack(tmpVM->stack);
		fklFreeVMproc(tmpVMproc);
		fklFreeComStack(tmpVM->rstack);
		fklFreeComStack(tmpVM->tstack);
		free(tmpVM);
		return NULL;
	}
	free(tmpVM->lnt);
	fklFreeVMstack(tmpVM->stack);
	fklFreeVMproc(tmpVMproc);
	fklFreeComStack(tmpVM->rstack);
	fklFreeComStack(tmpVM->tstack);
	free(tmpVM);
	fklCodelntCopyCat(t,tmpByteCode);
	return vEnv;
}

FklAstCptr* expandReaderMacro(const char* objStr,FklIntpr* inter,FklStringMatchPattern* pattern)
{
	FklPreEnv* tmpEnv=fklNewEnv(NULL);
	int num=0;
	char** parts=fklSplitStringInPattern(objStr,pattern,&num);
	int j=0;
	if(pattern->num-num)
	{
		fklFreeStringArry(parts,num);
		fklDestroyEnv(tmpEnv);
		return NULL;
	}
	for(;j<num;j++)
	{
		if(fklIsVar(pattern->parts[j]))
		{
			char* varName=fklGetVarName(pattern->parts[j]);
			if(fklIsMustList(pattern->parts[j]))
			{
				FklStringMatchPattern* tmpPattern=fklFindStringPattern(parts[j]);
				FklAstCptr* tmpCptr=fklNewCptr(inter->curline,NULL);
				tmpCptr->type=NIL;
				tmpCptr->u.all=NULL;
				FklAstCptr* tmpCptr2=fklCreateTree(parts[j],inter,tmpPattern);
				if(!tmpCptr2)
				{
					fklFreeStringArry(parts,num);
					fklDestroyEnv(tmpEnv);
					free(varName);
					return tmpCptr;
				}
				addToList(tmpCptr,tmpCptr2);
				fklDeleteCptr(tmpCptr2);
				free(tmpCptr2);
				int i=0;
				i=fklSkipInPattern(parts[j],tmpPattern);
				for(;isspace(parts[j][i]);i++)
					if(parts[j][i]=='\n')
						inter->curline+=1;
				while(parts[j][i]!=0)
				{
					tmpPattern=fklFindStringPattern(parts[j]+i);
					int32_t ti=(parts[j][i+fklSkipSpace(parts[j]+i)]==',');
					FklAstCptr* tmpCptr2=fklCreateTree(parts[j]+i+fklSkipSpace(parts[j]+i)+ti,inter,tmpPattern);
					if(!tmpCptr2)
					{
						if(tmpPattern)
						{
							fklDeleteCptr(tmpCptr);
							free(tmpCptr);
							return NULL;
						}
						else
							break;
					}
					if(ti)
					{
						addToTail(tmpCptr,tmpCptr2);
						fklDeleteCptr(tmpCptr2);
						free(tmpCptr2);
						break;
					}
					else
						addToList(tmpCptr,tmpCptr2);
					fklDeleteCptr(tmpCptr2);
					free(tmpCptr2);
					i+=fklSkipInPattern(parts[j]+i,tmpPattern);
					if(parts[j][i]==0)
						break;
					for(;isspace(parts[j][i]);i++)
						if(parts[j][i]=='\n')
							inter->curline+=1;
				}
				fklAddDefine(varName,tmpCptr,tmpEnv);
				fklDeleteCptr(tmpCptr);
				free(tmpCptr);
			}
			else
			{
				FklStringMatchPattern* tmpPattern=fklFindStringPattern(parts[j]);
				FklAstCptr* tmpCptr=fklCreateTree(parts[j],inter,tmpPattern);
				inter->curline+=fklCountChar(parts[j]+fklSkipInPattern(parts[j],tmpPattern),'\n',-1);
				fklAddDefine(varName,tmpCptr,tmpEnv);
				fklDeleteCptr(tmpCptr);
				free(tmpCptr);
			}
			free(varName);
		}
		else
			inter->curline+=fklCountChar(parts[j],'\n',-1);
	}
	FklVM* tmpVM=fklNewTmpVM(NULL);
	FklAstCptr* tmpCptr=NULL;
	if(pattern->type==BYTS)
	{
		FklByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
		FklVMenv* tmpGlobEnv=genGlobEnv(inter->glob,t,tmpVM->heap);
		if(!tmpGlobEnv)
		{
			fklDestroyEnv(tmpEnv);
			fklFreeByteCodelnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreeComStack(tmpVM->rstack);
			fklFreeComStack(tmpVM->tstack);
			fklFreeStringArry(parts,num);
			free(tmpVM);
			return NULL;
		}
		FklVMproc* tmpVMproc=fklNewVMproc(t->bc->size,pattern->u.bProc->bc->size);
		fklCodelntCopyCat(t,pattern->u.bProc);
		FklVMenv* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap);
		tmpVMproc->prevEnv=NULL;
		FklVMrunnable* mainrunnable=fklNewVMrunnable(tmpVMproc);
		mainrunnable->localenv=stringPatternEnv;
		tmpVM->code=t->bc->code;
		tmpVM->size=t->bc->size;
		fklPushComStack(mainrunnable,tmpVM->rstack);
		tmpVM->lnt=fklNewLineNumTable();
		tmpVM->lnt->list=pattern->u.bProc->l;
		tmpVM->lnt->num=pattern->u.bProc->ls;
		int state=fklRunVM(tmpVM);
		if(!state)
			tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),inter->curline);
		else
		{
			FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
			fklFreeByteCodelnt(t);
			free(tmpVM->lnt);
			fklDeleteCallChain(tmpVM);
			fklFreeVMenv(tmpGlobEnv);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreeVMproc(tmpVMproc);
			fklFreeComStack(tmpVM->rstack);
			fklFreeComStack(tmpVM->tstack);
			free(tmpVM);
			return NULL;
		}
		FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
		fklFreeByteCodelnt(t);
		free(tmpVM->lnt);
		fklFreeVMenv(tmpGlobEnv);
		fklFreeVMproc(tmpVMproc);
	}
	else if(pattern->type==FLPROC)
	{
		FklVMenv* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,NULL,tmpVM->heap);
		FklVMrunnable* mainrunnable=fklNewVMrunnable(NULL);
		mainrunnable->localenv=stringPatternEnv;
		fklPushComStack(mainrunnable,tmpVM->rstack);
		pattern->u.fProc(tmpVM);
		tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),inter->curline);
		free(mainrunnable);
		fklFreeVMenv(stringPatternEnv);
	}
	fklFreeVMheap(tmpVM->heap);
	fklFreeVMstack(tmpVM->stack);
	fklFreeComStack(tmpVM->rstack);
	fklFreeComStack(tmpVM->tstack);
	free(tmpVM);
	fklDestroyEnv(tmpEnv);
	fklFreeStringArry(parts,num);
	return tmpCptr;
}

FklAstCptr* fklCreateTree(const char* objStr,FklIntpr* inter,FklStringMatchPattern* pattern)
{
	if(objStr==NULL)return NULL;
	if(fklIsAllSpace(objStr))
		return NULL;
	if(pattern)
	{
		return expandReaderMacro(objStr,inter,pattern);
	}
	else
	{
		FklStringMatchPattern* pattern=NULL;
		FklComStack* s1=fklNewComStack(32);
		FklComStack* s2=fklNewComStack(32);
		int32_t i=0;
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		int32_t curline=(inter)?inter->curline:0;
		FklAstCptr* tmp=fklNewCptr(curline,NULL);
		fklPushComStack(tmp,s1);
		int hasComma=1;
		while(objStr[i]&&!fklIsComStackEmpty(s1))
		{
			for(;isspace(objStr[i]);i++)
				if(objStr[i]=='\n')
					inter->curline+=1;
			curline=inter->curline;
			FklAstCptr* root=fklPopComStack(s1);
			if(objStr[i]=='(')
			{
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					FklAstCptr* tmp=fklPopComStack(s1);
					if(tmp)
					{
						tmp->type=PAIR;
						tmp->u.pair=fklNewPair(curline,tmp->outer);
						fklPushComStack(fklGetASTPairCdr(tmp),s1);
						fklPushComStack(fklGetASTPairCar(tmp),s1);
					}
				}
				int j=0;
				for(;isspace(objStr[i+1+j]);j++);
				if(objStr[i+j+1]==')')
				{
					root->type=NIL;
					root->u.all=NULL;
					i+=j+2;
				}
				else
				{
					hasComma=0;
					root->type=PAIR;
					root->u.pair=fklNewPair(curline,root->outer);
					fklPushComStack((void*)s1->top,s2);
					fklPushComStack(fklGetASTPairCdr(root),s1);
					fklPushComStack(fklGetASTPairCar(root),s1);
					i++;
				}
			}
			else if(objStr[i]==',')
			{
				if(hasComma)
				{
					fklDeleteCptr(tmp);
					free(tmp);
					tmp=NULL;
					break;
				}
				else hasComma=1;
				if(root->outer->prev&&root->outer->prev->cdr.u.pair==root->outer)
				{
					//将为下一个部分准备的pair删除并将该pair的前一个pair的cdr部分入栈
					s1->top=(long)fklTopComStack(s2);
					FklAstCptr* tmp=&root->outer->prev->cdr;
					free(tmp->u.pair);
					tmp->type=NIL;
					tmp->u.all=NULL;
					fklPushComStack(tmp,s1);
				}
				i++;
			}
			else if(objStr[i]==')')
			{
				hasComma=0;
				long t=(long)fklPopComStack(s2);
				FklAstCptr* c=s1->data[t];
				if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
				{
					//如果还有为下一部分准备的pair，则将该pair删除
					FklAstCptr* tmpCptr=s1->data[t];
					tmpCptr=&tmpCptr->outer->prev->cdr;
					tmpCptr->type=NIL;
					free(tmpCptr->u.pair);
					tmpCptr->u.all=NULL;
				}
				//将栈顶恢复为将pair入栈前的位置
				s1->top=t;
				i++;
			}
			else
			{
				root->type=ATM;
				char* str=NULL;
				if(objStr[i]=='\"')
				{
					size_t len=0;
					str=fklCastEscapeCharater(objStr+i+1,'\"',&len);
					inter->curline+=fklCountChar(objStr+i,'\n',len);
					root->u.atom=fklNewAtom(STR,str,root->outer);
					i+=len+1;
					free(str);
				}
				else if((pattern=fklFindStringPattern(objStr+i)))
				{
					root->type=NIL;
					FklAstCptr* tmpCptr=fklCreateTree(objStr+i,inter,pattern);
					if(tmpCptr==NULL)
					{
						fklDeleteCptr(tmp);
						free(tmp);
						return NULL;
					}
					root->type=tmpCptr->type;
					root->u.all=tmpCptr->u.all;
					if(tmpCptr->type==ATM)
						tmpCptr->u.atom->prev=root->outer;
					else if(tmpCptr->type==PAIR)
						tmpCptr->u.pair->prev=root->outer;
					free(tmpCptr);
					i+=fklSkipInPattern(objStr+i,pattern);
				}
				else if(objStr[i]=='#')
				{
					i++;
					FklAstAtom* atom=NULL;
					switch(objStr[i])
					{
						case '\\':
							str=fklGetStringAfterBackslash(objStr+i+1);
							atom=fklNewAtom(CHR,NULL,root->outer);
							root->u.atom=atom;
							atom->value.chr=(str[0]=='\\')?
								fklStringToChar(str+1):
								str[0];
							i+=strlen(str)+1;
							break;
						case 'b':
							str=fklGetStringAfterBackslash(objStr+i+1);
							atom=fklNewAtom(BYTS,NULL,root->outer);
							atom->value.byts.size=strlen(str)/2+strlen(str)%2;
							atom->value.byts.str=fklCastStrByteStr(str);
							root->u.atom=atom;
							i+=strlen(str)+1;
							break;
						default:
							str=fklGetStringFromList(objStr+i-1);
							atom=fklNewAtom(SYM,str,root->outer);
							root->u.atom=atom;
							i+=strlen(str)-1;
							break;
					}
					free(str);
				}
				else
				{
					char* str=fklGetStringFromList(objStr+i);
					if(fklIsNum(str))
					{
						FklAstAtom* atom=NULL;
						if(fklIsDouble(str))
						{
							atom=fklNewAtom(DBL,NULL,root->outer);
							atom->value.dbl=fklStringToDouble(str);
						}
						else
						{
							atom=fklNewAtom(FKL_I32,NULL,root->outer);
							int64_t num=fklStringToInt(str);
							if(num>INT32_MAX||num<INT32_MIN)
							{
								atom->type=FKL_I64;
								atom->value.i64=num;
							}
							else
								atom->value.i32=num;
						}
						root->u.atom=atom;
					}
					else
						root->u.atom=fklNewAtom(SYM,str,root->outer);
					i+=strlen(str);
					free(str);
				}
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					FklAstCptr* tmp=fklPopComStack(s1);
					if(tmp)
					{
						tmp->type=PAIR;
						tmp->u.pair=fklNewPair(curline,tmp->outer);
						fklPushComStack(fklGetASTPairCdr(tmp),s1);
						fklPushComStack(fklGetASTPairCar(tmp),s1);
					}
				}
			}
		}
		fklFreeComStack(s1);
		fklFreeComStack(s2);
		return tmp;
	}
}

FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline)
{
	FklAstCptr* tmp=fklNewCptr(curline,NULL);
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	fklPushComStack(value,s1);
	fklPushComStack(tmp,s2);
	while(!fklIsComStackEmpty(s1))
	{
		FklVMvalue* root=fklPopComStack(s1);
		FklAstCptr* root1=fklPopComStack(s2);
		FklValueType cptrType=0;
		if(root==VM_NIL)
			cptrType=NIL;
		else if(IS_PAIR(root))
			cptrType=PAIR;
		else if(!IS_REF(root)&&!IS_CHF(root))
			cptrType=ATM;
		root1->type=cptrType;
		if(cptrType==ATM)
		{
			FklAstAtom* tmpAtm=fklNewAtom(SYM,NULL,root1->outer);
			FklVMptrTag tag=GET_TAG(root);
			switch(tag)
			{
				case SYM_TAG:
					tmpAtm->type=SYM;
					tmpAtm->value.str=fklCopyStr(fklGetGlobSymbolWithId(GET_SYM(root))->symbol);
					break;
				case FKL_I32_TAG:
					tmpAtm->type=FKL_I32;
					tmpAtm->value.i32=GET_I32(root);
					break;
				case CHR_TAG:
					tmpAtm->type=CHR;
					tmpAtm->value.chr=GET_CHR(root);
					break;
				case PTR_TAG:
					{
						tmpAtm->type=root->type;
						switch(root->type)
						{
							case DBL:
								tmpAtm->value.dbl=*root->u.dbl;
								break;
							case STR:
								tmpAtm->value.str=fklCopyStr(root->u.str);
								break;
							case BYTS:
								tmpAtm->value.byts.size=root->u.byts->size;
								tmpAtm->value.byts.str=fklCopyMemory(root->u.byts->str,root->u.byts->size);
								break;
							case PRC:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<proc>");
								break;
							case DLPROC:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<dlproc>");
								break;
							case CONT:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<proc>");
								break;
							case CHAN:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<chan>");
								break;
							case FP:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<fp>");
								break;
							case ERR:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<err>");
								break;
							case MEM:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<mem>");
								break;
							case CHF:
								tmpAtm->type=SYM;
								tmpAtm->value.str=fklCopyStr("#<ref>");
								break;
							default:
								return NULL;
								break;
						}
					}
					break;
				default:
					return NULL;
					break;
			}
			root1->u.atom=tmpAtm;
		}
		else if(cptrType==PAIR)
		{
			fklPushComStack(root->u.pair->car,s1);
			fklPushComStack(root->u.pair->cdr,s1);
			FklAstPair* tmpPair=fklNewPair(curline,root1->outer);
			root1->u.pair=tmpPair;
			tmpPair->car.outer=tmpPair;
			tmpPair->cdr.outer=tmpPair;
			fklPushComStack(&tmpPair->car,s2);
			fklPushComStack(&tmpPair->cdr,s2);
		}
	}
	fklFreeComStack(s1);
	fklFreeComStack(s2);
	return tmp;
}

void addToList(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type!=NIL)fir=&fir->u.pair->cdr;
	fir->type=PAIR;
	fir->u.pair=fklNewPair(sec->curline,fir->outer);
	fklReplaceCptr(&fir->u.pair->car,sec);
}

void addToTail(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type!=NIL)fir=&fir->u.pair->cdr;
	fklReplaceCptr(fir,sec);
}
