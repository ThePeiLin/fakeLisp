#include"ast.h"
#include"common.h"
#include"reader.h"
#include"VMtool.h"
#include"fakeVM.h"
#include<string.h>
#include<ctype.h>

static void addToList(AST_cptr*,const AST_cptr*);
AST_cptr* createTree(const char* objStr,Intpr* inter,StringMatchPattern* pattern)
{
	if(objStr==NULL)return NULL;
	size_t i=0;
	int braketsNum=0;
	AST_cptr* root=NULL;
	AST_pair* objPair=NULL;
	AST_cptr* objCptr=NULL;
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
					replace(getFirstCptr(tmpCptr),tmpCptr2);
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
		ByteCode* rawProcList=castRawproc(NULL,pattern->procs);
		FakeVM* tmpVM=newTmpFakeVM(NULL,rawProcList);
		VMenv* tmpGlobEnv=newVMenv(NULL);
		initGlobEnv(tmpGlobEnv,tmpVM->heap,inter->table);
		VMcode* tmpVMcode=newVMcode(pattern->proc,0);
		VMenv* stringPatternEnv=castPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap,inter->table);
		tmpVMcode->localenv=NULL;
		tmpVM->mainproc=newFakeProcess(tmpVMcode,NULL);
		tmpVM->mainproc->localenv=stringPatternEnv;
		tmpVM->curproc=tmpVM->mainproc;
		tmpVM->modules=inter->modules;
		tmpVM->table=inter->table;
		tmpVM->lnt=pattern->lnt;
		int status=runFakeVM(tmpVM);
		AST_cptr* tmpCptr=NULL;
		if(!status)
			tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],inter->curline);
		freeVMenv(tmpGlobEnv);
		freeVMheap(tmpVM->heap);
		freeVMstack(tmpVM->stack);
		freeVMcode(tmpVMcode);
		free(tmpVM);
		free(rawProcList);
		destroyEnv(tmpEnv);
		freeStringArry(parts,num);
		return tmpCptr;
	}
	else
	{
		while(*(objStr+i)!='\0')
		{
			if(*(objStr+i)=='(')
			{
				if(i!=0&&*(objStr+i-1)==')')
				{
					if(objPair!=NULL)
					{
						int curline=(inter)?inter->curline:0;
						AST_pair* tmp=newPair(curline,objPair);
						objPair->cdr.type=PAIR;
						objPair->cdr.value=(void*)tmp;
						objPair=tmp;
						objCptr=&objPair->car;
					}
				}
				i++;
				braketsNum++;
				if(root==NULL)
				{
					int curline=(inter)?inter->curline:0;
					root=newCptr(curline,objPair);
					root->type=PAIR;
					root->value=newPair(curline,NULL);
					objPair=root->value;
					objCptr=&objPair->car;
				}
				else
				{
					int curline=(inter)?inter->curline:0;
					objPair=newPair(curline,objPair);
					objCptr->type=PAIR;
					objCptr->value=(void*)objPair;
					objCptr=&objPair->car;
				}
			}
			else if(*(objStr+i)==')')
			{
				i++;
				braketsNum--;
				AST_pair* prev=NULL;
				if(objPair==NULL)break;
				while(objPair->prev!=NULL)
				{
					prev=objPair;
					objPair=objPair->prev;
					if(objPair->car.value==prev)break;
				}
				if(objPair->car.value==prev)
					objCptr=&objPair->car;
				else if(objPair->cdr.value==prev)
					objCptr=&objPair->cdr;
			}
			else if(*(objStr+i)==',')
			{
				if(objCptr==&objPair->cdr)
				{
					if(root)
						deleteCptr(root);
					free(root);
					return NULL;
				}
				i++;
				objCptr=&objPair->cdr;
			}
			else if(isspace(*(objStr+i)))
			{
				int j=0;
				char* tmpStr=(char*)objStr+i;
				while(j>(int)-i&&isspace(*(tmpStr+j)))j--;
				if(*(tmpStr+j)==','||*(tmpStr+j)=='(')
				{
					j=1;
					while(isspace(*(tmpStr+j)))j++;
					i+=j;
					continue;
				}
				j=0;
				while(isspace(*(tmpStr+j)))
				{
					if(*(tmpStr+j)=='\n')
					{
						if(inter)
							inter->curline+=1;
					}
					j++;
				}
				if(*(tmpStr+j)==','||*(tmpStr+j)==')')
				{
					i+=j;
					continue;
				}
				i+=j;
				if(objPair!=NULL)
				{
					int curline=(inter)?inter->curline:0;
					AST_pair* tmp=newPair(curline,objPair);
					objPair->cdr.type=PAIR;
					objPair->cdr.value=(void*)tmp;
					objPair=tmp;
					objCptr=&objPair->car;
				}
			}
			else if(*(objStr+i)=='\"')
			{
				int curline=(inter)?inter->curline:0;
				if(root==NULL)objCptr=root=newCptr(curline,objPair);
				int32_t len=0;
				char* tmpStr=castEscapeCharater(objStr+i+1,'\"',&len);
				inter->curline+=countChar(objStr+i,'\n',len);
				objCptr->type=ATM;
				objCptr->value=(void*)newAtom(STR,tmpStr,objPair);
				i+=len+1;
				free(tmpStr);
			}
			else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(*(objStr+i+1)))||(*(objStr+i)=='.'&&isdigit(*(objStr+i+1))))
			{
				int curline=(inter)?inter->curline:0;
				if(root==NULL)objCptr=root=newCptr(curline,objPair);
				int32_t len=findKeyString(objStr+i);
				char* tmp=(char*)malloc(sizeof(char)*(len+1));
				if(!tmp)
					errors("createTree",__FILE__,__LINE__);
				strncpy(tmp,objStr+i,len);
				tmp[len]='\0';
				AST_atom* tmpAtm=NULL;
				if(!isNum(tmp))
					tmpAtm=newAtom(SYM,tmp,objPair);
				else if(isDouble(tmp))
				{
					tmpAtm=newAtom(DBL,NULL,objPair);
					double num=stringToDouble(tmp);
					tmpAtm->value.dbl=num;
				}
				else
				{
					tmpAtm=newAtom(IN32,NULL,objPair);
					int32_t num=stringToInt(tmp);
					tmpAtm->value.num=num;
				}
				objCptr->type=ATM;
				objCptr->value=tmpAtm;
				i+=len;
				free(tmp);
			}
			else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='\\'))
			{
				int curline=(inter)?inter->curline:0;
				if(root==NULL)objCptr=root=newCptr(curline,objPair);
				char* tmp=getStringAfterBackslash(objStr+i+2);
				objCptr->type=ATM;
				objCptr->value=(void*)newAtom(CHR,NULL,objPair);
				AST_atom* tmpAtm=objCptr->value;
				if(tmp[0]!='\\')tmpAtm->value.chr=tmp[0];
				else tmpAtm->value.chr=stringToChar(tmp+1);
				i+=strlen(tmp)+2;
				free(tmp);
			}
			else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='b'))
			{
				int curline=(inter)?inter->curline:0;
				if(root==NULL)objCptr=root=newCptr(curline,objPair);
				char* tmp=getStringAfterBackslash(objStr+i+2);
				objCptr->type=ATM;
				objCptr->value=(void*)newAtom(BYTS,NULL,objPair);
				AST_atom* tmpAtm=objCptr->value;
				int32_t size=strlen(tmp)/2+strlen(tmp)%2;
				tmpAtm->value.byts.size=size;
				tmpAtm->value.byts.str=castStrByteStr(tmp);
				i+=strlen(tmp)+2;
				free(tmp);
			}
			else
			{
				int curline=(inter)?inter->curline:0;
				StringMatchPattern* tmpPattern=findStringPattern(objStr+i);
				if(tmpPattern)
				{
					AST_cptr* tmpCptr=createTree(objStr+i,inter,tmpPattern);
					if(tmpCptr==NULL)
					{
						deleteCptr(root);
						free(root);
						return NULL;
					}
					if(root==NULL)objCptr=root=tmpCptr;
					else
					{
						replace(objCptr,tmpCptr);
						deleteCptr(tmpCptr);
						free(tmpCptr);
					}
					i+=skipInPattern(objStr+i,tmpPattern);
					if(!braketsNum)
						break;
				}
				else
				{
					if(root==NULL)objCptr=root=newCptr(curline,objPair);
					char* tmp=getStringFromList(objStr+i);
					objCptr->type=ATM;
					objCptr->value=(void*)newAtom(SYM,tmp,objPair);
					i+=strlen(tmp);
					free(tmp);
				}
			}
			if(braketsNum<=0&&root!=NULL)break;
		}
	}
	if(braketsNum!=0)
	{
		if(root)
			deleteCptr(root);
		free(root);
		return NULL;
	}
	return root;
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
					tmpAtm->value.num=*root->u.num;
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
	replace(&((AST_pair*)fir->value)->car,sec);
}
