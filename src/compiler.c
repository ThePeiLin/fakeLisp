#include<fakeLisp/compiler.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/common.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<fakeLisp/VMtool.h>
#include<fakeLisp/fakeVM.h>
#include<fakeLisp/reader.h>
#include<stddef.h>
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#include<unistd.h>
#include<math.h>

extern char* InterpreterPath;
static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
static int fmatcmp(const AST_cptr*,const AST_cptr*,PreEnv**,CompEnv*);
static int isVal(const char*);
static int fklAddDefinedMacro(PreMacro* macro,CompEnv* curEnv);
static ErrorState defmacro(AST_cptr*,CompEnv*,Intpr*);
static CompEnv* createPatternCompEnv(char**,int32_t,CompEnv*);
extern void READER_MACRO_quote(FakeVM* exe);
extern void READER_MACRO_qsquote(FakeVM* exe);
extern void READER_MACRO_unquote(FakeVM* exe);
extern void READER_MACRO_unqtesp(FakeVM* exe);

static VMenv* genGlobEnv(CompEnv* cEnv,ByteCodelnt* t,VMheap* heap)
{
	ComStack* stack=fklNewComStack(32);
	CompEnv* tcEnv=cEnv;
	for(;tcEnv;tcEnv=tcEnv->prev)
		fklPushComStack(tcEnv,stack);
	VMenv* preEnv=NULL;
	VMenv* vEnv=NULL;
	uint32_t bs=t->bc->size;
	while(!fklIsComStackEmpty(stack))
	{
		CompEnv* curEnv=fklPopComStack(stack);
		vEnv=fklNewVMenv(preEnv);
		if(!preEnv)
			fklInitGlobEnv(vEnv,heap);
		preEnv=vEnv;
		ByteCodelnt* tmpByteCode=curEnv->proc;
		fklCodelntCopyCat(t,tmpByteCode);
		FakeVM* tmpVM=fklNewTmpFakeVM(NULL);
		VMproc* tmpVMproc=fklNewVMproc(bs,tmpByteCode->bc->size);
		bs+=tmpByteCode->bc->size;
		VMrunnable* mainrunnable=fklNewVMrunnable(tmpVMproc);
		mainrunnable->localenv=vEnv;
		fklPushComStack(mainrunnable,tmpVM->rstack);
		tmpVM->code=t->bc->code;
		tmpVM->size=t->bc->size;
		tmpVMproc->prevEnv=NULL;
		tmpVM->lnt=fklNewLineNumTable();
		tmpVM->lnt->num=t->ls;
		tmpVM->lnt->list=t->l;
		fklFreeVMheap(tmpVM->heap);
		tmpVM->heap=heap;
		fklIncreaseVMenvRefcount(vEnv);
		int i=fklRunFakeVM(tmpVM);
		if(i==1)
		{
			free(tmpVM->lnt);
			fklDeleteCallChain(tmpVM);
			VMenv* tmpEnv=vEnv->prev;
			for(;tmpEnv;tmpEnv=tmpEnv->prev)
				fklDecreaseVMenvRefcount(tmpEnv);
			fklFreeVMenv(vEnv);
			fklFreeVMstack(tmpVM->stack);
			fklFreeVMproc(tmpVMproc);
			fklFreeComStack(tmpVM->rstack);
			fklFreeComStack(tmpVM->tstack);
			free(tmpVM);
			fklFreeComStack(stack);
			return NULL;
		}
		free(tmpVM->lnt);
		fklFreeVMstack(tmpVM->stack);
		fklFreeVMproc(tmpVMproc);
		fklFreeComStack(tmpVM->rstack);
		fklFreeComStack(tmpVM->tstack);
		free(tmpVM);
	}
	fklFreeComStack(stack);
	VMenv* tmpEnv=vEnv->prev;
	for(;tmpEnv;tmpEnv=tmpEnv->prev)
		fklDecreaseVMenvRefcount(tmpEnv);
	return vEnv;
}

static int cmpString(const void* a,const void* b)
{
	return strcmp(*(const char**)a,*(const char**)b);
}

//static int cmpByteCodeLabel(const void* a,const void* b)
//{
//	return strcmp(((const ByteCodeLabel*)a)->label,((const ByteCodeLabel*)b)->label);
//}
//
//static uint8_t findOpcode(const char* str)
//{
//	uint8_t i=0;
//	uint32_t size=sizeof(codeName)/sizeof(codeinfor);
//	for(;i<size;i++)
//	{
//		if(!strcmp(codeName[i].codeName,str))
//			return i;
//	}
//	return 0;
//}

PreMacro* fklPreMacroMatch(const AST_cptr* objCptr,PreEnv** pmacroEnv,CompEnv* curEnv,CompEnv** pCEnv)
{
	for(;curEnv;curEnv=curEnv->prev)
	{
		PreMacro* current=curEnv->macro;
		for(;current;current=current->next)
			if(fmatcmp(objCptr,current->pattern,pmacroEnv,curEnv))
			{
				*pCEnv=curEnv;
				return current;
			}
	}
	return NULL;
}

int fklPreMacroExpand(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter)
{
	PreEnv* macroEnv=NULL;
	ByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
	PreMacro* tmp=fklPreMacroMatch(objCptr,&macroEnv,curEnv,&curEnv);
	if(tmp!=NULL)
	{
		FakeVM* tmpVM=fklNewTmpFakeVM(NULL);
		VMenv* tmpGlob=genGlobEnv(tmp->macroEnv,t,tmpVM->heap);
		if(!tmpGlob)
		{
			fklDestroyEnv(macroEnv);
			fklFreeVMstack(tmpVM->stack);
			fklFreeComStack(tmpVM->rstack);
			fklFreeVMheap(tmpVM->heap);
			free(tmpVM);
			return 2;
		}
		VMproc* tmpVMproc=fklNewVMproc(t->bc->size,tmp->proc->bc->size);
		fklCodelntCopyCat(t,tmp->proc);
		VMenv* macroVMenv=fklCastPreEnvToVMenv(macroEnv,tmpGlob,tmpVM->heap);
		fklDestroyEnv(macroEnv);
		VMrunnable* mainrunnable=fklNewVMrunnable(tmpVMproc);
		mainrunnable->localenv=macroVMenv;
		tmpVM->code=t->bc->code;
		tmpVM->size=t->bc->size;
		fklPushComStack(mainrunnable,tmpVM->rstack);
		tmpVMproc->prevEnv=NULL;
		tmpVM->lnt=fklNewLineNumTable();
		tmpVM->lnt->num=t->ls;
		tmpVM->lnt->list=t->l;
		AST_cptr* tmpCptr=NULL;
		int i=fklRunFakeVM(tmpVM);
		if(!i)
		{
			tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),objCptr->curline);
			fklReplaceCptr(objCptr,tmpCptr);
			fklDeleteCptr(tmpCptr);
			free(tmpCptr);
		}
		else
		{
			FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
			fklFreeByteCodelnt(t);
			free(tmpVM->lnt);
			fklDeleteCallChain(tmpVM);
			fklFreeVMenv(tmpGlob);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreeVMproc(tmpVMproc);
			fklFreeComStack(tmpVM->rstack);
			fklFreeComStack(tmpVM->tstack);
			free(tmpVM);
			macroEnv=NULL;
			return 2;
		}
		FREE_ALL_LINE_NUMBER_TABLE(t->l,t->ls);
		fklFreeByteCodelnt(t);
		free(tmpVM->lnt);
		fklFreeVMenv(tmpGlob);
		fklFreeVMheap(tmpVM->heap);
		fklFreeVMstack(tmpVM->stack);
		fklFreeVMproc(tmpVMproc);
		fklFreeComStack(tmpVM->rstack);
		fklFreeComStack(tmpVM->tstack);
		free(tmpVM);
		macroEnv=NULL;
		return 1;
	}
	fklFreeByteCodelnt(t);
	return 0;
}

static int fklAddDefinedMacro(PreMacro* macro,CompEnv* curEnv)
{
	AST_cptr* tmpCptr=NULL;
	PreMacro* current=curEnv->macro;
	AST_cptr* pattern=macro->pattern;
	while(current!=NULL&&!MacroPatternCmp(macro->pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		for(tmpCptr=&pattern->u.pair->car;tmpCptr!=NULL;tmpCptr=fklNextCptr(tmpCptr))
		{
			if(tmpCptr->type==ATM)
			{
				AST_atom* carAtm=tmpCptr->u.atom;
				AST_atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.u.atom:NULL;
				if(carAtm->type==SYM&&!isVal(carAtm->value.str))fklAddKeyWord(carAtm->value.str,curEnv);
				if(cdrAtm!=NULL&&cdrAtm->type==SYM&&!isVal(cdrAtm->value.str))fklAddKeyWord(cdrAtm->value.str,curEnv);
			}
		}
		PreMacro* current=(PreMacro*)malloc(sizeof(PreMacro));
		FAKE_ASSERT(current,"fklAddDefinedMacro",__FILE__,__LINE__);
		current->next=curEnv->macro;
		current->pattern=pattern;
		current->proc=macro->proc;
		current->macroEnv=macro->macroEnv;
		curEnv->macro=current;
	}
	else
	{
		fklDeleteCptr(current->pattern);
		fklDestroyCompEnv(current->macroEnv);
		FREE_ALL_LINE_NUMBER_TABLE(current->proc->l,current->proc->ls);
		fklFreeByteCodelnt(current->proc);
		current->pattern=macro->pattern;
		current->proc=macro->proc;
		current->macroEnv=macro->macroEnv;
	}
	return 0;
}

int fklAddMacro(AST_cptr* pattern,ByteCodelnt* proc,CompEnv* curEnv)
{
	if(pattern->type!=PAIR)return SYNTAXERROR;
	AST_cptr* tmpCptr=NULL;
	PreMacro* current=curEnv->macro;
	while(current!=NULL&&!MacroPatternCmp(pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		for(tmpCptr=&pattern->u.pair->car;tmpCptr!=NULL;tmpCptr=fklNextCptr(tmpCptr))
		{
			if(tmpCptr->type==ATM)
			{
				AST_atom* carAtm=tmpCptr->u.atom;
				AST_atom* cdrAtm=(tmpCptr->outer->cdr.type==ATM)?tmpCptr->outer->cdr.u.atom:NULL;
				if(carAtm->type==SYM&&!isVal(carAtm->value.str))fklAddKeyWord(carAtm->value.str,curEnv);
				if(cdrAtm!=NULL&&cdrAtm->type==SYM&&!isVal(cdrAtm->value.str))fklAddKeyWord(cdrAtm->value.str,curEnv);
			}
		}
		FAKE_ASSERT((current=(PreMacro*)malloc(sizeof(PreMacro))),"fklAddMacro",__FILE__,__LINE__);
		current->next=curEnv->macro;
		current->pattern=pattern;
		current->proc=proc;
		current->macroEnv=curEnv;
		curEnv->refcount+=1;
		curEnv->macro=current;
	}
	else
	{
		fklDeleteCptr(current->pattern);
		free(current->pattern);
		FREE_ALL_LINE_NUMBER_TABLE(current->proc->l,current->proc->ls);
		fklFreeByteCodelnt(current->proc);
		current->pattern=pattern;
		current->proc=proc;
	}
	//fklPrintAllKeyWord();
	return 0;
}

int MacroPatternCmp(const AST_cptr* first,const AST_cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	AST_pair* firPair=NULL;
	AST_pair* secPair=NULL;
	AST_pair* tmpPair=(first->type==PAIR)?first->u.pair:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAIR)
		{
			firPair=first->u.pair;
			secPair=second->u.pair;
			first=&firPair->car;
			second=&secPair->car;
			continue;
		}
		else if(first->type==ATM||first->type==NIL)
		{
			if(first->type==ATM)
			{
				AST_atom* firAtm=first->u.atom;
				AST_atom* secAtm=second->u.atom;
				if(firAtm->type!=secAtm->type)return 0;
				if(firAtm->type==SYM&&(!isVal(firAtm->value.str)||!isVal(secAtm->value.str))&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				if(firAtm->type==STR&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==IN32&&firAtm->value.in32!=secAtm->value.in32)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTS&&!fklEqByteString(&firAtm->value.byts,&secAtm->value.byts))return 0;
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

int fmatcmp(const AST_cptr* origin,const AST_cptr* format,PreEnv** pmacroEnv,CompEnv* curEnv)
{
	PreEnv* macroEnv=fklNewEnv(NULL);
	AST_pair* tmpPair=(format->type==PAIR)?format->u.pair:NULL;
	AST_pair* forPair=tmpPair;
	AST_pair* oriPair=(origin->type==PAIR)?origin->u.pair:NULL;
	while(origin!=NULL)
	{
		if(format->type==PAIR&&origin->type==PAIR)
		{
			forPair=format->u.pair;
			oriPair=origin->u.pair;
			format=&forPair->car;
			origin=&oriPair->car;
			continue;
		}
		else if(format->type==ATM)
		{
			AST_atom* tmpAtm=format->u.atom;
			if(tmpAtm->type==SYM)
			{
				if(isVal(tmpAtm->value.str))
				{
					if(origin->type==ATM)
					{
						AST_atom* tmpAtm2=origin->u.atom;
						if(tmpAtm2->type==SYM&&fklIsKeyWord(tmpAtm2->value.str,curEnv))
						{
							fklDestroyEnv(macroEnv);
							macroEnv=NULL;
							return 0;
						}
					}
					fklAddDefine(tmpAtm->value.str+1,origin,macroEnv);
				}
				else if(!fklAST_cptrcmp(origin,format))
				{
					fklDestroyEnv(macroEnv);
					macroEnv=NULL;
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
			fklDestroyEnv(macroEnv);
			macroEnv=NULL;
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
				if(oriPrev==oriPair->car.u.pair)break;
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
	*pmacroEnv=macroEnv;
	return 1;
}

CompEnv* fklCreateMacroCompEnv(const AST_cptr* objCptr,CompEnv* prev)
{
	if(objCptr==NULL)return NULL;
	CompEnv* tmpEnv=fklNewCompEnv(prev);
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->u.pair:NULL;
	AST_pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.u.pair;
				objCptr=&objPair->car;
			}
			else
			{
				objPair=objCptr->u.pair;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			if(objCptr->type!=NIL)
			{
				AST_atom* tmpAtm=objCptr->u.atom;
				if(tmpAtm->type==SYM)
				{
					const char* tmpStr=tmpAtm->value.str;
					if(isVal(tmpStr))
						fklAddCompDef(tmpStr+1,tmpEnv);
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
				if(prev==objPair->car.u.pair)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.u.pair||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	return tmpEnv;
}

int isVal(const char* name)
{
	if(name[0]=='?'&&strlen(name)>1)
		return 1;
	return 0;
}

int fklRetree(AST_cptr** fir,AST_cptr* sec)
{
	while(*fir!=sec)
	{
		AST_cptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
		fklReplaceCptr(preCptr,*fir);
		*fir=preCptr;
		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

void fklInitGlobKeyWord(CompEnv* glob)
{
	fklAddKeyWord("defmacro",glob);
	fklAddKeyWord("define",glob);
	fklAddKeyWord("setq",glob);
	fklAddKeyWord("quote",glob);
	fklAddKeyWord("cond",glob);
	fklAddKeyWord("and",glob);
	fklAddKeyWord("or",glob);
	fklAddKeyWord("lambda",glob);
	fklAddKeyWord("setf",glob);
	fklAddKeyWord("load",glob);
	fklAddKeyWord("begin",glob);
	fklAddKeyWord("unquote",glob);
	fklAddKeyWord("qsquote",glob);
	fklAddKeyWord("unqtesp",glob);
	//fklAddKeyWord("progn",glob);
	fklAddKeyWord("import",glob);
	fklAddKeyWord("library",glob);
	fklAddKeyWord("export",glob);
	fklAddKeyWord("try",glob);
	fklAddKeyWord("catch",glob);
	fklAddKeyWord("deftype",glob);
	fklAddKeyWord("getf",glob);
	fklAddKeyWord("szof",glob);
	fklAddKeyWord("flsym",glob);
}

void fklUnInitPreprocess()
{
	fklFreeAllStringPattern();
}

AST_cptr** dealArg(AST_cptr* argCptr,int num)
{
	AST_cptr** args=NULL;
	FAKE_ASSERT((args=(AST_cptr**)malloc(num*sizeof(AST_cptr*))),"dealArg",__FILE__,__LINE__);
	int i=0;
	for(;i<num;i++,argCptr=fklNextCptr(argCptr))
		args[i]=argCptr;
	return args;
}

ErrorState defmacro(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter)
{
	ErrorState state={0,NULL};
	AST_cptr* fir=fklGetFirstCptr(objCptr);
	AST_cptr* argCptr=fklNextCptr(fir);
	AST_cptr** args=dealArg(argCptr,2);
	if(args[0]->type!=PAIR)
	{
		if(args[0]->type!=NIL)
		{
			AST_atom* tmpAtom=args[0]->u.atom;
			if(tmpAtom->type!=STR)
			{
				fklExError(args[0],SYNTAXERROR,inter);
				free(args);
				return state;
			}
			char* tmpStr=tmpAtom->value.str;
			if(!fklIsReDefStringPattern(tmpStr)&&fklIsInValidStringPattern(tmpStr))
			{
				fklExError(args[0],INVALIDEXPR,inter);
				free(args);
				return state;
			}
			int32_t num=0;
			char** parts=fklSplitPattern(tmpStr,&num);
			if(fklIsReDefStringPattern(tmpStr))
				fklAddReDefStringPattern(parts,num,args[1],inter);
			else
				fklAddStringPattern(parts,num,args[1],inter);
			fklFreeStringArry(parts,num);
			free(args);
		}
		else
		{
			state.state=SYNTAXERROR;
			state.place=args[0];
			free(args);
			return state;
		}
	}
	else
	{
		AST_cptr* pattern=fklNewCptr(0,NULL);
		fklReplaceCptr(pattern,args[0]);
		AST_cptr* express=args[1];
		Intpr* tmpInter=fklNewTmpIntpr(NULL,NULL);
		tmpInter->filename=inter->filename;
		tmpInter->curline=inter->curline;
		tmpInter->glob=/*(curEnv->prev&&curEnv->prev->exp)?curEnv->prev:*/curEnv;
		tmpInter->curDir=inter->curDir;
		tmpInter->prev=NULL;
		tmpInter->lnt=NULL;
		CompEnv* tmpCompEnv=fklCreateMacroCompEnv(pattern,tmpInter->glob);
		ByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
		if(!state.state)
		{
			fklAddMacro(pattern,tmpByteCodelnt,curEnv);
			free(args);
		}
		else
		{
			if(tmpByteCodelnt)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmpByteCodelnt->l,tmpByteCodelnt->ls);
				fklFreeByteCodelnt(tmpByteCodelnt);
			}
			free(args);
		}
		fklDestroyCompEnv(tmpCompEnv);
		free(tmpInter);
	}
	return state;
}

StringMatchPattern* fklAddStringPattern(char** parts,int32_t num,AST_cptr* express,Intpr* inter)
{
	StringMatchPattern* tmp=NULL;
	ErrorState state={0,NULL};
	Intpr* tmpInter=fklNewTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=inter->glob;
	tmpInter->curDir=inter->curDir;
	tmpInter->prev=NULL;
	tmpInter->lnt=NULL;
	CompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
	ByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FAKE_ASSERT(tmParts,"fklAddStringPattern",__FILE__,__LINE__);
		int32_t i=0;
		for(;i<num;i++)
			tmParts[i]=fklCopyStr(parts[i]);
		tmp=fklNewStringMatchPattern(num,tmParts,tmpByteCodelnt);
	}
	else
	{
		if(tmpByteCodelnt)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmpByteCodelnt->l,tmpByteCodelnt->ls);
			fklFreeByteCodelnt(tmpByteCodelnt);
		}
		fklExError(state.place,state.state,inter);
		state.place=NULL;
		state.state=0;
	}
	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
	free(tmpInter);
	return tmp;
}

StringMatchPattern* addBuiltInStringPattern(const char* str,void(*fproc)(FakeVM* exe))
{
	int32_t num=0;
	char** parts=fklSplitPattern(str,&num);
	StringMatchPattern* tmp=fklNewFStringMatchPattern(num,parts,fproc);
	return tmp;
}

void fklInitBuiltInStringPattern(void)
{
	addBuiltInStringPattern("'(a)",READER_MACRO_quote);
	addBuiltInStringPattern("`(a)",READER_MACRO_qsquote);
	addBuiltInStringPattern("~(a)",READER_MACRO_unquote);
	addBuiltInStringPattern("~@(a)",READER_MACRO_unqtesp);
}

StringMatchPattern* fklAddReDefStringPattern(char** parts,int32_t num,AST_cptr* express,Intpr* inter)
{
	StringMatchPattern* tmp=fklFindStringPattern(parts[0]);
	ErrorState state={0,NULL};
	Intpr* tmpInter=fklNewTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=inter->glob;
	tmpInter->curDir=inter->curDir;
	tmpInter->prev=NULL;
	tmpInter->lnt=NULL;
	CompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
	ByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FAKE_ASSERT(tmParts,"fklAddStringPattern",__FILE__,__LINE__);
		int32_t i=0;
		for(;i<num;i++)
			tmParts[i]=fklCopyStr(parts[i]);
		fklFreeStringArry(tmp->parts,num);
		FREE_ALL_LINE_NUMBER_TABLE(tmp->u.bProc->l,tmp->u.bProc->ls);
		fklFreeByteCodelnt(tmp->u.bProc);
		tmp->parts=tmParts;
		tmp->u.bProc=tmpByteCodelnt;
	}
	else
	{
		if(tmpByteCodelnt)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmpByteCodelnt->l,tmpByteCodelnt->ls);
			fklFreeByteCodelnt(tmpByteCodelnt);
		}
		fklExError(state.place,state.state,inter);
		state.place=NULL;
		state.state=0;
	}
	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
	free(tmpInter);
	return tmp;
}

ByteCodelnt* fklCompile(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	for(;;)
	{
		if(fklIsLoadExpression(objCptr))return fklCompileLoad(objCptr,curEnv,inter,state,evalIm);
		if(fklIsConst(objCptr))return fklCompileConst(objCptr,curEnv,inter,state,evalIm);
		if(fklIsUnquoteExpression(objCptr))return fklCompileUnquote(objCptr,curEnv,inter,state,evalIm);
		if(fklIsQsquoteExpression(objCptr))return fklCompileQsquote(objCptr,curEnv,inter,state,evalIm);
		if(fklIsSymbol(objCptr))return fklCompileSym(objCptr,curEnv,inter,state,evalIm);
		if(fklIsDefExpression(objCptr))return fklCompileDef(objCptr,curEnv,inter,state,evalIm);
		if(fklIsSetqExpression(objCptr))return fklCompileSetq(objCptr,curEnv,inter,state,evalIm);
		if(fklIsSetfExpression(objCptr))return fklCompileSetf(objCptr,curEnv,inter,state,evalIm);
		if(fklIsGetfExpression(objCptr))return fklCompileGetf(objCptr,curEnv,inter,state,evalIm);
		if(fklIsSzofExpression(objCptr))return fklCompileSzof(objCptr,curEnv,inter,state,evalIm);
		if(fklIsCondExpression(objCptr))return fklCompileCond(objCptr,curEnv,inter,state,evalIm);
		if(fklIsAndExpression(objCptr))return fklCompileAnd(objCptr,curEnv,inter,state,evalIm);
		if(fklIsOrExpression(objCptr))return fklCompileOr(objCptr,curEnv,inter,state,evalIm);
		if(fklIsLambdaExpression(objCptr))return fklCompileLambda(objCptr,curEnv,inter,state,evalIm);
		if(fklIsBeginExpression(objCptr)) return fklCompileBegin(objCptr,curEnv,inter,state,evalIm);
		//if(fklIsPrognExpression(objCptr)) return fklCompileProgn(objCptr,curEnv,inter,state,evalIm);
		if(fklIsImportExpression(objCptr))return fklCompileImport(objCptr,curEnv,inter,state,evalIm);
		if(fklIsTryExpression(objCptr))return fklCompileTry(objCptr,curEnv,inter,state,evalIm);
		if(fklIsFlsymExpression(objCptr))return fklCompileFlsym(objCptr,curEnv,inter,state,evalIm);
		if(fklIsLibraryExpression(objCptr))
		{
			ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
			FAKE_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
			tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,0,inter->curline);
			return tmp;
		}
		if(fklIsCatchExpression(objCptr)||fklIsUnqtespExpression(objCptr)||fklIsExportExpression(objCptr))
		{
			state->state=INVALIDEXPR;
			state->place=objCptr;
			return NULL;
		}
		if(fklIsDefmacroExpression(objCptr))
		{
			ErrorState t=defmacro(objCptr,curEnv,inter);
			if(t.state)
			{
				state->state=t.state;
				state->place=t.place;
				return NULL;
			}
			ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
			FAKE_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
			tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id
					,0
					,0
					,objCptr->curline);
			return tmp;
		}
		else if(fklIsDeftypeExpression(objCptr))
		{
			Sid_t typeName=0;
			TypeId_t type=fklGenDefTypes(objCptr,inter->deftypes,&typeName);
			//VMTypeUnion type={.all=fklGenDefTypes(objCptr,inter->deftypes,&typeName).all};
			if(!type)
			{
				state->state=INVALIDTYPEDEF;
				state->place=objCptr;
				return NULL;
			}
			if(fklAddDefTypes(inter->deftypes,typeName,type))
			{
				state->state=INVALIDTYPEDEF;
				state->place=objCptr;
				return NULL;
			}
			ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
			FAKE_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
			tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id
					,0
					,0
					,objCptr->curline);
			return tmp;
		}
		int i=fklPreMacroExpand(objCptr,curEnv,inter);
		if(i==1)
			continue;
		else if(i==2)
		{
			state->state=MACROEXPANDFAILED;
			state->place=objCptr;
			return NULL;
		}
		else if(!fklIsValid(objCptr)||fklHasKeyWord(objCptr,curEnv))
		{
			state->state=SYNTAXERROR;
			state->place=objCptr;
			return NULL;
		}
		else if(fklIsFuncCall(objCptr,curEnv))return fklCompileFuncCall(objCptr,curEnv,inter,state,evalIm);
	}
}

CompEnv* createPatternCompEnv(char** parts,int32_t num,CompEnv* prev)
{
	if(parts==NULL)return NULL;
	CompEnv* tmpEnv=fklNewCompEnv(prev);
	int32_t i=0;
	for(;i<num;i++)
		if(fklIsVar(parts[i]))
		{
			char* varName=fklGetVarName(parts[i]);
			fklAddCompDef(varName,tmpEnv);
			free(varName);
		}
	return tmpEnv;
}

ByteCode* fklCompileAtom(AST_cptr* objCptr)
{
	AST_atom* tmpAtm=objCptr->u.atom;
	ByteCode* tmp=NULL;
	switch((int)tmpAtm->type)
	{
		case SYM:
			tmp=fklNewByteCode(sizeof(char)+sizeof(Sid_t));
			tmp->code[0]=FAKE_PUSH_SYM;
			*(Sid_t*)(tmp->code+sizeof(char))=fklAddSymbolToGlob(tmpAtm->value.str)->id;
			break;
		case STR:
			tmp=fklNewByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_STR;
			strcpy((char*)tmp->code+1,tmpAtm->value.str);
			break;
		case IN32:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FAKE_PUSH_IN32;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.in32;
			break;
		case IN64:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int64_t));
			tmp->code[0]=FAKE_PUSH_IN64;
			*(int64_t*)(tmp->code+sizeof(char))=tmpAtm->value.in64;
			break;
		case DBL:
			tmp=fklNewByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FAKE_PUSH_DBL;
			*(double*)(tmp->code+1)=tmpAtm->value.dbl;
			break;
		case CHR:
			tmp=fklNewByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FAKE_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
		case BYTS:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size);
			tmp->code[0]=FAKE_PUSH_BYTE;
			*(int32_t*)(tmp->code+1)=tmpAtm->value.byts.size;
			memcpy(tmp->code+5,tmpAtm->value.byts.str,tmpAtm->value.byts.size);
			break;
	}
	return tmp;
}

ByteCode* fklCompileNil()
{
	ByteCode* tmp=fklNewByteCode(1);
	tmp->code[0]=FAKE_PUSH_NIL;
	return tmp;
}

ByteCode* fklCompilePair(AST_cptr* objCptr)
{
	ByteCode* tmp=fklNewByteCode(0);
	AST_pair* objPair=objCptr->u.pair;
	AST_pair* tmpPair=objPair;
	ByteCode* popToCar=fklNewByteCode(1);
	ByteCode* popToCdr=fklNewByteCode(1);
	ByteCode* pushPair=fklNewByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			fklCodeCat(tmp,pushPair);
			objPair=objCptr->u.pair;
			objCptr=&objPair->car;
			continue;
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			ByteCode* tmp1=(objCptr->type==ATM)?fklCompileAtom(objCptr):fklCompileNil();
			fklCodeCat(tmp1,(objCptr==&objPair->car)?popToCar:popToCdr);
			fklCodeCat(tmp,tmp1);
			fklFreeByteCode(tmp1);
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
				fklCodeCat(tmp,(prev==objPair->car.u.pair)?popToCar:popToCdr);
				if(prev==objPair->car.u.pair)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.u.pair||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	fklFreeByteCode(popToCar);
	fklFreeByteCode(popToCdr);
	fklFreeByteCode(pushPair);
	return tmp;
}

ByteCodelnt* fklCompileQsquote(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	if(objCptr->type==ATM)
		return fklCompileConst(objCptr,curEnv,inter,state,evalIm);
	else if(fklIsUnquoteExpression(objCptr))
		return fklCompileUnquote(objCptr,curEnv,inter,state,evalIm);
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	AST_pair* objPair=objCptr->u.pair;
	AST_pair* tmpPair=objPair;
	ByteCode* appd=fklNewByteCode(1);
	ByteCode* popToCar=fklNewByteCode(1);
	ByteCode* popToCdr=fklNewByteCode(1);
	ByteCode* pushPair=fklNewByteCode(1);
	popToCar->code[0]=FAKE_POP_CAR;
	popToCdr->code[0]=FAKE_POP_CDR;
	pushPair->code[0]=FAKE_PUSH_PAIR;
	appd->code[0]=FAKE_APPEND;
	while(objCptr!=NULL)
	{
		if(fklIsUnquoteExpression(objCptr))
		{
			ByteCodelnt* tmp1=fklCompileUnquote(objCptr,curEnv,inter,state,evalIm);
			if(state->state!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				fklFreeByteCode(appd);
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				return NULL;
			}
			fklCodeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			fklCodefklLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(fklIsUnqtespExpression(objCptr))
		{
			if(objCptr==&objPair->cdr)
			{
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				fklFreeByteCode(appd);
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				state->state=INVALIDEXPR;
				state->place=objCptr;
				return NULL;
			}
			ByteCodelnt* tmp1=fklCompile(fklNextCptr(fklGetFirstCptr(objCptr)),curEnv,inter,state,evalIm);
			if(state->state!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				fklFreeByteCode(appd);
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				return NULL;
			}
			fklCodeCat(tmp1->bc,appd);
			tmp1->l[tmp1->ls-1]->cpc+=appd->size;
			fklCodefklLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(objCptr->type==PAIR)
		{
			if(!fklIsUnqtespExpression(fklGetFirstCptr(objCptr)))
			{
				fklCodeCat(tmp->bc,pushPair);
				if(!tmp->l)
				{
					tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
					FAKE_ASSERT(tmp->l,"fklCompileQsquote",__FILE__,__LINE__);
					tmp->ls=1;
					tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushPair->size,objCptr->curline);
				}
				else
					tmp->l[tmp->ls-1]->cpc+=pushPair->size;
			}
			objPair=objCptr->u.pair;
			objCptr=&objPair->car;
			continue;
		}
		else if((objCptr->type==ATM||objCptr->type==NIL)&&(!fklIsUnqtespExpression(&objPair->car)))
		{
			ByteCodelnt* tmp1=fklCompileConst(objCptr,curEnv,inter,state,evalIm);
			fklCodeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			fklCodefklLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
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
				if(prev!=NULL&&!fklIsUnqtespExpression(&prev->car))
				{
					fklCodeCat(tmp->bc,(prev==objPair->car.u.pair)?popToCar:appd);
					tmp->l[tmp->ls-1]->cpc+=(prev==objPair->car.u.pair)?popToCar->size:appd->size;
				}
				if(prev==objPair->car.u.pair)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.u.pair||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
	fklFreeByteCode(popToCar);
	fklFreeByteCode(popToCdr);
	fklFreeByteCode(pushPair);
	fklFreeByteCode(appd);
	return tmp;
}

ByteCode* fklCompileQuote(AST_cptr* objCptr)
{
	objCptr=&objCptr->u.pair->car;
	objCptr=fklNextCptr(objCptr);
	switch((int)objCptr->type)
	{
		case PAIR:
			return fklCompilePair(objCptr);
			break;
		case ATM:
			if(objCptr->u.atom->type==SYM)
			{
				ByteCode* tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t));
				char* sym=objCptr->u.atom->value.str;
				SymTabNode* node=fklAddSymbolToGlob(sym);
				tmp->code[0]=FAKE_PUSH_SYM;
				*(Sid_t*)(tmp->code+sizeof(char))=node->id;
				return tmp;
			}
			else
				return fklCompileAtom(objCptr);
			break;
		case NIL:
			return fklCompileNil();
			break;
	}
	return NULL;
}

ByteCodelnt* fklCompileUnquote(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	return fklCompile(objCptr,curEnv,inter,state,evalIm);
}

ByteCodelnt* fklCompileConst(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	int32_t line=objCptr->curline;
	ByteCode* tmp=NULL;
	if(objCptr->type==ATM)tmp=fklCompileAtom(objCptr);
	if(fklIsNil(objCptr))tmp=fklCompileNil();
	if(fklIsQuoteExpression(objCptr))tmp=fklCompileQuote(objCptr);
	if(!tmp)
	{
		state->state=INVALIDEXPR;
		state->place=objCptr;
		return NULL;
	}
	ByteCodelnt* t=fklNewByteCodelnt(tmp);
	LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->size,line);
	t->ls=1;
	t->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
	FAKE_ASSERT(t->l,"fklCompileConst",__FILE__,__LINE__);
	t->l[0]=n;
	return t;
}

ByteCodelnt* fklCompileFuncCall(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* headoflist=NULL;
	AST_pair* tmpPair=objCptr->u.pair;
	int32_t line=objCptr->curline;
	ByteCodelnt* tmp1=NULL;
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	ByteCode* setBp=fklNewByteCode(1);
	ByteCode* invoke=fklNewByteCode(1);
	setBp->code[0]=FAKE_SET_BP;
	invoke->code[0]=FAKE_INVOKE;
	for(;;)
	{
		if(objCptr==NULL)
		{
			fklCodeCat(tmp->bc,invoke);
			if(!tmp->l)
			{
				tmp->ls=1;
				tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
				FAKE_ASSERT(tmp->l,"fklCompileFuncCall",__FILE__,__LINE__);
				tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,invoke->size,line);
			}
			else
				tmp->l[tmp->ls-1]->cpc+=invoke->size;
			if(headoflist->outer==tmpPair)break;
			objCptr=fklPrevCptr(&headoflist->outer->prev->car);
			for(headoflist=&headoflist->outer->prev->car;fklPrevCptr(headoflist)!=NULL;headoflist=fklPrevCptr(headoflist));
			continue;
		}
		else if(fklIsFuncCall(objCptr,curEnv))
		{
			fklCodeCat(tmp->bc,setBp);
			if(!tmp->l)
			{
				tmp->ls=1;
				tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
				FAKE_ASSERT(tmp->l,"fklCompileFuncCall",__FILE__,__LINE__);
				tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,setBp->size,line);
			}
			else
				tmp->l[tmp->ls-1]->cpc+=setBp->size;
			headoflist=&objCptr->u.pair->car;
			for(objCptr=headoflist;fklNextCptr(objCptr)!=NULL;objCptr=fklNextCptr(objCptr));
		}
		else
		{
			tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
			if(state->state!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				tmp=NULL;
				break;
			}
			fklCodefklLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			objCptr=fklPrevCptr(objCptr);
		}
	}
	fklFreeByteCode(setBp);
	fklFreeByteCode(invoke);
	return tmp;
}

ByteCodelnt* fklCompileDef(AST_cptr* tir,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_pair* tmpPair=tir->u.pair;
	AST_cptr* fir=NULL;
	AST_cptr* sec=NULL;
	AST_cptr* objCptr=NULL;
	ByteCodelnt* tmp1=NULL;
	ByteCode* pushTop=fklNewByteCode(sizeof(char));
	ByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(Sid_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(fklIsDefExpression(tir))
	{
		fir=&tir->u.pair->car;
		sec=fklNextCptr(fir);
		tir=fklNextCptr(sec);
	}
	objCptr=tir;
	tmp1=(fklIsLambdaExpression(objCptr))?
		fklCompile(objCptr,curEnv,inter,state,0):
		fklCompile(objCptr,curEnv,inter,state,evalIm);
	if(state->state!=0)
	{
		fklFreeByteCode(popVar);
		fklFreeByteCode(pushTop);
		return NULL;
	}
	for(;;)
	{
		CompDef* tmpDef=NULL;
		AST_atom* tmpAtm=sec->u.atom;
		if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
		{
			if(curEnv->prev->prefix)
			{
				size_t len=strlen(tmpAtm->value.str)+strlen(curEnv->prev->prefix)+1;
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
				FAKE_ASSERT(symbolWithPrefix,"fklCompileDef",__FILE__,__LINE__);
				sprintf(symbolWithPrefix,"%s%s",curEnv->prev->prefix,tmpAtm->value.str);
				tmpDef=fklAddCompDef(symbolWithPrefix,curEnv->prev);
				free(symbolWithPrefix);
			}
			else
				tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv->prev);
			*(int32_t*)(popVar->code+sizeof(char))=(int32_t)1;
		}
		else
		{
			*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
			tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv);
		}
		*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
		ByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
		fklCodeCat(tmp1->bc,pushTop);
		fklCodeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
		if(fklIsLambdaExpression(objCptr)||fklIsConst(objCptr))
		{
			if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
			{
				ByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(int32_t));
				popVar->code[0]=FAKE_POP_VAR;
				*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
				*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=tmpDef->id;
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
				fklCodelntCopyCat(curEnv->prev->proc,tmp1Copy);
				FREE_ALL_LINE_NUMBER_TABLE(tmp1Copy->l,tmp1Copy->ls);
				fklFreeByteCodelnt(tmp1Copy);
				tmp1Copy=fklCopyByteCodelnt(tmp1);
				fklCodeCat(tmp1Copy->bc,pushTop);
				*(int32_t*)(popVar->code+sizeof(char))=(int32_t)0;
				*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=fklAddCompDef(tmpAtm->value.str,curEnv)->id;
				fklCodeCat(tmp1Copy->bc,popVar);
				fklCodeCat(tmp1->bc,pushTop);
				fklCodeCat(tmp1->bc,popVar);
				tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
				fklCodelntCopyCat(curEnv->proc,tmp1Copy);
				fklFreeByteCode(popVar);
			}
			else
				fklCodelntCopyCat(curEnv->proc,tmp1);
		}
		FREE_ALL_LINE_NUMBER_TABLE(tmp1Copy->l,tmp1Copy->ls);
		fklFreeByteCodelnt(tmp1Copy);
		if(fir->outer==tmpPair)
			break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=fklPrevCptr(tir);
			fir=fklPrevCptr(sec);
		}
	}
	fklFreeByteCode(popVar);
	fklFreeByteCode(pushTop);
	return tmp1;
}

ByteCodelnt* fklCompileSetq(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_pair* tmpPair=objCptr->u.pair;
	AST_cptr* fir=&objCptr->u.pair->car;
	AST_cptr* sec=fklNextCptr(fir);
	AST_cptr* tir=fklNextCptr(sec);
	ByteCodelnt* tmp1=NULL;
	ByteCode* pushTop=fklNewByteCode(sizeof(char));
	ByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(Sid_t));
	popVar->code[0]=FAKE_POP_VAR;
	pushTop->code[0]=FAKE_PUSH_TOP;
	while(fklIsSetqExpression(tir))
	{
		fir=&tir->u.pair->car;
		sec=fklNextCptr(fir);
		tir=fklNextCptr(sec);
	}
	objCptr=tir;
	tmp1=(fklIsLambdaExpression(objCptr))?
		fklCompile(objCptr,curEnv,inter,state,0):
		fklCompile(objCptr,curEnv,inter,state,evalIm);
	if(state->state!=0)
	{
		fklFreeByteCode(popVar);
		fklFreeByteCode(pushTop);
		return NULL;
	}
	for(;;)
	{
		int32_t scope=0;
		int32_t id=0;
		AST_atom* tmpAtm=sec->u.atom;
		CompDef* tmpDef=NULL;
		CompEnv* tmpEnv=curEnv;
		while(tmpEnv!=NULL)
		{
			tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
			if(tmpEnv->prefix&&!tmpDef)
			{
				size_t len=strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1;
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
				FAKE_ASSERT(symbolWithPrefix,"fklCompileSetq",__FILE__,__LINE__);
				sprintf(symbolWithPrefix,"%s%s",tmpEnv->prefix,tmpAtm->value.str);
				tmpDef=fklFindCompDef(symbolWithPrefix,tmpEnv);
				free(symbolWithPrefix);
			}
			if(tmpDef!=NULL)break;
			tmpEnv=tmpEnv->prev;
			scope++;
		}
		if(tmpDef==NULL)
		{
			SymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
			scope=-1;
			id=node->id;
		}
		else
			id=tmpDef->id;
		*(int32_t*)(popVar->code+sizeof(char))=scope;
		*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=id;
		ByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
		fklCodeCat(tmp1->bc,pushTop);
		fklCodeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
		if(!scope&&tmpDef&&(fklIsConst(objCptr)||fklIsLambdaExpression(objCptr)))
		{
			fklCodelntCopyCat(curEnv->proc,tmp1);
			if(tmpEnv->prev&&tmpEnv->prev->exp&&tmpEnv->prev->prefix)
			{
				ByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(Sid_t));
				popVar->code[0]=FAKE_POP_VAR;
				*(int32_t*)(popVar->code+sizeof(char))=scope+1;
				char* symbolWithPrefix=fklCopyStr(tmpEnv->prev->prefix);
				symbolWithPrefix=fklStrCat(symbolWithPrefix,tmpAtm->value.str);
				*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id;
				free(symbolWithPrefix);
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(pushTop->size+popVar->size);
				fklFreeByteCode(popVar);
				fklCodelntCopyCat(tmpEnv->proc,tmp1Copy);
			}
		}
		FREE_ALL_LINE_NUMBER_TABLE(tmp1Copy->l,tmp1Copy->ls);
		fklFreeByteCodelnt(tmp1Copy);
		if(tmpEnv->prev&&tmpEnv->prev->exp&&scope!=-1&&fklIsSymbolShouldBeExport(tmpAtm->value.str,tmpEnv->prev->exp,tmpEnv->prev->n))
		{
			*(int32_t*)(popVar->code+sizeof(char))=scope+1;
			if(tmpEnv->prev->prefix)
			{
				char* symbolWithPrefix=fklCopyStr(tmpEnv->prev->prefix);
				symbolWithPrefix=fklStrCat(symbolWithPrefix,tmpAtm->value.str);
				*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id;
				free(symbolWithPrefix);
			}
			else
				*(Sid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=fklFindCompDef(tmpAtm->value.str,tmpEnv->prev)->id;
			fklCodeCat(tmp1->bc,pushTop);
			fklCodeCat(tmp1->bc,popVar);
			tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
		}
		if(fir->outer==tmpPair)break;
		else
		{
			tir=&fir->outer->prev->car;
			sec=fklPrevCptr(tir);
			fir=fklPrevCptr(sec);
		}
	}
	fklFreeByteCode(pushTop);
	fklFreeByteCode(popVar);
	return tmp1;
}

ByteCodelnt* fklCompileSetf(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* fir=&objCptr->u.pair->car;
	AST_cptr* sec=fklNextCptr(fir);
	if(fklIsSymbol(sec))
		return fklCompileSetq(objCptr,curEnv,inter,state,0);
	else
	{
		AST_cptr* tir=fklNextCptr(sec);
		ByteCodelnt* tmp1=fklCompile(sec,curEnv,inter,state,evalIm);
		if(state->state!=0)
			return NULL;
		ByteCodelnt* tmp2=(fklIsLambdaExpression(tir))?
			fklCompile(tir,curEnv,inter,state,0):
			fklCompile(tir,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmp1->l,tmp1->ls);
			fklFreeByteCodelnt(tmp1);
			return NULL;
		}
		ByteCode* popRef=fklNewByteCode(sizeof(char));
		popRef->code[0]=FAKE_POP_REF;
		fklCodefklLntCat(tmp1,tmp2);
		fklCodeCat(tmp1->bc,popRef);
		tmp1->l[tmp1->ls-1]->cpc+=popRef->size;
		fklFreeByteCode(popRef);
		fklFreeByteCodelnt(tmp2);
		return tmp1;
	}
}

ByteCodelnt* fklCompileGetf(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* fir=fklGetFirstCptr(objCptr);
	AST_cptr* typeCptr=fklNextCptr(fir);
	AST_cptr* pathCptr=fklNextCptr(typeCptr);
	AST_cptr* expressionCptr=fklNextCptr(pathCptr);
	AST_cptr* indexCptr=fklNextCptr(expressionCptr);
	TypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!(expressionCptr&&type!=0))
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	VMTypeUnion typeUnion=fklGetVMTypeUnion(type);
	if(GET_TYPES_TAG(typeUnion.all)!=FUNC_TYPE_TAG)
	{
		TypeId_t memberType=type;
		ssize_t offset=0;
		ByteCodelnt* tmp=NULL;
		for(pathCptr=fklGetFirstCptr(pathCptr);pathCptr;pathCptr=fklNextCptr(pathCptr))
		{
			if(pathCptr->type!=ATM||(pathCptr->u.atom->type!=SYM&&pathCptr->u.atom->type!=IN32&&pathCptr->u.atom->type!=IN64))
			{
				state->state=SYNTAXERROR;
				state->place=pathCptr;
				return NULL;
			}
			else if(pathCptr->u.atom->type==IN32||pathCptr->u.atom->type==IN64)
			{
				size_t typeSize=fklGetVMTypeSizeWithTypeId(memberType);
				offset+=typeSize*((pathCptr->u.atom->type==IN32)?pathCptr->u.atom->value.in32:pathCptr->u.atom->value.in64);
			}
			else if(!strcmp(pathCptr->u.atom->value.str,"*"))
			{
				VMTypeUnion curTypeUnion=fklGetVMTypeUnion(memberType);
				void* tmpPtr=GET_TYPES_PTR(curTypeUnion.all);
				DefTypeTag tag=GET_TYPES_TAG(curTypeUnion.all);
				TypeId_t defTypeId=0;
				switch(tag)
				{
					case ARRAY_TYPE_TAG:
						defTypeId=((VMArrayType*)tmpPtr)->etype;
						break;
					case PTR_TYPE_TAG:
						defTypeId=((VMPtrType*)tmpPtr)->ptype;
						break;
					default:
						state->state=CANTDEREFERENCE;
						state->place=pathCptr;
						if(tmp)
						{
							FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
							fklFreeByteCodelnt(tmp);
						}
						return NULL;
						break;
				}
				ByteCodelnt* pushDefRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t)));
				pushDefRef->bc->code[0]=FAKE_PUSH_DEF_REF;
				*(ssize_t*)(pushDefRef->bc->code+sizeof(char))=offset;
				*(TypeId_t*)(pushDefRef->bc->code+sizeof(char)+sizeof(ssize_t))=defTypeId;
				LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushDefRef->bc->size,pathCptr->curline);
				pushDefRef->ls=1;
				pushDefRef->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
				FAKE_ASSERT(pushDefRef->l,"fklCompileGetf",__FILE__,__LINE__);
				pushDefRef->l[0]=n;
				offset=0;
				memberType=defTypeId;
				if(!tmp)
					tmp=pushDefRef;
				else
				{
					fklCodefklLntCat(tmp,pushDefRef);
					fklFreeByteCodelnt(pushDefRef);
				}
			}
			else if(!strcmp(pathCptr->u.atom->value.str,"&"))
			{
				ByteCodelnt* pushPtrRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t)));
				pushPtrRef->bc->code[0]=FAKE_PUSH_PTR_REF;
				TypeId_t ptrId=fklNewVMPtrType(memberType);
				*(ssize_t*)(pushPtrRef->bc->code+sizeof(char))=offset;
				*(TypeId_t*)(pushPtrRef->bc->code+sizeof(char)+sizeof(ssize_t))=ptrId;
				LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushPtrRef->bc->size,pathCptr->curline);
				pushPtrRef->ls=1;
				pushPtrRef->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
				FAKE_ASSERT(pushPtrRef->l,"fklCompileGetf",__FILE__,__LINE__);
				pushPtrRef->l[0]=n;
				offset=0;
				memberType=ptrId;
				if(!tmp)
					tmp=pushPtrRef;
				else
				{
					fklCodefklLntCat(tmp,pushPtrRef);
					fklFreeByteCodelnt(pushPtrRef);
				}
			}
			else
			{
				if(GET_TYPES_TAG(typeUnion.all)!=STRUCT_TYPE_TAG&&GET_TYPES_TAG(typeUnion.all)!=UNION_TYPE_TAG)
				{
					if(tmp)
					{
						FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
						fklFreeByteCodelnt(tmp);
					}
					state->state=NOMEMBERTYPE;
					state->place=typeCptr;
					return NULL;
				}
				Sid_t curPathNode=fklAddSymbolToGlob(pathCptr->u.atom->value.str)->id;
				VMTypeUnion typeUnion=fklGetVMTypeUnion(memberType);
				if(GET_TYPES_TAG(typeUnion.all)==STRUCT_TYPE_TAG)
				{
					VMStructType* structType=GET_TYPES_PTR(typeUnion.st);
					uint32_t i=0;
					uint32_t num=structType->num;
					TypeId_t tmpType=0;
					for(;i<num;i++)
					{
						size_t align=fklGetVMTypeAlign(fklGetVMTypeUnion(structType->layout[i].type));
						if(offset%align)
							offset+=align-offset%align;
						if(structType->layout[i].memberSymbol==curPathNode)
						{
							tmpType=structType->layout[i].type;
							break;
						}
						offset+=fklGetVMTypeSizeWithTypeId(structType->layout[i].type);
					}
					if(tmpType)
						memberType=tmpType;
					else
					{
						state->state=INVALIDMEMBER;
						state->place=pathCptr;
						if(tmp)
						{
							FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
							fklFreeByteCodelnt(tmp);
						}
						return NULL;
					}
					AST_cptr* next=fklNextCptr(pathCptr);
					if(!next||(next->type==ATM&&next->u.atom->type==SYM&&(!strcmp(next->u.atom->value.str,"&")||!strcmp(next->u.atom->value.str,"*"))))
					{
						ByteCodelnt* pushRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t)));
						pushRef->bc->code[0]=FAKE_PUSH_REF;
						*(ssize_t*)(pushRef->bc->code+sizeof(char))=offset;
						*(TypeId_t*)(pushRef->bc->code+sizeof(char)+sizeof(ssize_t))=memberType;
						LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushRef->bc->size,pathCptr->curline);
						pushRef->ls=1;
						pushRef->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
						FAKE_ASSERT(pushRef->l,"fklCompileGetf",__FILE__,__LINE__);
						pushRef->l[0]=n;
						if(!tmp)
							tmp=pushRef;
						else
						{
							fklCodefklLntCat(tmp,pushRef);
							fklFreeByteCodelnt(pushRef);
						}
						offset=0;
					}
				}
				else
				{
					VMUnionType* unionType=GET_TYPES_PTR(typeUnion.st);
					uint32_t i=0;
					uint32_t num=unionType->num;
					TypeId_t tmpType=0;
					for(;i<num;i++)
					{
						if(unionType->layout[i].memberSymbol==curPathNode)
						{
							tmpType=unionType->layout[i].type;
							break;
						}
					}
					if(tmpType)
						memberType=tmpType;
					else
					{
						state->state=INVALIDMEMBER;
						state->place=pathCptr;
						if(tmp)
						{
							FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
							fklFreeByteCodelnt(tmp);
						}
						return NULL;
					}
					AST_cptr* next=fklNextCptr(pathCptr);
					if(!next||(next->type==ATM&&next->u.atom->type==SYM&&(!strcmp(next->u.atom->value.str,"&")||!strcmp(next->u.atom->value.str,"*"))))
					{
						ByteCodelnt* pushRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t)));
						pushRef->bc->code[0]=FAKE_PUSH_REF;
						*(ssize_t*)(pushRef->bc->code+sizeof(char))=offset;
						*(TypeId_t*)(pushRef->bc->code+sizeof(char)+sizeof(ssize_t))=memberType;
						LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushRef->bc->size,pathCptr->curline);
						pushRef->ls=1;
						pushRef->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
						FAKE_ASSERT(pushRef->l,"fklCompileGetf",__FILE__,__LINE__);
						pushRef->l[0]=n;
						if(!tmp)
							tmp=pushRef;
						else
						{
							fklCodefklLntCat(tmp,pushRef);
							fklFreeByteCodelnt(pushRef);
						}
						offset=0;
					}
				}
			}
		}
		ByteCodelnt* expression=fklCompile(expressionCptr,curEnv,inter,state,evalIm);
		if(state->state)
		{
			if(tmp)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
			}
			return NULL;
		}
		if(!tmp)
			tmp=expression;
		else
		{
			reCodefklLntCat(expression,tmp);
			fklFreeByteCodelnt(expression);
		}
		ByteCodelnt* index=NULL;
		if(indexCptr!=NULL)
		{
			index=fklCompile(indexCptr,curEnv,inter,state,evalIm);
			if(state->state)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				return NULL;
			}
			TypeId_t elemType=0;
			VMTypeUnion outerType=fklGetVMTypeUnion(memberType);
			void* tmpPtr=GET_TYPES_PTR(outerType.all);
			DefTypeTag tag=GET_TYPES_TAG(outerType.all);
			switch(tag)
			{
				case PTR_TYPE_TAG:
					elemType=((VMPtrType*)tmpPtr)->ptype;
					break;
				case ARRAY_TYPE_TAG:
					elemType=((VMArrayType*)tmpPtr)->etype;
					break;
				default:
					state->state=CANTGETELEM;
					state->place=pathCptr;
					FREE_ALL_LINE_NUMBER_TABLE(index->l,index->ls);
					fklFreeByteCodelnt(index);
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					fklFreeByteCodelnt(tmp);
					return NULL;
					break;
			}
			ByteCodelnt* pushIndRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(TypeId_t)+sizeof(uint32_t)));
			pushIndRef->bc->code[0]=FAKE_PUSH_IND_REF;
			*(TypeId_t*)(pushIndRef->bc->code+sizeof(char))=elemType;
			*(uint32_t*)(pushIndRef->bc->code+sizeof(char)+sizeof(TypeId_t))=fklGetVMTypeSizeWithTypeId(elemType);
			LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushIndRef->bc->size,indexCptr->curline);
			pushIndRef->ls=1;
			pushIndRef->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
			FAKE_ASSERT(pushIndRef->l,"fklCompileGetf",__FILE__,__LINE__);
			pushIndRef->l[0]=n;
			fklCodefklLntCat(tmp,index);
			fklFreeByteCodelnt(index);
			fklCodefklLntCat(tmp,pushIndRef);
			fklFreeByteCodelnt(pushIndRef);
		}
		else
		{
			VMTypeUnion typeUnion=fklGetVMTypeUnion(memberType);
			DefTypeTag tag=GET_TYPES_TAG(typeUnion.all);
			if(tag!=NATIVE_TYPE_TAG&&tag!=PTR_TYPE_TAG&&tag!=ARRAY_TYPE_TAG&&tag!=FUNC_TYPE_TAG)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				state->state=NONSCALARTYPE;
				state->place=fklNextCptr(typeCptr);
				return NULL;
			}
		}
		return tmp;
	}
	else
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
}

ByteCodelnt* fklCompileFlsym(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* fir=fklGetFirstCptr(objCptr);
	AST_cptr* typeCptr=fklNextCptr(fir);
	TypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!type)
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	VMTypeUnion typeUnion=fklGetVMTypeUnion(type);
	if(GET_TYPES_TAG(typeUnion.all)!=FUNC_TYPE_TAG)
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	AST_cptr* nameCptr=fklNextCptr(typeCptr);
	if(fklNextCptr(nameCptr)!=NULL)
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	if(!nameCptr)
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	ByteCodelnt* name=fklCompile(nameCptr,curEnv,inter,state,evalIm);
	if(state->state)
		return NULL;
	if(state->state)
	{
		FREE_ALL_LINE_NUMBER_TABLE(name->l,name->ls);
		fklFreeByteCodelnt(name);
		return NULL;
	}
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(TypeId_t)));
	tmp->bc->code[0]=FAKE_PUSH_FPROC;
	*(TypeId_t*)(tmp->bc->code+sizeof(char))=type;
	LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
	tmp->ls=1;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
	FAKE_ASSERT(tmp->l,"fklCompileGetf",__FILE__,__LINE__);
	tmp->l[0]=n;
	reCodefklLntCat(name,tmp);
	fklFreeByteCodelnt(name);
	return tmp;
}
ByteCodelnt* fklCompileSzof(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* typeCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	TypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!type)
	{
		state->state=INVALIDTYPEDEF;
		state->place=typeCptr;
		return NULL;
	}
	size_t size=fklGetVMTypeSizeWithTypeId(type);
	ByteCodelnt* tmp=NULL;
	if(size>INT32_MAX)
	{
		tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(int64_t)));
		tmp->bc->code[0]=FAKE_PUSH_IN64;
		*(int64_t*)(tmp->bc->code+sizeof(char))=(int64_t)size;
	}
	else
	{
		tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(int32_t)));
		tmp->bc->code[0]=FAKE_PUSH_IN32;
		*(int32_t*)(tmp->bc->code+sizeof(char))=(int32_t)size;
	}
	LineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
	tmp->ls=1;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
	FAKE_ASSERT(tmp->l,"fklCompileSzof",__FILE__,__LINE__);
	tmp->l[0]=n;
	return tmp;
}

ByteCodelnt* fklCompileSym(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	int32_t line=objCptr->curline;
	if(fklHasKeyWord(objCptr,curEnv))
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	ByteCode* pushVar=fklNewByteCode(sizeof(char)+sizeof(Sid_t));
	pushVar->code[0]=FAKE_PUSH_VAR;
	AST_atom* tmpAtm=objCptr->u.atom;
	CompDef* tmpDef=NULL;
	CompEnv* tmpEnv=curEnv;
	int32_t id=0;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
		if(tmpEnv->prefix&&!tmpDef)
		{
			size_t len=strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1;
			char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
			FAKE_ASSERT(symbolWithPrefix,"fklCompileSym",__FILE__,__LINE__);
			sprintf(symbolWithPrefix,"%s%s",tmpEnv->prefix,tmpAtm->value.str);
			tmpDef=fklFindCompDef(symbolWithPrefix,tmpEnv);
			free(symbolWithPrefix);
		}
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		SymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
		id=node->id;
		if(evalIm)
		{
			state->state=SYMUNDEFINE;
			state->place=objCptr;
			fklFreeByteCode(pushVar);
			return NULL;
		}
	}
	else
		id=tmpDef->id;
	*(Sid_t*)(pushVar->code+sizeof(char))=id;
	ByteCodelnt* bcl=fklNewByteCodelnt(pushVar);
	bcl->ls=1;
	bcl->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	FAKE_ASSERT(bcl->l,"fklCompileSym",__FILE__,__LINE__);
	bcl->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushVar->size,line);
	return bcl;
}

ByteCodelnt* fklCompileAnd(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	int32_t curline=objCptr->curline;
	ByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* push1=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* resTp=fklNewByteCode(sizeof(char));
	ByteCode* setTp=fklNewByteCode(sizeof(char));
	ByteCode* popTp=fklNewByteCode(sizeof(char));
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	ComStack* stack=fklNewComStack(32);
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	push1->code[0]=FAKE_PUSH_IN32;
	resTp->code[0]=FAKE_RES_TP;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	*(int32_t*)(push1->code+sizeof(char))=1;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		ByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			if(tmp->l)
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeByteCodelnt(tmp);
			fklFreeByteCode(jumpiffalse);
			fklFreeByteCode(push1);
			fklFreeComStack(stack);
			return NULL;
		}
		fklPushComStack(tmp1,stack);
	}
	while(!fklIsComStackEmpty(stack))
	{
		ByteCodelnt* tmp1=fklPopComStack(stack);
		*(int32_t*)(jumpiffalse->code+sizeof(char))=tmp->bc->size;
		fklCodeCat(tmp1->bc,jumpiffalse);
		tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		reCodefklLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);

	}
	fklReCodeCat(push1,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		FAKE_ASSERT(tmp->l,"fklCompileAnd",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,curline);
	}
	else
	{
		tmp->l[0]->cpc+=push1->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,push1->size);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	fklFreeByteCode(jumpiffalse);
	fklFreeByteCode(push1);
	fklFreeComStack(stack);
	//fklPrintByteCodelnt(tmp,inter->table,stderr);
	return tmp;
}

ByteCodelnt* fklCompileOr(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	int32_t curline=objCptr->curline;
	ByteCode* setTp=fklNewByteCode(sizeof(char));
	ByteCode* popTp=fklNewByteCode(sizeof(char));
	ByteCode* resTp=fklNewByteCode(sizeof(char));
	ByteCode* jumpifture=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* pushnil=fklNewByteCode(sizeof(char));
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	ComStack* stack=fklNewComStack(32);
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpifture->code[0]=FAKE_JMP_IF_TRUE;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	resTp->code[0]=FAKE_RES_TP;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		ByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			if(tmp->l)
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeByteCodelnt(tmp);
			fklFreeByteCode(jumpifture);
			fklFreeByteCode(pushnil);
			fklFreeComStack(stack);
			return NULL;
		}
		fklPushComStack(tmp1,stack);
	}
	while(!fklIsComStackEmpty(stack))
	{
		ByteCodelnt* tmp1=fklPopComStack(stack);
		*(int32_t*)(jumpifture->code+sizeof(char))=tmp->bc->size;
		fklCodeCat(tmp1->bc,jumpifture);
		tmp1->l[tmp1->ls-1]->cpc+=jumpifture->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		reCodefklLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);
	}
	fklReCodeCat(pushnil,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		FAKE_ASSERT(tmp->l,"fklCompileOr",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,curline);
	}
	else
	{
		tmp->l[0]->cpc+=pushnil->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushnil->size);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	fklFreeByteCode(jumpifture);
	fklFreeByteCode(pushnil);
	fklFreeComStack(stack);
	return tmp;
}

ByteCodelnt* fklCompileBegin(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* firCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	ByteCode* resTp=fklNewByteCode(1);
	ByteCode* setTp=fklNewByteCode(1);
	ByteCode* popTp=fklNewByteCode(1);
	resTp->code[0]=FAKE_RES_TP;
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	while(firCptr)
	{
		ByteCodelnt* tmp1=fklCompile(firCptr,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeByteCodelnt(tmp);
			fklFreeByteCode(resTp);
			fklFreeByteCode(setTp);
			fklFreeByteCode(popTp);
			return NULL;
		}
		if(tmp->bc->size)
		{
			fklReCodeCat(resTp,tmp1->bc);
			tmp1->l[0]->cpc+=1;
			INCREASE_ALL_SCP(tmp1->l,tmp1->ls-1,resTp->size);
		}
		fklCodefklLntCat(tmp,tmp1);
		fklFreeByteCodelnt(tmp1);
		firCptr=fklNextCptr(firCptr);
	}
	fklReCodeCat(setTp,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
		FAKE_ASSERT(tmp->l,"fklCompileBegin",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
	}
	else
	{
		tmp->l[0]->cpc+=1;
		INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	}
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(setTp);
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	return tmp;
}

ByteCodelnt* fklCompileLambda(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	int32_t line=objCptr->curline;
	AST_cptr* tmpCptr=objCptr;
	AST_pair* objPair=NULL;
	CompEnv* tmpEnv=fklNewCompEnv(curEnv);
	ByteCode* popArg=fklNewByteCode(sizeof(char)+sizeof(Sid_t));
	ByteCode* popRestArg=fklNewByteCode(sizeof(char)+sizeof(Sid_t));
	ByteCode* pArg=fklNewByteCode(0);
	popArg->code[0]=FAKE_POP_ARG;
	popRestArg->code[0]=FAKE_POP_REST_ARG;
	objPair=objCptr->u.pair;
	objCptr=&objPair->car;
	if(fklNextCptr(objCptr)->type==PAIR)
	{
		AST_cptr* argCptr=&fklNextCptr(objCptr)->u.pair->car;
		while(argCptr!=NULL&&argCptr->type!=NIL)
		{
			AST_atom* tmpAtm=(argCptr->type==ATM)?argCptr->u.atom:NULL;
			if(argCptr->type!=ATM||tmpAtm==NULL||tmpAtm->type!=SYM)
			{
				state->state=SYNTAXERROR;
				state->place=tmpCptr;
				fklFreeByteCode(popArg);
				fklFreeByteCode(popRestArg);
				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
				return NULL;
			}
			CompDef* tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
			*(Sid_t*)(popArg->code+sizeof(char))=tmpDef->id;
			fklCodeCat(pArg,popArg);
			if(fklNextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==ATM)
			{
				AST_atom* tmpAtom1=(argCptr->outer->cdr.type==ATM)?argCptr->outer->cdr.u.atom:NULL;
				if(tmpAtom1!=NULL&&tmpAtom1->type!=SYM)
				{
					state->state=SYNTAXERROR;
					state->place=tmpCptr;
					fklFreeByteCode(popArg);
					fklFreeByteCode(popRestArg);
					fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
					return NULL;
				}
				tmpDef=fklAddCompDef(tmpAtom1->value.str,tmpEnv);
				*(Sid_t*)(popRestArg->code+sizeof(char))=tmpDef->id;
				fklCodeCat(pArg,popRestArg);
			}
			argCptr=fklNextCptr(argCptr);
		}
	}
	else if(fklNextCptr(objCptr)->type==ATM)
	{
		AST_atom* tmpAtm=fklNextCptr(objCptr)->u.atom;
		if(tmpAtm->type!=SYM)
		{
			state->state=SYNTAXERROR;
			state->place=tmpCptr;
			fklFreeByteCode(popArg);
			fklFreeByteCode(popRestArg);
			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
			return NULL;
		}
		CompDef* tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
		*(Sid_t*)(popRestArg->code+sizeof(char))=tmpDef->id;
		fklCodeCat(pArg,popRestArg);
	}
	fklFreeByteCode(popRestArg);
	fklFreeByteCode(popArg);
	ByteCode* fklResBp=fklNewByteCode(sizeof(char));
	fklResBp->code[0]=FAKE_RES_BP;
	ByteCode* setTp=fklNewByteCode(sizeof(char));
	setTp->code[0]=FAKE_SET_TP;
	fklCodeCat(pArg,fklResBp);
	fklCodeCat(pArg,setTp);
	fklFreeByteCode(fklResBp);
	fklFreeByteCode(setTp);
	objCptr=fklNextCptr(fklNextCptr(objCptr));
	ByteCodelnt* codeOfLambda=fklNewByteCodelnt(pArg);
	codeOfLambda->ls=1;
	codeOfLambda->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	FAKE_ASSERT(codeOfLambda->l,"fklCompileLambda",__FILE__,__LINE__);
	codeOfLambda->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pArg->size,line);
	ByteCode* resTp=fklNewByteCode(sizeof(char));
	resTp->code[0]=FAKE_RES_TP;
	for(;objCptr;objCptr=fklNextCptr(objCptr))
	{
		ByteCodelnt* tmp1=fklCompile(objCptr,tmpEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			FREE_ALL_LINE_NUMBER_TABLE(codeOfLambda->l,codeOfLambda->ls);
			fklFreeByteCode(resTp);
			fklFreeByteCodelnt(codeOfLambda);
			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
			return NULL;
		}
		if(fklNextCptr(objCptr)!=NULL)
		{
			fklCodeCat(tmp1->bc,resTp);
			tmp1->l[tmp1->ls-1]->cpc+=resTp->size;
		}
		fklCodefklLntCat(codeOfLambda,tmp1);
		fklFreeByteCodelnt(tmp1);
	}
	fklFreeByteCode(resTp);
	ByteCode* popTp=fklNewByteCode(sizeof(char));
	popTp->code[0]=FAKE_POP_TP;
	fklCodeCat(codeOfLambda->bc,popTp);
	codeOfLambda->l[codeOfLambda->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(popTp);
	ByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	pushProc->code[0]=FAKE_PUSH_PROC;
	*(int32_t*)(pushProc->code+sizeof(char))=codeOfLambda->bc->size;
	ByteCodelnt* toReturn=fklNewByteCodelnt(pushProc);
	toReturn->ls=1;
	toReturn->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	FAKE_ASSERT(toReturn->l,"fklCompileLambda",__FILE__,__LINE__);
	toReturn->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushProc->size,line);
	fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
	fklCodefklLntCat(toReturn,codeOfLambda);
	fklFreeByteCodelnt(codeOfLambda);
	return toReturn;
}

ByteCodelnt* fklCompileCond(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* cond=NULL;
	ByteCode* pushnil=fklNewByteCode(sizeof(char));
	ByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* jump=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	ByteCode* resTp=fklNewByteCode(sizeof(char));
	ByteCode* setTp=fklNewByteCode(sizeof(char));
	ByteCode* popTp=fklNewByteCode(sizeof(char));
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	ComStack* stack1=fklNewComStack(32);
	setTp->code[0]=FAKE_SET_TP;
	resTp->code[0]=FAKE_RES_TP;
	popTp->code[0]=FAKE_POP_TP;
	pushnil->code[0]=FAKE_PUSH_NIL;
	jumpiffalse->code[0]=FAKE_JMP_IF_FALSE;
	jump->code[0]=FAKE_JMP;
	for(cond=fklNextCptr(fklGetFirstCptr(objCptr));cond!=NULL;cond=fklNextCptr(cond))
	{
		objCptr=fklGetFirstCptr(cond);
		ByteCodelnt* conditionCode=fklCompile(objCptr,curEnv,inter,state,evalIm);
		ComStack* stack2=fklNewComStack(32);
		for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
		{
			ByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
			if(state->state!=0)
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				fklFreeByteCode(jumpiffalse);
				fklFreeByteCode(jump);
				fklFreeByteCode(resTp);
				fklFreeByteCode(setTp);
				fklFreeByteCode(popTp);
				fklFreeByteCode(pushnil);
				fklFreeComStack(stack1);
				fklFreeComStack(stack2);
				FREE_ALL_LINE_NUMBER_TABLE(conditionCode->l,conditionCode->ls);
				fklFreeByteCodelnt(conditionCode);
				return NULL;
			}
			fklPushComStack(tmp1,stack2);
		}
		ByteCodelnt* tmpCond=fklNewByteCodelnt(fklNewByteCode(0));
		while(!fklIsComStackEmpty(stack2))
		{
			ByteCodelnt* tmp1=fklPopComStack(stack2);
			fklReCodeCat(resTp,tmp1->bc);
			tmp1->l[0]->cpc+=resTp->size;
			INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
			reCodefklLntCat(tmp1,tmpCond);
			fklFreeByteCodelnt(tmp1);
		}
		fklFreeComStack(stack2);
		if(tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size))
		{
			*(int32_t*)(jumpiffalse->code+sizeof(char))=tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size);
			fklCodeCat(conditionCode->bc,jumpiffalse);
			conditionCode->l[conditionCode->ls-1]->cpc+=jumpiffalse->size;
		}
		reCodefklLntCat(conditionCode,tmpCond);
		fklFreeByteCodelnt(conditionCode);
		fklPushComStack(tmpCond,stack1);
	}
	uint32_t top=stack1->top-1;
	while(!fklIsComStackEmpty(stack1))
	{
		ByteCodelnt* tmpCond=fklPopComStack(stack1);
		if(!fklIsComStackEmpty(stack1))
		{
			fklReCodeCat(resTp,tmpCond->bc);
			tmpCond->l[0]->cpc+=1;
			INCREASE_ALL_SCP(tmpCond->l+1,tmpCond->ls-1,resTp->size);
		}
		if(top!=stack1->top)
		{
			*(int32_t*)(jump->code+sizeof(char))=tmp->bc->size;
			fklCodeCat(tmpCond->bc,jump);
			tmpCond->l[tmpCond->ls-1]->cpc+=jump->size;
		}
		reCodefklLntCat(tmpCond,tmp);
		fklFreeByteCodelnt(tmpCond);
	}
	fklFreeComStack(stack1);
	if(!tmp->l)
	{
		fklCodeCat(tmp->bc,pushnil);
		tmp->ls=1;
		tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));
		FAKE_ASSERT(tmp->l,"fklCompileCond",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushnil->size,objCptr->curline);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(pushnil);
	fklFreeByteCode(resTp);
	fklFreeByteCode(setTp);
	fklFreeByteCode(popTp);
	fklFreeByteCode(jumpiffalse);
	fklFreeByteCode(jump);
	return tmp;
}

ByteCodelnt* fklCompileLoad(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* fir=&objCptr->u.pair->car;
	AST_cptr* pFileName=fklNextCptr(fir);
	AST_atom* name=pFileName->u.atom;
	if(fklHasLoadSameFile(name->value.str,inter))
	{
		state->state=CIRCULARLOAD;
		state->place=pFileName;
		return NULL;
	}
	FILE* file=fopen(name->value.str,"r");
	if(file==NULL)
	{
		perror(name->value.str);
		return NULL;
	}
	fklAddSymbolToGlob(name->value.str);
	Intpr* tmpIntpr=fklNewIntpr(name->value.str,file,curEnv,inter->lnt,inter->deftypes);
	tmpIntpr->prev=inter;
	tmpIntpr->glob=curEnv;
	ByteCodelnt* tmp=fklCompileFile(tmpIntpr,evalIm,NULL);
	chdir(tmpIntpr->prev->curDir);
	tmpIntpr->glob=NULL;
	tmpIntpr->lnt=NULL;
	tmpIntpr->deftypes=NULL;
	fklFreeIntpr(tmpIntpr);
	//fklPrintByteCode(tmp,stderr);
	ByteCode* setTp=fklNewByteCode(1);
	ByteCode* popTp=fklNewByteCode(1);
	setTp->code[0]=FAKE_SET_TP;
	popTp->code[0]=FAKE_POP_TP;
	if(tmp)
	{
		fklReCodeCat(setTp,tmp->bc);
		fklCodeCat(tmp->bc,popTp);
		tmp->l[0]->cpc+=setTp->size;
		INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
		tmp->l[tmp->ls-1]->cpc+=popTp->size;

	}
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	return tmp;
}

ByteCodelnt* fklCompileFile(Intpr* inter,int evalIm,int* exitstate)
{
	chdir(inter->curDir);
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	tmp->ls=1;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
	FAKE_ASSERT(tmp->l,"fklCompileFile",__FILE__,__LINE__);
	tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,0,1);
	ByteCode* resTp=fklNewByteCode(1);
	resTp->code[0]=FAKE_RES_TP;
	char* prev=NULL;
	for(;;)
	{
		AST_cptr* begin=NULL;
		int unexpectEOF=0;
		char* list=fklReadInPattern(inter->file,&prev,&unexpectEOF);
		if(list==NULL)continue;
		ErrorState state={0,NULL};
		if(unexpectEOF)
		{
			switch(unexpectEOF)
			{
				case 1:
					fprintf(stderr,"error of reader:Unexpect EOF at line %d of %s\n",inter->curline,inter->filename);
					break;
				case 2:
					fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",inter->curline,inter->filename);
					break;
			}

			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeByteCodelnt(tmp);
			free(list);
			*exitstate=UNEXPECTEOF;
			tmp=NULL;
			break;
		}
		begin=fklCreateTree(list,inter,NULL);
		if(fklIsAllSpace(list))
		{
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			ByteCodelnt* tmpByteCodelnt=fklCompile(begin,inter->glob,inter,&state,!fklIsLambdaExpression(begin));
			if(!tmpByteCodelnt||state.state!=0)
			{
				if(state.state)
				{
					fklExError(state.place,state.state,inter);
					if(exitstate)*exitstate=state.state;
					fklDeleteCptr(state.place);
				}
				if(tmpByteCodelnt==NULL&&!state.state&&exitstate)*exitstate=MACROEXPANDFAILED;
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeByteCodelnt(tmp);
				free(list);
				fklDeleteCptr(begin);
				free(begin);
				tmp=NULL;
				break;
			}
			if(tmp->bc->size)
			{
				fklReCodeCat(resTp,tmpByteCodelnt->bc);
				tmpByteCodelnt->l[0]->cpc+=resTp->size;
				INCREASE_ALL_SCP(tmpByteCodelnt->l+1,tmpByteCodelnt->ls-1,resTp->size);
			}
			fklCodefklLntCat(tmp,tmpByteCodelnt);
			fklFreeByteCodelnt(tmpByteCodelnt);
			fklDeleteCptr(begin);
			free(begin);
		}
		else
		{
			free(list);
			list=NULL;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeByteCodelnt(tmp);
			fklFreeByteCode(resTp);
			return NULL;
		}
		free(list);
		list=NULL;
	}
	if(prev)
		free(prev);
	fklFreeByteCode(resTp);
	return tmp;
}

/*
#define GENERATE_LNT(BYTECODELNT,BYTECODE) {\
	BYTECODELNT=fklNewByteCodelnt(BYTECODE);\
	(BYTECODELNT)->ls=1;\
	(BYTECODELNT)->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*));\
	FAKE_ASSERT((BYTECODELNT)->l,"fklCompileProgn",__FILE__,__LINE__);\
	(BYTECODELNT)->l[0]=fklNewLineNumTabNode(fid,0,(BYTECODE)->size,fir->curline);\
}
*/

//ByteCodelnt* fklCompileProgn(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
//{
//	AST_cptr* fir=fklNextCptr(fklGetFirstCptr(objCptr));
//	ByteCodelnt* tmp=NULL;
//
//	ComStack* stack=fklNewComStack(32);
//	for(;fir;fir=fklNextCptr(fir))
//	{
//		if(fir->type!=ATM)
//		{
//			state->place=objCptr;
//			state->state=SYNTAXERROR;
//			return NULL;
//		}
//	}
//
//	int32_t fid=fklAddSymbolToGlob(inter->filename)->id;
//	fir=fklNextCptr(fklGetFirstCptr(objCptr));
//
//	int32_t sizeOfByteCode=0;
//
//	while(fir)
//	{
//		AST_atom* firAtm=fir->u.atom;
//		if(firAtm->value.str[0]==':')
//		{
//			fklPushComStack(fklNewByteCodeLable(sizeOfByteCode,firAtm->value.str+1),stack);
//			fir=fklNextCptr(fir);
//			continue;
//		}
//
//		if(firAtm->value.str[0]=='?')
//		{
//			fklAddCompDef(firAtm->value.str+1,curEnv);
//			fir=fklNextCptr(fir);
//			continue;
//		}
//
//		uint8_t opcode=findOpcode(firAtm->value.str);
//
//		if(opcode==0)
//		{
//			state->place=fir;
//			state->state=SYNTAXERROR;
//			uint32_t i=0;
//			for(;i<stack->top;i++)
//				fklFreeByteCodeLabel(stack->data[i]);
//			fklFreeComStack(stack);
//			return NULL;
//		}
//
//		if(codeName[opcode].len!=0&&fklNextCptr(fir)==NULL)
//		{
//			state->place=objCptr;
//			state->state=SYNTAXERROR;
//			uint32_t i=0;
//			for(;i<stack->top;i++)
//				fklFreeByteCodeLabel(stack->data[i]);
//			fklFreeComStack(stack);
//
//			return NULL;
//		}
//
//		switch(codeName[opcode].len)
//		{
//			case -3:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					int32_t scope=0;
//					CompEnv* tmpEnv=curEnv;
//					CompDef* tmpDef=NULL;
//
//					if(tmpAtm->type!=SYM)
//					{
//						state->place=objCptr;
//						state->state=SYNTAXERROR;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//
//					while(tmpEnv!=NULL)
//					{
//						tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//						if(tmpDef!=NULL)break;
//						tmpEnv=tmpEnv->prev;
//						scope++;
//					}
//
//					if(!tmpDef)
//					{
//						state->place=tmpCptr;
//						state->state=SYMUNDEFINE;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//					sizeOfByteCode+=sizeof(char)+2*sizeof(int32_t);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case -2:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					if(tmpAtm->type!=BYTS)
//					{
//						state->place=tmpCptr;
//						state->state=SYNTAXERROR;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//
//					sizeOfByteCode+=sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size;
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case -1:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					if(tmpAtm->type!=SYM&&tmpAtm->type!=STR)
//					{
//						state->place=tmpCptr;
//						state->state=SYNTAXERROR;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//
//					sizeOfByteCode+=sizeof(char)*2+strlen(tmpAtm->value.str);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 0:
//				{
//					sizeOfByteCode+=sizeof(char);
//					fir=fklNextCptr(fir);
//				}
//				break;
//			case 1:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					if(tmpAtm->type!=CHR)
//					{
//						state->place=tmpCptr;
//						state->state=SYNTAXERROR;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//
//					sizeOfByteCode+=sizeof(char)*2;
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 4:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					if(opcode==FAKE_PUSH_VAR)
//					{
//						CompEnv* tmpEnv=curEnv;
//						CompDef* tmpDef=NULL;
//						if(tmpAtm->type!=STR&&tmpAtm->type!=SYM)
//						{
//							state->place=tmpCptr;
//							state->state=SYMUNDEFINE;
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//						while(tmpEnv!=NULL)
//						{
//							tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//							if(tmpDef!=NULL)break;
//							tmpEnv=tmpEnv->prev;
//						}
//						if(!tmpDef)
//						{
//							state->place=tmpCptr;
//							state->state=SYMUNDEFINE;
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//					}
//					else if(opcode==FAKE_PUSH_SYM)
//					{
//						if(tmpAtm->type!=SYM)
//						{
//							state->place=tmpCptr;
//							state->state=SYNTAXERROR;
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//						fklAddSymbolToGlob(tmpAtm->value.str);
//					}
//					else if(opcode==FAKE_JMP||opcode==FAKE_JMP_IF_FALSE||opcode==FAKE_JMP_IF_TRUE)
//					{
//						if(tmpAtm->type!=SYM&&tmpAtm->type!=STR)
//						{
//							state->place=tmpCptr;
//							state->state=SYNTAXERROR;
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//					}
//					else
//					{
//						if(tmpAtm->type!=IN32)
//						{
//							state->place=tmpCptr;
//							state->state=SYNTAXERROR;
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//					}
//					sizeOfByteCode+=sizeof(char)+sizeof(int32_t);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 8:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					if(opcode!=FAKE_PUSH_DBL&&tmpAtm->type!=DBL&&tmpAtm->type!=IN64)
//					{
//						state->place=tmpCptr;
//						state->state=SYNTAXERROR;
//						uint32_t i=0;
//						for(;i<stack->top;i++)
//							fklFreeByteCodeLabel(stack->data[i]);
//						fklFreeComStack(stack);
//						return NULL;
//					}
//
//					sizeOfByteCode+=sizeof(char)+sizeof(double);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//		}
//	}
//
//	mergeSort(stack->data,stack->top,sizeof(void*),cmpByteCodeLabel);
//	fir=fklNextCptr(fklGetFirstCptr(objCptr));
//	tmp=fklNewByteCodelnt(fklNewByteCode(0));
//	while(fir)
//	{
//		AST_atom* firAtm=fir->u.atom;
//		if(firAtm->value.str[0]==':'||firAtm->value.str[0]=='?')
//		{
//			fir=fklNextCptr(fir);
//			continue;
//		}
//		uint8_t opcode=findOpcode(firAtm->value.str);
//
//		ByteCodelnt* tmpByteCodelnt=NULL;
//		switch(codeName[opcode].len)
//		{
//			case -3:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					int32_t scope=0;
//					CompEnv* tmpEnv=curEnv;
//					CompDef* tmpDef=NULL;
//
//					while(tmpEnv!=NULL)
//					{
//						tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//						if(tmpDef!=NULL)break;
//						tmpEnv=tmpEnv->prev;
//						scope++;
//					}
//
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char)+2*sizeof(int32_t));
//					tmpByteCode->code[0]=opcode;
//					*((int32_t*)(tmpByteCode->code+sizeof(char)))=scope;
//					*((int32_t*)(tmpByteCode->code+sizeof(char)+sizeof(int32_t)))=tmpDef->id;
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case -2:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size);
//					tmpByteCode->code[0]=opcode;
//					*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpAtm->value.byts.size;
//					memcpy(tmpByteCode->code+sizeof(char)+sizeof(int32_t),tmpAtm->value.byts.str,tmpAtm->value.byts.size);
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case -1:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char)*2+strlen(tmpAtm->value.str));
//					tmpByteCode->code[0]=opcode;
//					strcpy((char*)tmpByteCode->code+sizeof(char),tmpAtm->value.str);
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 0:
//				{
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char));
//					tmpByteCode->code[0]=opcode;
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(fir);
//				}
//				break;
//			case 1:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char)*2);
//
//					tmpByteCode->code[0]=opcode;
//					tmpByteCode->code[1]=tmpAtm->value.chr;
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 4:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//					ByteCode* tmpByteCode=NULL;
//					if(opcode==FAKE_PUSH_VAR)
//					{
//						CompEnv* tmpEnv=curEnv;
//						CompDef* tmpDef=NULL;
//						while(tmpEnv!=NULL)
//						{
//							tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//							if(tmpDef!=NULL)break;
//							tmpEnv=tmpEnv->prev;
//						}
//						tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(int32_t));
//						tmpByteCode->code[0]=opcode;
//						*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpDef->id;
//					}
//					else if(opcode==FAKE_PUSH_SYM)
//					{
//						tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(Sid_t));
//						tmpByteCode->code[0]=opcode;
//						*(Sid_t*)(tmpByteCode->code+sizeof(char))=fklAddSymbolToGlob(tmpAtm->value.str)->id;
//					}
//					else if(opcode==FAKE_JMP||opcode==FAKE_JMP_IF_TRUE||opcode==FAKE_JMP_IF_FALSE)
//					{
//						ByteCodeLabel* label=fklFindByteCodeLabel(tmpAtm->value.str,stack);
//						if(label==NULL)
//						{
//							state->place=tmpCptr;
//							state->state=SYMUNDEFINE;
//							fklFreeByteCodelnt(tmp);
//							uint32_t i=0;
//							for(;i<stack->top;i++)
//								fklFreeByteCodeLabel(stack->data[i]);
//							fklFreeComStack(stack);
//							return NULL;
//						}
//						tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(int32_t));
//						tmpByteCode->code[0]=opcode;
//						*((int32_t*)(tmpByteCode->code+sizeof(char)))=label->place-tmp->bc->size-5;
//					}
//					else
//					{
//						tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(int32_t));
//						tmpByteCode->code[0]=opcode;
//						*((int32_t*)(tmpByteCode->code+sizeof(char)))=tmpAtm->value.in32;
//					}
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//				}
//				break;
//			case 8:
//				{
//					AST_cptr* tmpCptr=fklNextCptr(fir);
//					AST_atom* tmpAtm=tmpCptr->u.atom;
//
//					ByteCode* tmpByteCode=fklNewByteCode(sizeof(char)+sizeof(double));
//
//					tmpByteCode->code[0]=opcode;
//					if(opcode==FAKE_PUSH_DBL)
//						*((double*)(tmpByteCode->code+sizeof(char)))=(tmpAtm->type==DBL)?tmpAtm->value.dbl:tmpAtm->value.in64;
//					else
//						*((int64_t*)(tmpByteCode->code+sizeof(char)))=(tmpAtm->type==DBL)?tmpAtm->value.dbl:tmpAtm->value.in64;
//
//					GENERATE_LNT(tmpByteCodelnt,tmpByteCode);
//					fir=fklNextCptr(tmpCptr);
//
//				}
//				break;
//		}
//		fklCodefklLntCat(tmp,tmpByteCodelnt);
//		fklFreeByteCodelnt(tmpByteCodelnt);
//	}
//	uint32_t i=0;
//	for(;i<stack->top;i++)
//		fklFreeByteCodeLabel(stack->data[i]);
//	fklFreeComStack(stack);
//	return tmp;
//}

ByteCodelnt* fklCompileImport(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
#ifdef _WIN32
	char divstr[]="\\";
#else
	char divstr[]="/";
#endif

	FakeMemMenager* memMenager=fklNewFakeMemMenager(32);
	char postfix[]=".fkl";
	ByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	fklPushFakeMem(tmp,(GenDestructor)fklFreeByteCodelnt,memMenager);
	chdir(inter->curDir);
	char* prev=NULL;
	/* 获取文件路径和文件名
	 * 然后打开并查找第一个library表达式
	 */
	AST_cptr* plib=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;plib;plib=fklNextCptr(plib))
	{
		char* libPrefix=NULL;
		AST_cptr* pairOfpPartsOfPath=plib;
		AST_cptr* pPartsOfPath=fklGetFirstCptr(plib);
		if(!pPartsOfPath)
		{
			state->state=SYNTAXERROR;
			state->place=plib;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeFakeMemMenager(memMenager);
			return NULL;
		}
		uint32_t count=0;
		const char** partsOfPath=(const char**)malloc(sizeof(const char*)*0);
		FAKE_ASSERT(partsOfPath,"fklCompileImport",__FILE__,__LINE__);
		fklPushFakeMem(partsOfPath,free,memMenager);
		while(pPartsOfPath)
		{
			if(pPartsOfPath->type!=ATM)
			{
				state->state=SYNTAXERROR;
				state->place=plib;
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeFakeMemMenager(memMenager);
				return NULL;
			}
			AST_atom* tmpAtm=pPartsOfPath->u.atom;
			if(tmpAtm->type!=STR&&tmpAtm->type!=SYM)
			{
				state->state=SYNTAXERROR;
				state->place=plib;
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeFakeMemMenager(memMenager);
				return NULL;
			}
			if(!strcmp(tmpAtm->value.str,"prefix")&&fklNextCptr(pPartsOfPath)->type==PAIR)
			{
				AST_cptr* tmpCptr=fklNextCptr(pPartsOfPath);
				if(!tmpCptr||tmpCptr->type!=PAIR)
				{
					state->state=SYNTAXERROR;
					state->place=objCptr;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					fklFreeFakeMemMenager(memMenager);
					return NULL;
				}
				tmpCptr=fklNextCptr(tmpCptr);
				if(!tmpCptr||fklNextCptr(tmpCptr)||tmpCptr->type!=ATM)
				{
					state->state=SYNTAXERROR;
					state->place=plib;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					fklFreeFakeMemMenager(memMenager);
					return NULL;
				}
				AST_atom* prefixAtom=tmpCptr->u.atom;
				if(prefixAtom->type!=STR&&prefixAtom->type!=SYM)
				{
					state->state=SYNTAXERROR;
					state->place=plib;
					FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
					fklFreeFakeMemMenager(memMenager);
					return NULL;
				}
				libPrefix=fklCopyStr(prefixAtom->value.str);
				pairOfpPartsOfPath=fklNextCptr(pPartsOfPath);
				pPartsOfPath=fklGetFirstCptr(pairOfpPartsOfPath);
				continue;
			}
			count++;
			partsOfPath=(const char**)fklReallocFakeMem(partsOfPath,realloc(partsOfPath,sizeof(const char*)*count),memMenager);
			FAKE_ASSERT(pPartsOfPath,"fklCompileImport",__FILE__,__LINE__);
			partsOfPath[count-1]=tmpAtm->value.str;
			pPartsOfPath=fklNextCptr(pPartsOfPath);
		}
		uint32_t totalPathLength=0;
		uint32_t i=0;
		for(;i<count;i++)
			totalPathLength+=strlen(partsOfPath[i]);
		totalPathLength+=count+strlen(postfix);
		char* path=(char*)malloc(sizeof(char)*totalPathLength);
		FAKE_ASSERT(path,"fklCompileImport",__FILE__,__LINE__);
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
		{
			char t[]="lib/";
			size_t len=strlen(InterpreterPath)+strlen(t)+strlen(path)+2;
			char* pathWithInterpreterPath=(char*)malloc(sizeof(char)*len);
			FAKE_ASSERT(pathWithInterpreterPath,"fklCompileImport",__FILE__,__LINE__);
			sprintf(pathWithInterpreterPath,"%s/%s%s",InterpreterPath,t,path);
			fp=fopen(pathWithInterpreterPath,"r");
			if(!fp)
			{
				fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);
				perror(path);
				free(pathWithInterpreterPath);
				free(path);
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				fklFreeFakeMemMenager(memMenager);
				return NULL;
			}
			sprintf(pathWithInterpreterPath,"%s%s",t,path);
			free(path);
			path=pathWithInterpreterPath;
		}
		if(fklHasLoadSameFile(path,inter))
		{
			state->state=CIRCULARLOAD;
			state->place=objCptr;
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			fklFreeFakeMemMenager(memMenager);
			return NULL;
		}
		fklAddSymbolToGlob(path);
		Intpr* tmpInter=fklNewIntpr(path,fp,fklNewCompEnv(NULL),inter->lnt,inter->deftypes);
		fklInitGlobKeyWord(tmpInter->glob);
		free(path);
		tmpInter->prev=inter;
		ByteCode* resTp=fklNewByteCode(1);
		ByteCode* setTp=fklNewByteCode(1);
		ByteCode* popTp=fklNewByteCode(1);
		resTp->code[0]=FAKE_RES_TP;
		setTp->code[0]=FAKE_SET_TP;
		popTp->code[0]=FAKE_POP_TP;
		fklPushFakeMem(resTp,(GenDestructor)fklFreeByteCode,memMenager);
		fklPushFakeMem(setTp,(GenDestructor)fklFreeByteCode,memMenager);
		fklPushFakeMem(popTp,(GenDestructor)fklFreeByteCode,memMenager);
		ByteCodelnt* libByteCodelnt=fklNewByteCodelnt(fklNewByteCode(0));
		fklPushFakeMem(libByteCodelnt,(GenDestructor)fklFreeByteCodelnt,memMenager);
		for(;;)
		{
			AST_cptr* begin=NULL;
			int unexpectEOF=0;
			char* list=fklReadInPattern(tmpInter->file,&prev,&unexpectEOF);
			if(list==NULL)continue;
			if(unexpectEOF)
			{
				switch(unexpectEOF)
				{
					case 1:
						fprintf(stderr,"error of reader:Unexpect EOF at line %d of %s\n",inter->curline,inter->filename);
						break;
					case 2:
						fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",inter->curline,inter->filename);
						break;
				}
				free(list);
				break;
			}
			begin=fklCreateTree(list,tmpInter,NULL);
			if(fklIsAllSpace(list))
			{
				free(list);
				break;
			}
			if(begin!=NULL)
			{
				//查找library并编译library
				if(fklIsLibraryExpression(begin))
				{
					free(prev);
					prev=NULL;
					AST_cptr* libName=fklNextCptr(fklGetFirstCptr(begin));
					if(fklAST_cptrcmp(libName,pairOfpPartsOfPath))
					{
						CompEnv* tmpCurEnv=fklNewCompEnv(tmpInter->glob);
						AST_cptr* exportCptr=fklNextCptr(libName);
						if(!exportCptr||!fklIsExportExpression(exportCptr))
						{
							fklExError(begin,INVALIDEXPR,tmpInter);
							fklDeleteCptr(begin);
							free(begin);
							FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
							FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
							chdir(tmpInter->prev->curDir);
							tmpInter->lnt=NULL;
							tmpInter->deftypes=NULL;
							fklFreeIntpr(tmpInter);
							if(libPrefix)
								free(libPrefix);
							fklFreeFakeMemMenager(memMenager);
							free(list);
							return NULL;
						}
						else
						{
							const char** exportSymbols=(const char**)malloc(sizeof(const char*)*0);
							fklPushFakeMem(exportSymbols,free,memMenager);
							FAKE_ASSERT(exportSymbols,"fklCompileImport",__FILE__,__LINE__);
							AST_cptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
							uint32_t num=0;
							for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
							{
								if(pExportSymbols->type!=ATM
										||pExportSymbols->u.atom->type!=SYM)
								{
									fklExError(exportCptr,SYNTAXERROR,tmpInter);
									fklDeleteCptr(begin);
									free(begin);
									FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
									chdir(tmpInter->prev->curDir);
									tmpInter->lnt=NULL;
									tmpInter->deftypes=NULL;
									fklFreeIntpr(tmpInter);
									if(libPrefix)
										free(libPrefix);
									fklFreeFakeMemMenager(memMenager);
									free(list);
									return NULL;
								}
								AST_atom* pSymbol=pExportSymbols->u.atom;
								num++;
								exportSymbols=(const char**)fklReallocFakeMem(exportSymbols,realloc(exportSymbols,sizeof(const char*)*num),memMenager);
								FAKE_ASSERT(exportSymbols,"fklCompileImport",__FILE__,__LINE__);
								exportSymbols[num-1]=pSymbol->value.str;
							}
							mergeSort(exportSymbols,num,sizeof(const char*),cmpString);
							AST_cptr* pBody=fklNextCptr(fklNextCptr(libName));
							fklInitCompEnv(tmpInter->glob);
							tmpInter->glob->prefix=libPrefix;
							tmpInter->glob->exp=exportSymbols;
							tmpInter->glob->n=num;
							for(;pBody;pBody=fklNextCptr(pBody))
							{
								ByteCodelnt* otherByteCodelnt=fklCompile(pBody,tmpCurEnv,tmpInter,state,1);
								if(state->state)
								{
									fklExError(state->place,state->state,tmpInter);
									fklDeleteCptr(begin);
									free(begin);
									FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
									FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
									chdir(tmpInter->prev->curDir);
									tmpInter->lnt=NULL;
									tmpInter->deftypes=NULL;
									fklFreeIntpr(tmpInter);
									state->state=0;
									state->place=NULL;
									if(libPrefix)
										free(libPrefix);
									fklFreeFakeMemMenager(memMenager);
									free(list);
									return NULL;
								}
								if(libByteCodelnt->bc->size)
								{
									fklReCodeCat(resTp,otherByteCodelnt->bc);
									otherByteCodelnt->l[0]->cpc+=1;
									INCREASE_ALL_SCP(otherByteCodelnt->l,otherByteCodelnt->ls-1,resTp->size);
								}
								fklCodefklLntCat(libByteCodelnt,otherByteCodelnt);
								fklFreeByteCodelnt(otherByteCodelnt);
							}
							fklReCodeCat(setTp,libByteCodelnt->bc);
							if(!libByteCodelnt->l)
							{
								libByteCodelnt->ls=1;
								libByteCodelnt->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
								FAKE_ASSERT(libByteCodelnt->l,"fklCompileImport",__FILE__,__LINE__);
								libByteCodelnt->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(tmpInter->filename)->id,0,libByteCodelnt->bc->size,objCptr->curline);
							}
							else
							{
								libByteCodelnt->l[0]->cpc+=1;
								INCREASE_ALL_SCP(libByteCodelnt->l,libByteCodelnt->ls-1,setTp->size);
							}
							fklCodeCat(libByteCodelnt->bc,popTp);
							libByteCodelnt->l[libByteCodelnt->ls-1]->cpc+=popTp->size;
							fklCodefklLntCat(tmp,libByteCodelnt);
							fklDeleteFakeMem(libByteCodelnt,memMenager);
							fklFreeByteCodelnt(libByteCodelnt);
							libByteCodelnt=NULL;
							ByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(int32_t));
							pushProc->code[0]=FAKE_PUSH_PROC;
							*(int32_t*)(pushProc->code+sizeof(char))=tmp->bc->size;
							fklReCodeCat(pushProc,tmp->bc);
							INCREASE_ALL_SCP(tmp->l,tmp->ls-1,pushProc->size);
							fklFreeByteCode(pushProc);
							ByteCode* invoke=fklNewByteCode(sizeof(char));
							invoke->code[0]=FAKE_INVOKE;
							fklCodeCat(tmp->bc,invoke);
							tmp->l[tmp->ls-1]->cpc+=invoke->size;
							fklFreeByteCode(invoke);
						}
						AST_cptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
						for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
						{
							if(pExportSymbols->type!=ATM
									||pExportSymbols->u.atom->type!=SYM)
							{
								fklExError(exportCptr,SYNTAXERROR,tmpInter);
								fklDeleteCptr(begin);
								free(begin);
								FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
								fklFreeByteCodelnt(tmp);
								chdir(tmpInter->prev->curDir);
								tmpInter->lnt=NULL;
								tmpInter->deftypes=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								fklFreeFakeMemMenager(memMenager);
								free(list);
								return NULL;
							}
							AST_atom* pSymbol=pExportSymbols->u.atom;
							CompDef* tmpDef=NULL;
							char* symbolWouldExport=NULL;
							if(libPrefix)
							{
								size_t len=strlen(libPrefix)+strlen(pSymbol->value.str)+1;
								symbolWouldExport=(char*)malloc(sizeof(char)*len);
								FAKE_ASSERT(symbolWouldExport,"fklCompileImport",__FILE__,__LINE__);
								sprintf(symbolWouldExport,"%s%s",libPrefix,pSymbol->value.str);
							}
							else
							{
								symbolWouldExport=(char*)malloc(sizeof(char)*(strlen(pSymbol->value.str)+1));
								FAKE_ASSERT(symbolWouldExport,"fklCompileImport",__FILE__,__LINE__);
								strcpy(symbolWouldExport,pSymbol->value.str);
							}
							tmpDef=fklFindCompDef(symbolWouldExport,tmpInter->glob);
							if(!tmpDef)
							{
								fklExError(pExportSymbols,SYMUNDEFINE,tmpInter);
								fklDeleteCptr(begin);
								free(begin);
								FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
								chdir(tmpInter->prev->curDir);
								tmpInter->lnt=NULL;
								tmpInter->deftypes=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								free(symbolWouldExport);
								fklFreeFakeMemMenager(memMenager);
								free(list);
								return NULL;
							}
							else
								fklAddCompDef(symbolWouldExport,curEnv);
							free(symbolWouldExport);
						}
						fklCodelntCopyCat(curEnv->proc,tmp);
						PreMacro* headOfMacroOfImportedFile=tmpCurEnv->macro;
						while(headOfMacroOfImportedFile)
						{
							fklAddDefinedMacro(headOfMacroOfImportedFile,curEnv);
							PreMacro* prev=headOfMacroOfImportedFile;
							headOfMacroOfImportedFile=headOfMacroOfImportedFile->next;
							free(prev);
						}
						fklDestroyCompEnv(tmpCurEnv);
						tmpInter->glob->macro=NULL;
						tmpCurEnv->macro=NULL;
						fklDeleteCptr(begin);
						free(begin);
						free(list);
						break;
					}
				}
				fklDeleteCptr(begin);
				free(begin);
			}
			else
			{
				FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
				if(libPrefix)
					free(libPrefix);
				fklFreeFakeMemMenager(memMenager);
				return NULL;
			}
			free(list);
			list=NULL;
		}

		if(libByteCodelnt&&!libByteCodelnt->bc->size)
		{
			state->state=(tmp)?LIBUNDEFINED:0;
			state->place=(tmp)?pairOfpPartsOfPath:NULL;
			FREE_ALL_LINE_NUMBER_TABLE(libByteCodelnt->l,libByteCodelnt->ls);
			FREE_ALL_LINE_NUMBER_TABLE(tmp->l,tmp->ls);
			tmpInter->lnt=NULL;
			tmpInter->deftypes=NULL;
			fklFreeIntpr(tmpInter);
			if(libPrefix)
				free(libPrefix);
			fklFreeFakeMemMenager(memMenager);
			return NULL;
		}
		chdir(tmpInter->prev->curDir);
		tmpInter->lnt=NULL;
		tmpInter->deftypes=NULL;
		fklFreeIntpr(tmpInter);
		if(libPrefix)
			free(libPrefix);
	}
	if(tmp)
	{
		fklDeleteFakeMem(tmp,memMenager);
		fklFreeFakeMemMenager(memMenager);
	}
	chdir(inter->curDir);
	return tmp;
}

ByteCodelnt* fklCompileTry(AST_cptr* objCptr,CompEnv* curEnv,Intpr* inter,ErrorState* state,int evalIm)
{
	AST_cptr* pExpression=fklNextCptr(fklGetFirstCptr(objCptr));
	AST_cptr* pCatchExpression=NULL;
	if(!pExpression||!(pCatchExpression=fklNextCptr(pExpression)))
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	ByteCodelnt* expressionByteCodelnt=fklCompile(pExpression,curEnv,inter,state,evalIm);
	if(state->state)
		return NULL;
	else
	{
		ByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(uint32_t));
		ByteCode* invoke=fklNewByteCode(1);
		invoke->code[0]=FAKE_INVOKE;
		pushProc->code[0]=FAKE_PUSH_PROC;
		*(uint32_t*)(pushProc->code+sizeof(char))=expressionByteCodelnt->bc->size;
		fklReCodeCat(pushProc,expressionByteCodelnt->bc);
		expressionByteCodelnt->l[0]->cpc+=pushProc->size;
		INCREASE_ALL_SCP(expressionByteCodelnt->l+1,expressionByteCodelnt->ls-1,pushProc->size);
		fklCodeCat(expressionByteCodelnt->bc,invoke);
		expressionByteCodelnt->l[expressionByteCodelnt->ls-1]->cpc+=invoke->size;
		fklFreeByteCode(pushProc);
		fklFreeByteCode(invoke);
	}
	AST_cptr* pErrSymbol=fklNextCptr(fklGetFirstCptr(pCatchExpression));
	AST_cptr* pHandlerExpression=NULL;
	if(!pErrSymbol
			||pErrSymbol->type!=ATM
			||pErrSymbol->u.atom->type!=SYM
			||!(pHandlerExpression=fklNextCptr(pErrSymbol)))
	{
		state->state=SYNTAXERROR;
		state->place=objCptr;
		FREE_ALL_LINE_NUMBER_TABLE(expressionByteCodelnt->l,expressionByteCodelnt->ls);
		fklFreeByteCodelnt(expressionByteCodelnt);
		return NULL;
	}
	char* errSymbol=pErrSymbol->u.atom->value.str;
	Sid_t sid=fklAddSymbolToGlob(errSymbol)->id;
	ComStack* handlerByteCodelntStack=fklNewComStack(32);
	for(;pHandlerExpression;pHandlerExpression=fklNextCptr(pHandlerExpression))
	{
		if(pHandlerExpression->type!=PAIR
				||fklGetFirstCptr(pHandlerExpression)->type!=ATM
				||fklGetFirstCptr(pHandlerExpression)->u.atom->type!=SYM)
		{
			state->state=SYNTAXERROR;
			state->place=objCptr;
			FREE_ALL_LINE_NUMBER_TABLE(expressionByteCodelnt->l,expressionByteCodelnt->ls);
			fklFreeByteCodelnt(expressionByteCodelnt);
			return NULL;
		}
		AST_cptr* pErrorType=fklGetFirstCptr(pHandlerExpression);
		AST_cptr* begin=fklNextCptr(pErrorType);
		if(!begin||pErrorType->type!=ATM||pErrorType->u.atom->type!=SYM)
		{
			state->state=SYNTAXERROR;
			state->place=objCptr;
			FREE_ALL_LINE_NUMBER_TABLE(expressionByteCodelnt->l,expressionByteCodelnt->ls);
			fklFreeByteCodelnt(expressionByteCodelnt);
			fklFreeComStack(handlerByteCodelntStack);
			return NULL;
		}
		CompEnv* tmpEnv=fklNewCompEnv(curEnv);
		fklAddCompDef(errSymbol,tmpEnv);
		ByteCodelnt* t=NULL;
		for(;begin;begin=fklNextCptr(begin))
		{
			ByteCodelnt* tmp1=fklCompile(begin,tmpEnv,inter,state,evalIm);
			if(state->state)
			{
				FREE_ALL_LINE_NUMBER_TABLE(expressionByteCodelnt->l,expressionByteCodelnt->ls);
				fklFreeByteCodelnt(expressionByteCodelnt);
				fklFreeComStack(handlerByteCodelntStack);
				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
				return NULL;
			}
			if(!t)
				t=tmp1;
			else
			{
				ByteCode* resTp=fklNewByteCode(1);
				resTp->code[0]=FAKE_RES_TP;
				fklReCodeCat(resTp,tmp1->bc);
				tmp1->l[0]->cpc+=1;
				INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
				fklFreeByteCode(resTp);
			}
		}
		fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
		ByteCode* setTp=fklNewByteCode(1);
		ByteCode* popTp=fklNewByteCode(1);
		setTp->code[0]=FAKE_SET_TP;
		popTp->code[0]=FAKE_POP_TP;
		fklReCodeCat(setTp,t->bc);
		t->l[0]->cpc+=1;
		INCREASE_ALL_SCP(t->l+1,t->ls-1,setTp->size);
		fklCodeCat(t->bc,popTp);
		t->l[t->ls-1]->cpc+=popTp->size;
		fklFreeByteCode(setTp);
		fklFreeByteCode(popTp);
		char* errorType=pErrorType->u.atom->value.str;
		Sid_t typeid=fklAddSymbolToGlob(errorType)->id;
		ByteCode* errorTypeByteCode=fklNewByteCode(sizeof(Sid_t)+sizeof(uint32_t));
		*(Sid_t*)(errorTypeByteCode->code)=typeid;
		*(uint32_t*)(errorTypeByteCode->code+sizeof(Sid_t))=t->bc->size;
		fklReCodeCat(errorTypeByteCode,t->bc);
		t->l[0]->cpc+=errorTypeByteCode->size;
		INCREASE_ALL_SCP(t->l+1,t->ls-1,errorTypeByteCode->size);
		fklFreeByteCode(errorTypeByteCode);
		fklPushComStack(t,handlerByteCodelntStack);
	}
	ByteCodelnt* t=expressionByteCodelnt;
	size_t numOfHandlerByteCode=handlerByteCodelntStack->top;
	while(!fklIsComStackEmpty(handlerByteCodelntStack))
	{
		ByteCodelnt* tmp=fklPopComStack(handlerByteCodelntStack);
		reCodefklLntCat(tmp,t);
		fklFreeByteCodelnt(tmp);
	}
	fklFreeComStack(handlerByteCodelntStack);
	ByteCode* header=fklNewByteCode(sizeof(Sid_t)+sizeof(int32_t)+sizeof(char));
	header->code[0]=FAKE_PUSH_TRY;
	*(Sid_t*)(header->code+sizeof(char))=sid;
	*(int32_t*)(header->code+sizeof(char)+sizeof(Sid_t))=numOfHandlerByteCode;
	fklReCodeCat(header,t->bc);
	t->l[0]->cpc+=header->size;
	INCREASE_ALL_SCP(t->l+1,t->ls-1,header->size);
	fklFreeByteCode(header);
	ByteCode* popTry=fklNewByteCode(sizeof(char));
	popTry->code[0]=FAKE_POP_TRY;
	fklCodeCat(t->bc,popTry);
	t->l[t->ls-1]->cpc+=popTry->size;
	fklFreeByteCode(popTry);
	return t;
}
