#include"preprocess.h"
#include"compiler.h"
#include"tool.h"
#include"syntax.h"
#include"VMtool.h"
#include"fakeVM.h"
#include"ast.h"
#include<string.h>
#include<math.h>



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

void addToList(AST_cptr* fir,const const AST_cptr* sec)
{
	while(fir->type!=NIL)fir=&((AST_pair*)fir->value)->cdr;
	fir->type=PAIR;
	fir->value=newPair(sec->curline,fir->outer);
	replace(&((AST_pair*)fir->value)->car,sec);
}
