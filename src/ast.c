#include"ast.h"
#include"tool.h"
#include"reader.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"compiler.h"
#include<string.h>
#include<ctype.h>

AST_cptr* createTree(const char* objStr,intpr* inter,StringMatchPattern* pattern)
{
	if(objStr==NULL)return NULL;
	size_t i=0;
	int braketsNum=0;
	AST_cptr* root=NULL;
	AST_pair* objPair=NULL;
	AST_cptr* objCptr;
	if(pattern)
	{
		PreEnv* tmpEnv=newEnv(NULL);
		int num=0;
		char** parts=splitStringInPattern(objStr,pattern,&num);
		int j=0;
		if(pattern->num-num)
		{
			fprintf(stderr,"In file \"%s\",line %d",inter->filename,inter->curline);
			fprintf(stderr,"%s:Syntax error.\n",objStr);
			return NULL;
		}
		for(;j<num;j++)
			if(isVar(pattern->parts[j]))
			{
				char* varName=getVarName(pattern->parts[j]);
				if(isMustList(pattern->parts[j]))
				{
					StringMatchPattern* tmpPattern=findStringPattern(parts[j],pattern);
					AST_cptr* tmpCptr=newCptr(inter->curline,NULL);
					AST_cptr* tmpCptr2=createTree(parts[j]+i,inter,tmpPattern);
					tmpCptr->type=PAIR;
					tmpCptr->value=newPair(inter->curline,NULL);
					replace(getFirst(tmpCptr),tmpCptr2);
					deleteCptr(tmpCptr2);
					free(tmpCptr2);
					int i=skipInPattern(parts[j],tmpPattern);
					for(;parts[j][i]!=0;i++)
					{
						tmpPattern=findStringPattern(parts[j]+i,pattern);
						AST_cptr* tmpCptr2=createTree(parts[j]+i,inter,tmpPattern);
						if(!tmpCptr2)
							break;
						addToList(tmpCptr,tmpCptr2);
						deleteCptr(tmpCptr2);
						free(tmpCptr2);
						i+=skipInPattern(parts[j]+i,tmpPattern);
						if(parts[j][i]==0)break;
					}
					addDefine(varName,tmpCptr,tmpEnv);
					deleteCptr(tmpCptr);
					free(tmpCptr);
				}
				else
				{
					StringMatchPattern* tmpPattern=findStringPattern(parts[j],pattern);
					AST_cptr* tmpCptr=createTree(parts[j],inter,tmpPattern);
					addDefine(varName,tmpCptr,tmpEnv);
					deleteCptr(tmpCptr);
					free(tmpCptr);
				}
				free(varName);
			}
		ByteCode* rawProcList=castRawproc(NULL,pattern->procs);
		fakeVM* tmpVM=newTmpFakeVM(NULL,rawProcList);
		VMenv* tmpGlobEnv=newVMenv(0,NULL);
		initGlobEnv(tmpGlobEnv,tmpVM->heap);
		VMcode* tmpVMcode=newVMcode(pattern->proc);
		VMenv* stringPatternEnv=castPreEnvToVMenv(tmpEnv,pattern->bound,tmpGlobEnv,tmpVM->heap);
		tmpVMcode->localenv=stringPatternEnv;
		tmpVM->mainproc->code=tmpVMcode;
		tmpVM->mainproc->localenv=stringPatternEnv;
		tmpVM->modules=inter->modules;
		runFakeVM(tmpVM);
		AST_cptr* tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],inter->curline,NULL);
		pthread_mutex_destroy(&tmpVM->lock);
		if(tmpVM->mainproc->code)
		{
			tmpGlobEnv->refcount-=1;
			freeVMenv(tmpGlobEnv);
			stringPatternEnv->prev=NULL;
			freeVMcode(tmpVM->mainproc->code);
		}
		freeVMheap(tmpVM->heap);
		free(tmpVM->mainproc);
		freeVMstack(tmpVM->stack);
		freeFileStack(tmpVM->files);
		freeMessage(tmpVM->queueHead);
		free(tmpVM);
		free(rawProcList);
		destroyEnv(tmpEnv);
		freeStringArry(parts,num);
		inter->curline+=countChar(objStr,'\n',skipInPattern(objStr,pattern));
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
						if(inter)
							inter->curline+=1;
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
				i+=len;
				free(tmpStr);
			}
			else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&(/**(objStr+i)&&*/isdigit(*(objStr+i+1)))))
			{
				int curline=(inter)?inter->curline:0;
				if(root==NULL)objCptr=root=newCptr(curline,objPair);
				char* tmp=getStringFromList(objStr+i);
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
				i+=strlen(tmp);
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
				objCptr->value=(void*)newAtom(BYTA,NULL,objPair);
				AST_atom* tmpAtm=objCptr->value;
				int32_t size=strlen(tmp)/2+strlen(tmp)%2;
				tmpAtm->value.byta.size=size;
				tmpAtm->value.byta.arry=castStrByteArry(tmp);
				i+=strlen(tmp)+2;
				free(tmp);
			}
			else
			{
				int curline=(inter)?inter->curline:0;
				StringMatchPattern* tmpPattern=findStringPattern(objStr+i,NULL);
				if(tmpPattern)
				{
					AST_cptr* tmpCptr=createTree(objStr+i,inter,tmpPattern);
					if(tmpCptr==NULL)
						return NULL;
					if(root==NULL)objCptr=root=tmpCptr;
					else
					{
						replace(objCptr,tmpCptr);
						deleteCptr(tmpCptr);
						free(tmpCptr);
					}
					i+=skipInPattern(objStr+i,tmpPattern);
				}
				else
				{
					if(root==NULL)objCptr=root=newCptr(curline,objPair);
					char* tmp=getStringFromList(objStr+i);
					objCptr->type=ATM;
					objCptr->value=(void*)newAtom(SYM,tmp,objPair);
					i+=strlen(tmp);
					free(tmp);
				//	if(!braketsNum)
				//		break;
					continue;
				}
			}
			if(braketsNum<=0&&root!=NULL)break;
		}
	}
	return root;
}

AST_cptr* castVMvalueToCptr(VMvalue* value,int32_t curline,AST_pair* prev)
{
	ValueType cptrType=0;
	if(value->type==NIL)
		cptrType=NIL;
	else if(value->type==PAIR)
		cptrType=PAIR;
	else
		cptrType=ATM;
	AST_cptr* tmp=newCptr(curline,NULL);
	tmp->type=cptrType;
	if(cptrType==ATM)
	{
		AST_atom* tmpAtm=newAtom(value->type,NULL,prev);
		switch(value->type)
		{
			case SYM:
			case STR:
				tmpAtm->value.str=copyStr(value->u.str->str);
				break;
			case IN32:
				tmpAtm->value.num=*value->u.num;
				break;
			case DBL:
				tmpAtm->value.dbl=*value->u.dbl;
				break;
			case CHR:
				tmpAtm->value.chr=*value->u.chr;
				break;
			case BYTA:
				tmpAtm->value.byta.size=value->u.byta->size;
				tmpAtm->value.byta.arry=copyMemory(value->u.byta->arry,value->u.byta->size);
				break;
		}
		tmp->value=tmpAtm;
	}
	else if(cptrType==PAIR)
	{
		AST_pair* tmpPair=newPair(curline,prev);
		AST_cptr* astCar=castVMvalueToCptr(value->u.pair->car,curline,tmpPair);
		AST_cptr* astCdr=castVMvalueToCptr(value->u.pair->cdr,curline,tmpPair);
		tmpPair->car.outer=tmpPair;
		tmpPair->car.value=astCar->value;
		tmpPair->car.type=astCar->type;
		tmpPair->cdr.outer=tmpPair;
		tmpPair->cdr.value=astCdr->value;
		tmpPair->cdr.type=astCdr->type;
		free(astCdr);
		free(astCar);
		tmp->value=tmpPair;
	}
	return tmp;
}

void addToList(AST_cptr* fir,const const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&((AST_pair*)fir->value)->cdr;
	fir->type=PAIR;
	fir->value=newPair(sec->curline,fir->outer);
	replace(&((AST_pair*)fir->value)->car,sec);
}
