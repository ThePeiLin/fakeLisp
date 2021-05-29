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

static VMenv* genGlobEnv(CompEnv* cEnv,VMheap* heap,SymbolTable* table)
{
	VMenv* vEnv=newVMenv(NULL);
	initGlobEnv(vEnv,heap,table);
	ByteCodelnt* tmpByteCode=cEnv->proc;
	FakeVM* tmpVM=newTmpFakeVM(NULL);
	VMcode* tmpVMcode=newVMcode(tmpByteCode->bc->code,tmpByteCode->bc->size,0);
	tmpVM->mainproc=newFakeProcess(tmpVMcode,NULL);
	tmpVM->mainproc->localenv=vEnv;
	tmpVM->curproc=tmpVM->mainproc;
	tmpVMcode->prevEnv=NULL;
	tmpVM->table=table;
	tmpVM->lnt=newLineNumTable();
	tmpVM->lnt->size=tmpByteCode->ls;
	tmpVM->lnt->list=tmpByteCode->l;
	freeVMheap(tmpVM->heap);
	tmpVM->heap=heap;
	increaseVMenvRefcount(vEnv);
	int i=runFakeVM(tmpVM);
	if(i==1)
	{
		deleteCallChain(tmpVM);
		freeVMenv(vEnv);
		freeVMstack(tmpVM->stack);
		freeVMcode(tmpVMcode);
		free(tmpVM);
		return NULL;
	}
	free(tmpVM->lnt);
	freeVMstack(tmpVM->stack);
	freeVMcode(tmpVMcode);
	free(tmpVM);
	return vEnv;
}

AST_cptr* createTree(const char* objStr,Intpr* inter,StringMatchPattern* pattern)
{
	if(objStr==NULL)return NULL;
	if(isAllSpace(objStr))
		return NULL;
	if(pattern)
	{
		inter->curline+=countChar(objStr,'\n',skipSpace(objStr));
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
					AST_cptr* tmpCptr2=createTree(parts[j],inter,tmpPattern);
					if(!tmpCptr2)
						return NULL;
					tmpCptr->type=PAIR;
					tmpCptr->value=newPair(inter->curline,NULL);
					replaceCptr(getFirstCptr(tmpCptr),tmpCptr2);
					deleteCptr(tmpCptr2);
					free(tmpCptr2);
					int i=skipInPattern(parts[j],tmpPattern);
					for(;parts[j][i]!=0;i++)
					{
						tmpPattern=findStringPattern(parts[j]+i);
						AST_cptr* tmpCptr2=createTree(parts[j]+i,inter,tmpPattern);
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
						if(parts[j][i+skipSpace(parts[j]+i)]==',')
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
		VMenv* tmpGlobEnv=genGlobEnv(inter->glob,tmpVM->heap,inter->table);
		if(!tmpGlobEnv)
		{
			destroyEnv(tmpEnv);
			freeVMheap(tmpVM->heap);
			freeVMstack(tmpVM->stack);
			freeStringArry(parts,num);
			free(tmpVM);
			return NULL;
		}
		VMcode* tmpVMcode=newVMcode(pattern->proc->bc->code,pattern->proc->bc->size,0);
		VMenv* stringPatternEnv=castPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap,inter->table);
		tmpVMcode->prevEnv=NULL;
		tmpVM->mainproc=newFakeProcess(tmpVMcode,NULL);
		tmpVM->mainproc->localenv=stringPatternEnv;
		tmpVM->curproc=tmpVM->mainproc;
		tmpVM->table=inter->table;
		tmpVM->lnt=newLineNumTable();
		tmpVM->lnt->list=pattern->proc->l;
		tmpVM->lnt->size=pattern->proc->ls;
		int status=runFakeVM(tmpVM);
		AST_cptr* tmpCptr=NULL;
		if(!status)
			tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],inter->curline);
		free(tmpVM->lnt);
		freeVMenv(tmpGlobEnv);
		freeVMheap(tmpVM->heap);
		freeVMstack(tmpVM->stack);
		freeVMcode(tmpVMcode);
		free(tmpVM);
		destroyEnv(tmpEnv);
		freeStringArry(parts,num);
		return tmpCptr;
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
			AST_cptr* root=popComStack(s1);
			if(objStr[i]=='(')
			{
				hasComma=0;
				root->type=PAIR;
				root->value=newPair(curline,root->outer);
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					AST_cptr* tmp=popComStack(s1);
					if(tmp)
					{
						tmp->type=PAIR;
						tmp->value=newPair(curline,tmp->outer);
						pushComStack(getANSPairCdr(tmp),s1);
						pushComStack(getANSPairCar(tmp),s1);
					}
				}
				pushComStack((void*)s1->top,s2);
				pushComStack(getANSPairCdr(root),s1);
				pushComStack(getANSPairCar(root),s1);
				i++;
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
				if(root->outer->prev&&root->outer->prev->cdr.value==root->outer)
				{
					//将为下一个部分准备的pair删除并将该pair的前一个pair的cdr部分入栈
					s1->top=(long)topComStack(s2);
					AST_cptr* tmp=&root->outer->prev->cdr;
					free(tmp->value);
					tmp->type=NIL;
					tmp->value=NULL;
					pushComStack(tmp,s1);
				}
				i++;
			}
			else if(objStr[i]==')')
			{
				hasComma=0;
				long t=(long)popComStack(s2);
				AST_cptr* c=s1->data[t];
				if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.value==c->outer)
				{
					//如果还有为下一部分准备的pair，则将该pair删除
					AST_cptr* tmpCptr=s1->data[t];
					tmpCptr=&tmpCptr->outer->prev->cdr;
					tmpCptr->type=NIL;
					free(tmpCptr->value);
					tmpCptr->value=NULL;
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
					root->value=newAtom(STR,str,root->outer);
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
					root->value=tmpCptr->value;
					if(tmpCptr->type==ATM)
						((AST_atom*)tmpCptr->value)->prev=root->outer;
					else
						((AST_pair*)tmpCptr->value)->prev=root->outer;
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
							root->value=atom;
							atom->value.chr=(str[0]=='\\')?
								stringToChar(str+1):
								str[0];
							i+=strlen(str)+1;
							break;
						case 'b':
							str=getStringAfterBackslash(objStr+i+1);
							atom=newAtom(BYTS,NULL,root->outer);
							atom->value.byts.refcount=0;
							atom->value.byts.size=strlen(str)/2+strlen(str)%2;
							atom->value.byts.str=castStrByteStr(str);
							root->value=atom;
							i+=strlen(str)+1;
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
							atom->value.in32=stringToInt(str);
						}
						root->value=atom;
					}
					else
						root->value=newAtom(SYM,str,root->outer);
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
						tmp->value=newPair(curline,tmp->outer);
						pushComStack(getANSPairCdr(tmp),s1);
						pushComStack(getANSPairCar(tmp),s1);
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
		if(root->type==NIL)
			cptrType=NIL;
		else if(root->type==PAIR)
			cptrType=PAIR;
		else
			cptrType=ATM;
		root1->type=cptrType;
		if(cptrType==ATM)
		{
			AST_atom* tmpAtm=newAtom(root->type,NULL,root1->outer);
			switch(root->type)
			{
				case SYM:
				case STR:
					tmpAtm->value.str=copyStr(root->u.str->str);
					break;
				case IN32:
					tmpAtm->value.in32=*root->u.in32;
					break;
				case DBL:
					tmpAtm->value.dbl=*root->u.dbl;
					break;
				case CHR:
					tmpAtm->value.chr=*root->u.chr;
					break;
				case BYTS:
					tmpAtm->value.byts.size=root->u.byts->size;
					tmpAtm->value.byts.str=copyMemory(root->u.byts->str,root->u.byts->size);
					break;
				case PRC:
					tmpAtm->type=SYM;
					tmpAtm->value.str=copyStr("#<proc>");
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
			}
			root1->value=tmpAtm;
		}
		else if(cptrType==PAIR)
		{
			pushComStack(root->u.pair->car,s1);
			pushComStack(root->u.pair->cdr,s1);
			AST_pair* tmpPair=newPair(curline,root1->outer);
			root1->value=tmpPair;
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
	while(fir->type!=NIL)fir=&((AST_pair*)fir->value)->cdr;
	fir->type=PAIR;
	fir->value=newPair(sec->curline,fir->outer);
	replaceCptr(&((AST_pair*)fir->value)->car,sec);
}

void addToTail(AST_cptr* fir,const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&((AST_pair*)fir->value)->cdr;
	replaceCptr(fir,sec);
}
