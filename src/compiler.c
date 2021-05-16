#define USE_CODE_NAME

#include"compiler.h"
#include"syntax.h"
#include"common.h"
#include"opcode.h"
#include"ast.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"reader.h"
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#include<unistd.h>
#include<math.h>
#include<setjmp.h>

static jmp_buf buf;
static void errorCallBackForPreMacroExpand(void* a)
{
	int* i=(int*)a;
	longjmp(buf,i[0]);
}

static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
static int fmatcmp(const AST_cptr*,const AST_cptr*);
static int isVal(const char*);
static ErrorStatus N_defmacro(AST_cptr*,PreEnv*,Intpr*);
static CompEnv* createPatternCompEnv(char**,int32_t,CompEnv*,SymbolTable*);
static PreFunc* funAndForm=NULL;
static PreMacro* FirstMacro=NULL;
static PreEnv* MacroEnv=NULL;

static uint8_t findOpcode(const char* str)
{
	uint8_t i=0;
	uint32_t size=sizeof(codeName)/sizeof(codeinfor);
	for(;i<size;i++)
	{
		if(!strcmp(codeName[i].codeName,str))
			return i;
	}
	return 0;
}

static int isSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n)
{
	uint32_t l=0;
	uint32_t h=n-1;
	int32_t mid=0;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int resultOfCmp=strcmp(pStr[mid],str);
		if(resultOfCmp>0)
			h=mid-1;
		else if(resultOfCmp<0)
			l=mid+1;
		else
			return 1;
	}
	return 0;
}

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
		VMcode* tmpVMcode=newVMcode(tmp->proc,0);
		VMenv* macroVMenv=castPreEnvToVMenv(MacroEnv,tmpGlob,tmpVM->heap,inter->table);
		tmpVM->mainproc=newFakeProcess(tmpVMcode,NULL);
		tmpVM->mainproc->localenv=macroVMenv;
		tmpVM->curproc=tmpVM->mainproc;
		tmpVMcode->prevEnv=NULL;
		tmpVM->table=inter->table;
		tmpVM->callback=errorCallBackForPreMacroExpand;
		tmpVM->lnt=tmp->lnt;
		AST_cptr* tmpCptr=NULL;
		int i=runFakeVM(tmpVM);
		if(!i)
		{
			tmpCptr=castVMvalueToCptr(tmpVM->stack->values[0],objCptr->curline);
			replace(objCptr,tmpCptr);
			deleteCptr(tmpCptr);
			free(tmpCptr);
		}
		else if(i==1)
		{
			deleteCallChain(tmpVM);
				freeVMenv(tmpGlob);
			freeVMheap(tmpVM->heap);
			freeVMstack(tmpVM->stack);
			free(tmpVM);
			free(rawProcList);
			destroyEnv(MacroEnv);
			MacroEnv=NULL;
			return 2;
		}

		freeVMenv(tmpGlob);
		freeVMheap(tmpVM->heap);
		freeVMstack(tmpVM->stack);
		freeVMcode(tmpVMcode);
		free(tmpVM);
		free(rawProcList);
		destroyEnv(MacroEnv);
		MacroEnv=NULL;
		return 1;
	}
	return 0;
}

int addMacro(AST_cptr* pattern,ByteCode* proc,RawProc* procs,LineNumberTable* lnt)
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
		current->lnt=lnt;
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
		current->lnt=lnt;
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
				else if(firAtm->type==IN32&&firAtm->value.in32!=secAtm->value.in32)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTS&&!bytsStrEq(&firAtm->value.byts,&secAtm->value.byts))return 0;
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
		freeLineNumberTable(prev->lnt);
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
	addKeyWord("unquote");
	addKeyWord("qsquote");
	addKeyWord("unqtesp");
	addKeyWord("proc");
	addKeyWord("import");
	addKeyWord("library");
	addKeyWord("export");
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
		tmpInter->curDir=inter->curDir;
		tmpInter->prev=NULL;
		tmpInter->lnt=newLineNumTable();
		CompEnv* tmpCompEnv=createMacroCompEnv(pattern,tmpGlobCompEnv,inter->table);
		ByteCodelnt* tmpByteCodelnt=compile(express,tmpCompEnv,tmpInter,&status,1);
		if(!status.status)
		{
			addLineNumTabId(tmpByteCodelnt->l,tmpByteCodelnt->ls,0,tmpInter->lnt);
			addMacro(pattern,tmpByteCodelnt->bc,tmpInter->procs,tmpInter->lnt);
			deleteCptr(express);
			free(express);
			free(args);
			free(tmpByteCodelnt);
		}
		else
		{
			if(tmpByteCodelnt)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmpByteCodelnt->l,tmpByteCodelnt->ls);
				freeByteCodelnt(tmpByteCodelnt);
			}
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
	tmpInter->curDir=inter->curDir;
	tmpInter->procs=NULL;
	tmpInter->prev=NULL;
	tmpInter->lnt=newLineNumTable();
	CompEnv* tmpCompEnv=createPatternCompEnv(parts,num,tmpGlobCompEnv,inter->table);
	ByteCodelnt* tmpByteCodelnt=compile(express,tmpCompEnv,tmpInter,&status,1);
	if(!status.status)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		if(!tmParts)errors("addStringPattern",__FILE__,__LINE__);
		int32_t i=0;
		for(;i<num;i++)
			tmParts[i]=copyStr(parts[i]);
		addLineNumTabId(tmpByteCodelnt->l,tmpByteCodelnt->ls,0,tmpInter->lnt);
		tmp=newStringMatchPattern(num,tmParts,tmpByteCodelnt->bc,tmpInter->procs,tmpInter->lnt);
	}
	else
	{
		if(tmpByteCodelnt)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmpByteCodelnt->l,tmpByteCodelnt->ls);
			freeByteCodelnt(tmpByteCodelnt);
		}
		exError(status.place,status.status,inter);
		status.place=NULL;
		status.status=0;
	}
	destroyCompEnv(tmpCompEnv);
	destroyCompEnv(tmpGlobCompEnv);
	free(tmpByteCodelnt);
	free(tmpInter);
	return tmp;
}

ByteCodelnt* compile(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	for(;;)
	{
		if(isLoadExpression(objCptr))return compileLoad(objCptr,curEnv,inter,status,evalIm);
		if(isConst(objCptr))return compileConst(objCptr,curEnv,inter,status,evalIm);
		if(isUnquoteExpression(objCptr))return compileUnquote(objCptr,curEnv,inter,status,evalIm);
		if(isQsquoteExpression(objCptr))return compileQsquote(objCptr,curEnv,inter,status,evalIm);
		if(isSymbol(objCptr))return compileSym(objCptr,curEnv,inter,status,evalIm);
		if(isDefExpression(objCptr))return compileDef(objCptr,curEnv,inter,status,evalIm);
		if(isSetqExpression(objCptr))return compileSetq(objCptr,curEnv,inter,status,evalIm);
		if(isSetfExpression(objCptr))return compileSetf(objCptr,curEnv,inter,status,evalIm);
		if(isCondExpression(objCptr))return compileCond(objCptr,curEnv,inter,status,evalIm);
		if(isAndExpression(objCptr))return compileAnd(objCptr,curEnv,inter,status,evalIm);
		if(isOrExpression(objCptr))return compileOr(objCptr,curEnv,inter,status,evalIm);
		if(isLambdaExpression(objCptr))return compileLambda(objCptr,curEnv,inter,status,evalIm);
		if(isBeginExpression(objCptr)) return compileBegin(objCptr,curEnv,inter,status,evalIm);
		if(isProcExpression(objCptr)) return compileProc(objCptr,curEnv,inter,status,evalIm);
		if(isImportExpression(objCptr))return compileImport(objCptr,curEnv,inter,status,evalIm);
		if(isLibraryExpression(objCptr))return newByteCodelnt(newByteCode(0));
		if(isUnqtespExpression(objCptr)||isExportExpression(objCptr))
		{
			status->status=INVALIDEXPR;
			status->place=objCptr;
			return NULL;
		}
		int i=PreMacroExpand(objCptr,inter);
		if(i==1)
			continue;
		else if(i==2)
		{
			status->status=MACROEXPANDFAILED;
			status->place=objCptr;
			return NULL;
		}
		else if(!isValid(objCptr)||hasKeyWord(objCptr))
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
			tmp=newByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_SYM;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case STR:
			tmp=newByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_STR;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case IN32:
			tmp=newByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FAKE_PUSH_INT;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.in32;
			break;
		case DBL:
			tmp=newByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FAKE_PUSH_DBL;
			*(double*)(tmp->code+1)=tmpAtm->value.dbl;
			break;
		case CHR:
			tmp=newByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FAKE_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
		case BYTS:
			tmp=newByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size);
			tmp->code[0]=FAKE_PUSH_BYTE;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.byts.size;
			memcpy(tmp->code+5,tmpAtm->value.byts.str,tmpAtm->value.byts.size);
			break;
	}
	return tmp;
}

ByteCode* compileNil()
{
	ByteCode* tmp=newByteCode(1);
	tmp->code[0]=FAKE_PUSH_NIL;
	return tmp;
}

ByteCode* compilePair(AST_cptr* objCptr)
{
	ByteCode* tmp=newByteCode(0);
	AST_pair* objPair=objCptr->value;
	AST_pair* tmpPair=objPair;
	ByteCode* popToCar=newByteCode(1);
	ByteCode* popToCdr=newByteCode(1);
	ByteCode* pushPair=newByteCode(1);
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

ByteCodelnt* compileQsquote(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	objCptr=nextCptr(getFirstCptr(objCptr));
	if(objCptr->type==ATM)
		return compileConst(objCptr,curEnv,inter,status,evalIm);
	else if(isUnquoteExpression(objCptr))
		return compileUnquote(objCptr,curEnv,inter,status,evalIm);
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	AST_pair* objPair=objCptr->value;
	AST_pair* tmpPair=objPair;
	ByteCode* appd=newByteCode(1);
	ByteCode* popToCar=newByteCode(1);
	ByteCode* popToCdr=newByteCode(1);
	ByteCode* pushPair=newByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	appd->code[0]=FAKE_APPD;
	while(objCptr!=NULL)
	{
		if(isUnquoteExpression(objCptr))
		{
			ByteCodelnt* tmp1=compileUnquote(objCptr,curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeByteCodelnt(tmp);
				freeByteCode(appd);
				freeByteCode(popToCar);
				freeByteCode(popToCdr);
				freeByteCode(pushPair);
				return NULL;
			}
			codeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			codelntCat(tmp,tmp1);
			freeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(isUnqtespExpression(objCptr))
		{
			if(objCptr==&objPair->cdr)
			{
				freeByteCode(popToCar);
				freeByteCode(popToCdr);
				freeByteCode(pushPair);
				freeByteCode(appd);
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeByteCodelnt(tmp);
				status->status=INVALIDEXPR;
				status->place=objCptr;
				return NULL;
			}
			ByteCodelnt* tmp1=compile(nextCptr(getFirstCptr(objCptr)),curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeByteCodelnt(tmp);
				freeByteCode(appd);
				freeByteCode(popToCar);
				freeByteCode(popToCdr);
				freeByteCode(pushPair);
				return NULL;
			}
			codeCat(tmp1->bc,appd);
			tmp1->l[tmp1->ls-1]->cpc+=appd->size;
			codelntCat(tmp,tmp1);
			freeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(objCptr->type==PAIR)
		{
			if(!isUnqtespExpression(getFirstCptr(objCptr)))
			{
				codeCat(tmp->bc,pushPair);
				if(!tmp->l)
				{
					tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
					if(!tmp->l)
						errors("compileQsquote",__FILE__,__LINE__);
					tmp->ls=1;
					tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,pushPair->size,objCptr->curline);
				}
				else
					tmp->l[tmp->ls-1]->cpc+=pushPair->size;
			}
			objPair=objCptr->value;
			objCptr=&objPair->car;
			continue;
		}
		else if((objCptr->type==ATM||objCptr->type==NIL)&&(!isUnqtespExpression(&objPair->car)))
		{
			ByteCodelnt* tmp1=compileConst(objCptr,curEnv,inter,status,evalIm);
			codeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			codelntCat(tmp,tmp1);
			freeByteCodelnt(tmp1);
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
				if(prev!=NULL&&!isUnqtespExpression(&prev->car))
				{
					codeCat(tmp->bc,(prev==objPair->car.value)?popToCar:appd);
					tmp->l[tmp->ls-1]->cpc+=(prev==objPair->car.value)?popToCar->size:appd->size;
				}
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
	freeByteCode(appd);
	return tmp;
}

ByteCode* compileQuote(AST_cptr* objCptr)
{
	objCptr=&((AST_pair*)objCptr->value)->car;
	objCptr=nextCptr(objCptr);
	switch((int)objCptr->type)
	{
		case PAIR:
			return compilePair(objCptr);
		case ATM:
			return compileAtom(objCptr);
		case NIL:
			return compileNil();
	}
	return NULL;
}

ByteCodelnt* compileUnquote(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	objCptr=nextCptr(getFirstCptr(objCptr));
	return compile(objCptr,curEnv,inter,status,evalIm);
}

ByteCodelnt* compileConst(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	int32_t line=objCptr->curline;
	ByteCode* tmp=NULL;
	if(objCptr->type==ATM)tmp=compileAtom(objCptr);
	if(isNil(objCptr))tmp=compileNil();
	if(isQuoteExpression(objCptr))tmp=compileQuote(objCptr);
	if(!tmp)
		return NULL;
	ByteCodelnt* t=newByteCodelnt(tmp);
	LineNumTabNode* n=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,tmp->size,line);
	t->ls=1;
	t->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
	if(!t->l)
		errors("compileConst",__FILE__,__LINE__);
	t->l[0]=n;
	return t;
}

ByteCodelnt* compileFuncCall(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* headoflist=NULL;
	AST_pair* tmpPair=objCptr->value;
	int32_t line=objCptr->curline;
	ByteCodelnt* tmp1=NULL;
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	ByteCode* setBp=newByteCode(1);
	ByteCode* invoke=newByteCode(1);
	setBp->code[0]=FAKE_SET_BP;
	invoke->code[0]=FAKE_INVOKE;
	for(;;)
	{
		if(objCptr==NULL)
		{
			codeCat(tmp->bc,invoke);
			if(!tmp->l)
			{
				tmp->ls=1;
				tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
				if(!tmp->l)
					errors("compileFuncCall",__FILE__,__LINE__);
				tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,invoke->size,line);
			}
			else
				tmp->l[tmp->ls-1]->cpc+=invoke->size;
			if(headoflist->outer==tmpPair)break;
			objCptr=prevCptr(&headoflist->outer->prev->car);
			for(headoflist=&headoflist->outer->prev->car;prevCptr(headoflist)!=NULL;headoflist=prevCptr(headoflist));
			continue;
		}
		else if(isFuncCall(objCptr))
		{
			codeCat(tmp->bc,setBp);
			if(!tmp->l)
			{
				tmp->ls=1;
				tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
				if(!tmp->l)
					errors("compileFuncCall",__FILE__,__LINE__);
				tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,setBp->size,line);
			}
			else
				tmp->l[tmp->ls-1]->cpc+=setBp->size;
			headoflist=&((AST_pair*)objCptr->value)->car;
			for(objCptr=headoflist;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		}
		else
		{
			tmp1=compile(objCptr,curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeByteCodelnt(tmp);
				tmp=NULL;
				break;
			}
			codelntCat(tmp,tmp1);
			freeByteCodelnt(tmp1);
			objCptr=prevCptr(objCptr);
		}
	}
	freeByteCode(setBp);
	freeByteCode(invoke);
	return tmp;
}

ByteCodelnt* compileDef(AST_cptr* tir,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_pair* tmpPair=tir->value;
	AST_cptr* fir=NULL;
	AST_cptr* sec=NULL;
	AST_cptr* objCptr=NULL;
	ByteCodelnt* tmp1=NULL;
	ByteCode* pushTop=newByteCode(sizeof(char));
	ByteCode* popVar=newByteCode(sizeof(char)+sizeof(int32_t)*2);
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
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
	for(;;)
	{
		CompDef* tmpDef=NULL;
		AST_atom* tmpAtm=sec->value;
		if(curEnv->prefix&&isSymbolShouldBeExport(tmpAtm->value.str,curEnv->exp,curEnv->n))
		{
			char* symbolWithPrefix=(char*)malloc(sizeof(char)*(strlen(tmpAtm->value.str)+strlen(curEnv->prefix)+1));
			if(!symbolWithPrefix)
				errors("compileDef",__FILE__,__LINE__);
			sprintf(symbolWithPrefix,"%s%s",curEnv->prefix,tmpAtm->value.str);
			tmpDef=addCompDef(symbolWithPrefix,curEnv,inter->table);
			free(symbolWithPrefix);
		}
		else
			tmpDef=addCompDef(tmpAtm->value.str,curEnv,inter->table);
		*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
		*(int32_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
		codeCat(tmp1->bc,pushTop);
		codeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=prevCptr(tir);
			fir=prevCptr(sec);
		}
	}
	freeByteCode(popVar);
	freeByteCode(pushTop);
	return tmp1;
}

ByteCodelnt* compileSetq(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_pair* tmpPair=objCptr->value;
	AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
	AST_cptr* sec=nextCptr(fir);
	AST_cptr* tir=nextCptr(sec);
	ByteCodelnt* tmp1=NULL;
	ByteCode* pushTop=newByteCode(sizeof(char));
	ByteCode* popVar=newByteCode(sizeof(char)+sizeof(int32_t)*2);
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
		freeByteCode(popVar);
		freeByteCode(pushTop);
		return NULL;
	}
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
			if(tmpEnv->prefix&&!tmpDef)
			{
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*(strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1));
				if(!symbolWithPrefix)
					errors("compileSetq",__FILE__,__LINE__);
				sprintf(symbolWithPrefix,"%s%s",tmpEnv->prefix,tmpAtm->value.str);
				tmpDef=findCompDef(symbolWithPrefix,tmpEnv,inter->table);
				free(symbolWithPrefix);
			}
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
		codeCat(tmp1->bc,pushTop);
		codeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
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
	return tmp1;
}

ByteCodelnt* compileSetf(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
	AST_cptr* sec=nextCptr(fir);
	AST_cptr* tir=nextCptr(sec);
	ByteCodelnt* tmp1=compile(sec,curEnv,inter,status,evalIm);
	if(status->status!=0)
		return NULL;
	ByteCodelnt* tmp2=compile(tir,curEnv,inter,status,evalIm);
	if(status->status!=0)
	{
		FREE_ALL_LINE_NUMBER_TABLE(tmp1->l,tmp1->ls);
		freeByteCodelnt(tmp1);
		return NULL;
	}
	ByteCode* popRef=newByteCode(sizeof(char));
	popRef->code[0]=FAKE_POP_REF;
	codelntCat(tmp1,tmp2);
	codeCat(tmp1->bc,popRef);
	tmp1->l[tmp1->ls-1]->cpc+=popRef->size;
	freeByteCode(popRef);
	freeByteCodelnt(tmp2);
	return tmp1;
}

ByteCodelnt* compileSym(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	int32_t line=objCptr->curline;
	if(hasKeyWord(objCptr))
	{
		status->status=SYNTAXERROR;
		status->place=objCptr;
		return NULL;
	}
	ByteCode* pushVar=newByteCode(sizeof(char)+sizeof(int32_t));
	pushVar->code[0]=FAKE_PUSH_VAR;
	AST_atom* tmpAtm=objCptr->value;
	CompDef* tmpDef=NULL;
	CompEnv* tmpEnv=curEnv;
	int32_t id=0;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
		if(tmpEnv->prefix&&!tmpDef)
		{
			char* symbolWithPrefix=(char*)malloc(sizeof(char)*(strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1));
			if(!symbolWithPrefix)
				errors("compileSym",__FILE__,__LINE__);
			sprintf(symbolWithPrefix,"%s%s",tmpEnv->prefix,tmpAtm->value.str);
			tmpDef=findCompDef(symbolWithPrefix,tmpEnv,inter->table);
			free(symbolWithPrefix);
		}
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		SymTabNode* node=findSymbol(tmpAtm->value.str,inter->table);
		if(!node)
		{
			node=newSymTabNode(tmpAtm->value.str);
			addSymTabNode(node,inter->table);
		}
		id=node->id;
		if(evalIm)
		{
			status->status=SYMUNDEFINE;
			status->place=objCptr;
			freeByteCode(pushVar);
			return NULL;
		}
	}
	else
		id=tmpDef->id;
	*(int32_t*)(pushVar->code+sizeof(char))=id;
	ByteCodelnt* bcl=newByteCodelnt(pushVar);
	bcl->ls=1;
	bcl->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	if(!bcl->l)
		errors("compileSym",__FILE__,__LINE__);
	bcl->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,pushVar->size,line);
	return bcl;
}

ByteCodelnt* compileAnd(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	ByteCode* jumpiffalse=newByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* push1=newByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* resTp=newByteCode(sizeof(char));
	ByteCode* setTp=newByteCode(sizeof(char));
	ByteCode* popTp=newByteCode(sizeof(char));
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	push1->code[0]=FAKE_PUSH_INT;
	resTp->code[0]=FAKE_RES_TP;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	*(int32_t*)(push1->code+sizeof(char))=1;
	for(objCptr=&((AST_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
	for(;prevCptr(objCptr)!=NULL;objCptr=prevCptr(objCptr))
	{
		ByteCodelnt* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			freeByteCode(resTp);
			freeByteCode(popTp);
			freeByteCode(setTp);
			if(tmp->l)
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeByteCodelnt(tmp);
			freeByteCode(jumpiffalse);
			freeByteCode(push1);
			return NULL;
		}
		*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->bc->size;
		codeCat(tmp1->bc,jumpiffalse);
		tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
		reCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		reCodelntCat(tmp1,tmp);
		freeByteCodelnt(tmp1);
	}
	reCodeCat(push1,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		if(!tmp->l)
			errors("compileAnd",__FILE__,__LINE__);
		tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,tmp->bc->size,objCptr->curline);
	}
	else
	{
		tmp->l[0]->cpc+=push1->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,push1->size);
	}
	reCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
	codeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	freeByteCode(resTp);
	freeByteCode(popTp);
	freeByteCode(setTp);
	freeByteCode(jumpiffalse);
	freeByteCode(push1);
	//printByteCodelnt(tmp,inter->table,stderr);
	return tmp;
}

ByteCodelnt* compileOr(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	ByteCode* setTp=newByteCode(sizeof(char));
	ByteCode* popTp=newByteCode(sizeof(char));
	ByteCode* resTp=newByteCode(sizeof(char));
	ByteCode* jumpifture=newByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pushnil=newByteCode(sizeof(char));
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpifture->code[0]=FAKE_JMP_IF_TRUE;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	resTp->code[0]=FAKE_RES_TP;
	for(objCptr=&((AST_pair*)objCptr->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
	for(;prevCptr(objCptr)!=NULL;objCptr=prevCptr(objCptr))
	{
		ByteCodelnt* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			freeByteCode(resTp);
			freeByteCode(popTp);
			freeByteCode(setTp);
			if(tmp->l)
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeByteCodelnt(tmp);
			freeByteCode(jumpifture);
			freeByteCode(pushnil);
			return NULL;
		}
		*(int32_t*)(jumpifture->code+sizeof(char))=tmp->bc->size;
		codeCat(tmp1->bc,jumpifture);
		tmp1->l[tmp1->ls-1]->cpc+=jumpifture->size;
		reCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		reCodelntCat(tmp1,tmp);
		freeByteCodelnt(tmp1);
	}
	reCodeCat(pushnil,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		if(!tmp->l)
			errors("compileOr",__FILE__,__LINE__);
		tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,tmp->bc->size,objCptr->curline);
	}
	else
	{
		tmp->l[0]->cpc+=pushnil->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushnil->size);
	}
	reCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	codeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	freeByteCode(resTp);
	freeByteCode(popTp);
	freeByteCode(setTp);
	freeByteCode(jumpifture);
	freeByteCode(pushnil);
	return tmp;
}

ByteCodelnt* compileBegin(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* firCptr=nextCptr(getFirstCptr(objCptr));
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	ByteCode* resTp=newByteCode(1);
	ByteCode* setTp=newByteCode(1);
	ByteCode* popTp=newByteCode(1);
	resTp->code[0]=FAKE_RES_TP;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	while(firCptr)
	{
		ByteCodelnt* tmp1=compile(firCptr,curEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeByteCodelnt(tmp);
			freeByteCode(resTp);
			freeByteCode(setTp);
			freeByteCode(popTp);
			return NULL;
		}
		if(tmp->bc->size)
		{
			reCodeCat(resTp,tmp1->bc);
			tmp1->l[0]->cpc+=1;
			INCREASE_ALL_SCP(tmp1->l,tmp1->ls-1,resTp->size);
		}
		codelntCat(tmp,tmp1);
		freeByteCodelnt(tmp1);
		firCptr=nextCptr(firCptr);
	}
	reCodeCat(setTp,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
		if(!tmp->l)
			errors("compileBegin",__FILE__,__LINE__);
		tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,tmp->bc->size,objCptr->curline);
	}
	else
	{
		tmp->l[0]->cpc+=1;
		INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	}
	codeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	freeByteCode(setTp);
	freeByteCode(resTp);
	freeByteCode(popTp);
	return tmp;
}

ByteCodelnt* compileLambda(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	int32_t line=objCptr->curline;
	AST_cptr* tmpCptr=objCptr;
	AST_pair* objPair=NULL;
	CompEnv* tmpEnv=newCompEnv(curEnv);
	ByteCode* popArg=newByteCode(sizeof(char)+sizeof(int32_t)*2);
	ByteCode* popRestArg=newByteCode(sizeof(char)+sizeof(int32_t)*2);
	ByteCode* pArg=newByteCode(0);
	popArg->code[0]=FAKE_POP_ARG;
	popRestArg->code[0]=FAKE_POP_REST_ARG;
	objPair=objCptr->value;
	objCptr=&objPair->car;
	RawProc* curproc=getHeadRawProc(inter);
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
				freeByteCode(popArg);
				freeByteCode(popRestArg);
				destroyCompEnv(tmpEnv);
				return NULL;
			}
			CompDef* tmpDef=addCompDef(tmpAtm->value.str,tmpEnv,inter->table);
			*(int32_t*)(popArg->code+sizeof(char))=(int32_t)0;
			*(int32_t*)(popArg->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
			codeCat(pArg,popArg);
			if(nextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==ATM)
			{
				AST_atom* tmpAtom1=(argCptr->outer->cdr.type==ATM)?argCptr->outer->cdr.value:NULL;
				if(tmpAtom1!=NULL&&tmpAtom1->type!=SYM)
				{
					status->status=SYNTAXERROR;
					status->place=tmpCptr;
					freeByteCode(popArg);
					freeByteCode(popRestArg);
					destroyCompEnv(tmpEnv);
					return NULL;
				}
				tmpDef=addCompDef(tmpAtom1->value.str,tmpEnv,inter->table);
				*(int32_t*)(popRestArg->code+sizeof(char))=(int32_t)0;
				*(int32_t*)(popRestArg->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
				codeCat(pArg,popRestArg);
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
			freeByteCode(popArg);
			freeByteCode(popRestArg);
			destroyCompEnv(tmpEnv);
			return NULL;
		}
		CompDef* tmpDef=addCompDef(tmpAtm->value.str,tmpEnv,inter->table);
		*(int32_t*)(popRestArg->code+sizeof(char))=(int32_t)0;
		*(int32_t*)(popRestArg->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
		codeCat(pArg,popRestArg);
	}
	freeByteCode(popRestArg);
	freeByteCode(popArg);
	ByteCode* resBp=newByteCode(sizeof(char));
	resBp->code[0]=FAKE_RES_BP;
	ByteCode* setTp=newByteCode(sizeof(char));
	setTp->code[0]=FAKE_SET_TP;
	codeCat(pArg,resBp);
	codeCat(pArg,setTp);
	freeByteCode(resBp);
	freeByteCode(setTp);
	objCptr=nextCptr(nextCptr(objCptr));
	ByteCodelnt* codeInRawProc=newByteCodelnt(pArg);
	codeInRawProc->ls=1;
	codeInRawProc->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	if(!codeInRawProc->l)
		errors("compileLambda",__FILE__,__LINE__);
	codeInRawProc->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,pArg->size,line);
	ByteCode* resTp=newByteCode(sizeof(char));
	resTp->code[0]=FAKE_RES_TP;
	for(;objCptr;objCptr=nextCptr(objCptr))
	{
		ByteCodelnt* tmp1=compile(objCptr,tmpEnv,inter,status,evalIm);
		if(status->status!=0)
		{
			RawProc* tmpProc=getHeadRawProc(inter);
			while(tmpProc!=curproc)
			{
				RawProc* prev=tmpProc;
				tmpProc=tmpProc->next;
				freeByteCode(prev->proc);
				free(prev);
			}
			getFirstIntpr(inter)->procs=curproc;
			FREE_ALL_LINE_NUMBER_TABLE(codeInRawProc->l,codeInRawProc->ls);
			freeByteCode(resTp);
			freeByteCodelnt(codeInRawProc);
			destroyCompEnv(tmpEnv);
			return NULL;
		}
		if(nextCptr(objCptr)!=NULL)
		{
			codeCat(tmp1->bc,resTp);
			tmp1->l[tmp1->ls-1]->cpc+=resTp->size;
		}
		codelntCat(codeInRawProc,tmp1);
		freeByteCodelnt(tmp1);
	}
	freeByteCode(resTp);
	ByteCode* popTp=newByteCode(sizeof(char));
	popTp->code[0]=FAKE_POP_TP;
	codeCat(codeInRawProc->bc,popTp);
	codeInRawProc->l[codeInRawProc->ls-1]->cpc+=popTp->size;
	freeByteCode(popTp);
	int32_t pId=addRawProc(codeInRawProc->bc,inter)->count;
	ByteCode* pushProc=newByteCode(sizeof(char)+sizeof(int32_t));
	pushProc->code[0]=FAKE_PUSH_PROC;
	*(int32_t*)(pushProc->code+sizeof(char))=pId;
	addLineNumTabId(codeInRawProc->l,codeInRawProc->ls,pId+1,inter->lnt);
	freeByteCode(codeInRawProc->bc);
	free(codeInRawProc);
	ByteCodelnt* toReturn=newByteCodelnt(pushProc);
	toReturn->ls=1;
	toReturn->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	if(!toReturn->l)
		errors("compileLambda",__FILE__,__LINE__);
	toReturn->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,pushProc->size,line);
	destroyCompEnv(tmpEnv);
	return toReturn;
}

ByteCodelnt* compileCond(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* cond=NULL;
	ByteCode* pushnil=newByteCode(sizeof(char));
	ByteCode* jumpiffalse=newByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* jump=newByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* resTp=newByteCode(sizeof(char));
	ByteCode* setTp=newByteCode(sizeof(char));
	ByteCode* popTp=newByteCode(sizeof(char));
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	setTp->code[0]=FAKE_SET_TP;
	resTp->code[0]=FAKE_RES_TP;
	popTp->code[0]=FAKE_POP_TP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	for(cond=&((AST_pair*)objCptr->value)->car;nextCptr(cond)!=NULL;cond=nextCptr(cond));
	while(prevCptr(cond)!=NULL)
	{
		for(objCptr=&((AST_pair*)cond->value)->car;nextCptr(objCptr)!=NULL;objCptr=nextCptr(objCptr));
		ByteCodelnt* tmpCond=newByteCodelnt(newByteCode(0));
		while(objCptr!=NULL)
		{
			ByteCodelnt* tmp1=compile(objCptr,curEnv,inter,status,evalIm);
			if(status->status!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeByteCodelnt(tmp);
				freeByteCode(jumpiffalse);
				freeByteCode(jump);
				freeByteCode(resTp);
				freeByteCode(setTp);
				freeByteCode(popTp);
				FREE_ALL_LINE_NUMBER_TABLE(tmpCond->l,tmpCond->ls);
				freeByteCodelnt(tmpCond);
				freeByteCode(pushnil);
				return NULL;
			}
			if(prevCptr(objCptr)==NULL)
			{
				*(int32_t*)(jumpiffalse->code+sizeof(char))=tmpCond->bc->size+((nextCptr(cond)==NULL)?0:jump->size)+((objCptr->outer->cdr.type==NIL)?pushnil->size:0);
				codeCat(tmp1->bc,jumpiffalse);
				tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
				if(objCptr->outer->cdr.type==NIL)
				{
					codeCat(tmp1->bc,resTp);
					codeCat(tmp1->bc,pushnil);
					tmp1->l[tmp1->ls-1]->cpc+=resTp->size+pushnil->size;
				}
			}
			if(prevCptr(objCptr)!=NULL)
			{
				reCodeCat(resTp,tmp1->bc);
				tmp1->l[0]->cpc+=resTp->size;
				INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
			}
			reCodelntCat(tmp1,tmpCond);
			freeByteCodelnt(tmp1);
			objCptr=prevCptr(objCptr);
		}
		if(prevCptr(prevCptr(cond))!=NULL)
		{
			reCodeCat(resTp,tmpCond->bc);
			tmpCond->l[0]->cpc+=1;
			INCREASE_ALL_SCP(tmpCond->l+1,tmpCond->ls-1,resTp->size);
		}
		if(nextCptr(cond)!=NULL)
		{
			*(int32_t*)(jump->code+sizeof(char))=tmp->bc->size;
			codeCat(tmpCond->bc,jump);
			tmpCond->l[tmpCond->ls-1]->cpc+=jump->size;
		}
		reCodelntCat(tmpCond,tmp);
		freeByteCodelnt(tmpCond);
		cond=prevCptr(cond);
	}
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		if(!tmp->l)
			errors("compileCond",__FILE__,__LINE__);
		tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,0,objCptr->curline);
	}
	reCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
	codeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	freeByteCode(pushnil);
	freeByteCode(resTp);
	freeByteCode(setTp);
	freeByteCode(popTp);
	freeByteCode(jumpiffalse);
	freeByteCode(jump);
	return tmp;
}

ByteCodelnt* compileLoad(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
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
		return NULL;
	}
	SymTabNode* node=newSymTabNode(name->value.str);
	addSymTabNode(node,inter->table);
	Intpr* tmpIntpr=newIntpr(name->value.str,file,curEnv,inter->table,inter->lnt);
	tmpIntpr->prev=inter;
	tmpIntpr->procs=NULL;
	tmpIntpr->glob=curEnv;
	ByteCodelnt* tmp=compileFile(tmpIntpr,evalIm,NULL);
	chdir(tmpIntpr->prev->curDir);
	tmpIntpr->glob=NULL;
	tmpIntpr->table=NULL;
	tmpIntpr->lnt=NULL;
	freeIntpr(tmpIntpr);
	//printByteCode(tmp,stderr);
	ByteCode* setTp=newByteCode(1);
	ByteCode* popTp=newByteCode(1);
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	if(tmp)
	{
		reCodeCat(setTp,tmp->bc);
		codeCat(tmp->bc,popTp);
		tmp->l[0]->cpc+=setTp->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
		tmp->l[tmp->ls-1]->cpc+=popTp->size;

	}
	freeByteCode(popTp);
	freeByteCode(setTp);
	return tmp;
}

ByteCodelnt* compileFile(Intpr* inter,int evalIm,int* exitstatus)
{
	chdir(inter->curDir);
	char ch;
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	tmp->ls=1;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	if(!tmp->l)
		errors("compileFile",__FILE__,__LINE__);
	tmp->l[0]=newLineNumTabNode(findSymbol(inter->filename,inter->table)->id,0,0,1);
	ByteCode* resTp=newByteCode(1);
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
		int ch=getc(inter->file);
		if(!begin&&(list&&!(isAllSpace(list)&&ch==EOF)))
		{
			fprintf(stderr,"In file \"%s\",line %d\n",inter->filename,inter->curline);
			if(list&&!isAllSpace(list))
				fprintf(stderr,"%s:Invalid expression here.\n",list);
			else
				fprintf(stderr,"Can't create a valid object.\n");
			if(list)
			{
				if(prev)
					free(prev);
				free(list);
				list=NULL;
			}
			if(exitstatus)*exitstatus=INVALIDEXPR;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeByteCodelnt(tmp);
			tmp=NULL;
			break;
		}
		else if(!begin&&(isAllSpace(list)||ch==EOF))
		{
			if(list)
				free(list);
			break;
		}
		ungetc(ch,inter->file);
		if(begin!=NULL)
		{
			if(isPreprocess(begin))
			{
				status=eval(begin,NULL,inter);
				if(status.status!=0)
				{
					exError(status.place,status.status,inter);
					if(exitstatus)*exitstatus=status.status;
					deleteCptr(status.place);
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					freeByteCodelnt(tmp);
					free(list);
					deleteCptr(begin);
					free(begin);
					tmp=NULL;
					break;
				}
			}
			else
			{
				ByteCodelnt* tmpByteCodelnt=compile(begin,inter->glob,inter,&status,!isLambdaExpression(begin));
				if(!tmpByteCodelnt||status.status!=0)
				{
					if(status.status)
					{
						exError(status.place,status.status,inter);
						if(exitstatus)*exitstatus=status.status;
						deleteCptr(status.place);
					}
					if(tmpByteCodelnt==NULL&&!status.status&&exitstatus)*exitstatus=MACROEXPANDFAILED;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					freeByteCodelnt(tmp);
					free(list);
					deleteCptr(begin);
					free(begin);
					tmp=NULL;
					break;
				}
				if(tmp->bc->size)
				{
					reCodeCat(resTp,tmpByteCodelnt->bc);
					tmpByteCodelnt->l[0]->cpc+=resTp->size;
					INCREASE_ALL_SCP(tmpByteCodelnt->l+1,tmpByteCodelnt->ls-1,resTp->size);
				}
				codelntCat(tmp,tmpByteCodelnt);
				freeByteCodelnt(tmpByteCodelnt);
			}
			deleteCptr(begin);
			free(begin);
		}
		else
		{
			free(list);
			list=NULL;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeByteCodelnt(tmp);
			freeByteCode(resTp);
			return NULL;
		}
		free(list);
		list=NULL;
	}
	freeByteCode(resTp);
	return tmp;
}

#define GENERATE_LNT(BYTECODELNT,BYTECODE) {\
	BYTECODELNT=newByteCodelnt(BYTECODE);\
	(BYTECODELNT)->ls=1;\
	(BYTECODELNT)->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));\
	if((BYTECODELNT)->l==NULL)\
		errors("compileProc",__FILE__,__LINE__);\
	(BYTECODELNT)->l[0]=newLineNumTabNode(fid,0,(BYTECODE)->size,fir->curline);\
}

ByteCodelnt* compileProc(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
	AST_cptr* fir=nextCptr(getFirstCptr(objCptr));
	ByteCodelnt* tmp=NULL;

	for(;fir;fir=nextCptr(fir))
	{
		if(fir->type!=ATM)
		{
			status->place=objCptr;
			status->status=SYNTAXERROR;
			return NULL;
		}
	}
	
	int32_t fid=findSymbol(inter->filename,inter->table)->id;
	fir=nextCptr(getFirstCptr(objCptr));

	int32_t sizeOfByteCode=0;

	ByteCodeLabel* head=NULL;
	while(fir)
	{
		AST_atom* firAtm=fir->value;
		if(firAtm->value.str[0]==':')
		{
			addByteCodeLabel(newByteCodeLable(sizeOfByteCode,firAtm->value.str+1),&head);
			fir=nextCptr(fir);
			continue;
		}

		if(firAtm->value.str[0]=='$')
		{
			addCompDef(firAtm->value.str+1,curEnv,inter->table);
			fir=nextCptr(fir);
			continue;
		}

		uint8_t opcode=findOpcode(firAtm->value.str);

		if(opcode==0)
		{
			status->place=fir;
			status->status=SYNTAXERROR;
			destroyByteCodeLabelChain(head);
			return NULL;
		}

		if(codeName[opcode].len!=0&&nextCptr(fir)==NULL)
		{
			status->place=objCptr;
			status->status=SYNTAXERROR;
			destroyByteCodeLabelChain(head);
			return NULL;
		}

		switch(codeName[opcode].len)
		{
			case -3:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					int32_t scope=0;
					CompEnv* tmpEnv=curEnv;
					CompDef* tmpDef=NULL;

					if(tmpAtm->type!=SYM)
					{
						status->place=objCptr;
						status->status=SYNTAXERROR;
						destroyByteCodeLabelChain(head);
						return NULL;
					}

					while(tmpEnv!=NULL)
					{
						tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
						if(tmpDef!=NULL)break;
						tmpEnv=tmpEnv->prev;
						scope++;
					}

					if(!tmpDef)
					{
						status->place=tmpCptr;
						status->status=SYMUNDEFINE;
						destroyByteCodeLabelChain(head);
						return NULL;
					}
					sizeOfByteCode+=sizeof(char)+2*sizeof(int32_t);
					fir=nextCptr(tmpCptr);
				}
				break;
			case -2:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					if(tmpAtm->type!=BYTS)
					{
						status->place=tmpCptr;
						status->status=SYNTAXERROR;
						destroyByteCodeLabelChain(head);
						return NULL;
					}

					sizeOfByteCode+=sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size;
					fir=nextCptr(tmpCptr);
				}
				break;
			case -1:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					if(tmpAtm->type!=SYM&&tmpAtm->type!=STR)
					{
						status->place=tmpCptr;
						status->status=SYNTAXERROR;
						destroyByteCodeLabelChain(head);
						return NULL;
					}

					sizeOfByteCode+=sizeof(char)*2+strlen(tmpAtm->value.str);
					fir=nextCptr(tmpCptr);
				}
				break;
			case 0:
				{
					sizeOfByteCode+=sizeof(char);
					fir=nextCptr(fir);
				}
				break;
			case 1:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					if(tmpAtm->type!=CHR)
					{
						status->place=tmpCptr;
						status->status=SYNTAXERROR;
						destroyByteCodeLabelChain(head);
						return NULL;
					}

					sizeOfByteCode+=sizeof(char)*2;
					fir=nextCptr(tmpCptr);
				}
				break;
			case 4:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					if(opcode==FAKE_PUSH_VAR)
					{
						CompEnv* tmpEnv=curEnv;
						CompDef* tmpDef=NULL;
						while(tmpEnv!=NULL)
						{
							tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
							if(tmpDef!=NULL)break;
							tmpEnv=tmpEnv->prev;
						}
						if(!tmpDef)
						{
							status->place=tmpCptr;
							status->status=SYMUNDEFINE;
							destroyByteCodeLabelChain(head);
							return NULL;
						}
					}
					else if(opcode==FAKE_JMP||opcode==FAKE_JMP_IF_FALSE||opcode==FAKE_JMP_IF_TRUE)
					{
						if(tmpAtm->type!=SYM&&tmpAtm->type!=STR)
						{
							status->place=tmpCptr;
							status->status=SYNTAXERROR;
							destroyByteCodeLabelChain(head);
							return NULL;
						}
					}
					else
					{
						if(tmpAtm->type!=IN32)
						{
							status->place=tmpCptr;
							status->status=SYNTAXERROR;
							destroyByteCodeLabelChain(head);
							return NULL;
						}
					}
					sizeOfByteCode+=sizeof(char)+sizeof(int32_t);
					fir=nextCptr(tmpCptr);
				}
				break;
			case 8:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					if(tmpAtm->type!=DBL)
					{
						status->place=tmpCptr;
						status->status=SYNTAXERROR;
						destroyByteCodeLabelChain(head);
						return NULL;
					}

					sizeOfByteCode+=sizeof(char)+sizeof(double);
					fir=nextCptr(tmpCptr);
				}
				break;
		}
	}

	fir=nextCptr(getFirstCptr(objCptr));
	tmp=newByteCodelnt(newByteCode(0));
	while(fir)
	{
		AST_atom* firAtm=fir->value;
		if(firAtm->value.str[0]==':'||firAtm->value.str[0]=='$')
		{
			fir=nextCptr(fir);
			continue;
		}
		uint8_t opcode=findOpcode(firAtm->value.str);

		ByteCodelnt* tmpByteCodelnt=NULL;
		switch(codeName[opcode].len)
		{
			case -3:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					int32_t scope=0;
					CompEnv* tmpEnv=curEnv;
					CompDef* tmpDef=NULL;

					while(tmpEnv!=NULL)
					{
						tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
						if(tmpDef!=NULL)break;
						tmpEnv=tmpEnv->prev;
						scope++;
					}

					ByteCode* tmpByteCode=newByteCode(sizeof(char)+2*sizeof(int32_t));
					tmpByteCode->code[0]=opcode;
					*((int32_t*)(tmpByteCode->code+sizeof(char)))=scope;
					*((int32_t*)(tmpByteCode->code+sizeof(char)+sizeof(int32_t)))=tmpDef->id;

					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
			case -2:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;

					ByteCode* tmpByteCode=newByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size);
					tmpByteCode->code[0]=opcode;
					*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpAtm->value.byts.size;
					memcpy(tmpByteCode->code+sizeof(char)+sizeof(int32_t),tmpAtm->value.byts.str,tmpAtm->value.byts.size);

					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
			case -1:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;

					ByteCode* tmpByteCode=newByteCode(sizeof(char)*2+strlen(tmpAtm->value.str));
					tmpByteCode->code[0]=opcode;
					strcpy(tmpByteCode->code+sizeof(char),tmpAtm->value.str);

					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
			case 0:
				{
					ByteCode* tmpByteCode=newByteCode(sizeof(char));
					tmpByteCode->code[0]=opcode;

					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(fir);
				}
				break;
			case 1:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;

					ByteCode* tmpByteCode=newByteCode(sizeof(char)*2);

					tmpByteCode->code[0]=opcode;
					tmpByteCode->code[1]=tmpAtm->value.chr;
					
					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
			case 4:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;
					ByteCode* tmpByteCode=NULL;
					if(opcode==FAKE_PUSH_VAR)
					{
						CompEnv* tmpEnv=curEnv;
						CompDef* tmpDef=NULL;
						while(tmpEnv!=NULL)
						{
							tmpDef=findCompDef(tmpAtm->value.str,tmpEnv,inter->table);
							if(tmpDef!=NULL)break;
							tmpEnv=tmpEnv->prev;
						}
						tmpByteCode=newByteCode(sizeof(char)+sizeof(int32_t));
						tmpByteCode->code[0]=opcode;
						*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpDef->id;
					}
					else if(opcode==FAKE_JMP||opcode==FAKE_JMP_IF_TRUE||opcode==FAKE_JMP_IF_FALSE)
					{
						ByteCodeLabel* label=findByteCodeLable(tmpAtm->value.str,head);
						if(label==NULL)
						{
							status->place=tmpCptr;
							status->status=SYMUNDEFINE;
							freeByteCodelnt(tmp);
							destroyByteCodeLabelChain(head);
							return NULL;
						}
						tmpByteCode=newByteCode(sizeof(char)+sizeof(int32_t));
						tmpByteCode->code[0]=opcode;
						*((int32_t*)(tmpByteCode->code+sizeof(char)))=label->place-tmp->bc->size-5;
					}
					else
					{
						tmpByteCode=newByteCode(sizeof(char)+sizeof(int32_t));
						tmpByteCode->code[0]=opcode;
						*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpAtm->value.in32;
					}
					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
			case 8:
				{
					AST_cptr* tmpCptr=nextCptr(fir);
					AST_atom* tmpAtm=tmpCptr->value;

					ByteCode* tmpByteCode=newByteCode(sizeof(char)+sizeof(double));

					tmpByteCode->code[0]=opcode;
					*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpAtm->value.dbl;

					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
					fir=nextCptr(tmpCptr);
				}
				break;
		}
		codelntCat(tmp,tmpByteCodelnt);
		freeByteCodelnt(tmpByteCodelnt);
	}
	destroyByteCodeLabelChain(head);
	return tmp;
}

ByteCodelnt* compileImport(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorStatus* status,int evalIm)
{
#ifdef _WIN32
	char divstr[]="\\";
#else
	char divstr[]="/";
#endif

	FakeMemMenager* memMenager=newFakeMemMenager(32);
	char postfix[]=".fkl";
	ByteCodelnt* tmp=newByteCodelnt(newByteCode(0));
	pushFakeMem(tmp,(GenDestructor)freeByteCodelnt,memMenager);
	chdir(inter->curDir);
	int ch=0;
	char* prev=NULL;
	/* 获取文件路径和文件名
	 * 然后打开并查找第一个library表达式
	 */
	AST_cptr* plib=nextCptr(getFirstCptr(objCptr));
	for(;plib;plib=nextCptr(plib))
	{
		char* libPrefix=NULL;
		AST_cptr* pairOfpPartsOfPath=plib;
		AST_cptr* pPartsOfPath=getFirstCptr(plib);
		if(!pPartsOfPath)
		{
			status->status=SYNTAXERROR;
			status->place=plib;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeFakeMemMenager(memMenager);
			return NULL;
		}
		uint32_t count=0;
		const char** partsOfPath=(const char**)malloc(sizeof(const char*)*0);
		if(!partsOfPath)
			errors("compileImport",__FILE__,__LINE__);
		pushFakeMem(partsOfPath,free,memMenager);
		while(pPartsOfPath)
		{
			if(pPartsOfPath->type!=ATM)
			{
				status->status=SYNTAXERROR;
				status->place=plib;
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeFakeMemMenager(memMenager);
				return NULL;
			}
			AST_atom* tmpAtm=pPartsOfPath->value;
			if(tmpAtm->type!=STR&&tmpAtm->type!=SYM)
			{
				status->status=SYNTAXERROR;
				status->place=plib;
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeFakeMemMenager(memMenager);
				return NULL;
			}
			if(!strcmp(tmpAtm->value.str,"prefix")&&nextCptr(pPartsOfPath)->type==PAIR)
			{
				AST_cptr* tmpCptr=nextCptr(pPartsOfPath);
				if(!tmpCptr||tmpCptr->type!=PAIR)
				{
					status->status=SYNTAXERROR;
					status->place=objCptr;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeFakeMemMenager(memMenager);
					return NULL;
				}
				tmpCptr=nextCptr(tmpCptr);
				if(!tmpCptr||nextCptr(tmpCptr)||tmpCptr->type!=ATM)
				{
					status->status=SYNTAXERROR;
					status->place=plib;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				freeFakeMemMenager(memMenager);
					return NULL;
				}
				AST_atom* prefixAtom=tmpCptr->value;
				if(prefixAtom->type!=STR&&prefixAtom->type!=SYM)
				{
					status->status=SYNTAXERROR;
					status->place=plib;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					freeFakeMemMenager(memMenager);
					return NULL;
				}
				libPrefix=copyStr(prefixAtom->value.str);
				pairOfpPartsOfPath=nextCptr(pPartsOfPath);
				pPartsOfPath=getFirstCptr(pairOfpPartsOfPath);
				continue;
			}
			count++;
			partsOfPath=(const char**)reallocFakeMem(partsOfPath,realloc(partsOfPath,sizeof(const char*)*count),memMenager);
			if(!pPartsOfPath)
				errors("compileImport",__FILE__,__LINE__);
			partsOfPath[count-1]=tmpAtm->value.str;
			pPartsOfPath=nextCptr(pPartsOfPath);
		}
		uint32_t totalPathLength=0;
		uint32_t i=0;
		for(;i<count;i++)
			totalPathLength+=strlen(partsOfPath[i]);
		totalPathLength+=count+strlen(postfix);
		char* path=(char*)malloc(sizeof(char)*totalPathLength);
		if(!path)
			errors("compileImport",__FILE__,__LINE__);
		path[0]='\0';
		for(i=0;i<count;i++)
		{
			if(i>0)
				strcat(path,divstr);
			strcat(path,partsOfPath[i]);
		}
		strcat(path,postfix);
		FILE* fp=fopen(path,"r");
		if(!fp)
			errors("compileImport",__FILE__,__LINE__);
		if(hasLoadSameFile(path,inter))
		{
			status->status=CIRCULARLOAD;
			status->place=objCptr;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			freeFakeMemMenager(memMenager);
			return NULL;
		}
		SymTabNode* node=newSymTabNode(path);
		addSymTabNode(node,inter->table);
		Intpr* tmpInter=newIntpr(path,fp,newCompEnv(NULL),inter->table,inter->lnt);
		free(path);
		tmpInter->prev=inter;
		tmpInter->procs=NULL;
		ByteCode* resTp=newByteCode(1);
		ByteCode* setTp=newByteCode(1);
		ByteCode* popTp=newByteCode(1);
		resTp->code[0]=FAKE_RES_TP;
		setTp->code[0]=FAKE_SET_TP;
		popTp->code[0]=FAKE_POP_TP;
		pushFakeMem(resTp,(GenDestructor)freeByteCode,memMenager);
		pushFakeMem(setTp,(GenDestructor)freeByteCode,memMenager);
		pushFakeMem(popTp,(GenDestructor)freeByteCode,memMenager);
		ByteCodelnt* libByteCodelnt=newByteCodelnt(newByteCode(0));
		pushFakeMem(libByteCodelnt,(GenDestructor)freeByteCodelnt,memMenager);
		while((ch=getc(tmpInter->file))!=EOF)
		{
			ungetc(ch,tmpInter->file);
			AST_cptr* begin=NULL;
			StringMatchPattern* tmpPattern=NULL;
			char* list=readInPattern(tmpInter->file,&tmpPattern,&prev);
			if(list==NULL)continue;
			begin=createTree(list,tmpInter,tmpPattern);
			int ch=getc(tmpInter->file);
			if(!begin&&(list&&!(isAllSpace(list)&&ch==EOF)))
			{
				fprintf(stderr,"In file \"%s\",line %d\n",tmpInter->filename,tmpInter->curline);
				if(list&&!isAllSpace(list))
					fprintf(stderr,"%s:Invalid expression here.\n",list);
				else
					fprintf(stderr,"Can't create a valid object.\n");
				if(list)
				{
					if(prev)
						free(prev);
					free(list);
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					freeFakeMemMenager(memMenager);
					libByteCodelnt=NULL;
					tmp=NULL;
					list=NULL;
				}
				break;
			}
			else if(!begin&&(isAllSpace(list)||ch==EOF))
			{
				free(list);
				break;
			}
			ungetc(ch,tmpInter->file);
			if(begin!=NULL)
			{
				//查找library并编译library
				if(isLibraryExpression(begin))
				{
					AST_cptr* libName=nextCptr(getFirstCptr(begin));
					if(AST_cptrcmp(libName,pairOfpPartsOfPath))
					{
						AST_cptr* exportCptr=nextCptr(libName);
						if(!exportCptr||!isExportExpression(exportCptr))
						{
							exError(begin,INVALIDEXPR,tmpInter);
							deleteCptr(begin);
							free(begin);
							FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
							FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
							chdir(tmpInter->prev->curDir);
							tmpInter->table=NULL;
							tmpInter->lnt=NULL;
							freeIntpr(tmpInter);
							if(libPrefix)
								free(libPrefix);
							freeFakeMemMenager(memMenager);
							free(list);
							return NULL;
						}
						else
						{
							const char** exportSymbols=(const char**)malloc(sizeof(const char*)*0);
							pushFakeMem(exportSymbols,free,memMenager);
							if(!exportSymbols)
								errors("compileImport",__FILE__,__LINE__);
							AST_cptr* pExportSymbols=nextCptr(getFirstCptr(exportCptr));
							uint32_t num=0;
							for(;pExportSymbols;pExportSymbols=nextCptr(pExportSymbols))
							{
								if(pExportSymbols->type!=ATM
										||((AST_atom*)pExportSymbols->value)->type!=SYM)
								{
									exError(exportCptr,SYNTAXERROR,tmpInter);
									deleteCptr(begin);
									free(begin);
									FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
									chdir(tmpInter->prev->curDir);
									tmpInter->table=NULL;
									tmpInter->lnt=NULL;
									freeIntpr(tmpInter);
									if(libPrefix)
										free(libPrefix);
									freeFakeMemMenager(memMenager);
									free(list);
									return NULL;
								}
								AST_atom* pSymbol=pExportSymbols->value;
								num++;
								exportSymbols=(const char**)reallocFakeMem(exportSymbols,realloc(exportSymbols,sizeof(const char*)*num),memMenager);
								exportSymbols[num-1]=pSymbol->value.str;
							}
							qsort(exportSymbols,num,sizeof(const char*),(int (*)(const void*,const void*))strcmp);
							AST_cptr* pBody=nextCptr(nextCptr(libName));
							tmpInter->glob->prefix=libPrefix;
							tmpInter->glob->exp=exportSymbols;
							tmpInter->glob->n=num;
							for(;pBody;pBody=nextCptr(pBody))
							{
								ByteCodelnt* otherByteCodelnt=compile(pBody,tmpInter->glob,tmpInter,status,1);
								if(status->status)
								{
									exError(status->place,status->status,tmpInter);
									deleteCptr(begin);
									free(begin);
									FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
									FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
									chdir(tmpInter->prev->curDir);
									tmpInter->table=NULL;
									tmpInter->lnt=NULL;
									freeIntpr(tmpInter);
									status->status=0;
									status->place=NULL;
									if(libPrefix)
										free(libPrefix);
									freeFakeMemMenager(memMenager);
									free(list);
									return NULL;
								}
								if(libByteCodelnt->bc->size)
								{
									reCodeCat(resTp,otherByteCodelnt->bc);
									otherByteCodelnt->l[0]->cpc+=1;
									INCREASE_ALL_SCP(otherByteCodelnt->l,otherByteCodelnt->ls-1,resTp->size);
								}
								codelntCat(libByteCodelnt,otherByteCodelnt);
								freeByteCodelnt(otherByteCodelnt);
							}
							reCodeCat(setTp,libByteCodelnt->bc);
							if(!libByteCodelnt->l)
							{
								libByteCodelnt->ls=1;
								libByteCodelnt->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
								if(!libByteCodelnt->l)
									errors("compileImport",__FILE__,__LINE__);
								libByteCodelnt->l[0]=newLineNumTabNode(findSymbol(tmpInter->filename,tmpInter->table)->id,0,libByteCodelnt->bc->size,objCptr->curline);
							}
							else
							{
								libByteCodelnt->l[0]->cpc+=1;
								INCREASE_ALL_SCP(libByteCodelnt->l,libByteCodelnt->ls-1,setTp->size);
							}
							codeCat(libByteCodelnt->bc,popTp);
							libByteCodelnt->l[libByteCodelnt->ls-1]->cpc+=popTp->size;
							codelntCat(tmp,libByteCodelnt);
							deleteFakeMem(libByteCodelnt,memMenager);
							freeByteCodelnt(libByteCodelnt);
							libByteCodelnt=NULL;
						}
						AST_cptr* pExportSymbols=nextCptr(getFirstCptr(exportCptr));
						for(;pExportSymbols;pExportSymbols=nextCptr(pExportSymbols))
						{
							if(pExportSymbols->type!=ATM
									||((AST_atom*)pExportSymbols->value)->type!=SYM)
							{
								exError(exportCptr,SYNTAXERROR,tmpInter);
								deleteCptr(begin);
								free(begin);
								FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
								freeByteCodelnt(tmp);
								chdir(tmpInter->prev->curDir);
								tmpInter->table=NULL;
								tmpInter->lnt=NULL;
								freeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								freeFakeMemMenager(memMenager);
								free(list);
								return NULL;
							}
							AST_atom* pSymbol=pExportSymbols->value;
							CompDef* tmpDef=NULL;
							char* symbolWouldExport=NULL;
							if(libPrefix)
							{
								symbolWouldExport=(char*)malloc(sizeof(char)*(strlen(libPrefix)+strlen(pSymbol->value.str)+1));
								if(!symbolWouldExport)
									errors("compileImport",__FILE__,__LINE__);
								sprintf(symbolWouldExport,"%s%s",libPrefix,pSymbol->value.str);
							}
							else
							{
								symbolWouldExport=(char*)malloc(sizeof(char)*(strlen(pSymbol->value.str)+1));
								if(!symbolWouldExport)
									errors("compileImport",__FILE__,__LINE__);
								strcpy(symbolWouldExport,pSymbol->value.str);
							}
							tmpDef=findCompDef(symbolWouldExport,tmpInter->glob,inter->table);
							if(!tmpDef)
							{
								exError(pExportSymbols,SYMUNDEFINE,tmpInter);
								deleteCptr(begin);
								free(begin);
								FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
								chdir(tmpInter->prev->curDir);
								tmpInter->table=NULL;
								tmpInter->lnt=NULL;
								freeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								free(symbolWouldExport);
								freeFakeMemMenager(memMenager);
								free(list);
								return NULL;
							}
							else
								addCompDef(symbolWouldExport,curEnv,inter->table);
							free(symbolWouldExport);
						}
						deleteCptr(begin);
						free(begin);
						free(list);
						break;
					}
				}
				deleteCptr(begin);
				free(begin);
			}
			else
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				if(libPrefix)
					free(libPrefix);
				freeFakeMemMenager(memMenager);
				return NULL;
			}
			free(list);
			list=NULL;
		}

		if(libByteCodelnt&&!libByteCodelnt->bc->size)
		{
			status->status=(tmp)?LIBUNDEFINED:0;
			status->place=(tmp)?pairOfpPartsOfPath:NULL;
			FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			tmpInter->table=NULL;
			tmpInter->lnt=NULL;
			freeIntpr(tmpInter);
			if(libPrefix)
				free(libPrefix);
			freeFakeMemMenager(memMenager);
			return NULL;
		}
		chdir(tmpInter->prev->curDir);
		tmpInter->table=NULL;
		tmpInter->lnt=NULL;
		freeIntpr(tmpInter);
		if(libPrefix)
			free(libPrefix);
	}
	if(tmp)
	{
		deleteFakeMem(tmp,memMenager);
		freeFakeMemMenager(memMenager);
	}
	return tmp;
}
