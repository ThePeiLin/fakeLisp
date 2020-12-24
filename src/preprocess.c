#include"preprocess.h"
#include"compiler.h"
#include"tool.h"
#include"syntax.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"ast.h"
#include<string.h>
#include<math.h>

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
{
	if(pattern->type!=PAIR)return SYNTAXERROR;
	AST_cptr* tmpCptr=NULL;
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!MacroPatternCmp(pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		for(tmpCptr=&((AST_pair*)pattern->value)->car;tmpCptr!=NULL;tmpCptr=nextCptr(tmpCptr))
		{
			if(tmpCptr->type==ATM)
			{
				AST_atom* carAtm=tmpCptr->value;
				AST_atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.value:NULL;
				if(carAtm->type==SYM&&!isVal(carAtm->value.str))addKeyWord(carAtm->value.str);
				if(cdrAtm!=NULL&&cdrAtm->type==SYM&&!isVal(cdrAtm->value.str))addKeyWord(cdrAtm->value.str);
			}
		}
		if(!(current=(PreMacro*)malloc(sizeof(PreMacro))))errors("addMacro",__FILE__,__LINE__);
		current->next=FirstMacro;
		current->pattern=pattern;
		current->proc=proc;
		current->bound=bound;
		current->procs=procs;
		FirstMacro=current;
	}
	else
	{
		deleteCptr(current->pattern);
		free(current->pattern);
		freeByteCode(current->proc);
		RawProc* tmp=current->procs;
		while(tmp!=NULL)
		{
			RawProc* prev=tmp;
			tmp=tmp->next;
			freeByteCode(prev->proc);
			free(prev);
		}
		current->pattern=pattern;
		current->proc=proc;
		current->bound=bound;
		current->procs=procs;
	}
	//printAllKeyWord();
	return 0;
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
			if(tmpAtm->type==SYM)
			{
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
			if(forPair!=NULL&&format==&forPair->car)
			{
				origin=&oriPair->cdr;
				format=&forPair->cdr;
				continue;
			}
		}
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
		AST_cptr* tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],objCptr->curline,NULL);
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
		free(rawProcList);
		destroyEnv(MacroEnv);
		return 1;
	}
	return 0;
}

void initPreprocess()
{
	addFunc("defmacro",N_defmacro);
	addFunc("import",N_import);
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

int isVal(const char* name)
{
	if(name[0]=='$'&&strlen(name)>1)
		return 1;
	return 0;
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

void unInitPreprocess()
{
	freeAllFunc();
	freeMacroEnv();
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
		free(prev->pattern);
		freeByteCode(prev->proc);
		RawProc* tmp=prev->procs;
		while(tmp!=NULL)
		{
			RawProc* prev=tmp;
			tmp=tmp->next;
			freeByteCode(prev->proc);
			free(prev);
		}
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

AST_cptr** dealArg(AST_cptr* argCptr,int num)
{
	AST_cptr* tmpCptr=(argCptr->type==PAIR)?argCptr:NULL;
	AST_cptr** args=NULL;
	if(!(args=(AST_cptr**)malloc(num*sizeof(AST_cptr*))))errors("dealArg",__FILE__,__LINE__);
	int i;
	for(i=0;i<num;i++)
	{
		if(argCptr->type==NIL||argCptr->type==ATM)
			args[i]=newCptr(0,NULL);
		else
		{
			args[i]=newCptr(0,NULL);
			replace(args[i],&((AST_pair*)argCptr->value)->car);
			deleteCptr(&((AST_pair*)argCptr->value)->car);
			argCptr=&((AST_pair*)argCptr->value)->cdr;
		}
	}
	if(tmpCptr!=NULL)
	{
		AST_cptr* beDel=newCptr(0,NULL);
		beDel->type=tmpCptr->type;
		beDel->value=tmpCptr->value;
		((AST_pair*)tmpCptr->value)->prev=NULL;
		tmpCptr->type=argCptr->type;
		tmpCptr->value=argCptr->value;
		if(argCptr->type==PAIR)
			((AST_pair*)argCptr->value)->prev=tmpCptr->outer;
		else if(argCptr->type==ATM)
			((AST_atom*)argCptr->value)->prev=tmpCptr->outer;
		argCptr->type=NIL;
		argCptr->value=NULL;
		deleteCptr(beDel);
		free(beDel);
	}
	return args;
}

int coutArg(AST_cptr* argCptr)
{
	int num=0;
	while(argCptr->type==PAIR)
	{
		AST_cptr* tmp=&((AST_pair*)argCptr->value)->car;
		if(tmp->type!=NIL)num++;
		argCptr=&((AST_pair*)argCptr->value)->cdr;
	}
	return num;
}

void deleteArg(AST_cptr** args,int num)
{
	int i;
	for(i=0;i<num;i++)
	{
		deleteCptr(args[i]);
		free(args[i]);
	}
	free(args);
}

ErrorStatus N_import(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,1);
	if(args[0]->type!=ATM)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,1);
		return status;
	}
#ifdef _WIN32
	char* filetype=".dll";
#else
	char* filetype=".so";
#endif
	AST_atom* tmpAtom=args[0]->value;
	char* modname=(char*)malloc(sizeof(char)*(strlen(filetype)+strlen(tmpAtom->value.str)+1));
	strcpy(modname,tmpAtom->value.str);
	strcat(modname,filetype);
#ifdef _WIN32
	char* rp=_fullpath(NULL,modname,0);
#else
	char* rp=realpath(modname,0);
#endif
	if(rp==NULL)
	{
		perror(modname);
		exit(EXIT_FAILURE);
	}
	char* rep=relpath(getLastWorkDir(inter),rp);
	char* rmodname=(char*)malloc(sizeof(char)*(strlen(rep)-strlen(filetype)+1));
	if(!rmodname)errors("N_import",__FILE__,__LINE__);
	memcpy(rmodname,rep,strlen(rep)-strlen(filetype));
	rmodname[strlen(rep)-strlen(filetype)]='\0';
	if(!ModHasLoad(rmodname,*getpHead(inter)))
	{

		if(rp==NULL)
		{
			perror(rep);
			exit(EXIT_FAILURE);
		}
		Dlls** pDlls=getpDlls(inter);
		Modlist** pTail=getpTail(inter);
		Modlist** pHead=getpHead(inter);
		loadDll(rp,pDlls,rmodname,pTail);
		if(*pHead==NULL)*pHead=*pTail;
	}
	free(rmodname);
	free(modname);
	free(rp);
	free(rep);
	replace(objCptr,args[0]);
	deleteArg(args,1);
	return status;
}

ErrorStatus N_defmacro(AST_cptr* objCptr,PreEnv* curEnv,intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=PAIR)
	{
		status.status=SYNTAXERROR;
		status.place=newCptr(0,NULL);
		replace(status.place,args[0]);
		deleteArg(args,2);
		return status;
	}
	AST_cptr* pattern=args[0];
	AST_cptr* express=args[1];
	CompEnv* tmpGlobCompEnv=newCompEnv(NULL);
	initCompEnv(tmpGlobCompEnv);
	intpr* tmpInter=newTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=tmpGlobCompEnv;
	tmpInter->head=inter->head;
	tmpInter->tail=inter->tail;
	tmpInter->modules=inter->modules;
	tmpInter->curDir=inter->curDir;
	tmpInter->prev=NULL;
	CompEnv* tmpCompEnv=createMacroCompEnv(args[0],tmpGlobCompEnv);
	int32_t bound=(tmpCompEnv->symbols==NULL)?-1:tmpCompEnv->symbols->count;
	ByteCode* tmpByteCode=compile(args[1],tmpCompEnv,tmpInter,&status);
	if(!status.status)
	{
		addMacro(pattern,tmpByteCode,bound,tmpInter->procs);
		deleteCptr(express);
		free(express);
		free(args);
	}
	else
	{
		if(tmpByteCode)
			freeByteCode(tmpByteCode);
		exError(status.place,status.status,inter);
		deleteArg(args,2);
		status.place=NULL;
		status.status=0;
	}
	destroyCompEnv(tmpCompEnv);
	destroyCompEnv(tmpGlobCompEnv);
	objCptr->type=NIL;
	objCptr->value=NULL;
	free(tmpInter);
	return status;
}

int MacroPatternCmp(const AST_cptr* first,const AST_cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	AST_pair* firPair=NULL;
	AST_pair* secPair=NULL;
	AST_pair* tmpPair=(first->type==PAIR)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAIR)
		{
			firPair=first->value;
			secPair=second->value;
			first=&firPair->car;
			second=&secPair->car;
			continue;
		}
		else if(first->type==ATM||first->type==NIL)
		{
			if(first->type==ATM)
			{
				AST_atom* firAtm=first->value;
				AST_atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if(firAtm->type==SYM&&(!isVal(firAtm->value.str)||!isVal(secAtm->value.str))&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==STR&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==IN32&&firAtm->value.num!=secAtm->value.num)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTA&&!bytaArryEq(&firAtm->value.byta,&secAtm->value.byta))return 0;
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
			AST_pair* firPrev=NULL;
			AST_pair* secPrev=NULL;
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
				secPrev=secPair;
				firPair=firPair->prev;
				secPair=secPair->prev;
				if(firPrev==firPair->car.value)break;
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
