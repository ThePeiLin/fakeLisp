#include"ast.h"
#include"common.h"
#include"reader.h"
#include"VMtool.h"
#include"fakeVM.h"
#include<string.h>
#include<ctype.h>

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	freeLineNumTabNode((l)[i]);\
}

static void addToTail(AST_cptr*,const AST_cptr*);
static void addToList(AST_cptr*,const AST_cptr*);

static VMenv* genGlobEnv(CompEnv* cEnv,ByteCodelnt* t,VMheap* heap)
{
	VMenv* vEnv=newVMenv(NULL);
	initGlobEnv(vEnv,heap);
	ByteCodelnt* tmpByteCode=cEnv->proc;
	FakeVM* tmpVM=newTmpFakeVM(NULL);
	VMproc* tmpVMproc=newVMproc(0,tmpByteCode->bc->size);
	VMrunnable* mainrunnable=newVMrunnable(tmpVMproc);
	mainrunnable->localenv=vEnv;
	tmpVM->code=tmpByteCode->bc->code;
	tmpVM->size=tmpByteCode->bc->size;
	pushComStack(mainrunnable,tmpVM->rstack);
	tmpVMproc->prevEnv=NULL;
	tmpVM->lnt=newLineNumTable();
	tmpVM->lnt->num=tmpByteCode->ls;
	tmpVM->lnt->list=tmpByteCode->l;
	freeVMheap(tmpVM->heap);
	tmpVM->heap=heap;
	increaseVMenvRefcount(vEnv);
	int i=runFakeVM(tmpVM);
	if(i==1)
	{
		free(tmpVM->lnt);
		deleteCallChain(tmpVM);
		freeVMenv(vEnv);
		freeVMstack(tmpVM->stack);
		freeVMproc(tmpVMproc);
		freeComStack(tmpVM->rstack);
		freeComStack(tmpVM->tstack);
		free(tmpVM);
		return NULL;
	}
	free(tmpVM->lnt);
	freeVMstack(tmpVM->stack);
	freeVMproc(tmpVMproc);
	freeComStack(tmpVM->rstack);
	freeComStack(tmpVM->tstack);
	free(tmpVM);
	codelntCopyCat(t,tmpByteCode);
	return vEnv;
}

AST_cptr* expandReaderMacro(const char* objStr,Intpr* inter,StringMatchPattern* pattern)
{
	PreEnv* tmpEnv=newEnv(NULL);
	int num=0;
	char** parts=splitStringInPattern(objStr,pattern,&num);
	int j=0;
	if(pattern->num-num)
	{
		freeStringArry(parts,num);
		destroyEnv(tmpEnv);
		return NULL;
	}
	for(;j<num;j++)
	{
		if(isVar(pattern->parts[j]))
		{
			char* varName=getVarName(pattern->parts[j]);
			if(isMustList(pattern->parts[j]))
			{
				StringMatchPattern* tmpPattern=findStringPattern(parts[j]);
				AST_cptr* tmpCptr=newCptr(inter->curline,NULL);
				tmpCptr->type=NIL;
				tmpCptr->u.all=NULL;
				AST_cptr* tmpCptr2=createTree(parts[j],inter,tmpPattern);
				if(!tmpCptr2)
				{
					freeStringArry(parts,num);
					destroyEnv(tmpEnv);
					free(varName);
					return tmpCptr;
				}
				addToList(tmpCptr,tmpCptr2);
				deleteCptr(tmpCptr2);
				free(tmpCptr2);
				int i=0;
				i=skipInPattern(parts[j],tmpPattern);
				for(;isspace(parts[j][i]);i++)
					if(parts[j][i]=='\n')
						inter->curline+=1;
				while(parts[j][i]!=0)
				{
					tmpPattern=findStringPattern(parts[j]+i);
					int32_t ti=(parts[j][i+skipSpace(parts[j]+i)]==',');
					AST_cptr* tmpCptr2=createTree(parts[j]+i+skipSpace(parts[j]+i)+ti,inter,tmpPattern);
					if(!tmpCptr2)
					{
						if(tmpPattern)
						{
							deleteCptr(tmpCptr);
							free(tmpCptr);
							return NULL;
						}
						else
							break;
					}
					if(ti)
					{
						addToTail(tmpCptr,tmpCptr2);
						deleteCptr(tmpCptr2);
						free(tmpCptr2);
						break;
					}
					else
						addToList(tmpCptr,tmpCptr2);
					deleteCptr(tmpCptr2);
					free(tmpCptr2);
					i+=skipInPattern(parts[j]+i,tmpPattern);
					if(parts[j][i]==0)
						break;
					for(;isspace(parts[j][i]);i++)
						if(parts[j][i]=='\n')
							inter->curline+=1;
				}
				addDefine(varName,tmpCptr,tmpEnv);
				deleteCptr(tmpCptr);
				free(tmpCptr);
			}
			else
			{
				StringMatchPattern* tmpPattern=findStringPattern(parts[j]);
				AST_cptr* tmpCptr=createTree(parts[j],inter,tmpPattern);
				inter->curline+=countChar(parts[j]+skipInPattern(parts[j],tmpPattern),'\n',-1);
				addDefine(varName,tmpCptr,tmpEnv);
				deleteCptr(tmpCptr);
				free(tmpCptr);
			}
			free(varName);
		}
		else
			inter->curline+=countChar(parts[j],'\n',-1);
	}
	FakeVM* tmpVM=newTmpFakeVM(NULL);
	ByteCodelnt* t=newByteCodelnt(newByteCode(0));
	VMenv* tmpGlobEnv=genGlobEnv(inter->glob,t,tmpVM->heap);
	if(!tmpGlobEnv)
	{
		destroyEnv(tmpEnv);
		freeByteCodelnt(t);
		freeVMheap(tmpVM->heap);
		freeVMstack(tmpVM->stack);
		freeComStack(tmpVM->rstack);
		freeComStack(tmpVM->tstack);
		freeStringArry(parts,num);
		free(tmpVM);
		return NULL;
	}
	VMproc* tmpVMproc=newVMproc(t->bc->size,pattern->proc->bc->size);
	codelntCopyCat(t,pattern->proc);
	VMenv* stringPatternEnv=castPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap);
	tmpVMproc->prevEnv=NULL;
	VMrunnable* mainrunnable=newVMrunnable(tmpVMproc);
	mainrunnable->localenv=stringPatternEnv;
	tmpVM->code=t->bc->code;
	tmpVM->size=t->bc->size;
	pushComStack(mainrunnable,tmpVM->rstack);
	tmpVM->lnt=newLineNumTable();
	tmpVM->lnt->list=pattern->proc->l;
	tmpVM->lnt->num=pattern->proc->ls;
	int status=runFakeVM(tmpVM);
	AST_cptr* tmpCptr=NULL;
	if(!status)
		tmpCptr=castVMvalueToCptr(GET_VAL(tmpVM->stack->values[0],tmpVM->heap),inter->curline);
	else
	{
		FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
		freeByteCodelnt(t);
		free(tmpVM->lnt);
		deleteCallChain(tmpVM);
		freeVMenv(tmpGlobEnv);
		freeVMheap(tmpVM->heap);
		freeVMstack(tmpVM->stack);
		freeVMproc(tmpVMproc);
		freeComStack(tmpVM->rstack);
		freeComStack(tmpVM->tstack);
		free(tmpVM);
		return NULL;
	}
	FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
	freeByteCodelnt(t);
	free(tmpVM->lnt);
	freeVMenv(tmpGlobEnv);
	freeVMheap(tmpVM->heap);
	freeVMstack(tmpVM->stack);
	freeVMproc(tmpVMproc);
	freeComStack(tmpVM->rstack);
	freeComStack(tmpVM->tstack);
	free(tmpVM);
	destroyEnv(tmpEnv);
	freeStringArry(parts,num);
	return tmpCptr;

}

AST_cptr* createTree(const char* objStr,Intpr* inter,StringMatchPattern* pattern)
{
	if(objStr==NULL)return NULL;
	if(isAllSpace(objStr))
		return NULL;
	if(pattern)
	{
		return expandReaderMacro(objStr,inter,pattern);
	}
	else
	{
		StringMatchPattern* pattern=NULL;
		ComStack* s1=newComStack(32);
		ComStack* s2=newComStack(32);
		int32_t i=0;
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		int32_t curline=(inter)?inter->curline:0;
		AST_cptr* tmp=newCptr(curline,NULL);
		pushComStack(tmp,s1);
		int hasComma=1;
		while(objStr[i]&&!isComStackEmpty(s1))
		{
			for(;isspace(objStr[i]);i++)
				if(objStr[i]=='\n')
					inter->curline+=1;
			curline=inter->curline;
			AST_cptr* root=popComStack(s1);
			if(objStr[i]=='(')
			{
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					AST_cptr* tmp=popComStack(s1);
					if(tmp)
					{
						tmp->type=PAIR;
						tmp->u.pair=newPair(curline,tmp->outer);
						pushComStack(getASTPairCdr(tmp),s1);
						pushComStack(getASTPairCar(tmp),s1);
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
					root->u.pair=newPair(curline,root->outer);
					pushComStack((void*)s1->top,s2);
					pushComStack(getASTPairCdr(root),s1);
					pushComStack(getASTPairCar(root),s1);
					i++;
				}
			}
			else if(objStr[i]==',')
			{
				if(hasComma)
				{
					deleteCptr(tmp);
					free(tmp);
					tmp=NULL;
					break;
				}
				else hasComma=1;
				if(root->outer->prev&&root->outer->prev->cdr.u.pair==root->outer)
				{
					//将为下一个部分准备的pair删除并将该pair的前一个pair的cdr部分入栈
					s1->top=(long)topComStack(s2);
					AST_cptr* tmp=&root->outer->prev->cdr;
					free(tmp->u.pair);
					tmp->type=NIL;
					tmp->u.all=NULL;
					pushComStack(tmp,s1);
				}
				i++;
			}
			else if(objStr[i]==')')
			{
				hasComma=0;
				long t=(long)popComStack(s2);
				AST_cptr* c=s1->data[t];
				if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
				{
					//如果还有为下一部分准备的pair，则将该pair删除
					AST_cptr* tmpCptr=s1->data[t];
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
					str=castEscapeCharater(objStr+i+1,'\"',&len);
					inter->curline+=countChar(objStr+i,'\n',len);
					root->u.atom=newAtom(STR,str,root->outer);
					i+=len+1;
					free(str);
				}
				else if((pattern=findStringPattern(objStr+i)))
				{
					root->type=NIL;
					AST_cptr* tmpCptr=createTree(objStr+i,inter,pattern);
					if(tmpCptr==NULL)
					{
						deleteCptr(tmp);
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
					i+=skipInPattern(objStr+i,pattern);
				}
				else if(objStr[i]=='#')
				{
					i++;
					AST_atom* atom=NULL;
					switch(objStr[i])
					{
						case '\\':
							str=getStringAfterBackslash(objStr+i+1);
							atom=newAtom(CHR,NULL,root->outer);
							root->u.atom=atom;
							atom->value.chr=(str[0]=='\\')?
								stringToChar(str+1):
								str[0];
							i+=strlen(str)+1;
							break;
						case 'b':
							str=getStringAfterBackslash(objStr+i+1);
							atom=newAtom(BYTS,NULL,root->outer);
							atom->value.byts.size=strlen(str)/2+strlen(str)%2;
							atom->value.byts.str=castStrByteStr(str);
							root->u.atom=atom;
							i+=strlen(str)+1;
							break;
						default:
							str=getStringFromList(objStr+i-1);
							atom=newAtom(SYM,str,root->outer);
							root->u.atom=atom;
							i+=strlen(str)-1;
							break;
					}
					free(str);
				}
				else
				{
					char* str=getStringFromList(objStr+i);
					if(isNum(str))
					{
						AST_atom* atom=NULL;
						if(isDouble(str))
						{
							atom=newAtom(DBL,NULL,root->outer);
							atom->value.dbl=stringToDouble(str);
						}
						else
						{
							atom=newAtom(IN32,NULL,root->outer);
							int64_t num=stringToInt(str);
							if(num>=INT32_MAX||num<=INT32_MIN)
							{
								atom->type=IN64;
								atom->value.in64=num;
							}
							else
								atom->value.in32=num;
						}
						root->u.atom=atom;
					}
					else
						root->u.atom=newAtom(SYM,str,root->outer);
					i+=strlen(str);
					free(str);
				}
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					AST_cptr* tmp=popComStack(s1);
					if(tmp)
					{
						tmp->type=PAIR;
						tmp->u.pair=newPair(curline,tmp->outer);
						pushComStack(getASTPairCdr(tmp),s1);
						pushComStack(getASTPairCar(tmp),s1);
					}
				}
			}
		}
		freeComStack(s1);
		freeComStack(s2);
		return tmp;
	}
}

AST_cptr* castVMvalueToCptr(VMvalue* value,int32_t curline)
{
	AST_cptr* tmp=newCptr(curline,NULL);
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	pushComStack(value,s1);
	pushComStack(tmp,s2);
	while(!isComStackEmpty(s1))
	{
		VMvalue* root=popComStack(s1);
		AST_cptr* root1=popComStack(s2);
		ValueType cptrType=0;
		if(root==VM_NIL)
			cptrType=NIL;
		else if(IS_PAIR(root))
			cptrType=PAIR;
		else if(!IS_REF(root)&&!IS_CHF(root))
			cptrType=ATM;
		root1->type=cptrType;
		if(cptrType==ATM)
		{
			AST_atom* tmpAtm=newAtom(SYM,NULL,root1->outer);
			VMptrTag tag=GET_TAG(root);
			switch(tag)
			{
				case SYM_TAG:
					tmpAtm->type=SYM;
					tmpAtm->value.str=copyStr(getGlobSymbolWithId(GET_SYM(root))->symbol);
					break;
				case IN32_TAG:
					tmpAtm->type=IN32;
					tmpAtm->value.in32=GET_IN32(root);
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
								tmpAtm->value.str=copyStr(root->u.str);
								break;
							case BYTS:
								tmpAtm->value.byts.size=root->u.byts->size;
								tmpAtm->value.byts.str=copyMemory(root->u.byts->str,root->u.byts->size);
								break;
							case PRC:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<proc>");
								break;
							case DLPROC:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<dlproc>");
								break;
							case CONT:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<proc>");
								break;
							case CHAN:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<chan>");
								break;
							case FP:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<fp>");
								break;
							case ERR:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<err>");
								break;
							case MEM:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<mem>");
								break;
							case CHF:
								tmpAtm->type=SYM;
								tmpAtm->value.str=copyStr("#<ref>");
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
			pushComStack(root->u.pair->car,s1);
			pushComStack(root->u.pair->cdr,s1);
			AST_pair* tmpPair=newPair(curline,root1->outer);
			root1->u.pair=tmpPair;
			tmpPair->car.outer=tmpPair;
			tmpPair->cdr.outer=tmpPair;
			pushComStack(&tmpPair->car,s2);
			pushComStack(&tmpPair->cdr,s2);
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

void addToList(AST_cptr* fir,const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&fir->u.pair->cdr;
	fir->type=PAIR;
	fir->u.pair=newPair(sec->curline,fir->outer);
	replaceCptr(&fir->u.pair->car,sec);
}

void addToTail(AST_cptr* fir,const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&fir->u.pair->cdr;
	replaceCptr(fir,sec);
}
