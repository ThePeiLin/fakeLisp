#include"compiler.h"
#include"syntax.h"
#include"common.h"
#include"opcode.h"
#include"ast.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"reader.h"
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<math.h>

static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
static int fmatcmp(const AST_cptr*,const AST_cptr*);
static int isVal(const char*);
static ErrorStatus N_import(AST_cptr*,PreEnv*,Intpr*);
static ErrorStatus N_defmacro(AST_cptr*,PreEnv*,Intpr*);
static CompEnv* createPatternCompEnv(char**,int32_t,CompEnv*,SymbolTable*);
static PreFunc* funAndForm=NULL;
static PreMacro* FirstMacro=NULL;
static PreEnv* MacroEnv=NULL;

PreMacro* PreMacroMatch(const AST_cptr* objCptr)
{
	PreMacro* current=FirstMacro;
	while(current!=NULL&&!fmatcmp(objCptr,current->pattern))
		current=current->next;
	return current;
}

int PreMacroExpand(AST_cptr* objCptr,Intpr* inter)
{
	PreMacro* tmp=PreMacroMatch(objCptr);
	if(tmp!=NULL)
	{
		VMenv* tmpGlob=newVMenv(NULL);
		ByteCode* rawProcList=castRawproc(NULL,tmp->procs);
		FakeVM* tmpVM=newTmpFakeVM(NULL,rawProcList);
		initGlobEnv(tmpGlob,tmpVM->heap,inter->table);
		VMcode* tmpVMcode=newVMcode(tmp->proc);
		VMenv* macroVMenv=castPreEnvToVMenv(MacroEnv,tmpGlob,tmpVM->heap,inter->table);
		tmpVM->mainproc->localenv=macroVMenv;
		tmpVMcode->localenv=macroVMenv;
		tmpVM->mainproc->code=tmpVMcode;
		tmpVM->modules=inter->modules;
		tmpVM->table=inter->table;
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
		MacroEnv=NULL;
		return 1;
	}
	return 0;
}

int addMacro(AST_cptr* pattern,ByteCode* proc,RawProc* procs)
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
		current->procs=procs;
	}
	//printAllKeyWord();
	return 0;
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
				if(firAtm->type==STR&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
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
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
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

void freeMacroEnv()
{
	destroyEnv(MacroEnv);
	free(MacroEnv);
	MacroEnv=NULL;
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
				{
					if(origin->type==ATM)
					{
						AST_atom* tmpAtm2=origin->value;
						if(tmpAtm2->type==SYM&&isKeyWord(tmpAtm2->value.str))
						{
							destroyEnv(MacroEnv);
							MacroEnv=NULL;
							return 0;
						}
					}
					addDefine(tmpAtm->value.str+1,origin,MacroEnv);
				}
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
			if(oriPair->prev==NULL)break;
			while(oriPair->prev!=NULL&&forPair!=tmpPair)
			{
				oriPrev=oriPair;
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

CompEnv* createMacroCompEnv(const AST_cptr* objCptr,CompEnv* prev,SymbolTable* table)
{
	if(objCptr==NULL)return NULL;
	CompEnv* tmpEnv=newCompEnv(prev);
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
						addCompDef(tmpStr+1,tmpEnv,table);
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

int isVal(const char* name)
{
	if(name[0]=='$'&&strlen(name)>1)
		return 1;
	return 0;
}

void addFunc(const char* name,ErrorStatus (*pFun)(AST_cptr*,PreEnv*,Intpr*))
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

ErrorStatus (*(findFunc(const char* name)))(AST_cptr*,PreEnv*,Intpr*)
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



ErrorStatus eval(AST_cptr* objCptr,PreEnv* curEnv,Intpr* inter)
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
				ErrorStatus (*pfun)(AST_cptr*,PreEnv*,Intpr*)=NULL;
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
	addKeyWord("begin");
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

void unInitPreprocess()
{
	freeAllStringPattern();
	freeAllFunc();
	freeMacroEnv();
	freeAllKeyWord();
	freeAllMacro();
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

ErrorStatus N_import(AST_cptr* objCptr,PreEnv* curEnv,Intpr* inter)
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

ErrorStatus N_defmacro(AST_cptr* objCptr,PreEnv* curEnv,Intpr* inter)
{
	ErrorStatus status={0,NULL};
	deleteCptr(objCptr);
	AST_cptr** args=dealArg(&objCptr->outer->cdr,2);
	if(args[0]->type!=PAIR)
	{
		if(args[0]->type!=NIL)
		{
			AST_atom* tmpAtom=args[0]->value;
			if(tmpAtom->type!=STR)
			{
				exError(args[0],SYNTAXERROR,inter);
				deleteArg(args,2);
				return status;
			}
			char* tmpStr=tmpAtom->value.str;
			if(isInValidStringPattern(tmpStr))
			{
				exError(args[0],INVALIDEXPR,inter);
				deleteArg(args,2);
				return status;
			}
			int32_t num=0;
			char** parts=splitPattern(tmpStr,&num);
			addStringPattern(parts,num,args[1],inter);
			freeStringArry(parts,num);
			deleteArg(args,2);
		}
		else
		{
			status.status=SYNTAXERROR;
			status.place=newCptr(0,NULL);
			replace(status.place,args[0]);
			deleteArg(args,2);
			return status;
		}
	}
	else
	{
		AST_cptr* pattern=args[0];
		AST_cptr* express=args[1];
		CompEnv* tmpGlobCompEnv=newCompEnv(NULL);
		initCompEnv(tmpGlobCompEnv,inter->table);
		Intpr* tmpInter=newTmpIntpr(NULL,NULL);
		tmpInter->filename=inter->filename;
		tmpInter->curline=inter->curline;
		tmpInter->glob=tmpGlobCompEnv;
		tmpInter->table=inter->table;
		tmpInter->head=inter->head;
		tmpInter->tail=inter->tail;
		tmpInter->modules=inter->modules;
		tmpInter->curDir=inter->curDir;
		tmpInter->prev=NULL;
		CompEnv* tmpCompEnv=createMacroCompEnv(pattern,tmpGlobCompEnv,inter->table);
		ByteCode* tmpByteCode=compile(express,tmpCompEnv,tmpInter,&status,1);
		if(!status.status)
		{
			addMacro(pattern,tmpByteCode,tmpInter->procs);
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
	}
	return status;
}

StringMatchPattern* addStringPattern(char** parts,int32_t num,AST_cptr* express,Intpr* inter)
{
	StringMatchPattern* tmp=NULL;
	ErrorStatus status={0,NULL};
	CompEnv* tmpGlobCompEnv=newCompEnv(NULL);
	initCompEnv(tmpGlobCompEnv,inter->table);
	Intpr* tmpInter=newTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->table=inter->table;
	tmpInter->curline=inter->curline;
	tmpInter->glob=tmpGlobCompEnv;
	tmpInter->head=inter->head;
	tmpInter->tail=inter->tail;
	tmpInter->modules=inter->modules;
	tmpInter->curDir=inter->curDir;
	tmpInter->procs=NULL;
	tmpInter->prev=NULL;
	CompEnv* tmpCompEnv=createPatternCompEnv(parts,num,tmpGlobCompEnv,inter->table);
	ByteCode* tmpByteCode=compile(express,tmpCompEnv,tmpInter,&status,1);
	if(!status.status)
	{
		tmp=newStringMatchPattern();
		tmp->num=num;
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		if(!tmParts)errors("addStringPattern",__FILE__,__LINE__);
		int i=0;
		for(;i<num;i++)
			tmParts[i]=copyStr(parts[i]);
		tmp->parts=tmParts;
		tmp->prev=NULL;
		tmp->procs=tmpInter->procs;
		tmp->proc=tmpByteCode;
	}
	else
	{
		if(tmpByteCode)
			freeByteCode(tmpByteCode);
		exError(status.place,status.status,inter);
		status.place=NULL;
		status.status=0;
	}
	destroyCompEnv(tmpCompEnv);
	destroyCompEnv(tmpGlobCompEnv);
	free(tmpInter);
	return tmp;
}

ByteCode* compile(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	for(;;)
	{
		if(isLoadExpression(objCptr))return compileLoad(objCptr,curEnv,inter,status,evalIm);
		if(isConst(objCptr))return compileConst(objCptr,curEnv,inter,status);
		if(isSymbol(objCptr))return compileSym(objCptr,curEnv,inter,status,evalIm);
		if(isDefExpression(objCptr))return compileDef(objCptr,curEnv,inter,status,evalIm);
		if(isSetqExpression(objCptr))return compileSetq(objCptr,curEnv,inter,status,evalIm);
		if(isSetfExpression(objCptr))return compileSetf(objCptr,curEnv,inter,status,evalIm);
		if(isCondExpression(objCptr))return compileCond(objCptr,curEnv,inter,status,evalIm);
		if(isAndExpression(objCptr))return compileAnd(objCptr,curEnv,inter,status,evalIm);
		if(isOrExpression(objCptr))return compileOr(objCptr,curEnv,inter,status,evalIm);
		if(isLambdaExpression(objCptr))return compileLambda(objCptr,curEnv,inter,status,evalIm);
		if(isBeginExpression(objCptr)) return compileBegin(objCptr,curEnv,inter,status,evalIm);
		if(PreMacroExpand(objCptr,inter))continue;
		else if(!isLegal(objCptr)||hasKeyWord(objCptr))
		{
			status->status=SYNTAXERROR;
			status->place=objCptr;
			return NULL;
		}
		else if(isFuncCall(objCptr))return compileFuncCall(objCptr,curEnv,inter,status,evalIm);
	}
}

CompEnv* createPatternCompEnv(char** parts,int32_t num,CompEnv* prev,SymbolTable* table)
{
	if(parts==NULL)return NULL;
	CompEnv* tmpEnv=newCompEnv(prev);
	int32_t i=0;
	for(;i<num;i++)
		if(isVar(parts[i]))
		{
			char* varName=getVarName(parts[i]);
			addCompDef(varName,tmpEnv,table);
			free(varName);
		}
	return tmpEnv;
}

ByteCode* compileAtom(AST_cptr* objCptr)
{
	AST_atom* tmpAtm=objCptr->value;
	ByteCode* tmp=NULL;
	switch((int)tmpAtm->type)
	{
		case SYM:
			tmp=createByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_SYM;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case STR:
			tmp=createByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_STR;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case IN32:
			tmp=createByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FAKE_PUSH_INT;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.num;
			break;
		case DBL:
			tmp=createByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FAKE_PUSH_DBL;
			*(double*)(tmp->code+1)=tmpAtm->value.dbl;
			break;
		case CHR:
			tmp=createByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FAKE_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
		case BYTA:
			tmp=createByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byta.size);
			tmp->code[0]=FAKE_PUSH_BYTE;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.byta.size;
			memcpy(tmp->code+5,tmpAtm->value.byta.arry,tmpAtm->value.byta.size);
			break;
	}
	return tmp;
}

ByteCode* compileNil()
{
	ByteCode* tmp=createByteCode(1);
	tmp->code[0]=FAKE_PUSH_NIL;
	return tmp;
}

ByteCode* compilePair(AST_cptr* objCptr)
{
	ByteCode* tmp=createByteCode(0);
	AST_pair* objPair=objCptr->value;
	AST_pair* tmpPair=objPair;
	ByteCode* popToCar=createByteCode(1);
	ByteCode* popToCdr=createByteCode(1);
	ByteCode* pushPair=createByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			codeCat(tmp,pushPair);
			objPair=objCptr->value;
			objCptr=&objPair->car;
			continue;
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			ByteCode* tmp1=(objCptr->type==ATM)?compileAtom(objCptr):compileNil();
			codeCat(tmp1,(objCptr==&objPair->car)?popToCar:popToCdr);
			codeCat(tmp,tmp1);
			freeByteCode(tmp1);
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
				codeCat(tmp,(prev==objPair->car.value)?popToCar:popToCdr);
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.value||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	freeByteCode(popToCar);
	freeByteCode(popToCdr);
	freeByteCode(pushPair);
	return tmp;
}

ByteCode* compileQuote(AST_cptr* objCptr)
{
	objCptr=&((AST_pair*)objCptr->value)->car;
	objCptr=nextCptr(objCptr);
	switch((int)objCptr->type)
	{
		case PAIR:return compilePair(objCptr);
		case ATM:return compileAtom(objCptr);
		case NIL:return compileNil();
	}
	return NULL;
}

ByteCode* compileConst(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status)
{
		if(objCptr->type==ATM)return compileAtom(objCptr);
		if(isNil(objCptr))return compileNil();
		if(isQuoteExpression(objCptr))return compileQuote(objCptr);
		return NULL;
}

ByteCode* compileFuncCall(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* headoflist=NULL;
	AST_pair* tmpPair=objCptr->value;
	ByteCode* tmp1=NULL;
	ByteCode* tmp=createByteCode(0);
	ByteCode* setBp=createByteCode(1);
	ByteCode* invoke=createByteCode(1);
	setBp->code[0]=FAKE_SET_BP;
	invoke->code[0]=FAKE_INVOKE;
	for(;;)
	{
		if(objCptr==NULL)
		{
			codeCat(tmp,invoke);
			if(headoflist->outer==tmpPair)break;
			objCptr=prevCptr(&headoflist->outer->prev->car);
			for(headoflist=&headoflist->outer->prev->car;prevCptr(headoflist)!=NULL;headoflist=prevCptr(headoflist));
			continue;
		}
		if(isDefExpression(objCptr))
		{
			status->status=INVALIDEXPR;
			status->place=objCptr;
			freeByteCode(tmp);
			tmp=NULL;
			break;
		}
		else if(isFuncCall(objCptr))
		{
			codeCat(tmp,setBp);
			headoflist=&((AST_pair*)objCptr->value)->car;
			for(objCptr=headoflist;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		}
		else
		{
			tmp1=compile(objCptr,curEnv,inter,status,evalIm&1);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				tmp=NULL;
				break;
			}
			codeCat(tmp,tmp1);
			freeByteCode(tmp1);
			objCptr=prevCptr(objCptr);
		}
	}
	freeByteCode(setBp);
	freeByteCode(invoke);
	return tmp;
}

ByteCode* compileDef(AST_cptr* tir,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_pair* tmpPair=tir->value;
	AST_cptr* fir=NULL;
	AST_cptr* sec=NULL;
	AST_cptr* objCptr=NULL;
	ByteCode* tmp=createByteCode(0);
	ByteCode* tmp1=NULL;
	ByteCode* pushTop=createByteCode(sizeof(char));
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t)*2);
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isDefExpression(tir))
	{
		fir=&((AST_pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	objCptr=tir;
	tmp1=(isLambdaExpression(objCptr))?
		compile(objCptr,curEnv,inter,status,0):
		compile(objCptr,curEnv,inter,status,evalIm);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
	for(;;)
	{
		AST_atom* tmpAtm=sec->value;
		CompDef* tmpDef=addCompDef(tmpAtm->value.str,curEnv,inter->table);
		*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
		*(int32_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
		codeCat(tmp,pushTop);
		codeCat(tmp,popVar);
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=prevCptr(tir);
			fir=prevCptr(sec);
		}
	}
	reCodeCat(tmp1,tmp);
	freeByteCode(tmp1);
	freeByteCode(popVar);
	freeByteCode(pushTop);
	return tmp;
}

ByteCode* compileSetq(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_pair* tmpPair=objCptr->value;
	AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
	AST_cptr* sec=nextCptr(fir);
	AST_cptr* tir=nextCptr(sec);
	ByteCode* tmp=createByteCode(0);
	ByteCode* tmp1=NULL;
	ByteCode* pushTop=createByteCode(sizeof(char));
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t)*2);
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(isSetqExpression(tir))
	{
		fir=&((AST_pair*)tir->value)->car;
		sec=nextCptr(fir);
		tir=nextCptr(sec);
	}
	tmp1=compile(tir,curEnv,inter,status,evalIm);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
	codeCat(tmp,tmp1);
	freeByteCode(tmp1);
	for(;;)
	{
		int32_t scope=0;
		int32_t id=0;
		AST_atom* tmpAtm=sec->value;
		CompDef* tmpDef=NULL;
		CompEnv* tmpEnv=curEnv;
		while(tmpEnv!=NULL)
		{
			tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
			if(tmpDef!=NULL)break;
			tmpEnv=tmpEnv->prev;
			scope++;
		}
		if(tmpDef==NULL)
		{
			SymTabNode* node=newSymTabNode(tmpAtm->value.str);
			addSymTabNode(node,inter->table);
			scope=-1;
			id=node->id;
		}
		else
			id=tmpDef->id;
		*(int32_t*)(popVar->code+sizeof(char))=scope;
		*(int32_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=id;
		codeCat(tmp,pushTop);
		codeCat(tmp,popVar);
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=prevCptr(tir);
			fir=prevCptr(sec);
		}
	}
	freeByteCode(pushTop);
	freeByteCode(popVar);
	return tmp;
}

ByteCode* compileSetf(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
	AST_cptr* sec=nextCptr(fir);
	AST_cptr* tir=nextCptr(sec);
	ByteCode* tmp=createByteCode(0);
	ByteCode* popRef=createByteCode(sizeof(char));
	popRef->code[0]=FAKE_POP_REF;
	ByteCode* tmp1=NULL;
	tmp1=compile(sec,curEnv,inter,status,evalIm);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(popRef);
		return NULL;
	}
	codeCat(tmp,tmp1);
	ByteCode* tmp2=compile(tir,curEnv,inter,status,evalIm);
	if(status->status!=0)
	{
		freeByteCode(tmp);
		freeByteCode(tmp1);
		freeByteCode(popRef);
		return NULL;
	}
	codeCat(tmp,tmp2);
	codeCat(tmp,popRef);
	freeByteCode(popRef);
	freeByteCode(tmp1);
	freeByteCode(tmp2);
	return tmp;
}

ByteCode* compileSym(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	ByteCode* pushVar=createByteCode(sizeof(char)+sizeof(int32_t));
	pushVar->code[0]=FAKE_PUSH_VAR;
	AST_atom* tmpAtm=objCptr->value;
	CompDef* tmpDef=NULL;
	CompEnv* tmpEnv=curEnv;
	int32_t id=0;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		if(evalIm)
		{
			status->status=SYMUNDEFINE;
			status->place=objCptr;
			freeByteCode(pushVar);
			return NULL;
		}
		SymTabNode* node=findSymbol(tmpAtm->value.str,inter->table);
		if(!node)
		{
			node=newSymTabNode(tmpAtm->value.str);
			addSymTabNode(node,inter->table);
		}
		id=node->id;
	}
	else
		id=tmpDef->id;
	*(int32_t*)(pushVar->code+sizeof(char))=id;
	return pushVar;
}

ByteCode* compileAnd(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	ByteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* push1=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pop=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	push1->code[0]=FAKE_PUSH_INT;
	pop->code[0]=FAKE_POP;
	*(int32_t*)(push1->code+sizeof(char))=1;
	for(objCptr=&((AST_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
	for(;prevCptr(objCptr)!=NULL;objCptr=prevCptr(objCptr))
	{
		if(isDefExpression(objCptr))
		{
			status->status=INVALIDEXPR;
			status->place=objCptr;
			freeByteCode(pop);
			freeByteCode(tmp);
			freeByteCode(jumpiffalse);
			freeByteCode(push1);
			return NULL;
		}
		ByteCode* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			freeByteCode(pop);
			freeByteCode(tmp);
			freeByteCode(jumpiffalse);
			freeByteCode(push1);
			return NULL;
		}
		*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->size;
		codeCat(tmp1,jumpiffalse);
		reCodeCat(pop,tmp1);
		reCodeCat(tmp1,tmp);
		freeByteCode(tmp1);
	}
	reCodeCat(push1,tmp);
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(push1);
	return tmp;
}

ByteCode* compileOr(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_pair* tmpPair=objCptr->value;
	ByteCode* jumpifture=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pushnil=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpifture->code[0]=FAKE_JMP_IF_TURE;
	while(objCptr!=NULL)
	{
		if(isOrExpression(objCptr))
		{
			for(objCptr=&((AST_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
			continue;
		}
		else if(prevCptr(objCptr)!=NULL)
		{
			if(isDefExpression(objCptr))
			{
				status->status=INVALIDEXPR;
				status->place=objCptr;
				freeByteCode(tmp);
				freeByteCode(jumpifture);
				freeByteCode(pushnil);
				return NULL;
			}
			ByteCode* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(jumpifture);
				freeByteCode(pushnil);
				return NULL;
			}
			reCodeCat(tmp1,tmp);
			freeByteCode(tmp1);
			*(int32_t*)(jumpifture->code+sizeof(char))=tmp->size;
			reCodeCat(jumpifture,tmp);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(objCptr)==NULL)
		{
			reCodeCat(pushnil,tmp);
			if(objCptr->outer==tmpPair)break;
			objCptr=prevCptr(&objCptr->outer->prev->car);
		}
	}
	freeByteCode(jumpifture);
	freeByteCode(pushnil);
	return tmp;
}

ByteCode* compileBegin(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* firCptr=nextCptr(getFirst(objCptr));
	ByteCode* tmp=createByteCode(0);
	while(firCptr)
	{
		ByteCode* tmp1=compile(firCptr,curEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			freeByteCode(tmp);
			return NULL;
		}
		codeCat(tmp,tmp1);
		firCptr=nextCptr(firCptr);
	}
	return tmp;
}

ByteCode* compileLambda(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* tmpCptr=objCptr;
	AST_pair* tmpPair=objCptr->value;
	AST_pair* objPair=NULL;
	RawProc* tmpRawProc=NULL;
	RawProc* prevRawProc=getHeadRawProc(inter);
	ByteCode* tmp=createByteCode(0);
	ByteCode* proc=createByteCode(0);
	CompEnv* tmpEnv=curEnv;
	ByteCode* popVar=createByteCode(sizeof(char)+sizeof(int32_t)*2);
	ByteCode* pushProc=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* endproc=createByteCode(sizeof(char));
	ByteCode* resTp=createByteCode(sizeof(char));
	ByteCode* resBp=createByteCode(sizeof(char));
	ByteCode* popRestVar=createByteCode(sizeof(char)+sizeof(int32_t)*2);
	endproc->code[0]=FAKE_END_PROC;
	popVar->code[0]=FAKE_POP_VAR;
	pushProc->code[0]=FAKE_PUSH_PROC;
	resBp->code[0]=FAKE_RES_BP;
	resTp->code[0]=FAKE_POP;
	popRestVar->code[0]=FAKE_POP_REST_VAR;
	for(;;)
	{
		if(objCptr==NULL)
		{
			*(int32_t*)(pushProc->code+sizeof(char))=tmpRawProc->count;
			ByteCode* tmp1=copyByteCode(pushProc);
			if(tmpRawProc->next==prevRawProc)
				codeCat(tmp,tmp1);
			else
				codeCat(tmpRawProc->next->proc,tmp1);
			codeCat(tmpRawProc->proc,endproc);
			freeByteCode(tmp1);
			CompEnv* prev=tmpEnv;
			tmpEnv=tmpEnv->prev;
			destroyCompEnv(prev);
			if(objPair!=tmpPair)
			{
				objPair=objPair->prev;
				objCptr=nextCptr(&objPair->car);
				while(objPair->prev!=NULL&&objPair->prev->cdr.value==objPair)objPair=objPair->prev;
				tmpRawProc=tmpRawProc->next;
				continue;
			}
			else break;
		}
		if(isLambdaExpression(objCptr))
		{
			tmpEnv=newCompEnv(tmpEnv);
			objPair=objCptr->value;
			objCptr=&objPair->car;
			tmpRawProc=addRawProc(proc,inter);
			if(nextCptr(objCptr)->type==PAIR)
			{
				AST_cptr* argCptr=&((AST_pair*)nextCptr(objCptr)->value)->car;
				while(argCptr!=NULL&&argCptr->type!=NIL)
				{
					AST_atom* tmpAtm=(argCptr->type==ATM)?argCptr->value:NULL;
					if(argCptr->type!=ATM||tmpAtm==NULL||tmpAtm->type!=SYM)
					{
						status->status=SYNTAXERROR;
						status->place=tmpCptr;
						freeByteCode(tmp);
						freeByteCode(endproc);
						freeByteCode(pushProc);
						freeByteCode(popVar);
						freeByteCode(proc);
						freeByteCode(resTp);
						freeByteCode(popRestVar);
						freeByteCode(resBp);
						inter->procs=prevRawProc;
						while(tmpRawProc!=prevRawProc)
						{
							RawProc* prev=tmpRawProc;
							tmpRawProc=tmpRawProc->next;
							freeByteCode(prev->proc);
							free(prev);
						}
						return NULL;
					}
					CompDef* tmpDef=addCompDef(tmpAtm->value.str,tmpEnv,inter->table);
					*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
					*(int32_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
					codeCat(tmpRawProc->proc,popVar);
					if(nextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==ATM)
					{
						AST_atom* tmpAtom1=(argCptr->outer->cdr.type==ATM)?argCptr->outer->cdr.value:NULL;
						if(tmpAtom1!=NULL&&tmpAtom1->type!=SYM)
						{
							status->status=SYNTAXERROR;
							status->place=tmpCptr;
							freeByteCode(tmp);
							freeByteCode(endproc);
							freeByteCode(pushProc);
							freeByteCode(popVar);
							freeByteCode(proc);
							freeByteCode(resTp);
							freeByteCode(popRestVar);
							freeByteCode(resBp);
							inter->procs=prevRawProc;
							while(tmpRawProc!=prevRawProc)
							{
								RawProc* prev=tmpRawProc;
								tmpRawProc=tmpRawProc->next;
								freeByteCode(prev->proc);
								free(prev);
							}
							return NULL;
						}
						tmpDef=addCompDef(tmpAtom1->value.str,tmpEnv,inter->table);
						*(int32_t*)(popRestVar->code+sizeof(char))=(int32_t)0;
						*(int32_t*)(popRestVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
						codeCat(tmpRawProc->proc,popRestVar);
					}
					argCptr=nextCptr(argCptr);
				}
			}
			else if(nextCptr(objCptr)->type==ATM)
			{
				AST_atom* tmpAtm=nextCptr(objCptr)->value;
				if(tmpAtm->type!=SYM)
				{
					status->status=SYNTAXERROR;
					status->place=tmpCptr;
					freeByteCode(tmp);
					freeByteCode(endproc);
					freeByteCode(pushProc);
					freeByteCode(popVar);
					freeByteCode(proc);
					freeByteCode(resTp);
					freeByteCode(popRestVar);
					freeByteCode(resBp);
					inter->procs=prevRawProc;
					while(tmpRawProc!=prevRawProc)
					{
						RawProc* prev=tmpRawProc;
						tmpRawProc=tmpRawProc->next;
						freeByteCode(prev->proc);
						free(prev);
					}
					return NULL;
				}
				CompDef* tmpDef=addCompDef(tmpAtm->value.str,tmpEnv,inter->table);
				*(int32_t*)(popRestVar->code+sizeof(char))=(int32_t)0;
				*(int32_t*)(popRestVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
				codeCat(tmpRawProc->proc,popRestVar);
			}
			codeCat(tmpRawProc->proc,resBp);
			objCptr=nextCptr(nextCptr(objCptr));
			continue;
		}
		else
		{
			ByteCode* tmp1=compile(objCptr,tmpEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(endproc);
				freeByteCode(pushProc);
				freeByteCode(popVar);
				freeByteCode(proc);
				freeByteCode(resTp);
				freeByteCode(popRestVar);
				freeByteCode(resBp);
				inter->procs=prevRawProc;
				while(tmpRawProc!=prevRawProc)
				{
					RawProc* prev=tmpRawProc;
					tmpRawProc=tmpRawProc->next;
					freeByteCode(prev->proc);
					free(prev);
				}
				return NULL;
			}
			if(nextCptr(objCptr)!=NULL)
				codeCat(tmp1,resTp);
			codeCat(tmpRawProc->proc,tmp1);
			freeByteCode(tmp1);
			objCptr=nextCptr(objCptr);
		}
	}
	freeByteCode(pushProc);
	freeByteCode(popVar);
	freeByteCode(proc);
	freeByteCode(resTp);
	freeByteCode(popRestVar);
	freeByteCode(resBp);
	freeByteCode(endproc);
	return tmp;
}

ByteCode* compileCond(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* cond=NULL;
	ByteCode* pushnil=createByteCode(sizeof(char));
	ByteCode* jumpiffalse=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* jump=createByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pop=createByteCode(sizeof(char));
	ByteCode* tmp=createByteCode(0);
	pop->code[0]=FAKE_POP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	for(cond=&((AST_pair*)objCptr->value)->car;nextCptr(cond)!=NULL;cond=nextCptr(cond));
	while(prevCptr(cond)!=NULL)
	{
		for(objCptr=&((AST_pair*)cond->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		ByteCode* tmpCond=createByteCode(0);
		while(objCptr!=NULL)
		{
			if(isDefExpression(objCptr))
			{
				status->status=INVALIDEXPR;
				status->place=objCptr;
				freeByteCode(tmp);
				freeByteCode(jumpiffalse);
				freeByteCode(jump);
				freeByteCode(pop);
				freeByteCode(tmpCond);
				freeByteCode(pushnil);
				return NULL;
			}
			ByteCode* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				freeByteCode(tmp);
				freeByteCode(jumpiffalse);
				freeByteCode(jump);
				freeByteCode(pop);
				freeByteCode(tmpCond);
				freeByteCode(pushnil);
				return NULL;
			}
			if(prevCptr(objCptr)==NULL)
			{
				*(int32_t*)(jumpiffalse->code+sizeof(char))=tmpCond->size+((nextCptr(cond)==NULL)?0:jump->size)+((objCptr->outer->cdr.type==NIL)?pushnil->size:0);
				codeCat(tmp1,jumpiffalse);
				if(objCptr->outer->cdr.type==NIL)
				{
					codeCat(tmp1,pop);
					codeCat(tmp1,pushnil);
				}
			}
			if(prevCptr(objCptr)!=NULL)
				reCodeCat(pop,tmp1);
			reCodeCat(tmp1,tmpCond);
			freeByteCode(tmp1);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(prevCptr(cond))!=NULL)
			reCodeCat(pop,tmpCond);
		if(nextCptr(cond)!=NULL)
		{
			*(int32_t*)(jump->code+sizeof(char))=tmp->size;
			codeCat(tmpCond,jump);
		}
		reCodeCat(tmpCond,tmp);
		freeByteCode(tmpCond);
		cond=prevCptr(cond);
	}
	freeByteCode(pushnil);
	freeByteCode(pop);
	freeByteCode(jumpiffalse);
	freeByteCode(jump);
	return tmp;
}

ByteCode* compileLoad(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
	AST_cptr* pFileName=nextCptr(fir);
	AST_atom* name=pFileName->value;
	if(hasLoadSameFile(name->value.str,inter))
	{
		status->status=CIRCULARLOAD;
		status->place=pFileName;
		return NULL;
	}
	FILE* file=fopen(name->value.str,"r");
	if(file==NULL)
	{
		perror(name->value.str);
		exit(EXIT_FAILURE);
	}
	Intpr* tmpIntpr=newIntpr(name->value.str,file,curEnv,inter->table);
	tmpIntpr->prev=inter;
	tmpIntpr->procs=NULL;
	tmpIntpr->glob=curEnv;
	tmpIntpr->head=NULL;
	tmpIntpr->tail=NULL;
	tmpIntpr->modules=NULL;
	ByteCode* tmp=compileFile(tmpIntpr,evalIm);
	chdir(tmpIntpr->prev->curDir);
	tmpIntpr->glob=NULL;
	freeIntpr(tmpIntpr);
	//printByteCode(tmp,stderr);
	return tmp;
}

ByteCode* compileFile(Intpr* inter,int evalIm)
{
	chdir(inter->curDir);
	char ch;
	ByteCode* tmp=createByteCode(0);
	ByteCode* resTp=createByteCode(1);
	resTp->code[0]=FAKE_RES_TP;
	char* prev=NULL;
	while((ch=getc(inter->file))!=EOF)
	{
		ungetc(ch,inter->file);
		AST_cptr* begin=NULL;
		StringMatchPattern* tmpPattern=NULL;
		char* list=readInPattern(inter->file,&tmpPattern,&prev);
		if(list==NULL)continue;
		ErrorStatus status={0,NULL};
		begin=createTree(list,inter,tmpPattern);
		if(begin!=NULL)
		{
			if(isPreprocess(begin))
			{
				if(isImportExpression(begin)&&tmp->size)
				{
					exError(begin,INVALIDEXPR,inter);
					exit(EXIT_FAILURE);
				}
				status=eval(begin,NULL,inter);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					deleteCptr(begin);
					exit(EXIT_FAILURE);
				}
			}
			else
			{
				ByteCode* tmpByteCode=compile(begin,inter->glob,inter,&status,evalIm);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					deleteCptr(status.place);
					freeByteCode(resTp);
					freeByteCode(tmp);
					exit(EXIT_FAILURE);
				}
				if(tmp->size)reCodeCat(resTp,tmpByteCode);
				codeCat(tmp,tmpByteCode);
				freeByteCode(tmpByteCode);
			}
			deleteCptr(begin);
			free(begin);
		}
		free(list);
		list=NULL;
	}
	freeByteCode(resTp);
	return tmp;
}
