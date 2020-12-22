#include<string.h>
#include"preprocess.h"
#include"tool.h"
#include"syntax.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"ast.h"

static PreMacro* FirstMacro=NULL;
static PreMasym* FirstMasym=NULL;
static PreEnv* MacroEnv=NULL;
static PreFunc* funAndForm=NULL;


void addFunc(const char* name,ErrorStatus (*pFun)(AST_cptr*,PreEnv*,intpr*))
{
	PreFunc* current=funAndForm;
	PreFunc* prev=NULL;
	if(current==NULL)
	{
		if(!(funAndForm=(PreFunc*)malloc(sizeof(PreFunc))))errors("addFunc",__FILE__,__LINE__);
		if(!(funAndForm->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors("addFunc",__FILE__,__LINE__);
		strcpy(funAndForm->functionName,name);
		funAndForm->function=pFun;
		funAndForm->next=NULL;
	}
	else
	{
		while(current!=NULL)
		{
			if(!strcmp(current->functionName,name))exit(0);
			prev=current;
			current=current->next;
		}
		if(!(current=(PreFunc*)malloc(sizeof(PreFunc))))errors("addFunc",__FILE__,__LINE__);
		if(!(current->functionName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors("addFunc",__FILE__,__LINE__);
		strcpy(current->functionName,name);
		current->function=pFun;
		current->next=NULL;
		prev->next=current;
	}
}

ErrorStatus (*(findFunc(const char* name)))(AST_cptr*,PreEnv*,intpr*)
{
	PreFunc* current=funAndForm;
	while(current!=NULL&&strcmp(current->functionName,name))current=current->next;
	if(current==NULL)
		return NULL;
	else
		return current->function;
}



int retree(AST_cptr** fir,AST_cptr* sec)
{
	while(*fir!=sec)
	{
		AST_cptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
		replace(preCptr,*fir);
		*fir=preCptr;
		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

PreDef* addDefine(const char* symName,const AST_cptr* objCptr,PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)
	{
		curEnv->symbols=newDefines(symName);
		replace(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		PreDef* curDef=findDefine(symName,curEnv);
		if(curDef==NULL)
		{
			curDef=newDefines(symName);
			curDef->next=curEnv->symbols;
			curEnv->symbols=curDef;
			replace(&curDef->obj,objCptr);
		}
		else
			replace(&curDef->obj,objCptr);
		return curDef;
	}
}


PreDef* findDefine(const char* name,const PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		PreDef* curDef=curEnv->symbols;
		PreDef* prev=NULL;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}

ErrorStatus eval(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	if(objCptr==NULL)
		return status;
	AST_cptr* tmpCptr=objCptr;
	while(objCptr->type!=NIL)
	{
		if(objCptr->type==ATM)
		{
			AST_atom* objAtm=objCptr->value;
			if(objCptr->outer==NULL||((objCptr->outer->prev!=NULL)&&(objCptr->outer->prev->cdr.value==objCptr->outer)))
			{
				if(objAtm->type!=SYM)break;
				AST_cptr* reCptr=NULL;
				PreDef* objDef=NULL;
				PreEnv* tmpEnv=curEnv;
				while(tmpEnv!=NULL&&!(objDef=findDefine(objAtm->value.str,tmpEnv)))
					tmpEnv=tmpEnv->prev;
				if(objDef!=NULL)
				{
					reCptr=&objDef->obj;
					replace(objCptr,reCptr);
					break;
				}
				else
				{
					status.status=SYMUNDEFINE;
					status.place=objCptr;
					return status;
				}
			}
			else
			{
				if(objAtm->type!=SYM)
				{
					status.status=SYNTAXERROR;
					status.place=objCptr;
					return status;
				}
				int before=objCptr->outer->cdr.type;
				ErrorStatus (*pfun)(AST_cptr*,PreEnv*,intpr*)=NULL;
				pfun=findFunc(objAtm->value.str);
				if(pfun!=NULL)
				{
					status=pfun(objCptr,curEnv,inter);
					if(status.status!=0)return status;
				}
				else
				{
					AST_cptr* reCptr=NULL;
					PreDef* objDef=NULL;
					PreEnv* tmpEnv=curEnv;
					while(tmpEnv!=NULL&&!(objDef=findDefine(objAtm->value.str,tmpEnv)))
						tmpEnv=tmpEnv->prev;
					if(objDef!=NULL)
					{
						reCptr=&objDef->obj;
						replace(objCptr,reCptr);
						continue;
					}
					else
					{
						status.status=SYMUNDEFINE;
						status.place=objCptr;
						return status;
					}
				}
			}
		}
		else if(objCptr->type==PAIR)
		{
			if(objCptr->type!=PAIR)continue;
			objCptr=&(((AST_pair*)objCptr->value)->car);
			continue;
		}
		if(retree(&objCptr,tmpCptr))break;
	}
	return status;
}

PreMacro* PreMacroMatch(const AST_cptr* objCptr)
{
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!fmatcmp(objCptr,current->pattern))
		current=current->next;
	return current;
}

int addMacro(AST_cptr* pattern,ByteCode* proc,int32_t bound,RawProc* procs)
//int addMacro(AST_cptr* format,AST_cptr* express)
{
	if(pattern->type!=PAIR)return SYNTAXERROR;
	AST_cptr* tmpCptr=NULL;
	for(tmpCptr=&((AST_pair*)pattern->value)->car;tmpCptr!=NULL;tmpCptr=nextCptr(tmpCptr))
	{
		if(tmpCptr->type==ATM)
		{
			AST_atom* carAtm=tmpCptr->value;
			AST_atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.value:NULL;
			if(carAtm->type==SYM)
			{
				//if(!isVal(carAtm->value.str))addKeyWord(carAtm->value.str);
				if(!hasAnotherName(carAtm->value.str))addKeyWord(carAtm->value.str);
				else if(hasAnotherName(carAtm->value.str)&&!memcmp(carAtm->value.str,"COLT",4))
				{
					const char* name=hasAnotherName(carAtm->value.str);
					int i;
					int num=getWordNum(name);
					for(i=0;i<num;i++)
					{
						char* word=getWord(name,i+1);
						addKeyWord(word);
						free(word);
					}
				}
			}
			if(cdrAtm!=NULL&&cdrAtm->type==SYM)
			{
				//if(!isVal(cdrAtm->value.str))addKeyWord(cdrAtm->value.str);
				if(!hasAnotherName(cdrAtm->value.str))addKeyWord(cdrAtm->value.str);
				else if(hasAnotherName(cdrAtm->value.str)&&!memcmp(cdrAtm->value.str,"COLT",4))
				{
					const char* name=hasAnotherName(cdrAtm->value.str);
					int i;
					int num=getWordNum(name);
					for(i=0;i<num;i++)
					{
						char* word=getWord(name,i+1);
						addKeyWord(word);
						free(word);
					}
				}
			}
		}
	}
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!AST_cptrcmp(pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		if(!(current=(PreMacro*)malloc(sizeof(PreMacro))))errors("addMacro",__FILE__,__LINE__);
		//current->format=format;
		//current->express=express;
		current->next=FirstMacro;
		current->pattern=pattern;
		current->proc=proc;
		current->bound=bound;
		current->procs=procs;
		FirstMacro=current;
	}
	else
	{
		current->pattern=pattern;
		current->proc=proc;
		current->bound=bound;
		current->procs=procs;
		//current->express=express;
	}
	//printAllKeyWord();
	return 0;
}

PreMasym* findMasym(const char* name)
{
	PreMasym* current=FirstMasym;
	const char* anotherName=hasAnotherName(name);
	int64_t len=0;if(anotherName==NULL)len=strlen(name);else len=(anotherName-name-1);
	while(current!=NULL&&memcmp(current->symName,name,len))current=current->next;
	return current;
}

int fmatcmp(const AST_cptr* origin,const AST_cptr* format)
{
	MacroEnv=newEnv(NULL);
	AST_pair* tmpPair=(format->type==PAIR)?format->value:NULL;
	AST_pair* forPair=tmpPair;
	AST_pair* oriPair=(origin->type==PAIR)?origin->value:NULL;
	while(origin!=NULL)
	{
		if(format->type==PAIR&&origin->type==PAIR)
		{
			forPair=format->value;
			oriPair=origin->value;
			format=&forPair->car;
			origin=&oriPair->car;
			continue;
		}
		else if(format->type==ATM)
		{
			AST_atom* tmpAtm=format->value;
			//PreMasym* tmpSym=NULL;
			if(tmpAtm->type==SYM)
			{
				//tmpSym=findMasym(tmpAtm->value.str);
				//if(tmpSym==NULL||!hasAnotherName(tmpAtm->value.str))
				//{
				//	if(!AST_cptrcmp(origin,format))
				//	{
				//		destroyEnv(MacroEnv);
				//		MacroEnv=NULL;
				//		return 0;
				//	}
				//}
				//else if(!tmpSym->Func(origin,format,hasAnotherName(tmpAtm->value.str),MacroEnv))
				//{
				//	destroyEnv(MacroEnv);
				//	MacroEnv=NULL;
				//	return 0;
				//}
				if(isVal(tmpAtm->value.str))
					addDefine(tmpAtm->value.str+1,origin,MacroEnv);
				else if(!AST_cptrcmp(origin,format))
				{
					destroyEnv(MacroEnv);
					MacroEnv=NULL;
					return 0;
				}
			}
			forPair=format->outer;
			oriPair=origin->outer;
			//if(tmpSym!=NULL&&tmpSym->Func==M_VAREPT)
			//{
			//	format=&forPair->cdr;
			//	origin=&oriPair->cdr;
			//}
			if(forPair!=NULL&&format==&forPair->car)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
				continue;
			}
		}
		//else if(origin->type==NIL&&
		//		format->type==PAIR&&
		//		((AST_pair*)format->value)->car.type==ATM)
		//{
		//	AST_atom* tmpAtm=((AST_pair*)format->value)->car.value;
		//	PreMasym* tmpSym=findMasym(tmpAtm->value.str);
		//	if(tmpSym!=NULL&&tmpSym->Func!=M_VAREPT)
		//	{
		//		destroyEnv(MacroEnv);
		//		MacroEnv=NULL;
		//		return 0;
		//	}
		//	else if(tmpSym!=NULL)
		//	{
		//		AST_cptr* tmp=&((AST_pair*)format->value)->car;
		//		AST_atom* tmpAtm=tmp->value;
		//		M_VAREPT(NULL,tmp,hasAnotherName(tmpAtm->value.str),MacroEnv);
		//	}
		//}
		else if(origin->type!=format->type)
		{
			destroyEnv(MacroEnv);
			MacroEnv=NULL;
			return 0;
		}
		if(forPair!=NULL&&format==&forPair->car)
		{
			origin=&oriPair->cdr;
			format=&forPair->cdr;
			continue;
		}
		else if(forPair!=NULL&&format==&forPair->cdr)
		{
			AST_pair* oriPrev=NULL;
			AST_pair* forPrev=NULL;
			if(oriPair->prev==NULL)break;
			while(oriPair->prev!=NULL&&forPair!=tmpPair)
			{
				oriPrev=oriPair;
				forPrev=forPair;
				oriPair=oriPair->prev;
				forPair=forPair->prev;
				if(oriPrev==oriPair->car.value)break;
			}
			if(oriPair!=NULL)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
			}
			if(forPair==tmpPair&&format==&forPair->cdr)break;
		}
		if(oriPair==NULL&&forPair==NULL)break;
	}
	return 1;
}

int PreMacroExpand(AST_cptr* objCptr,intpr* inter)
{
	ErrorStatus status={0,NULL};
	PreMacro* tmp=PreMacroMatch(objCptr);
	if(tmp!=NULL)
	{
		VMenv* tmpGlob=newVMenv(0,NULL);
		ByteCode* rawProcList=castRawproc(NULL,tmp->procs);
		fakeVM* tmpVM=newTmpFakeVM(NULL,rawProcList);
		initGlobEnv(tmpGlob,tmpVM->heap);
		VMcode* tmpVMcode=newVMcode(tmp->proc);
		VMenv* macroVMenv=castPreEnvToVMenv(MacroEnv,tmp->bound,tmpGlob,tmpVM->heap);
		tmpVM->mainproc->localenv=macroVMenv;
		tmpVMcode->localenv=macroVMenv;
		tmpVM->mainproc->code=tmpVMcode;
		tmpVM->modules=inter->modules;
		runFakeVM(tmpVM);
		AST_cptr* tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],inter->curline,NULL);
		pthread_mutex_destroy(&tmpVM->lock);
		if(tmpVM->mainproc->code)
		{
			tmpGlob->refcount-=1;
			freeVMenv(tmpGlob);
			macroVMenv->prev=NULL;
			freeVMcode(tmpVM->mainproc->code);
		}
		freeVMheap(tmpVM->heap);
		free(tmpVM->mainproc);
		freeVMstack(tmpVM->stack);
		freeFileStack(tmpVM->files);
		freeMessage(tmpVM->queueHead);
		free(tmpVM);
		replace(objCptr,tmpCptr);
		deleteCptr(tmpCptr);
		free(tmpCptr);
		destroyEnv(MacroEnv);
		//AST_cptr* tmpCptr=newCptr(0,NULL);
		//replace(tmpCptr,tmp->express);
		//status=eval(tmp->express,MacroEnv,inter);
		//if(status.status!=0)exError(status.place,status.status,NULL);
		//replace(objCptr,tmp->express);
		//deleteCptr(tmp->express);
		//free(tmp->express);
		//tmp->express=tmpCptr;
		//destroyEnv(MacroEnv);
		return 1;
	}
	return 0;
}

void initPreprocess()
{
	addRule("ATOM",M_ATOM);
	addRule("PAIR",M_PAIR);
	addRule("ANY",M_ANY);
	addRule("VAREPT",M_VAREPT);
	addRule("COLT",M_COLT);
	addKeyWord("define");
	addKeyWord("setq");
	addKeyWord("quote");
	addKeyWord("cond");
	addKeyWord("and");
	addKeyWord("or");
	addKeyWord("lambda");
	addKeyWord("setf");
	addKeyWord("load");
}
void addRule(const char* name,int (*obj)(const AST_cptr*,const AST_cptr*,const char*,PreEnv*))
{
	PreMasym* current=NULL;
	if(!(current=(PreMasym*)malloc(sizeof(PreMasym))))errors("addRule",__FILE__,__LINE__);
	if(!(current->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors("addRule",__FILE__,__LINE__);
	current->next=FirstMasym;
	FirstMasym=current;
	strcpy(current->symName,name);
	current->Func=obj;
}

int M_ATOM(const AST_cptr* oriCptr,const AST_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==ATM)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_PAIR(const AST_cptr* oriCptr,const AST_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else if(oriCptr->type==PAIR)
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_ANY(const AST_cptr* oriCptr,const AST_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	if(oriCptr==NULL)return 0;
	else
	{
		addDefine(name,oriCptr,curEnv);
		return 1;
	}
	return 0;
}

int M_VAREPT(const AST_cptr* oriCptr,const AST_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	fmtCptr=&fmtCptr->outer->prev->car;
	const AST_cptr* format=fmtCptr;
	const AST_cptr tmp={NULL,0,NIL,NULL};
	AST_pair* forPair=(format->type==PAIR)?format->value:NULL;
	AST_pair* tmpPair=forPair;
	PreDef* valreptdef=addDefine(name,&tmp,MacroEnv);
	if(oriCptr==NULL)
	{
		const AST_cptr* format=fmtCptr;
		AST_pair* forPair=(format->type==PAIR)?format->value:NULL;
		PreEnv* forValRept=newEnv(NULL);
		while(format!=NULL)
		{
			if(format->type==PAIR)
			{
				forPair=format->value;
				format=&forPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				AST_atom* tmpAtm=format->value;
				if(tmpAtm->type==SYM)
				{
					const char* anotherName=hasAnotherName(tmpAtm->value.str);
					if(anotherName)
					{
						forPair=(format==&fmtCptr->outer->prev->car)?NULL:format->outer;
						int len=strlen(name)+strlen(anotherName)+2;
						char symName[len];
						strcpy(symName,name);
						strcat(symName,"#");
						strcat(symName,anotherName);
						PreDef* tmpDef=findDefine(symName,MacroEnv);
						if(tmpDef==NULL)tmpDef=addDefine(symName,&tmp,MacroEnv);
					}
				}
				if(forPair!=NULL&&format==&forPair->car)
				{
					format=&forPair->cdr;
					continue;
				}
			}
			if(forPair!=NULL&&format==&forPair->car)
			{
				format=&forPair->cdr;
				continue;
			}
			else if(forPair!=NULL&&format==&forPair->cdr)
			{
				AST_pair* forPrev=NULL;
				while(forPair!=tmpPair)
				{
					forPrev=forPair;
					forPair=forPair->prev;
					if(forPair==forPair->car.value)break;
				}
				if(forPair!=NULL)
					format=&forPair->cdr;
				if(forPair==tmpPair&&format==&forPair->cdr)break;
			}
			if(forPair==NULL)break;
		}
		destroyEnv(forValRept);
	}
	while(oriCptr!=NULL&&fmtCptr!=NULL)
	{
		const AST_cptr* origin=oriCptr;
		const AST_cptr* format=fmtCptr;
		AST_pair* forPair=(format->type==PAIR)?format->value:NULL;
		AST_pair* oriPair=(origin->type==PAIR)?origin->value:NULL;
		PreEnv* forValRept=newEnv(NULL);
		while(origin!=NULL&&format!=NULL)
		{
			if(format->type==PAIR&&origin->type==PAIR)
			{
				forPair=format->value;
				oriPair=origin->value;
				format=&forPair->car;
				origin=&oriPair->car;
				continue;
			}
			else if(format->type==ATM)
			{
				AST_atom* tmpAtm=format->value;
				if(tmpAtm->type==SYM)
				{
					PreMasym* tmpSym=findMasym(tmpAtm->value.str);
					const char* anotherName=hasAnotherName(tmpAtm->value.str);
					if(tmpSym==NULL||tmpSym->Func==M_VAREPT||!anotherName)
					{
						if(!AST_cptrcmp(origin,format))
						{
							destroyEnv(forValRept);
							return 0;
						}
					}
					else
					{
						if(tmpSym->Func(origin,format,anotherName,forValRept))
						{
							forPair=(format==&fmtCptr->outer->prev->car)?NULL:format->outer;
							int len=strlen(name)+strlen(anotherName)+2;
							char symName[len];
							strcpy(symName,name);
							strcat(symName,"#");
							strcat(symName,anotherName);
							PreDef* tmpDef=findDefine(symName,MacroEnv);
							if(tmpDef==NULL)tmpDef=addDefine(symName,&tmp,MacroEnv);
							addToList(&tmpDef->obj,&findDefine(anotherName,forValRept)->obj);
						}
						else break;
					}
				}
				if(forPair!=NULL&&format==&forPair->car)
				{
					origin=&oriPair->cdr;
					format=&forPair->cdr;
					continue;
				}
			}
			else if(origin->type!=format->type)break;
			if(forPair!=NULL&&format==&forPair->car)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
				continue;
			}
			else if(forPair!=NULL&&format==&forPair->cdr)
			{
				AST_pair* oriPrev=NULL;
				AST_pair* forPrev=NULL;
				if(oriPair->prev==NULL)break;
				while(oriPair->prev!=NULL&&forPair!=tmpPair)
				{
					oriPrev=oriPair;
					forPrev=forPair;
					oriPair=oriPair->prev;
					forPair=forPair->prev;
					if(oriPrev==oriPair->car.value)break;
				}
				if(oriPair!=NULL)
				{
					origin=&oriPair->cdr;
					format=&forPair->cdr;
				}
				if(forPair==tmpPair&&format==&forPair->cdr)break;
			}
			if(forPair==NULL)break;
		}
		destroyEnv(forValRept);
		addToList(&valreptdef->obj,oriCptr);
		const AST_cptr* tmp=oriCptr;
		oriCptr=nextCptr(tmp);
	}
	return 1;
}

int M_COLT(const AST_cptr* oriCptr,const AST_cptr* fmtCptr,const char* name,PreEnv* curEnv)
{
	int i;
	int num=getWordNum(name);
	AST_atom* tmpAtm=(oriCptr->type==ATM)?oriCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM)
		for(i=0;i<num;i++)
		{
			char* word=getWord(name,i+1);
			if(!strcmp(word,tmpAtm->value.str))
			{
				char* tmpName=getWord(name,0);
				addDefine(tmpName,oriCptr,curEnv);
				free(word);
				free(tmpName);
				return 1;
			}
		}
	return 0;
}

void deleteMacro(AST_cptr* objCptr)
{
	PreMacro* current=FirstMacro;
	PreMacro* prev=NULL;
	while(current!=NULL&&!AST_cptrcmp(objCptr,current->pattern))
	{
		prev=current;
		current=current->next;
	}
	if(current!=NULL)
	{
		if(current==FirstMacro)FirstMacro=current->next;
		deleteCptr(current->pattern);
		freeByteCode(current->proc);
		RawProc* curRawProc=current->procs;
		while(curRawProc)
		{
			RawProc* prev=curRawProc;
			curRawProc=curRawProc->next;
			freeByteCode(prev->proc);
			free(prev);
		}
		//deleteCptr(current->express);
		free(current->pattern);
		//free(current->express);
		if(prev!=NULL)prev->next=current->next;
		free(current);
	}
}

const char* hasAnotherName(const char* name)
{
	int len=strlen(name);
	const char* tmp=name;
	for(;tmp<name+len;tmp++)
		if(*tmp=='#'&&(*(tmp-1)!='#'||*(tmp+1)!='#'))break;
	return (tmp==name+len)?NULL:tmp+1;
}

int isVal(const char* name)
{
	if(name[0]=='$'&&strlen(name)>1)
		return 1;
	return 0;
}

void addToList(AST_cptr* fir,const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&((AST_pair*)fir->value)->cdr;
	fir->type=PAIR;
	fir->value=newPair(0,fir->outer);
	replace(&((AST_pair*)fir->value)->car,sec);
}

int getWordNum(const char* name)
{
	int i;
	int num=0;
	int len=strlen(name);
	for(i=0;i<len-1;i++)
		if(name[i]=='#')num++;
	return num;
}

char* getWord(const char* name,int num)
{
	int len=strlen(name);
	int tmpNum=0;
	int tmplen=0;
	char* tmpStr=NULL;
	int i;
	int j;
	for(i=0;i<len&&tmpNum<num;i++)
		if(name[i]=='#')tmpNum++;
	for(j=i;j<len;j++)
	{
		if(name[j]!='#')tmplen++;
		else break;
	}
	if(!(tmpStr=(char*)malloc(sizeof(char)*(tmplen+1))))errors("getWord",__FILE__,__LINE__);
	strncpy(tmpStr,name+i,tmplen);
	tmpStr[tmplen]='\0';
	return tmpStr;
}

PreDef* newDefines(const char* name)
{
	PreDef* tmp=(PreDef*)malloc(sizeof(PreDef));
	if(tmp==NULL)errors("newDefines",__FILE__,__LINE__);
	tmp->symName=(char*)malloc(sizeof(char)*(strlen(name)+1));
	if(tmp->symName==NULL)errors("newDefines",__FILE__,__LINE__);
	strcpy(tmp->symName,name);
	tmp->obj=(AST_cptr){NULL,0,NIL,NULL};
	tmp->next=NULL;
	return tmp;
}

void freeAllFunc()
{
	PreFunc* cur=funAndForm;
	while(cur!=NULL)
	{
		free(cur->functionName);
		PreFunc* prev=cur;
		cur=cur->next;
		free(prev);
	}
}

void freeMacroEnv()
{
	destroyEnv(MacroEnv);
	free(MacroEnv);
}

void freeAllRule()
{
	PreMasym* cur=FirstMasym;
	while(cur!=NULL)
	{
		PreMasym* prev=cur;
		cur=cur->next;
		free(prev->symName);
		free(prev);
	}
}

void unInitPreprocess()
{
	freeAllFunc();
	freeMacroEnv();
	freeAllRule();
	freeAllKeyWord();
	freeAllMacro();
}

void freeAllMacro()
{
	PreMacro* cur=FirstMacro;
	while(cur!=NULL)
	{
		PreMacro* prev=cur;
		cur=cur->next;
		deleteCptr(prev->pattern);
		//deleteCptr(prev->express);
		free(prev->pattern);
		//free(prev->express);
		freeByteCode(prev->proc);
		free(prev);
	}
}

CompEnv* createMacroCompEnv(const AST_cptr* objCptr,CompEnv* prev)
{
	CompEnv* tmpEnv=newCompEnv(prev);
	if(objCptr==NULL)return NULL;
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->value:NULL;
	AST_pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.value;
				objCptr=&objPair->car;
			}
			else
			{
				objPair=objCptr->value;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			if(objCptr->type!=NIL)
			{
				AST_atom* tmpAtm=objCptr->value;
				if(tmpAtm->type==SYM)
				{
					const char* tmpStr=tmpAtm->value.str;
					if(isVal(tmpStr))
						addCompDef(tmpEnv,tmpStr+1);
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
			AST_pair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.value||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	return tmpEnv;
}

VMenv* castPreEnvToVMenv(PreEnv* pe,int32_t b,VMenv* prev,VMheap* heap)
{
	int32_t size=0;
	PreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	VMenv* tmp=newVMenv(b,prev);
	tmp->size=size;
	VMvalue** values=(VMvalue**)malloc(sizeof(VMvalue*)*size);
	int i=size-1;
	for(tmpDef=pe->symbols;i>-1;i--)
	{
		values[i]=castCptrVMvalue(&tmpDef->obj,heap);
		tmpDef=tmpDef->next;
	}
	tmp->values=values;
	return tmp;
}
