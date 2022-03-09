#include<fakeLisp/ast.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/reader.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<ctype.h>

#define FKL_FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

static void addToTail(FklAstCptr*,const FklAstCptr*);
static void addToList(FklAstCptr*,const FklAstCptr*);

static FklVMenv* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,FklVMheap* heap)
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
	fklPushPtrStack(mainrunnable,tmpVM->rstack);
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
		fklFreePtrStack(tmpVM->rstack);
		fklFreePtrStack(tmpVM->tstack);
		free(tmpVM);
		return NULL;
	}
	free(tmpVM->lnt);
	fklFreeVMstack(tmpVM->stack);
	fklFreeVMproc(tmpVMproc);
	fklFreePtrStack(tmpVM->rstack);
	fklFreePtrStack(tmpVM->tstack);
	free(tmpVM);
	fklCodelntCopyCat(t,tmpByteCode);
	return vEnv;
}

FklAstCptr* expandReaderMacro(const char* objStr,FklInterpreter* inter,FklStringMatchPattern* pattern)
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
				tmpCptr->type=FKL_NIL;
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
	if(pattern->type==FKL_BYTS)
	{
		FklByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
		FklVMenv* tmpGlobEnv=genGlobEnv(inter->glob,t,tmpVM->heap);
		if(!tmpGlobEnv)
		{
			fklDestroyEnv(tmpEnv);
			fklFreeByteCodelnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreePtrStack(tmpVM->rstack);
			fklFreePtrStack(tmpVM->tstack);
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
		fklPushPtrStack(mainrunnable,tmpVM->rstack);
		tmpVM->lnt=fklNewLineNumTable();
		tmpVM->lnt->list=pattern->u.bProc->l;
		tmpVM->lnt->num=pattern->u.bProc->ls;
		int state=fklRunVM(tmpVM);
		if(!state)
			tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),inter->curline);
		else
		{
			FKL_FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
			fklFreeByteCodelnt(t);
			free(tmpVM->lnt);
			fklDeleteCallChain(tmpVM);
			fklFreeVMenv(tmpGlobEnv);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreeVMproc(tmpVMproc);
			fklFreePtrStack(tmpVM->rstack);
			fklFreePtrStack(tmpVM->tstack);
			free(tmpVM);
			return NULL;
		}
		FKL_FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
		fklFreeByteCodelnt(t);
		free(tmpVM->lnt);
		fklFreeVMenv(tmpGlobEnv);
		fklFreeVMproc(tmpVMproc);
	}
	else if(pattern->type==FKL_FLPROC)
	{
		FklVMenv* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,NULL,tmpVM->heap);
		FklVMrunnable* mainrunnable=fklNewVMrunnable(NULL);
		mainrunnable->localenv=stringPatternEnv;
		fklPushPtrStack(mainrunnable,tmpVM->rstack);
		pattern->u.fProc(tmpVM);
		tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),inter->curline);
		free(mainrunnable);
		fklFreeVMenv(stringPatternEnv);
	}
	fklFreeVMheap(tmpVM->heap);
	fklFreeVMstack(tmpVM->stack);
	fklFreePtrStack(tmpVM->rstack);
	fklFreePtrStack(tmpVM->tstack);
	free(tmpVM);
	fklDestroyEnv(tmpEnv);
	fklFreeStringArry(parts,num);
	return tmpCptr;
}

FklAstCptr* fklCreateTree(const char* objStr,FklInterpreter* inter,FklStringMatchPattern* pattern)
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
		FklPtrStack* s1=fklNewPtrStack(32,16);
		FklIntStack* s2=fklNewIntStack(32,16);
		int32_t i=0;
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		int32_t curline=(inter)?inter->curline:0;
		FklAstCptr* tmp=fklNewCptr(curline,NULL);
		fklPushPtrStack(tmp,s1);
		int hasComma=1;
		while(objStr[i]&&!fklIsPtrStackEmpty(s1))
		{
			for(;isspace(objStr[i]);i++)
				if(objStr[i]=='\n')
					inter->curline+=1;
			curline=inter->curline;
			FklAstCptr* root=fklPopPtrStack(s1);
			if(objStr[i]=='(')
			{
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					FklAstCptr* tmp=fklPopPtrStack(s1);
					if(tmp)
					{
						tmp->type=FKL_PAIR;
						tmp->u.pair=fklNewPair(curline,tmp->outer);
						fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
						fklPushPtrStack(fklGetASTPairCar(tmp),s1);
					}
				}
				int j=0;
				for(;isspace(objStr[i+1+j]);j++);
				if(objStr[i+j+1]==')')
				{
					root->type=FKL_NIL;
					root->u.all=NULL;
					i+=j+2;
				}
				else
				{
					hasComma=0;
					root->type=FKL_PAIR;
					root->u.pair=fklNewPair(curline,root->outer);
					fklPushIntStack(s1->top,s2);
					fklPushPtrStack(fklGetASTPairCdr(root),s1);
					fklPushPtrStack(fklGetASTPairCar(root),s1);
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
					s1->top=fklTopIntStack(s2);
					FklAstCptr* tmp=&root->outer->prev->cdr;
					free(tmp->u.pair);
					tmp->type=FKL_NIL;
					tmp->u.all=NULL;
					fklPushPtrStack(tmp,s1);
				}
				i++;
			}
			else if(objStr[i]==')')
			{
				hasComma=0;
				int64_t t=fklPopIntStack(s2);
				FklAstCptr* c=s1->base[t];
				if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
				{
					//如果还有为下一部分准备的pair，则将该pair删除
					FklAstCptr* tmpCptr=s1->base[t];
					tmpCptr=&tmpCptr->outer->prev->cdr;
					tmpCptr->type=FKL_NIL;
					free(tmpCptr->u.pair);
					tmpCptr->u.all=NULL;
				}
				//将栈顶恢复为将pair入栈前的位置
				s1->top=t;
				i++;
			}
			else
			{
				root->type=FKL_ATM;
				char* str=NULL;
				if(objStr[i]=='\"')
				{
					size_t len=0;
					str=fklCastEscapeCharater(objStr+i+1,'\"',&len);
					inter->curline+=fklCountChar(objStr+i,'\n',len);
					root->u.atom=fklNewAtom(FKL_STR,str,root->outer);
					i+=len+1;
					free(str);
				}
				else if((pattern=fklFindStringPattern(objStr+i)))
				{
					root->type=FKL_NIL;
					FklAstCptr* tmpCptr=fklCreateTree(objStr+i,inter,pattern);
					if(tmpCptr==NULL)
					{
						fklDeleteCptr(tmp);
						free(tmp);
						return NULL;
					}
					root->type=tmpCptr->type;
					root->u.all=tmpCptr->u.all;
					if(tmpCptr->type==FKL_ATM)
						tmpCptr->u.atom->prev=root->outer;
					else if(tmpCptr->type==FKL_PAIR)
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
							atom=fklNewAtom(FKL_CHR,NULL,root->outer);
							root->u.atom=atom;
							atom->value.chr=(str[0]=='\\')?
								fklStringToChar(str+1):
								str[0];
							i+=strlen(str)+1;
							break;
						case 'b':
							str=fklGetStringAfterBackslash(objStr+i+1);
							atom=fklNewAtom(FKL_BYTS,NULL,root->outer);
							atom->value.byts.size=strlen(str)/2+strlen(str)%2;
							atom->value.byts.str=fklCastStrByteStr(str);
							root->u.atom=atom;
							i+=strlen(str)+1;
							break;
						default:
							str=fklGetStringFromList(objStr+i-1);
							atom=fklNewAtom(FKL_SYM,str,root->outer);
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
							atom=fklNewAtom(FKL_DBL,NULL,root->outer);
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
						root->u.atom=fklNewAtom(FKL_SYM,str,root->outer);
					i+=strlen(str);
					free(str);
				}
				if(&root->outer->car==root)
				{
					//如果root是root所在pair的car部分，
					//则在对应的pair后追加一个pair为下一个部分准备
					FklAstCptr* tmp=fklPopPtrStack(s1);
					if(tmp)
					{
						tmp->type=FKL_PAIR;
						tmp->u.pair=fklNewPair(curline,tmp->outer);
						fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
						fklPushPtrStack(fklGetASTPairCar(tmp),s1);
					}
				}
			}
		}
		fklFreePtrStack(s1);
		fklFreeIntStack(s2);
		return tmp;
	}
}

void addToList(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type!=FKL_NIL)fir=&fir->u.pair->cdr;
	fir->type=FKL_PAIR;
	fir->u.pair=fklNewPair(sec->curline,fir->outer);
	fklReplaceCptr(&fir->u.pair->car,sec);
}

void addToTail(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type!=FKL_NIL)fir=&fir->u.pair->cdr;
	fklReplaceCptr(fir,sec);
}

int fklEqByteString(const FklByteString* fir,const FklByteString* sec)
{
	if(fir->size!=sec->size)return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

FklAstCptr* fklBaseCreateTree(const char* objStr,FklInterpreter* inter)
{
	if(!objStr)
		return NULL;
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklIntStack* s2=fklNewIntStack(32,16);
	int32_t i=0;
	for(;isspace(objStr[i]);i++)
		if(objStr[i]=='\n')
			inter->curline+=1;
	int32_t curline=(inter)?inter->curline:0;
	FklAstCptr* tmp=fklNewCptr(curline,NULL);
	fklPushPtrStack(tmp,s1);
	int hasComma=1;
	while(objStr[i]&&!fklIsPtrStackEmpty(s1))
	{
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		curline=inter->curline;
		FklAstCptr* root=fklPopPtrStack(s1);
		if(objStr[i]=='(')
		{
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				FklAstCptr* tmp=fklPopPtrStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
					fklPushPtrStack(fklGetASTPairCar(tmp),s1);
				}
			}
			int j=0;
			for(;isspace(objStr[i+1+j]);j++);
			if(objStr[i+j+1]==')')
			{
				root->type=FKL_NIL;
				root->u.all=NULL;
				i+=j+2;
			}
			else
			{
				hasComma=0;
				root->type=FKL_PAIR;
				root->u.pair=fklNewPair(curline,root->outer);
				fklPushIntStack(s1->top,s2);
				fklPushPtrStack(fklGetASTPairCdr(root),s1);
				fklPushPtrStack(fklGetASTPairCar(root),s1);
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
				s1->top=fklTopIntStack(s2);
				FklAstCptr* tmp=&root->outer->prev->cdr;
				free(tmp->u.pair);
				tmp->type=FKL_NIL;
				tmp->u.all=NULL;
				fklPushPtrStack(tmp,s1);
			}
			i++;
		}
		else if(objStr[i]==')')
		{
			hasComma=0;
			long t=fklPopIntStack(s2);
			FklAstCptr* c=s1->base[t];
			if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
			{
				//如果还有为下一部分准备的pair，则将该pair删除
				FklAstCptr* tmpCptr=s1->base[t];
				tmpCptr=&tmpCptr->outer->prev->cdr;
				tmpCptr->type=FKL_NIL;
				free(tmpCptr->u.pair);
				tmpCptr->u.all=NULL;
			}
			//将栈顶恢复为将pair入栈前的位置
			s1->top=t;
			i++;
		}
		else
		{
			root->type=FKL_ATM;
			char* str=NULL;
			if(objStr[i]=='\"')
			{
				size_t len=0;
				str=fklCastEscapeCharater(objStr+i+1,'\"',&len);
				inter->curline+=fklCountChar(objStr+i,'\n',len);
				root->u.atom=fklNewAtom(FKL_STR,str,root->outer);
				i+=len+1;
				free(str);
			}
			else if(objStr[i]=='#')
			{
				i++;
				FklAstAtom* atom=NULL;
				switch(objStr[i])
				{
					case '\\':
						str=fklGetStringAfterBackslash(objStr+i+1);
						atom=fklNewAtom(FKL_CHR,NULL,root->outer);
						root->u.atom=atom;
						atom->value.chr=(str[0]=='\\')?
							fklStringToChar(str+1):
							str[0];
						i+=strlen(str)+1;
						break;
					case 'b':
						str=fklGetStringAfterBackslash(objStr+i+1);
						atom=fklNewAtom(FKL_BYTS,NULL,root->outer);
						atom->value.byts.size=strlen(str)/2+strlen(str)%2;
						atom->value.byts.str=fklCastStrByteStr(str);
						root->u.atom=atom;
						i+=strlen(str)+1;
						break;
					default:
						str=fklGetStringFromList(objStr+i-1);
						atom=fklNewAtom(FKL_SYM,str,root->outer);
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
						atom=fklNewAtom(FKL_DBL,NULL,root->outer);
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
					root->u.atom=fklNewAtom(FKL_SYM,str,root->outer);
				i+=strlen(str);
				free(str);
			}
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				FklAstCptr* tmp=fklPopPtrStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
					fklPushPtrStack(fklGetASTPairCar(tmp),s1);
				}
			}
		}
	}
	fklFreePtrStack(s1);
	fklFreeIntStack(s2);
	return tmp;
}

void fklPrintCptr(const FklAstCptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	FklAstPair* tmpPair=(objCptr->type==FKL_PAIR)?objCptr->u.pair:NULL;
	FklAstPair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==FKL_PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				putc(' ',out);
				objPair=objPair->cdr.u.pair;
				objCptr=&objPair->car;
			}
			else
			{
				putc('(',out);
				objPair=objCptr->u.pair;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr&&objCptr->type==FKL_ATM)putc(',',out);
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==FKL_NIL)
			||(objCptr->outer==NULL&&objCptr->type==FKL_NIL))fputs("()",out);
			if(objCptr->type!=FKL_NIL)
			{
				FklAstAtom* tmpAtm=objCptr->u.atom;
				switch((int)tmpAtm->type)
				{
					case FKL_SYM:
						fprintf(out,"%s",tmpAtm->value.str);
						break;
					case FKL_STR:
						fklPrintRawString(tmpAtm->value.str,out);
						break;
					case FKL_I32:
						fprintf(out,"%d",tmpAtm->value.i32);
						break;
					case FKL_DBL:
						fprintf(out,"%lf",tmpAtm->value.dbl);
						break;
					case FKL_CHR:
						fklPrintRawChar(tmpAtm->value.chr,out);
						break;
					case FKL_BYTS:
						fklPrintByteStr(tmpAtm->value.byts.size,tmpAtm->value.byts.str,out,1);
						break;
				}
			}
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		if(objPair!=NULL&&objCptr==&objPair->cdr)
		{
			putc(')',out);
			FklAstPair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(prev==objPair->car.u.pair)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.u.pair||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
}

void fklFreeAtom(FklAstAtom* objAtm)
{
	if(objAtm->type==FKL_SYM||objAtm->type==FKL_STR)free(objAtm->value.str);
	else if(objAtm->type==FKL_BYTS)
	{
		objAtm->value.byts.size=0;
		free(objAtm->value.byts.str);
	}
	free(objAtm);
}

FklAstPair* fklNewPair(int curline,FklAstPair* prev)
{
	FklAstPair* tmp;
	FKL_ASSERT((tmp=(FklAstPair*)malloc(sizeof(FklAstPair))),"fklNewPair",__FILE__,__LINE__);
	tmp->car.outer=tmp;
	tmp->car.type=FKL_NIL;
	tmp->car.u.all=NULL;
	tmp->cdr.outer=tmp;
	tmp->cdr.type=FKL_NIL;
	tmp->cdr.u.all=NULL;
	tmp->prev=prev;
	tmp->car.curline=curline;
	tmp->cdr.curline=curline;
	return tmp;
}

FklAstCptr* fklNewCptr(int curline,FklAstPair* outer)
{
	FklAstCptr* tmp=NULL;
	FKL_ASSERT((tmp=(FklAstCptr*)malloc(sizeof(FklAstCptr))),"fklNewCptr",__FILE__,__LINE__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=FKL_NIL;
	tmp->u.all=NULL;
	return tmp;
}

FklAstAtom* fklNewAtom(int type,const char* value,FklAstPair* prev)
{
	FklAstAtom* tmp=NULL;
	FKL_ASSERT((tmp=(FklAstAtom*)malloc(sizeof(FklAstAtom))),"fklNewAtom",__FILE__,__LINE__);
	switch(type)
	{
		case FKL_SYM:
		case FKL_STR:
			if(value!=NULL)
			{
				FKL_ASSERT((tmp->value.str=(char*)malloc(strlen(value)+1)),"fklNewAtom",__FILE__,__LINE__);
				strcpy(tmp->value.str,value);
			}
			else
				tmp->value.str=NULL;
			break;
		case FKL_CHR:
		case FKL_I32:
		case FKL_DBL:
			*(int32_t*)(&tmp->value)=0;
			break;
		case FKL_BYTS:
			tmp->value.byts.size=0;
			tmp->value.byts.str=NULL;
			break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int fklCopyCptr(FklAstCptr* objCptr,const FklAstCptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	fklPushPtrStack(objCptr,s1);
	fklPushPtrStack((void*)copiedCptr,s2);
	FklAstAtom* atom1=NULL;
	FklAstAtom* atom2=NULL;
	while(!fklIsPtrStackEmpty(s2))
	{
		FklAstCptr* root1=fklPopPtrStack(s1);
		FklAstCptr* root2=fklPopPtrStack(s2);
		root1->type=root2->type;
		root1->curline=root2->curline;
		switch(root1->type)
		{
			case FKL_PAIR:
				root1->u.pair=fklNewPair(0,root1->outer);
				fklPushPtrStack(fklGetASTPairCar(root1),s1);
				fklPushPtrStack(fklGetASTPairCdr(root1),s1);
				fklPushPtrStack(fklGetASTPairCar(root2),s2);
				fklPushPtrStack(fklGetASTPairCdr(root2),s2);
				break;
			case FKL_ATM:
				atom1=NULL;
				atom2=root2->u.atom;
				switch(atom2->type)
				{
					case FKL_SYM:
					case FKL_STR:
						atom1=fklNewAtom(atom2->type,atom2->value.str,root1->outer);
						break;
					case FKL_BYTS:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.byts.size=atom2->value.byts.size;
						atom1->value.byts.str=fklCopyMemory(atom2->value.byts.str,atom2->value.byts.size);
						break;
					case FKL_I32:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.i32=atom2->value.i32;
						break;
					case FKL_DBL:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.dbl=atom2->value.dbl;
						break;
					case FKL_CHR:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.chr=atom2->value.chr;
						break;
					default:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						break;
				}
				root1->u.atom=atom1;
				break;
			default:
				root1->u.all=NULL;
				break;
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return 1;
}
void fklReplaceCptr(FklAstCptr* fir,const FklAstCptr* sec)
{
	FklAstPair* tmp=fir->outer;
	FklAstCptr tmpCptr={NULL,0,FKL_NIL,{NULL}};
	tmpCptr.type=fir->type;
	tmpCptr.u.all=fir->u.all;
	fklCopyCptr(fir,sec);
	fklDeleteCptr(&tmpCptr);
	if(fir->type==FKL_PAIR)
		fir->u.pair->prev=tmp;
	else if(fir->type==FKL_ATM)
		fir->u.atom->prev=tmp;
}

int fklDeleteCptr(FklAstCptr* objCptr)
{
	if(objCptr==NULL)return 0;
	FklAstPair* tmpPair=(objCptr->type==FKL_PAIR)?objCptr->u.pair:NULL;
	FklAstPair* objPair=tmpPair;
	FklAstCptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==FKL_PAIR)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.u.pair;
				tmpCptr=&objPair->car;
				continue;
			}
			else
			{
				objPair=tmpCptr->u.pair;
				tmpCptr=&objPair->car;
				continue;
			}
		}
		else if(tmpCptr->type==FKL_ATM)
		{
			fklFreeAtom(tmpCptr->u.atom);
			tmpCptr->type=FKL_NIL;
			tmpCptr->u.all=NULL;
			continue;
		}
		else if(tmpCptr->type==FKL_NIL)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->car)
			{
				tmpCptr=&objPair->cdr;
				continue;
			}
			else if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				FklAstPair* prev=objPair;
				objPair=objPair->prev;
				free(prev);
				if(objPair==NULL||prev==tmpPair)break;
				if(prev==objPair->car.u.pair)
				{
					objPair->car.type=FKL_NIL;
					objPair->car.u.all=NULL;
				}
				else if(prev==objPair->cdr.u.pair)
				{
					objPair->cdr.type=FKL_NIL;
					objPair->cdr.u.all=NULL;
				}
				tmpCptr=&objPair->cdr;
			}
		}
		if(objPair==NULL)break;
	}
	objCptr->type=FKL_NIL;
	objCptr->u.all=NULL;
	return 0;
}

int fklCptrcmp(const FklAstCptr* first,const FklAstCptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	FklAstPair* firPair=NULL;
	FklAstPair* secPair=NULL;
	FklAstPair* tmpPair=(first->type==FKL_PAIR)?first->u.pair:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==FKL_PAIR)
		{
			firPair=first->u.pair;
			secPair=second->u.pair;
			first=&firPair->car;
			second=&secPair->car;
			continue;
		}
		else if(first->type==FKL_ATM||first->type==FKL_NIL)
		{
			if(first->type==FKL_ATM)
			{
				FklAstAtom* firAtm=first->u.atom;
				FklAstAtom* secAtm=second->u.atom;
				if(firAtm->type!=secAtm->type)return 0;
				if((firAtm->type==FKL_SYM||firAtm->type==FKL_STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==FKL_CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==FKL_BYTS&&!fklEqByteString(&firAtm->value.byts,&secAtm->value.byts))return 0;
			}
			if(firPair!=NULL&&first==&firPair->car)
			{ first=&firPair->cdr;
				second=&secPair->cdr;
				continue;
			}
		}
		if(firPair!=NULL&&first==&firPair->car)
		{
			first=&firPair->cdr;
			second=&secPair->cdr;
			continue;
		}
		else if(firPair!=NULL&&first==&firPair->cdr)
		{
			FklAstPair* firPrev=NULL;
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
				firPair=firPair->prev;
				secPair=secPair->prev;
				if(firPrev==firPair->car.u.pair)break;
			}
			if(firPair!=NULL)
			{
				first=&firPair->cdr;
				second=&secPair->cdr;
			}
			if(firPair==tmpPair&&first==&firPair->cdr)break;
		}
		if(firPair==NULL&&secPair==NULL)break;
	}
	return 1;
}

FklAstCptr* fklNextCptr(const FklAstCptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==FKL_PAIR)
		return &objCptr->outer->cdr.u.pair->car;
	return NULL;
}

FklAstCptr* fklPrevCptr(const FklAstCptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->prev!=NULL&&objCptr->outer->prev->cdr.u.pair==objCptr->outer)
		return &objCptr->outer->prev->car;
	return NULL;
}

FklAstCptr* fklGetLastCptr(const FklAstCptr* objList)
{
	if(objList->type!=FKL_PAIR)
		return NULL;
	FklAstPair* objPair=objList->u.pair;
	FklAstCptr* first=&objPair->car;
	for(;fklNextCptr(first)!=NULL;first=fklNextCptr(first));
	return first;
}

FklAstCptr* fklGetFirstCptr(const FklAstCptr* objList)
{
	if(objList->type!=FKL_PAIR)
		return NULL;
	FklAstPair* objPair=objList->u.pair;
	FklAstCptr* first=&objPair->car;
	return first;
}

FklAstCptr* fklGetASTPairCar(const FklAstCptr* obj)
{
	return &obj->u.pair->car;
}

FklAstCptr* fklGetASTPairCdr(const FklAstCptr* obj)
{
	return &obj->u.pair->cdr;
}

FklAstCptr* fklGetCptrCar(const FklAstCptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->car;
	return NULL;
}

FklAstCptr* fklGetCptrCdr(const FklAstCptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->cdr;
	return NULL;
}

int fklIsListCptr(const FklAstCptr* list)
{
	FklAstCptr* last=NULL;
	for(last=(list->type==FKL_PAIR)?&list->u.pair->car:NULL
			;fklGetCptrCdr(last)->type==FKL_PAIR
			;last=fklNextCptr(last));
	return fklGetCptrCdr(last)->type!=FKL_ATM;
}

unsigned int fklLengthListCptr(const FklAstCptr* list)
{
	unsigned int n=0;
	for(FklAstCptr* fir=(list->type==FKL_PAIR)?&list->u.pair->car:NULL
			;fir
			;fir=fklNextCptr(fir))
		n++;
	return n;
}
