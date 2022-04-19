#include<fakeLisp/compiler.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/mem.h>
#include<stddef.h>
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#include<unistd.h>
#include<math.h>

static char* InterpreterPath=NULL;
static int MacroPatternCmp(const FklAstCptr*,const FklAstCptr*);
static int fmatcmp(const FklAstCptr*,const FklAstCptr*,FklPreEnv**,FklCompEnv*);
static int fklAddDefinedMacro(FklPreMacro* macro,FklCompEnv* curEnv);
static FklErrorState defmacro(FklAstCptr*,FklCompEnv*,FklInterpreter*);
static FklCompEnv* createPatternCompEnv(char**,int32_t,FklCompEnv*);
static FklVMenv* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,FklVMheap* heap)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
	FklCompEnv* tcEnv=cEnv;
	for(;tcEnv;tcEnv=tcEnv->prev)
		fklPushPtrStack(tcEnv,stack);
	FklVMenv* preEnv=NULL;
	FklVMenv* vEnv=NULL;
	uint32_t bs=t->bc->size;
	while(!fklIsPtrStackEmpty(stack))
	{
		FklCompEnv* curEnv=fklPopPtrStack(stack);
		vEnv=fklNewVMenv(preEnv);
		if(!preEnv)
			fklInitGlobEnv(vEnv,heap);
		else
			fklDecreaseVMenvRefcount(preEnv);
		preEnv=vEnv;
		if(curEnv->proc->bc->size)
		{
			FklVM* tmpVM=fklNewTmpVM(NULL);
			fklIncreaseVMenvRefcount(vEnv);
			fklCodelntCopyCat(t,curEnv->proc);
			fklInitVMRunningResource(tmpVM,vEnv,heap,t,bs,curEnv->proc->bc->size);
			bs+=curEnv->proc->bc->size;
			int i=fklRunVM(tmpVM);
			if(i==1)
			{
				fklUninitVMRunningResource(tmpVM);
				fklFreePtrStack(stack);
				return NULL;
			}
			fklUninitVMRunningResource(tmpVM);
		}
	}
	fklFreePtrStack(stack);
	return vEnv;
}

static int cmpString(const void* a,const void* b)
{
	return strcmp(*(const char**)a,*(const char**)b);
}

//static int cmpByteCodeLabel(const void* a,const void* b)
//{
//	return strcmp(((const FklByteCodeLabel*)a)->label,((const FklByteCodeLabel*)b)->label);
//}

FklPreMacro* fklPreMacroMatch(const FklAstCptr* objCptr,FklPreEnv** pmacroEnv,FklCompEnv* curEnv,FklCompEnv** pCEnv)
{
	for(;curEnv;curEnv=curEnv->prev)
	{
		FklPreMacro* current=curEnv->macro;
		for(;current;current=current->next)
			if(fmatcmp(objCptr,current->pattern,pmacroEnv,curEnv))
			{
				*pCEnv=curEnv;
				return current;
			}
	}
	return NULL;
}

int fklPreMacroExpand(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter)
{
	FklPreEnv* macroEnv=NULL;
	FklByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
	FklPreMacro* tmp=fklPreMacroMatch(objCptr,&macroEnv,curEnv,&curEnv);
	if(tmp!=NULL)
	{
		FklVM* tmpVM=fklNewTmpVM(NULL);
		FklVMenv* tmpGlob=genGlobEnv(tmp->macroEnv,t,tmpVM->heap);
		if(!tmpGlob)
		{
			fklDestroyEnv(macroEnv);
			fklFreeVMstack(tmpVM->stack);
			fklFreePtrStack(tmpVM->rstack);
			fklFreePtrStack(tmpVM->tstack);
			fklFreeVMheap(tmpVM->heap);
			fklFreeByteCodeAndLnt(t);
			free(tmpVM);
			return 2;
		}
		FklVMenv* macroVMenv=fklCastPreEnvToVMenv(macroEnv,tmpGlob,tmpVM->heap);
		fklDestroyEnv(macroEnv);
		macroEnv=NULL;
		uint32_t start=t->bc->size;
		fklCodelntCopyCat(t,tmp->proc);
		fklInitVMRunningResource(tmpVM,macroVMenv,tmpVM->heap,t,start,tmp->proc->bc->size);
		FklAstCptr* tmpCptr=NULL;
		int i=fklRunVM(tmpVM);
		if(!i)
		{
			tmpCptr=fklCastVMvalueToCptr(fklPopAndGetVMstack(tmpVM->stack),objCptr->curline);
			if(!tmpCptr)
			{
				if(inter->filename)
					fprintf(stderr,"error of compiling: Circular reference occur in expanding macro at line %u of %s\n",objCptr->curline,inter->filename);
				else
					fprintf(stderr,"error of compiling: Circular reference occur in expanding macro at line %u\n",objCptr->curline);
				fklFreeVMenv(tmpGlob);
				fklFreeByteCodeAndLnt(t);
				fklFreeVMheap(tmpVM->heap);
				fklUninitVMRunningResource(tmpVM);
				return 2;
			}
			fklReplaceCptr(objCptr,tmpCptr);
			fklDeleteCptr(tmpCptr);
			free(tmpCptr);
		}
		else
		{
			fklFreeVMenv(tmpGlob);
			fklFreeByteCodeAndLnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklUninitVMRunningResource(tmpVM);
			return 2;
		}
		fklFreeVMenv(tmpGlob);
		fklFreeByteCodeAndLnt(t);
		fklFreeVMheap(tmpVM->heap);
		fklUninitVMRunningResource(tmpVM);
		return 1;
	}
	fklFreeByteCodeAndLnt(t);
	return 0;
}

static int fklAddDefinedMacro(FklPreMacro* macro,FklCompEnv* curEnv)
{
	FklAstCptr* tmpCptr=NULL;
	FklPreMacro* current=curEnv->macro;
	FklAstCptr* pattern=macro->pattern;
	while(current!=NULL&&!MacroPatternCmp(macro->pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		tmpCptr=&pattern->u.pair->car;
		fklAddKeyWord(tmpCptr->u.atom->value.str,curEnv);
		//for(tmpCptr=&pattern->u.pair->car;tmpCptr!=NULL;tmpCptr=fklNextCptr(tmpCptr))
		//{
		//	if(tmpCptr->type==FKL_ATM)
		//	{
		//		FklAstAtom* carAtm=tmpCptr->u.atom;
		//		FklAstAtom* cdrAtm=(tmpCptr->outer->cdr.type==FKL_ATM)?tmpCptr->outer->cdr.u.atom:NULL;
		//		if(carAtm->type==FKL_SYM&&!isVal(carAtm->value.str))fklAddKeyWord(carAtm->value.str,curEnv);
		//		if(cdrAtm!=NULL&&cdrAtm->type==FKL_SYM&&!isVal(cdrAtm->value.str))fklAddKeyWord(cdrAtm->value.str,curEnv);
		//	}
		//}
		FklPreMacro* current=(FklPreMacro*)malloc(sizeof(FklPreMacro));
		FKL_ASSERT(current,__func__);
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
		fklFreeByteCodeAndLnt(current->proc);
		current->pattern=macro->pattern;
		current->proc=macro->proc;
		current->macroEnv=macro->macroEnv;
	}
	return 0;
}

int fklAddMacro(FklAstCptr* pattern,FklByteCodelnt* proc,FklCompEnv* curEnv)
{
	if(pattern->type!=FKL_PAIR)return FKL_SYNTAXERROR;
	FklAstCptr* tmpCptr=NULL;
	FklPreMacro* current=curEnv->macro;
	while(current!=NULL&&!MacroPatternCmp(pattern,current->pattern))
		current=current->next;
	if(current==NULL)
	{
		tmpCptr=&pattern->u.pair->car;
		fklAddKeyWord(tmpCptr->u.atom->value.str,curEnv);
//		for(tmpCptr=&pattern->u.pair->car;tmpCptr!=NULL;tmpCptr=fklNextCptr(tmpCptr))
//		{
//			if(tmpCptr->type==FKL_ATM)
//			{
//				FklAstAtom* carAtm=tmpCptr->u.atom;
//				FklAstAtom* cdrAtm=(tmpCptr->outer->cdr.type==FKL_ATM)?tmpCptr->outer->cdr.u.atom:NULL;
//				if(carAtm->type==FKL_SYM&&!isVal(carAtm->value.str))fklAddKeyWord(carAtm->value.str,curEnv);
//				if(cdrAtm!=NULL&&cdrAtm->type==FKL_SYM&&!isVal(cdrAtm->value.str))fklAddKeyWord(cdrAtm->value.str,curEnv);
//			}
//		}
		FKL_ASSERT((current=(FklPreMacro*)malloc(sizeof(FklPreMacro))),__func__);
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
		fklFreeByteCodeAndLnt(current->proc);
		current->pattern=pattern;
		current->proc=proc;
	}
	//fklPrintAllKeyWord();
	return 0;
}

int MacroPatternCmp(const FklAstCptr* first,const FklAstCptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	FklAstPair* firPair=NULL;
	FklAstPair* secPair=NULL;
	FklAstPair* tmpPair=(first->type==FKL_PAIR)?first->u.pair:NULL;
	for(;;)
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
				if(firAtm->type==FKL_SYM)return 0;
				if(firAtm->type==FKL_STR&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_F64&&fabs(firAtm->value.f64-secAtm->value.f64)!=0)return 0;
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

int fmatcmp(const FklAstCptr* origin,const FklAstCptr* format,FklPreEnv** pmacroEnv,FklCompEnv* curEnv)
{
	FklAstPair* tmpPair=(format->type==FKL_PAIR)?format->u.pair:NULL;
	FklAstPair* forPair=tmpPair;
	FklAstPair* oriPair=(origin->type==FKL_PAIR)?origin->u.pair:NULL;
	if(tmpPair->car.type!=oriPair->car.type)
		return 0;
	if(oriPair->car.u.atom->type==FKL_SYM&&strcmp(tmpPair->car.u.atom->value.str,oriPair->car.u.atom->value.str))
		return 0;
	format=&forPair->cdr;
	origin=&oriPair->cdr;
	FklPreEnv* macroEnv=fklNewEnv(NULL);
	while(origin!=NULL)
	{
		if(format->type==FKL_PAIR&&origin->type==FKL_PAIR)
		{
			forPair=format->u.pair;
			oriPair=origin->u.pair;
			format=&forPair->car;
			origin=&oriPair->car;
			continue;
		}
		else if(format->type==FKL_ATM)
		{
			FklAstAtom* tmpAtm=format->u.atom;
			if(tmpAtm->type==FKL_SYM)
				fklAddDefine(tmpAtm->value.str,origin,macroEnv);
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
			FklAstPair* oriPrev=NULL;
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

FklCompEnv* fklCreateMacroCompEnv(const FklAstCptr* objCptr,FklCompEnv* prev)
{
	if(objCptr==NULL)return NULL;
	FklCompEnv* tmpEnv=fklNewCompEnv(prev);
	FklAstPair* tmpPair=(objCptr->type==FKL_PAIR)?objCptr->u.pair:NULL;
	FklAstPair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==FKL_PAIR)
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
		else if(objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)
		{
			if(objCptr->type!=FKL_NIL)
			{
				FklAstAtom* tmpAtm=objCptr->u.atom;
				if(tmpAtm->type==FKL_SYM)
				{
					const char* tmpStr=tmpAtm->value.str;
					fklAddCompDef(tmpStr,tmpEnv);
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
	return tmpEnv;
}

int fklRetree(FklAstCptr** fir,FklAstCptr* sec)
{
	while(*fir!=sec)
	{
		FklAstCptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
		fklReplaceCptr(preCptr,*fir);
		*fir=preCptr;
		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=FKL_NIL)return 0;
	}
	return 1;
}

void fklInitGlobKeyWord(FklCompEnv* glob)
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
}

void fklUninitPreprocess()
{
	fklFreeAllStringPattern();
}

FklAstCptr** dealArg(FklAstCptr* argCptr,int num)
{
	FklAstCptr** args=NULL;
	FKL_ASSERT((args=(FklAstCptr**)malloc(num*sizeof(FklAstCptr*))),__func__);
	int i=0;
	for(;i<num;i++,argCptr=fklNextCptr(argCptr))
		args[i]=argCptr;
	return args;
}

FklErrorState defmacro(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter)
{
	FklErrorState state={0,NULL};
	FklAstCptr* fir=fklGetFirstCptr(objCptr);
	FklAstCptr* argCptr=fklNextCptr(fir);
	FklAstCptr** args=dealArg(argCptr,2);
	if(args[0]->type!=FKL_PAIR)
	{
		if(args[0]->type!=FKL_NIL)
		{
			FklAstAtom* tmpAtom=args[0]->u.atom;
			if(tmpAtom->type!=FKL_STR)
			{
				fklPrintCompileError(args[0],FKL_SYNTAXERROR,inter);
				free(args);
				return state;
			}
			char* tmpStr=tmpAtom->value.str;
			if(!fklIsReDefStringPattern(tmpStr)&&fklIsInValidStringPattern(tmpStr))
			{
				fklPrintCompileError(args[0],FKL_INVALIDEXPR,inter);
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
			state.state=FKL_SYNTAXERROR;
			state.place=args[0];
			free(args);
			return state;
		}
	}
	else
	{
		FklAstCptr* head=fklGetFirstCptr(args[0]);
		if(head->type!=FKL_ATM||head->u.atom->type!=FKL_SYM)
		{
			state.state=FKL_INVALID_MACRO_PATTERN;
			state.place=args[0];
			free(args);
			return state;
		}
		FklAstCptr* pattern=fklNewCptr(0,NULL);
		fklReplaceCptr(pattern,args[0]);
		head=fklGetFirstCptr(pattern);
		if(curEnv->prev
				&&curEnv->prev->exp
				&&curEnv->prev->prefix)
		{
			char* t=fklCopyStr(curEnv->prev->prefix);
			t=fklStrCat(t,head->u.atom->value.str);
			free(head->u.atom->value.str);
			head->u.atom->value.str=t;
		}
		FklAstCptr* express=args[1];
		FklInterpreter* tmpInter=fklNewTmpIntpr(NULL,NULL);
		tmpInter->filename=inter->filename;
		tmpInter->curline=inter->curline;
		tmpInter->glob=curEnv;
		tmpInter->curDir=inter->curDir;
		tmpInter->prev=NULL;
		tmpInter->lnt=NULL;
		FklCompEnv* tmpCompEnv=fklCreateMacroCompEnv(&pattern->u.pair->cdr,tmpInter->glob);
		FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
		if(!state.state)
		{
			fklAddMacro(pattern,tmpByteCodelnt,curEnv);
			free(args);
		}
		else
		{
			if(tmpByteCodelnt)
				fklFreeByteCodeAndLnt(tmpByteCodelnt);
			free(args);
		}
		fklDestroyCompEnv(tmpCompEnv);
		free(tmpInter);
	}
	return state;
}

FklStringMatchPattern* fklAddStringPattern(char** parts,int32_t num,FklAstCptr* express,FklInterpreter* inter)
{
	FklStringMatchPattern* tmp=NULL;
	FklErrorState state={0,NULL};
	FklInterpreter* tmpInter=fklNewTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=inter->glob;
	tmpInter->curDir=inter->curDir;
	tmpInter->prev=NULL;
	tmpInter->lnt=NULL;
	FklCompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FKL_ASSERT(tmParts,__func__);
		int32_t i=0;
		for(;i<num;i++)
			tmParts[i]=fklCopyStr(parts[i]);
		tmp=fklNewStringMatchPattern(num,tmParts,tmpByteCodelnt);
	}
	else
	{
		if(tmpByteCodelnt)
			fklFreeByteCodeAndLnt(tmpByteCodelnt);
		fklPrintCompileError(state.place,state.state,inter);
		state.place=NULL;
		state.state=0;
	}
	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
	free(tmpInter);
	return tmp;
}

FklStringMatchPattern* fklAddReDefStringPattern(char** parts,int32_t num,FklAstCptr* express,FklInterpreter* inter)
{
	FklStringMatchPattern* tmp=fklFindStringPattern(parts[0]);
	FklErrorState state={0,NULL};
	FklInterpreter* tmpInter=fklNewTmpIntpr(NULL,NULL);
	tmpInter->filename=inter->filename;
	tmpInter->curline=inter->curline;
	tmpInter->glob=inter->glob;
	tmpInter->curDir=inter->curDir;
	tmpInter->prev=NULL;
	tmpInter->lnt=NULL;
	FklCompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FKL_ASSERT(tmParts,__func__);
		int32_t i=0;
		for(;i<num;i++)
			tmParts[i]=fklCopyStr(parts[i]);
		fklFreeStringArry(tmp->parts,num);
		fklFreeByteCodeAndLnt(tmp->u.bProc);
		tmp->parts=tmParts;
		tmp->u.bProc=tmpByteCodelnt;
	}
	else
	{
		if(tmpByteCodelnt)
			fklFreeByteCodeAndLnt(tmpByteCodelnt);
		fklPrintCompileError(state.place,state.state,inter);
		state.place=NULL;
		state.state=0;
	}
	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
	free(tmpInter);
	return tmp;
}

FklByteCodelnt* fklCompile(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	for(;;)
	{
		if(fklIsLoadExpression(objCptr))return fklCompileLoad(objCptr,curEnv,inter,state);
		if(fklIsConst(objCptr))return fklCompileConst(objCptr,curEnv,inter,state);
		if(fklIsUnquoteExpression(objCptr))return fklCompileUnquote(objCptr,curEnv,inter,state);
		if(fklIsQsquoteExpression(objCptr))return fklCompileQsquote(objCptr,curEnv,inter,state);
		if(fklIsSymbol(objCptr))return fklCompileSym(objCptr,curEnv,inter,state);
		if(fklIsDefExpression(objCptr))return fklCompileDef(objCptr,curEnv,inter,state);
		if(fklIsSetqExpression(objCptr))return fklCompileSetq(objCptr,curEnv,inter,state);
		if(fklIsSetfExpression(objCptr))return fklCompileSetf(objCptr,curEnv,inter,state);
		//if(fklIsGetfExpression(objCptr))return fklCompileGetf(objCptr,curEnv,inter,state);
		//if(fklIsSzofExpression(objCptr))return fklCompileSzof(objCptr,curEnv,inter,state);
		if(fklIsCondExpression(objCptr))return fklCompileCond(objCptr,curEnv,inter,state);
		if(fklIsAndExpression(objCptr))return fklCompileAnd(objCptr,curEnv,inter,state);
		if(fklIsOrExpression(objCptr))return fklCompileOr(objCptr,curEnv,inter,state);
		if(fklIsLambdaExpression(objCptr))return fklCompileLambda(objCptr,curEnv,inter,state);
		if(fklIsBeginExpression(objCptr)) return fklCompileBegin(objCptr,curEnv,inter,state);
		if(fklIsImportExpression(objCptr))return fklCompileImport(objCptr,curEnv,inter,state);
		if(fklIsTryExpression(objCptr))return fklCompileTry(objCptr,curEnv,inter,state);
		//if(fklIsFlsymExpression(objCptr))return fklCompileFlsym(objCptr,curEnv,inter,state);
		if(fklIsLibraryExpression(objCptr))
		{
			FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
			FKL_ASSERT(tmp->l,__func__);
			tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename
					,0,0,inter->curline);
			return tmp;
		}
		if(fklIsCatchExpression(objCptr)||fklIsUnqtespExpression(objCptr)||fklIsExportExpression(objCptr))
		{
			state->state=FKL_INVALIDEXPR;
			state->place=objCptr;
			return NULL;
		}
		if(fklIsDefmacroExpression(objCptr))
		{
			FklErrorState t=defmacro(objCptr,curEnv,inter);
			if(t.state)
			{
				state->state=t.state;
				state->place=t.place;
				return NULL;
			}
			FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
			FKL_ASSERT(tmp->l,__func__);
			tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename
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
			state->state=FKL_MACROEXPANDFAILED;
			state->place=objCptr;
			return NULL;
		}
		else if(!fklIsValid(objCptr)||fklHasKeyWord(objCptr,curEnv))
		{
			state->state=FKL_SYNTAXERROR;
			state->place=objCptr;
			return NULL;
		}
		else if(fklIsFuncCall(objCptr,curEnv))return fklCompileFuncCall(objCptr,curEnv,inter,state);
	}
}

FklCompEnv* createPatternCompEnv(char** parts,int32_t num,FklCompEnv* prev)
{
	if(parts==NULL)return NULL;
	FklCompEnv* tmpEnv=fklNewCompEnv(prev);
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

FklByteCode* fklCompileAtom(FklAstCptr* objCptr)
{
	FklAstAtom* tmpAtm=objCptr->u.atom;
	FklByteCode* tmp=NULL;
	switch(tmpAtm->type)
	{
		case FKL_SYM:
			tmp=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
			tmp->code[0]=FKL_PUSH_SYM;
			fklSetI32ToByteCode(tmp->code+sizeof(char),fklAddSymbolToGlob(tmpAtm->value.str)->id);
			break;
		case FKL_STR:
			tmp=fklNewByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FKL_PUSH_STR;
			strcpy((char*)tmp->code+1,tmpAtm->value.str);
			break;
		case FKL_I32:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FKL_PUSH_I32;
			fklSetI32ToByteCode(tmp->code+sizeof(char),tmpAtm->value.i32);
			break;
		case FKL_I64:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int64_t));
			tmp->code[0]=FKL_PUSH_I64;
			fklSetI64ToByteCode(tmp->code+sizeof(char),tmpAtm->value.i64);
			break;
		case FKL_F64:
			tmp=fklNewByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FKL_PUSH_F64;
			fklSetF64ToByteCode(tmp->code+sizeof(char),tmpAtm->value.f64);
			break;
		case FKL_CHR:
			tmp=fklNewByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FKL_PUSH_CHAR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
		case FKL_BYTS:
			tmp=fklNewByteCode(sizeof(char)+sizeof(tmpAtm->value.byts.size)+tmpAtm->value.byts.size);
			tmp->code[0]=FKL_PUSH_BYTS;
			fklSetU64ToByteCode(tmp->code+sizeof(char),tmpAtm->value.byts.size);
			memcpy(tmp->code+sizeof(char)+sizeof(tmpAtm->value.byts.size)
					,tmpAtm->value.byts.str
					,tmpAtm->value.byts.size);
			break;
		default:
			break;
	}
	return tmp;
}

FklByteCode* fklCompileNil()
{
	FklByteCode* tmp=fklNewByteCode(1);
	tmp->code[0]=FKL_PUSH_NIL;
	return tmp;
}

static FklByteCode* innerCompileConst(FklAstCptr* objCptr)
{
	switch(objCptr->type)
	{
		case FKL_PAIR:
			return fklCompilePair(objCptr);
			break;
		case FKL_ATM:
			if(objCptr->u.atom->type==FKL_SYM)
			{
				FklByteCode* tmp=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
				char* sym=objCptr->u.atom->value.str;
				FklSymTabNode* node=fklAddSymbolToGlob(sym);
				tmp->code[0]=FKL_PUSH_SYM;
				fklSetSidToByteCode(tmp->code+sizeof(char),node->id);
				return tmp;
			}
			else if(objCptr->u.atom->type!=FKL_VECTOR)
				return fklCompileAtom(objCptr);
			else
				return fklCompileVector(objCptr);
			break;
		case FKL_NIL:
			return fklCompileNil();
			break;
		default:
			break;
	}
	return NULL;
}

FklByteCode* fklCompileVector(FklAstCptr* objCptr)
{
	FklAstVector* vec=&objCptr->u.atom->value.vec;
	FklByteCode* pushVector=fklNewByteCode(sizeof(char)+sizeof(uint64_t));
	pushVector->code[0]=FKL_PUSH_VECTOR;
	fklSetU64ToByteCode(pushVector->code+sizeof(char),vec->size);
	FklByteCode* retval=fklNewByteCode(0);
	for(size_t i=0;i<vec->size;i++)
	{
		FklByteCode* tmp=innerCompileConst(&vec->base[i]);
		fklCodeCat(retval,tmp);
		fklFreeByteCode(tmp);
	}
	fklCodeCat(retval,pushVector);
	fklFreeByteCode(pushVector);
	return retval;
}

FklByteCode* fklCompilePair(FklAstCptr* objCptr)
{
	FklByteCode* tmp=fklNewByteCode(0);
	FklAstPair* objPair=objCptr->u.pair;
	FklAstPair* tmpPair=objPair;
	FklByteCode* popToCar=fklNewByteCode(1);
	FklByteCode* popToCdr=fklNewByteCode(1);
	FklByteCode* pushPair=fklNewByteCode(1);
	popToCar->code[0]=FKL_POP_CAR;
	popToCdr->code[0]=FKL_POP_CDR;
	pushPair->code[0]=FKL_PUSH_PAIR;
	while(objCptr!=NULL)
	{
		if(objCptr->type==FKL_PAIR)
		{
			fklCodeCat(tmp,pushPair);
			objPair=objCptr->u.pair;
			objCptr=&objPair->car;
			continue;
		}
		else if(objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)
		{
			FklByteCode* tmp1=(objCptr->type==FKL_ATM)?fklCompileAtom(objCptr):fklCompileNil();
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
			FklAstPair* prev=NULL;
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

FklByteCodelnt* fklCompileQsquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	FklAstCptr* top=objCptr;
	if(objCptr->type==FKL_ATM)
		return fklCompileConst(objCptr,curEnv,inter,state);
	else if(fklIsUnquoteExpression(objCptr))
		return fklCompileUnquote(objCptr,curEnv,inter,state);
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklAstPair* objPair=objCptr->u.pair;
	FklAstPair* tmpPair=objPair;
	FklByteCode* appd=fklNewByteCode(1);
	FklByteCode* popToCar=fklNewByteCode(1);
	FklByteCode* popToCdr=fklNewByteCode(1);
	FklByteCode* pushPair=fklNewByteCode(1);
	popToCar->code[0]=FKL_POP_CAR;
	popToCdr->code[0]=FKL_POP_CDR;
	pushPair->code[0]=FKL_PUSH_PAIR;
	appd->code[0]=FKL_APPEND;
	while(objCptr!=NULL)
	{
		if(fklIsUnquoteExpression(objCptr))
		{
			FklByteCodelnt* tmp1=fklCompileUnquote(objCptr,curEnv,inter,state);
			if(state->state!=0)
			{
				fklFreeByteCodeAndLnt(tmp);
				fklFreeByteCode(appd);
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				return NULL;
			}
			fklCodeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			fklCodeLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(fklIsUnqtespExpression(objCptr))
		{
			if(objCptr==&objPair->cdr||objCptr==top)
			{
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				fklFreeByteCode(appd);
				fklFreeByteCodeAndLnt(tmp);
				state->state=FKL_INVALIDEXPR;
				state->place=objCptr;
				return NULL;
			}
			FklByteCodelnt* tmp1=fklCompile(fklNextCptr(fklGetFirstCptr(objCptr)),curEnv,inter,state);
			if(state->state!=0)
			{
				fklFreeByteCodeAndLnt(tmp);
				fklFreeByteCode(appd);
				fklFreeByteCode(popToCar);
				fklFreeByteCode(popToCdr);
				fklFreeByteCode(pushPair);
				return NULL;
			}
			fklCodeCat(tmp1->bc,appd);
			tmp1->l[tmp1->ls-1]->cpc+=appd->size;
			fklCodeLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		else if(objCptr->type==FKL_PAIR)
		{
			if(!fklIsUnqtespExpression(fklGetFirstCptr(objCptr)))
			{
				fklCodeCat(tmp->bc,pushPair);
				if(!tmp->l)
				{
					tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
					FKL_ASSERT(tmp->l,__func__);
					tmp->ls=1;
					tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,pushPair->size,objCptr->curline);
				}
				else
					tmp->l[tmp->ls-1]->cpc+=pushPair->size;
			}
			objPair=objCptr->u.pair;
			objCptr=&objPair->car;
			continue;
		}
		else if((objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)&&(!fklIsUnqtespExpression(&objPair->car)))
		{
			FklByteCodelnt* tmp1=fklCompileConst(objCptr,curEnv,inter,state);
			fklCodeCat(tmp1->bc,(objCptr==&objPair->car)?popToCar:popToCdr);
			tmp1->l[tmp1->ls-1]->cpc+=(objCptr==&objPair->car)?popToCar->size:popToCdr->size;
			fklCodeLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		if(objPair!=NULL&&objCptr==&objPair->cdr)
		{
			FklAstPair* prev=NULL;
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

FklByteCode* fklCompileQuote(FklAstCptr* objCptr)
{
	objCptr=&objCptr->u.pair->car;
	objCptr=fklNextCptr(objCptr);
	switch(objCptr->type)
	{
		case FKL_PAIR:
			return fklCompilePair(objCptr);
			break;
		case FKL_ATM:
			if(objCptr->u.atom->type==FKL_SYM)
			{
				FklByteCode* tmp=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
				char* sym=objCptr->u.atom->value.str;
				FklSymTabNode* node=fklAddSymbolToGlob(sym);
				tmp->code[0]=FKL_PUSH_SYM;
				fklSetSidToByteCode(tmp->code+sizeof(char),node->id);
				return tmp;
			}
			else if(objCptr->u.atom->type!=FKL_VECTOR)
				return fklCompileAtom(objCptr);
			else
				return fklCompileVector(objCptr);
			break;
		case FKL_NIL:
			return fklCompileNil();
			break;
		default:
			break;
	}
	return NULL;
}

FklByteCodelnt* fklCompileUnquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	return fklCompile(objCptr,curEnv,inter,state);
}

FklByteCodelnt* fklCompileConst(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	int32_t line=objCptr->curline;
	FklByteCode* tmp=NULL;
	if(objCptr->type==FKL_ATM&&objCptr->u.atom->type!=FKL_VECTOR)tmp=fklCompileAtom(objCptr);
	if(objCptr->type==FKL_ATM&&objCptr->u.atom->type==FKL_VECTOR)tmp=fklCompileVector(objCptr);
	if(fklIsNil(objCptr))tmp=fklCompileNil();
	if(fklIsQuoteExpression(objCptr))tmp=fklCompileQuote(objCptr);
	if(!tmp)
	{
		state->state=FKL_INVALIDEXPR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCodelnt* t=fklNewByteCodelnt(tmp);
	FklLineNumTabNode* n=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->size,line);
	t->ls=1;
	t->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
	FKL_ASSERT(t->l,__func__);
	t->l[0]=n;
	return t;
}

FklByteCodelnt* fklCompileFuncCall(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* headoflist=NULL;
	FklAstPair* tmpPair=objCptr->u.pair;
	int32_t line=objCptr->curline;
	FklByteCodelnt* tmp1=NULL;
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklByteCode* setBp=fklNewByteCode(1);
	FklByteCode* invoke=fklNewByteCode(1);
	setBp->code[0]=FKL_SET_BP;
	invoke->code[0]=FKL_INVOKE;
	for(;;)
	{
		if(objCptr==NULL)
		{
			fklCodeCat(tmp->bc,invoke);
			if(!tmp->l)
			{
				tmp->ls=1;
				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
				FKL_ASSERT(tmp->l,__func__);
				tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,invoke->size,line);
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
				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
				FKL_ASSERT(tmp->l,__func__);
				tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,setBp->size,line);
			}
			else
				tmp->l[tmp->ls-1]->cpc+=setBp->size;
			headoflist=&objCptr->u.pair->car;
			for(objCptr=headoflist;fklNextCptr(objCptr)!=NULL;objCptr=fklNextCptr(objCptr));
		}
		else
		{
			tmp1=fklCompile(objCptr,curEnv,inter,state);
			if(state->state!=0)
			{
				fklFreeByteCodeAndLnt(tmp);
				tmp=NULL;
				break;
			}
			fklCodeLntCat(tmp,tmp1);
			fklFreeByteCodelnt(tmp1);
			objCptr=fklPrevCptr(objCptr);
		}
	}
	fklFreeByteCode(setBp);
	fklFreeByteCode(invoke);
	return tmp;
}

FklByteCodelnt* fklCompileDef(FklAstCptr* tir,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstPair* tmpPair=tir->u.pair;
	FklAstCptr* fir=NULL;
	FklAstCptr* sec=NULL;
	FklAstCptr* objCptr=NULL;
	FklByteCodelnt* tmp1=NULL;
	FklByteCode* pushTop=fklNewByteCode(sizeof(char));
	FklByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
	popVar->code[0]=FKL_POP_VAR;
	pushTop->code[0]=FKL_PUSH_TOP;
	while(fklIsDefExpression(tir))
	{
		fir=&tir->u.pair->car;
		sec=fklNextCptr(fir);
		tir=fklNextCptr(sec);
	}
	objCptr=tir;
	tmp1=fklCompile(objCptr,curEnv,inter,state);
	if(state->state!=0)
	{
		fklFreeByteCode(popVar);
		fklFreeByteCode(pushTop);
		return NULL;
	}
	for(;;)
	{
		FklCompDef* tmpDef=NULL;
		FklAstAtom* tmpAtm=sec->u.atom;
		if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
		{
			if(curEnv->prev->prefix)
			{
				size_t len=strlen(tmpAtm->value.str)+strlen(curEnv->prev->prefix)+1;
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
				FKL_ASSERT(symbolWithPrefix,__func__);
				sprintf(symbolWithPrefix,"%s%s",curEnv->prev->prefix,tmpAtm->value.str);
				tmpDef=fklAddCompDef(symbolWithPrefix,curEnv->prev);
				free(symbolWithPrefix);
			}
			else
				tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv->prev);
			fklSetI32ToByteCode(popVar->code+sizeof(char),1);
		}
		else
		{
			fklSetI32ToByteCode(popVar->code+sizeof(char),0);
			tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv);
		}
		fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
		FklByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
		fklCodeCat(tmp1->bc,pushTop);
		fklCodeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
		if(fklIsLambdaExpression(objCptr)||fklIsConst(objCptr))
		{
			if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
			{
				FklByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
				popVar->code[0]=FKL_POP_VAR;
				fklSetI32ToByteCode(popVar->code+sizeof(char),0);
				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
				fklCodelntCopyCat(curEnv->prev->proc,tmp1Copy);
				fklFreeByteCodeAndLnt(tmp1Copy);
				tmp1Copy=fklCopyByteCodelnt(tmp1);
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklSetI32ToByteCode(popVar->code+sizeof(char),0);
				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklAddCompDef(tmpAtm->value.str,curEnv)->id);
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
		fklFreeByteCodeAndLnt(tmp1Copy);
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

FklByteCodelnt* fklCompileSetq(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstPair* tmpPair=objCptr->u.pair;
	FklAstCptr* fir=&objCptr->u.pair->car;
	FklAstCptr* sec=fklNextCptr(fir);
	FklAstCptr* tir=fklNextCptr(sec);
	FklByteCodelnt* tmp1=NULL;
	FklByteCode* pushTop=fklNewByteCode(sizeof(char));
	FklByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
	popVar->code[0]=FKL_POP_VAR;
	pushTop->code[0]=FKL_PUSH_TOP;
	while(fklIsSetqExpression(tir))
	{
		fir=&tir->u.pair->car;
		sec=fklNextCptr(fir);
		tir=fklNextCptr(sec);
	}
	objCptr=tir;
	tmp1=fklCompile(objCptr,curEnv,inter,state);
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
		FklAstAtom* tmpAtm=sec->u.atom;
		FklCompDef* tmpDef=NULL;
		FklCompEnv* tmpEnv=curEnv;
		while(tmpEnv!=NULL)
		{
			tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
			if(tmpEnv->prefix&&!tmpDef)
			{
				size_t len=strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1;
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
				FKL_ASSERT(symbolWithPrefix,__func__);
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
			FklSymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
			scope=-1;
			id=node->id;
		}
		else
			id=tmpDef->id;
		fklSetI32ToByteCode(popVar->code+sizeof(char),scope);
		fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),id);
		FklByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
		fklCodeCat(tmp1->bc,pushTop);
		fklCodeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
		if(!scope&&tmpDef&&(fklIsConst(objCptr)||fklIsLambdaExpression(objCptr)))
		{
			fklCodelntCopyCat(curEnv->proc,tmp1);
			if(tmpEnv->prev&&tmpEnv->prev->exp&&tmpEnv->prev->prefix)
			{
				FklByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
				popVar->code[0]=FKL_POP_VAR;
				fklSetI32ToByteCode(popVar->code+sizeof(char),scope+1);
				char* symbolWithPrefix=fklCopyStr(tmpEnv->prev->prefix);
				symbolWithPrefix=fklStrCat(symbolWithPrefix,tmpAtm->value.str);
				*(FklSid_t*)(popVar->code+sizeof(char)+sizeof(int32_t))=fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id;
				free(symbolWithPrefix);
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(pushTop->size+popVar->size);
				fklFreeByteCode(popVar);
				fklCodelntCopyCat(tmpEnv->proc,tmp1Copy);
			}
		}
		fklFreeByteCodeAndLnt(tmp1Copy);
		if(scope!=-1&&tmpEnv->prev&&tmpEnv->prev->exp&&fklIsSymbolShouldBeExport(tmpAtm->value.str,tmpEnv->prev->exp,tmpEnv->prev->n))
		{
			fklSetI32ToByteCode(popVar->code+sizeof(char),scope+1);
			if(tmpEnv->prev->prefix)
			{
				char* symbolWithPrefix=fklCopyStr(tmpEnv->prev->prefix);
				symbolWithPrefix=fklStrCat(symbolWithPrefix,tmpAtm->value.str);
				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id);
				free(symbolWithPrefix);
			}
			else
				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklFindCompDef(tmpAtm->value.str,tmpEnv->prev)->id);
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

FklByteCodelnt* fklCompileSetf(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* fir=&objCptr->u.pair->car;
	FklAstCptr* sec=fklNextCptr(fir);
	if(fklIsSymbol(sec))
		return fklCompileSetq(objCptr,curEnv,inter,state);
	else
	{
		FklAstCptr* tir=fklNextCptr(sec);
		FklByteCodelnt* tmp1=fklCompile(sec,curEnv,inter,state);
		if(state->state!=0)
			return NULL;
		FklByteCodelnt* tmp2=fklCompile(tir,curEnv,inter,state);
		if(state->state!=0)
		{
			fklFreeByteCodeAndLnt(tmp1);
			return NULL;
		}
		FklByteCode* popRef=fklNewByteCode(sizeof(char));
		popRef->code[0]=FKL_POP_REF;
		fklCodeLntCat(tmp1,tmp2);
		fklCodeCat(tmp1->bc,popRef);
		tmp1->l[tmp1->ls-1]->cpc+=popRef->size;
		fklFreeByteCode(popRef);
		fklFreeByteCodelnt(tmp2);
		return tmp1;
	}
}

FklByteCodelnt* fklCompileSym(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	int32_t line=objCptr->curline;
	if(fklHasKeyWord(objCptr,curEnv))
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCode* pushVar=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
	pushVar->code[0]=FKL_PUSH_VAR;
	FklAstAtom* tmpAtm=objCptr->u.atom;
	FklCompDef* tmpDef=NULL;
	FklCompEnv* tmpEnv=curEnv;
	int32_t id=0;
	while(tmpEnv!=NULL&&tmpDef==NULL)
	{
		tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
		if(tmpEnv->prefix&&!tmpDef)
		{
			size_t len=strlen(tmpEnv->prefix)+strlen(tmpAtm->value.str)+1;
			char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
			FKL_ASSERT(symbolWithPrefix,__func__);
			sprintf(symbolWithPrefix,"%s%s",tmpEnv->prefix,tmpAtm->value.str);
			tmpDef=fklFindCompDef(symbolWithPrefix,tmpEnv);
			free(symbolWithPrefix);
		}
		tmpEnv=tmpEnv->prev;
	}
	if(tmpDef==NULL)
	{
		FklSymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
		id=node->id;
	}
	else
		id=tmpDef->id;
	fklSetSidToByteCode(pushVar->code+sizeof(char),id);
	FklByteCodelnt* bcl=fklNewByteCodelnt(pushVar);
	bcl->ls=1;
	bcl->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(bcl->l,__func__);
	bcl->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,pushVar->size,line);
	return bcl;
}

FklByteCodelnt* fklCompileAnd(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	int32_t curline=objCptr->curline;
	FklByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int64_t));
	FklByteCode* push1=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCode* pushREnv=fklNewByteCode(sizeof(char));
	FklByteCode* popREnv=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack=fklNewPtrStack(32,16);
	jumpiffalse->code[0]=FKL_JMP_IF_FALSE;
	push1->code[0]=FKL_PUSH_I32;
	fklSetI32ToByteCode(push1->code+sizeof(char),1);
	resTp->code[0]=FKL_RES_TP;
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	pushREnv->code[0]=FKL_PUSH_R_ENV;
	popREnv->code[0]=FKL_POP_R_ENV;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	FklCompEnv* andExprEnv=fklNewCompEnv(curEnv);
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,andExprEnv,inter,state);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(jumpiffalse);
			fklFreeByteCode(push1);
			fklFreeByteCode(pushREnv);
			fklFreeByteCode(popREnv);
			fklFreePtrStack(stack);
			fklDestroyCompEnv(andExprEnv);
			return NULL;
		}
		fklPushPtrStack(tmp1,stack);
	}
	fklDestroyCompEnv(andExprEnv);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
		fklSetI64ToByteCode(jumpiffalse->code+sizeof(char),tmp->bc->size);
		fklCodeCat(tmp1->bc,jumpiffalse);
		tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		fklReCodeLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);
	}
	if(tmp->bc->size)
	{
		fklReCodeCat(pushREnv,tmp->bc);
		fklCodeCat(tmp->bc,popREnv);
		if(!tmp->l)
		{
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
			FKL_ASSERT(tmp->l,__func__);
			tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
		}
		else
		{
			tmp->l[0]->cpc+=pushREnv->size;
			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushREnv->size);
			tmp->l[tmp->ls-1]->cpc+=popREnv->size;
		}
	}
	fklReCodeCat(push1,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,__func__);
		tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
	}
	else
	{
		tmp->l[0]->cpc+=push1->size;
		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,push1->size);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	fklFreeByteCode(jumpiffalse);
	fklFreeByteCode(push1);
	fklFreeByteCode(pushREnv);
	fklFreeByteCode(popREnv);
	fklFreePtrStack(stack);
	return tmp;
}

FklByteCodelnt* fklCompileOr(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	int32_t curline=objCptr->curline;
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* jumpifture=fklNewByteCode(sizeof(char)+sizeof(int64_t));
	FklByteCode* pushnil=fklNewByteCode(sizeof(char));
	FklByteCode* pushREnv=fklNewByteCode(sizeof(char));
	FklByteCode* popREnv=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack=fklNewPtrStack(32,16);
	pushnil->code[0]=FKL_PUSH_NIL;
	jumpifture->code[0]=FKL_JMP_IF_TRUE;
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	resTp->code[0]=FKL_RES_TP;
	pushREnv->code[0]=FKL_PUSH_R_ENV;
	popREnv->code[0]=FKL_POP_R_ENV;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	FklCompEnv* orExprEnv=fklNewCompEnv(curEnv);
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,orExprEnv,inter,state);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(jumpifture);
			fklFreeByteCode(pushnil);
			fklFreeByteCode(pushREnv);
			fklFreeByteCode(popREnv);
			fklFreePtrStack(stack);
			fklDestroyCompEnv(orExprEnv);
			return NULL;
		}
		fklPushPtrStack(tmp1,stack);
	}
	fklDestroyCompEnv(orExprEnv);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
		fklSetI64ToByteCode(jumpifture->code+sizeof(char),tmp->bc->size);
		fklCodeCat(tmp1->bc,jumpifture);
		tmp1->l[tmp1->ls-1]->cpc+=jumpifture->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		fklReCodeLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);
	}
	if(tmp->bc->size)
	{
		fklReCodeCat(pushREnv,tmp->bc);
		fklCodeCat(tmp->bc,popREnv);
		if(!tmp->l)
		{
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
			FKL_ASSERT(tmp->l,__func__);
			tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
		}
		else
		{
			tmp->l[0]->cpc+=pushREnv->size;
			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushREnv->size);
			tmp->l[tmp->ls-1]->cpc+=popREnv->size;
		}
	}
	fklReCodeCat(pushnil,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,__func__);
		tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
	}
	else
	{
		tmp->l[0]->cpc+=pushnil->size;
		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushnil->size);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	FKL_INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	fklFreeByteCode(jumpifture);
	fklFreeByteCode(pushnil);
	fklFreeByteCode(pushREnv);
	fklFreeByteCode(popREnv);
	fklFreePtrStack(stack);
	return tmp;
}

FklByteCodelnt* fklCompileBegin(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* firCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklByteCode* resTp=fklNewByteCode(1);
	FklByteCode* setTp=fklNewByteCode(1);
	FklByteCode* popTp=fklNewByteCode(1);
	resTp->code[0]=FKL_RES_TP;
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	while(firCptr)
	{
		FklByteCodelnt* tmp1=fklCompile(firCptr,curEnv,inter,state);
		if(state->state!=0)
		{
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(resTp);
			fklFreeByteCode(setTp);
			fklFreeByteCode(popTp);
			return NULL;
		}
		if(tmp->bc->size)
		{
			fklReCodeCat(resTp,tmp1->bc);
			tmp1->l[0]->cpc+=1;
			FKL_INCREASE_ALL_SCP(tmp1->l,tmp1->ls-1,resTp->size);
		}
		fklCodeLntCat(tmp,tmp1);
		fklFreeByteCodelnt(tmp1);
		firCptr=fklNextCptr(firCptr);
	}
	fklReCodeCat(setTp,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
		FKL_ASSERT(tmp->l,__func__);
		tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,objCptr->curline);
	}
	else
	{
		tmp->l[0]->cpc+=1;
		FKL_INCREASE_ALL_SCP(tmp->l,tmp->ls-1,setTp->size);
	}
	fklCodeCat(tmp->bc,popTp);
	tmp->l[tmp->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(setTp);
	fklFreeByteCode(resTp);
	fklFreeByteCode(popTp);
	return tmp;
}

FklByteCodelnt* fklCompileLambda(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	int32_t line=objCptr->curline;
	FklAstCptr* tmpCptr=objCptr;
	FklAstPair* objPair=NULL;
	FklCompEnv* tmpEnv=fklNewCompEnv(curEnv);
	FklByteCode* popArg=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
	FklByteCode* popRestArg=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
	FklByteCode* pArg=fklNewByteCode(0);
	popArg->code[0]=FKL_POP_ARG;
	popRestArg->code[0]=FKL_POP_REST_ARG;
	objPair=objCptr->u.pair;
	objCptr=&objPair->car;
	if(fklNextCptr(objCptr)->type==FKL_PAIR)
	{
		FklAstCptr* argCptr=&fklNextCptr(objCptr)->u.pair->car;
		while(argCptr!=NULL&&argCptr->type!=FKL_NIL)
		{
			FklAstAtom* tmpAtm=(argCptr->type==FKL_ATM)?argCptr->u.atom:NULL;
			if(argCptr->type!=FKL_ATM||tmpAtm==NULL||tmpAtm->type!=FKL_SYM)
			{
				state->state=FKL_SYNTAXERROR;
				state->place=tmpCptr;
				fklFreeByteCode(popArg);
				fklFreeByteCode(popRestArg);
				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
				return NULL;
			}
			FklCompDef* tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
			fklSetSidToByteCode(popArg->code+sizeof(char),tmpDef->id);
			fklCodeCat(pArg,popArg);
			if(fklNextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==FKL_ATM)
			{
				FklAstAtom* tmpAtom1=(argCptr->outer->cdr.type==FKL_ATM)?argCptr->outer->cdr.u.atom:NULL;
				if(tmpAtom1!=NULL&&tmpAtom1->type!=FKL_SYM)
				{
					state->state=FKL_SYNTAXERROR;
					state->place=tmpCptr;
					fklFreeByteCode(popArg);
					fklFreeByteCode(popRestArg);
					fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
					return NULL;
				}
				tmpDef=fklAddCompDef(tmpAtom1->value.str,tmpEnv);
				fklSetSidToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
				fklCodeCat(pArg,popRestArg);
			}
			argCptr=fklNextCptr(argCptr);
		}
	}
	else if(fklNextCptr(objCptr)->type==FKL_ATM)
	{
		FklAstAtom* tmpAtm=fklNextCptr(objCptr)->u.atom;
		if(tmpAtm->type!=FKL_SYM)
		{
			state->state=FKL_SYNTAXERROR;
			state->place=tmpCptr;
			fklFreeByteCode(popArg);
			fklFreeByteCode(popRestArg);
			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
			return NULL;
		}
		FklCompDef* tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
		fklSetSidToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
		fklCodeCat(pArg,popRestArg);
	}
	fklFreeByteCode(popRestArg);
	fklFreeByteCode(popArg);
	FklByteCode* fklResBp=fklNewByteCode(sizeof(char));
	fklResBp->code[0]=FKL_RES_BP;
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	setTp->code[0]=FKL_SET_TP;
	fklCodeCat(pArg,fklResBp);
	fklCodeCat(pArg,setTp);
	fklFreeByteCode(fklResBp);
	fklFreeByteCode(setTp);
	objCptr=fklNextCptr(fklNextCptr(objCptr));
	FklByteCodelnt* codeOfLambda=fklNewByteCodelnt(pArg);
	codeOfLambda->ls=1;
	codeOfLambda->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(codeOfLambda->l,__func__);
	codeOfLambda->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,pArg->size,line);
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	resTp->code[0]=FKL_RES_TP;
	for(;objCptr;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,tmpEnv,inter,state);
		if(state->state!=0)
		{
			fklFreeByteCodeAndLnt(codeOfLambda);
			fklFreeByteCode(resTp);
			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
			return NULL;
		}
		if(fklNextCptr(objCptr)!=NULL)
		{
			fklCodeCat(tmp1->bc,resTp);
			tmp1->l[tmp1->ls-1]->cpc+=resTp->size;
		}
		fklCodeLntCat(codeOfLambda,tmp1);
		fklFreeByteCodelnt(tmp1);
	}
	fklFreeByteCode(resTp);
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	popTp->code[0]=FKL_POP_TP;
	fklCodeCat(codeOfLambda->bc,popTp);
	codeOfLambda->l[codeOfLambda->ls-1]->cpc+=popTp->size;
	fklFreeByteCode(popTp);
	FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(codeOfLambda->bc->size));
	pushProc->code[0]=FKL_PUSH_PROC;
	fklSetU64ToByteCode(pushProc->code+sizeof(char),codeOfLambda->bc->size);
	FklByteCodelnt* toReturn=fklNewByteCodelnt(pushProc);
	toReturn->ls=1;
	toReturn->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(toReturn->l,__func__);
	toReturn->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,pushProc->size,line);
	fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
	fklCodeLntCat(toReturn,codeOfLambda);
	fklFreeByteCodelnt(codeOfLambda);
	return toReturn;
}

FklByteCodelnt* fklCompileCond(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* cond=NULL;
	FklByteCode* pushnil=fklNewByteCode(sizeof(char));
	FklByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int64_t));
	FklByteCode* jump=fklNewByteCode(sizeof(char)+sizeof(int64_t));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCode* pushREnv=fklNewByteCode(sizeof(char));
	FklByteCode* popREnv=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack1=fklNewPtrStack(32,16);
	FklMemMenager* memMenager=fklNewMemMenager(32);
	fklPushMem(pushnil,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(jumpiffalse,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(jump,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(resTp,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(setTp,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(popTp,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(pushREnv,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(popREnv,(FklGenDestructor)fklFreeByteCode,memMenager);
	fklPushMem(stack1,(FklGenDestructor)fklFreePtrStack,memMenager);
	setTp->code[0]=FKL_SET_TP;
	resTp->code[0]=FKL_RES_TP;
	popTp->code[0]=FKL_POP_TP;
	pushREnv->code[0]=FKL_PUSH_R_ENV;
	popREnv->code[0]=FKL_POP_R_ENV;
	pushnil->code[0]=FKL_PUSH_NIL;
	jumpiffalse->code[0]=FKL_JMP_IF_FALSE;
	jump->code[0]=FKL_JMP;
	for(cond=fklNextCptr(fklGetFirstCptr(objCptr));cond!=NULL;cond=fklNextCptr(cond))
	{
		FklCompEnv* conditionEnv=fklNewCompEnv(curEnv);
		objCptr=fklGetFirstCptr(cond);
		FklByteCodelnt* conditionCode=fklCompile(objCptr,conditionEnv,inter,state);
		FklPtrStack* stack2=fklNewPtrStack(32,16);
		fklReCodeCat(pushREnv,conditionCode->bc);
		conditionCode->l[conditionCode->ls-1]->cpc+=pushREnv->size;
		for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
		{
			FklByteCodelnt* tmp1=fklCompile(objCptr,conditionEnv,inter,state);
			if(state->state!=0)
			{
				fklFreeByteCodeAndLnt(tmp);
				fklFreeMemMenager(memMenager);
				fklFreePtrStack(stack2);
				if(conditionCode)
				{
					fklFreeByteCodeAndLnt(conditionCode);
				}
				return NULL;
			}
			fklPushPtrStack(tmp1,stack2);
		}
		FklByteCodelnt* tmpCond=fklNewByteCodelnt(fklNewByteCode(0));
		while(!fklIsPtrStackEmpty(stack2))
		{
			FklByteCodelnt* tmp1=fklPopPtrStack(stack2);
			fklReCodeCat(resTp,tmp1->bc);
			tmp1->l[0]->cpc+=resTp->size;
			FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
			fklReCodeLntCat(tmp1,tmpCond);
			fklFreeByteCodelnt(tmp1);
		}
		fklFreePtrStack(stack2);
		if(tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size))
		{
			fklSetI64ToByteCode(jumpiffalse->code+sizeof(char),tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size));
			fklCodeCat(conditionCode->bc,jumpiffalse);
			conditionCode->l[conditionCode->ls-1]->cpc+=jumpiffalse->size;
		}
		fklReCodeLntCat(conditionCode,tmpCond);
		fklFreeByteCodelnt(conditionCode);
		fklPushPtrStack(tmpCond,stack1);
		fklDestroyCompEnv(conditionEnv);
	}
	uint32_t top=stack1->top-1;
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklByteCodelnt* tmpCond=fklPopPtrStack(stack1);
		if(!fklIsPtrStackEmpty(stack1))
		{
			fklReCodeCat(resTp,tmpCond->bc);
			fklReCodeCat(popREnv,tmpCond->bc);
			tmpCond->l[0]->cpc+=resTp->size+popREnv->size;
			FKL_INCREASE_ALL_SCP(tmpCond->l+1,tmpCond->ls-1,resTp->size+popREnv->size);
		}
		if(top!=stack1->top)
		{
			fklSetI64ToByteCode(jump->code+sizeof(char),tmp->bc->size);
			fklCodeCat(tmpCond->bc,jump);
			tmpCond->l[tmpCond->ls-1]->cpc+=jump->size;
		}
		fklReCodeLntCat(tmpCond,tmp);
		fklFreeByteCodelnt(tmpCond);
	}
	if(!tmp->l)
	{
		fklCodeCat(tmp->bc,pushnil);
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,__func__);
		tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,pushnil->size,objCptr->curline);
	}
	else
	{
		fklReCodeCat(setTp,tmp->bc);
		tmp->l[0]->cpc+=setTp->size;
		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
		fklCodeCat(tmp->bc,popREnv);
		tmp->l[tmp->ls-1]->cpc+=popREnv->size;
		fklCodeCat(tmp->bc,popTp);
		tmp->l[tmp->ls-1]->cpc+=popTp->size;
	}
	fklFreeMemMenager(memMenager);
	return tmp;
}

FklByteCodelnt* fklCompileLoad(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* fir=&objCptr->u.pair->car;
	FklAstCptr* pFileName=fklNextCptr(fir);
	FklAstAtom* name=pFileName->u.atom;
	if(fklHasLoadSameFile(name->value.str,inter))
	{
		state->state=FKL_CIRCULARLOAD;
		state->place=pFileName;
		return NULL;
	}
	FILE* file=fopen(name->value.str,"r");
	if(file==NULL)
	{
		state->state=FKL_FILEFAILURE;
		state->place=pFileName;
		return NULL;
	}
	fklAddSymbolToGlob(name->value.str);
	FklInterpreter* tmpIntpr=fklNewIntpr(name->value.str,file,curEnv,inter->lnt);
	tmpIntpr->prev=inter;
	tmpIntpr->glob=curEnv;
	FklByteCodelnt* tmp=fklCompileFile(tmpIntpr,NULL);
	chdir(tmpIntpr->prev->curDir);
	tmpIntpr->glob=NULL;
	tmpIntpr->lnt=NULL;
	fklFreeIntpr(tmpIntpr);
	//fklPrintByteCode(tmp,stderr);
	FklByteCode* setTp=fklNewByteCode(1);
	FklByteCode* popTp=fklNewByteCode(1);
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	if(tmp)
	{
		fklReCodeCat(setTp,tmp->bc);
		fklCodeCat(tmp->bc,popTp);
		tmp->l[0]->cpc+=setTp->size;
		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
		tmp->l[tmp->ls-1]->cpc+=popTp->size;

	}
	fklFreeByteCode(popTp);
	fklFreeByteCode(setTp);
	return tmp;
}

FklByteCodelnt* fklCompileFile(FklInterpreter* inter,int* exitstate)
{
	chdir(inter->curDir);
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	tmp->ls=1;
	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(tmp->l,__func__);
	tmp->l[0]=fklNewLineNumTabNodeWithFilename(inter->filename,0,0,1);
	FklByteCode* resTp=fklNewByteCode(1);
	resTp->code[0]=FKL_RES_TP;
	char* prev=NULL;
	size_t prevSize=0;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	for(;;)
	{
		FklAstCptr* begin=NULL;
		int unexpectEOF=0;
		size_t size=0;
		char* list=fklReadInStringPattern(inter->file,&prev,&size,&prevSize,inter->curline,&unexpectEOF,tokenStack,NULL);
		if(list==NULL&&!unexpectEOF&&size)continue;
		FklErrorState state={0,NULL};
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
			fklFreeByteCodeAndLnt(tmp);
			free(list);
			if(exitstate)
				*exitstate=FKL_UNEXPECTEOF;
			tmp=NULL;
			break;
		}
		begin=fklCreateAstWithTokens(tokenStack,inter->filename,inter->glob);
		inter->curline+=fklCountChar(list,'\n',size);
		//begin=fklCreateTree(list,inter,NULL);
		if(fklIsAllSpaceBufSize(list,size))
		{
			while(!fklIsPtrStackEmpty(tokenStack))
				fklFreeToken(fklPopPtrStack(tokenStack));
			free(list);
			break;
		}
		if(begin!=NULL)
		{
			FklByteCodelnt* tmpByteCodelnt=fklCompile(begin,inter->glob,inter,&state);
			if(!tmpByteCodelnt||state.state!=0)
			{
				if(state.state)
				{
					fklPrintCompileError(state.place,state.state,inter);
					if(exitstate)
						*exitstate=state.state;
					fklDeleteCptr(state.place);
				}
				if(tmpByteCodelnt==NULL&&!state.state&&exitstate)
					*exitstate=FKL_MACROEXPANDFAILED;
				fklFreeByteCodeAndLnt(tmp);
				free(list);
				fklDeleteCptr(begin);
				free(begin);
				tmp=NULL;
				while(!fklIsPtrStackEmpty(tokenStack))
					fklFreeToken(fklPopPtrStack(tokenStack));
				break;
			}
			if(tmp->bc->size)
			{
				fklReCodeCat(resTp,tmpByteCodelnt->bc);
				tmpByteCodelnt->l[0]->cpc+=resTp->size;
				FKL_INCREASE_ALL_SCP(tmpByteCodelnt->l+1,tmpByteCodelnt->ls-1,resTp->size);
			}
			fklCodeLntCat(tmp,tmpByteCodelnt);
			fklFreeByteCodelnt(tmpByteCodelnt);
			fklDeleteCptr(begin);
			free(begin);
		}
		else if(begin==NULL&&!fklIsAllComment(tokenStack))
		{
			while(!fklIsPtrStackEmpty(tokenStack))
				fklFreeToken(fklPopPtrStack(tokenStack));
			free(list);
			list=NULL;
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(resTp);
			fklFreePtrStack(tokenStack);
			return NULL;
		}
		while(!fklIsPtrStackEmpty(tokenStack))
			fklFreeToken(fklPopPtrStack(tokenStack));
		free(list);
		list=NULL;
	}
	if(prev)
		free(prev);
	fklFreeByteCode(resTp);
	fklFreePtrStack(tokenStack);
	return tmp;
}

FklByteCodelnt* fklCompileImport(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
#ifdef _WIN32
	char divstr[]="\\";
#else
	char divstr[]="/";
#endif

	FklMemMenager* memMenager=fklNewMemMenager(32);
	char postfix[]=".fkl";
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	fklPushMem(tmp,(FklGenDestructor)fklFreeByteCodeAndLnt,memMenager);
	chdir(inter->curDir);
	char* prev=NULL;
	size_t prevSize=0;
	/* 获取文件路径和文件名
	 * 然后打开并查找第一个library表达式
	 */
	FklAstCptr* plib=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;plib;plib=fklNextCptr(plib))
	{
		char* libPrefix=NULL;
		FklAstCptr* pairOfpPartsOfPath=plib;
		FklAstCptr* pPartsOfPath=fklGetFirstCptr(plib);
		if(!pPartsOfPath)
		{
			state->state=FKL_SYNTAXERROR;
			state->place=plib;
			fklFreeMemMenager(memMenager);
			return NULL;
		}
		uint32_t count=0;
		const char** partsOfPath=(const char**)malloc(sizeof(const char*)*0);
		FKL_ASSERT(partsOfPath,__func__);
		fklPushMem(partsOfPath,free,memMenager);
		while(pPartsOfPath)
		{
			if(pPartsOfPath->type!=FKL_ATM)
			{
				state->state=FKL_SYNTAXERROR;
				state->place=plib;
				fklFreeMemMenager(memMenager);
				return NULL;
			}
			FklAstAtom* tmpAtm=pPartsOfPath->u.atom;
			if(tmpAtm->type!=FKL_STR&&tmpAtm->type!=FKL_SYM)
			{
				state->state=FKL_SYNTAXERROR;
				state->place=plib;
				fklFreeMemMenager(memMenager);
				return NULL;
			}
			if(!strcmp(tmpAtm->value.str,"prefix")&&fklNextCptr(pPartsOfPath)->type==FKL_PAIR)
			{
				FklAstCptr* tmpCptr=fklNextCptr(pPartsOfPath);
				if(!tmpCptr||tmpCptr->type!=FKL_PAIR)
				{
					state->state=FKL_SYNTAXERROR;
					state->place=objCptr;
					fklFreeMemMenager(memMenager);
					return NULL;
				}
				tmpCptr=fklNextCptr(tmpCptr);
				if(!tmpCptr||fklNextCptr(tmpCptr)||tmpCptr->type!=FKL_ATM)
				{
					state->state=FKL_SYNTAXERROR;
					state->place=plib;
					fklFreeMemMenager(memMenager);
					return NULL;
				}
				FklAstAtom* prefixAtom=tmpCptr->u.atom;
				if(prefixAtom->type!=FKL_STR&&prefixAtom->type!=FKL_SYM)
				{
					state->state=FKL_SYNTAXERROR;
					state->place=plib;
					fklFreeMemMenager(memMenager);
					return NULL;
				}
				libPrefix=fklCopyStr(prefixAtom->value.str);
				pairOfpPartsOfPath=fklNextCptr(pPartsOfPath);
				pPartsOfPath=fklGetFirstCptr(pairOfpPartsOfPath);
				continue;
			}
			count++;
			partsOfPath=(const char**)fklReallocMem(partsOfPath,realloc(partsOfPath,sizeof(const char*)*count),memMenager);
			FKL_ASSERT(pPartsOfPath,__func__);
			partsOfPath[count-1]=tmpAtm->value.str;
			pPartsOfPath=fklNextCptr(pPartsOfPath);
		}
		uint32_t totalPathLength=0;
		uint32_t i=0;
		for(;i<count;i++)
			totalPathLength+=strlen(partsOfPath[i]);
		totalPathLength+=count+strlen(postfix);
		char* path=(char*)malloc(sizeof(char)*totalPathLength);
		FKL_ASSERT(path,__func__);
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
			state->state=FKL_IMPORTFAILED;
			state->place=pairOfpPartsOfPath;
			free(path);
			fklFreeMemMenager(memMenager);
			return NULL;
		}
		if(fklHasLoadSameFile(path,inter))
		{
			state->state=FKL_CIRCULARLOAD;
			state->place=objCptr;
			fklFreeMemMenager(memMenager);
			return NULL;
		}
		fklAddSymbolToGlob(path);
		FklInterpreter* tmpInter=fklNewIntpr(path,fp,fklNewCompEnv(NULL),inter->lnt);
		fklInitGlobKeyWord(tmpInter->glob);
		free(path);
		tmpInter->prev=inter;
		FklByteCode* resTp=fklNewByteCode(1);
		FklByteCode* setTp=fklNewByteCode(1);
		FklByteCode* popTp=fklNewByteCode(1);
		resTp->code[0]=FKL_RES_TP;
		setTp->code[0]=FKL_SET_TP;
		popTp->code[0]=FKL_POP_TP;
		fklPushMem(resTp,(FklGenDestructor)fklFreeByteCode,memMenager);
		fklPushMem(setTp,(FklGenDestructor)fklFreeByteCode,memMenager);
		fklPushMem(popTp,(FklGenDestructor)fklFreeByteCode,memMenager);
		FklByteCodelnt* libByteCodelnt=fklNewByteCodelnt(fklNewByteCode(0));
		fklPushMem(libByteCodelnt,(FklGenDestructor)fklFreeByteCodeAndLnt,memMenager);
		FklPtrStack* tokenStack=fklNewPtrStack(32,16);
		for(;;)
		{
			FklAstCptr* begin=NULL;
			int unexpectEOF=0;
			size_t size=0;
			char* list=fklReadInStringPattern(tmpInter->file,&prev,&size,&prevSize,tmpInter->curline,&unexpectEOF,tokenStack,NULL);
			if(list==NULL&&!unexpectEOF&&size)continue;
			if(unexpectEOF)
			{
				switch(unexpectEOF)
				{
					case 1:
						fprintf(stderr,"error of reader:Unexpect EOF at line %d of %s\n",tmpInter->curline,tmpInter->filename);
						break;
					case 2:
						fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",tmpInter->curline,tmpInter->filename);
						break;
				}
				free(list);
				break;
			}
			begin=fklCreateAstWithTokens(tokenStack,tmpInter->filename,tmpInter->glob);
			tmpInter->curline+=fklCountChar(list,'\n',size);
			if(fklIsAllSpaceBufSize(list,size))
			{
				while(!fklIsPtrStackEmpty(tokenStack))
					fklFreeToken(fklPopPtrStack(tokenStack));
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
					FklAstCptr* libName=fklNextCptr(fklGetFirstCptr(begin));
					FklAstCptr* objLibName=fklGetLastCptr(pairOfpPartsOfPath);
					if(fklCptrcmp(libName,objLibName))
					{
						FklCompEnv* tmpCurEnv=fklNewCompEnv(tmpInter->glob);
						FklAstCptr* exportCptr=fklNextCptr(libName);
						if(!exportCptr||!fklIsExportExpression(exportCptr))
						{
							fklPrintCompileError(begin,FKL_INVALIDEXPR,tmpInter);
							fklDeleteCptr(begin);
							free(begin);
							chdir(tmpInter->prev->curDir);
							tmpInter->lnt=NULL;
							fklFreeIntpr(tmpInter);
							if(libPrefix)
								free(libPrefix);
							fklFreeMemMenager(memMenager);
							free(list);
							while(!fklIsPtrStackEmpty(tokenStack))
								fklFreeToken(fklPopPtrStack(tokenStack));
							fklFreePtrStack(tokenStack);
							return NULL;
						}
						else
						{
							const char** exportSymbols=(const char**)malloc(sizeof(const char*)*0);
							fklPushMem(exportSymbols,free,memMenager);
							FKL_ASSERT(exportSymbols,__func__);
							FklAstCptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
							uint32_t num=0;
							for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
							{
								if(pExportSymbols->type!=FKL_ATM
										||pExportSymbols->u.atom->type!=FKL_SYM)
								{
									fklPrintCompileError(exportCptr,FKL_SYNTAXERROR,tmpInter);
									fklDeleteCptr(begin);
									free(begin);
									chdir(tmpInter->prev->curDir);
									tmpInter->lnt=NULL;
									fklFreeIntpr(tmpInter);
									if(libPrefix)
										free(libPrefix);
									fklFreeMemMenager(memMenager);
									free(list);
									while(!fklIsPtrStackEmpty(tokenStack))
										fklFreeToken(fklPopPtrStack(tokenStack));
									fklFreePtrStack(tokenStack);
									return NULL;
								}
								FklAstAtom* pSymbol=pExportSymbols->u.atom;
								num++;
								exportSymbols=(const char**)fklReallocMem(exportSymbols,realloc(exportSymbols,sizeof(const char*)*num),memMenager);
								FKL_ASSERT(exportSymbols,__func__);
								exportSymbols[num-1]=pSymbol->value.str;
							}
							mergeSort(exportSymbols,num,sizeof(const char*),cmpString);
							FklAstCptr* pBody=fklNextCptr(fklNextCptr(libName));
							fklInitCompEnv(tmpInter->glob);
							tmpInter->glob->prefix=libPrefix;
							tmpInter->glob->exp=exportSymbols;
							tmpInter->glob->n=num;
							for(;pBody;pBody=fklNextCptr(pBody))
							{
								FklByteCodelnt* otherByteCodelnt=fklCompile(pBody,tmpCurEnv,tmpInter,state);
								if(state->state||!otherByteCodelnt)
								{
									fklPrintCompileError(state->place,state->state,tmpInter);
									fklDeleteCptr(begin);
									free(begin);
									chdir(tmpInter->prev->curDir);
									tmpInter->lnt=NULL;
									fklFreeIntpr(tmpInter);
									state->state=0;
									state->place=NULL;
									if(libPrefix)
										free(libPrefix);
									fklFreeMemMenager(memMenager);
									free(list);
									while(!fklIsPtrStackEmpty(tokenStack))
										fklFreeToken(fklPopPtrStack(tokenStack));
									fklFreePtrStack(tokenStack);
									return NULL;
								}
								if(libByteCodelnt->bc->size&&otherByteCodelnt)
								{
									fklReCodeCat(resTp,otherByteCodelnt->bc);
									otherByteCodelnt->l[0]->cpc+=1;
									FKL_INCREASE_ALL_SCP(otherByteCodelnt->l,otherByteCodelnt->ls-1,resTp->size);
								}
								if(otherByteCodelnt)
								{
									fklCodeLntCat(libByteCodelnt,otherByteCodelnt);
									fklFreeByteCodelnt(otherByteCodelnt);
								}
							}
							fklReCodeCat(setTp,libByteCodelnt->bc);
							if(!libByteCodelnt->l)
							{
								libByteCodelnt->ls=1;
								libByteCodelnt->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
								FKL_ASSERT(libByteCodelnt->l,__func__);
								libByteCodelnt->l[0]=fklNewLineNumTabNodeWithFilename(tmpInter->filename,0,libByteCodelnt->bc->size,objCptr->curline);
							}
							else
							{
								libByteCodelnt->l[0]->cpc+=1;
								FKL_INCREASE_ALL_SCP(libByteCodelnt->l,libByteCodelnt->ls-1,setTp->size);
							}
							fklCodeCat(libByteCodelnt->bc,popTp);
							libByteCodelnt->l[libByteCodelnt->ls-1]->cpc+=popTp->size;
							fklCodeLntCat(tmp,libByteCodelnt);
							fklDeleteMem(libByteCodelnt,memMenager);
							fklFreeByteCodelnt(libByteCodelnt);
							libByteCodelnt=NULL;
							FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(tmp->bc->size));
							pushProc->code[0]=FKL_PUSH_PROC;
							fklSetU64ToByteCode(pushProc->code+sizeof(char),tmp->bc->size);
							fklReCodeCat(pushProc,tmp->bc);
							FKL_INCREASE_ALL_SCP(tmp->l,tmp->ls-1,pushProc->size);
							fklFreeByteCode(pushProc);
							FklByteCode* invoke=fklNewByteCode(sizeof(char));
							invoke->code[0]=FKL_INVOKE;
							fklCodeCat(tmp->bc,invoke);
							tmp->l[tmp->ls-1]->cpc+=invoke->size;
							fklFreeByteCode(invoke);
						}
						FklAstCptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
						for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
						{
							if(pExportSymbols->type!=FKL_ATM
									||pExportSymbols->u.atom->type!=FKL_SYM)
							{
								fklPrintCompileError(exportCptr,FKL_SYNTAXERROR,tmpInter);
								fklDeleteCptr(begin);
								free(begin);
								fklFreeByteCodelnt(tmp);
								chdir(tmpInter->prev->curDir);
								tmpInter->lnt=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								fklFreeMemMenager(memMenager);
								free(list);
								while(!fklIsPtrStackEmpty(tokenStack))
									fklFreeToken(fklPopPtrStack(tokenStack));
								fklFreePtrStack(tokenStack);
								return NULL;
							}
							FklAstAtom* pSymbol=pExportSymbols->u.atom;
							FklCompDef* tmpDef=NULL;
							char* symbolWouldExport=NULL;
							if(libPrefix)
							{
								size_t len=strlen(libPrefix)+strlen(pSymbol->value.str)+1;
								symbolWouldExport=(char*)malloc(sizeof(char)*len);
								FKL_ASSERT(symbolWouldExport,__func__);
								sprintf(symbolWouldExport,"%s%s",libPrefix,pSymbol->value.str);
							}
							else
							{
								symbolWouldExport=(char*)malloc(sizeof(char)*(strlen(pSymbol->value.str)+1));
								FKL_ASSERT(symbolWouldExport,__func__);
								strcpy(symbolWouldExport,pSymbol->value.str);
							}
							tmpDef=fklFindCompDef(symbolWouldExport,tmpInter->glob);
							if(!tmpDef)
							{
								fklPrintCompileError(pExportSymbols,FKL_SYMUNDEFINE,tmpInter);
								fklDeleteCptr(begin);
								free(begin);
								chdir(tmpInter->prev->curDir);
								tmpInter->lnt=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								free(symbolWouldExport);
								fklFreeMemMenager(memMenager);
								free(list);
								while(!fklIsPtrStackEmpty(tokenStack))
									fklFreeToken(fklPopPtrStack(tokenStack));
								fklFreePtrStack(tokenStack);
								return NULL;
							}
							else
								fklAddCompDef(symbolWouldExport,curEnv);
							free(symbolWouldExport);
						}
						fklCodelntCopyCat(curEnv->proc,tmp);
						FklPreMacro* headOfMacroOfImportedFile=tmpCurEnv->macro;
						tmpCurEnv->macro=NULL;
						while(headOfMacroOfImportedFile)
						{
							fklAddDefinedMacro(headOfMacroOfImportedFile,curEnv);
							FklPreMacro* prev=headOfMacroOfImportedFile;
							headOfMacroOfImportedFile=headOfMacroOfImportedFile->next;
							free(prev);
						}
						fklDestroyCompEnv(tmpCurEnv);
						tmpInter->glob->macro=NULL;
						fklDeleteCptr(begin);
						free(begin);
						free(list);
						while(!fklIsPtrStackEmpty(tokenStack))
							fklFreeToken(fklPopPtrStack(tokenStack));
						break;
					}
				}
				fklDeleteCptr(begin);
				free(begin);
			}
			else if(begin==NULL&&!fklIsAllComment(tokenStack))
			{
				while(!fklIsPtrStackEmpty(tokenStack))
					fklFreeToken(fklPopPtrStack(tokenStack));
				if(libPrefix)
					free(libPrefix);
				fklFreeMemMenager(memMenager);
				while(!fklIsPtrStackEmpty(tokenStack))
					fklFreeToken(fklPopPtrStack(tokenStack));
				fklFreePtrStack(tokenStack);
				return NULL;
			}
			while(!fklIsPtrStackEmpty(tokenStack))
				fklFreeToken(fklPopPtrStack(tokenStack));
			free(list);
			list=NULL;
		}
		if(libByteCodelnt&&!libByteCodelnt->bc->size)
		{
			state->state=(tmp)?FKL_LIBUNDEFINED:0;
			state->place=(tmp)?pairOfpPartsOfPath:NULL;
			tmpInter->lnt=NULL;
			fklFreeIntpr(tmpInter);
			if(libPrefix)
				free(libPrefix);
			fklFreeMemMenager(memMenager);
			fklFreePtrStack(tokenStack);
			return NULL;
		}
		chdir(tmpInter->prev->curDir);
		tmpInter->lnt=NULL;
		fklFreeIntpr(tmpInter);
		fklFreePtrStack(tokenStack);
		if(libPrefix)
			free(libPrefix);
	}
	if(tmp)
	{
		fklDeleteMem(tmp,memMenager);
		fklFreeMemMenager(memMenager);
	}
	chdir(inter->curDir);
	return tmp;
}

FklByteCodelnt* fklCompileTry(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
{
	FklAstCptr* pExpression=fklNextCptr(fklGetFirstCptr(objCptr));
	FklAstCptr* pCatchExpression=NULL;
	if(!pExpression||!(pCatchExpression=fklNextCptr(pExpression)))
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCodelnt* expressionByteCodelnt=fklCompile(pExpression,curEnv,inter,state);
	if(state->state)
		return NULL;
	else
	{
		FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(expressionByteCodelnt->bc->size));
		FklByteCode* invoke=fklNewByteCode(1);
		invoke->code[0]=FKL_INVOKE;
		pushProc->code[0]=FKL_PUSH_PROC;
		fklSetU64ToByteCode(pushProc->code+sizeof(char),expressionByteCodelnt->bc->size);
		fklReCodeCat(pushProc,expressionByteCodelnt->bc);
		expressionByteCodelnt->l[0]->cpc+=pushProc->size;
		FKL_INCREASE_ALL_SCP(expressionByteCodelnt->l+1,expressionByteCodelnt->ls-1,pushProc->size);
		fklCodeCat(expressionByteCodelnt->bc,invoke);
		expressionByteCodelnt->l[expressionByteCodelnt->ls-1]->cpc+=invoke->size;
		fklFreeByteCode(pushProc);
		fklFreeByteCode(invoke);
	}
	FklAstCptr* pErrSymbol=fklNextCptr(fklGetFirstCptr(pCatchExpression));
	FklAstCptr* pHandlerExpression=NULL;
	if(!pErrSymbol
			||pErrSymbol->type!=FKL_ATM
			||pErrSymbol->u.atom->type!=FKL_SYM
			||!(pHandlerExpression=fklNextCptr(pErrSymbol)))
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		fklFreeByteCodeAndLnt(expressionByteCodelnt);
		return NULL;
	}
	char* errSymbol=pErrSymbol->u.atom->value.str;
	FklSid_t sid=fklAddSymbolToGlob(errSymbol)->id;
	FklPtrStack* handlerByteCodelntStack=fklNewPtrStack(32,16);
	for(;pHandlerExpression;pHandlerExpression=fklNextCptr(pHandlerExpression))
	{
		if(pHandlerExpression->type!=FKL_PAIR
				||fklGetFirstCptr(pHandlerExpression)->type!=FKL_ATM
				||fklGetFirstCptr(pHandlerExpression)->u.atom->type!=FKL_SYM)
		{
			state->state=FKL_SYNTAXERROR;
			state->place=objCptr;
			fklFreeByteCodeAndLnt(expressionByteCodelnt);
			return NULL;
		}
		FklAstCptr* pErrorType=fklGetFirstCptr(pHandlerExpression);
		FklAstCptr* begin=fklNextCptr(pErrorType);
		if(!begin||pErrorType->type!=FKL_ATM||pErrorType->u.atom->type!=FKL_SYM)
		{
			state->state=FKL_SYNTAXERROR;
			state->place=objCptr;
			fklFreeByteCodeAndLnt(expressionByteCodelnt);
			fklFreePtrStack(handlerByteCodelntStack);
			return NULL;
		}
		FklCompEnv* tmpEnv=fklNewCompEnv(curEnv);
		fklAddCompDef(errSymbol,tmpEnv);
		FklByteCodelnt* t=NULL;
		for(;begin;begin=fklNextCptr(begin))
		{
			FklByteCodelnt* tmp1=fklCompile(begin,tmpEnv,inter,state);
			if(state->state)
			{
				fklFreeByteCodeAndLnt(expressionByteCodelnt);
				fklFreePtrStack(handlerByteCodelntStack);
				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
				return NULL;
			}
			if(!t)
				t=tmp1;
			else
			{
				FklByteCode* resTp=fklNewByteCode(1);
				resTp->code[0]=FKL_RES_TP;
				fklReCodeCat(resTp,tmp1->bc);
				tmp1->l[0]->cpc+=1;
				FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
				fklFreeByteCode(resTp);
			}
		}
		fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
		FklByteCode* setTp=fklNewByteCode(1);
		FklByteCode* popTp=fklNewByteCode(1);
		setTp->code[0]=FKL_SET_TP;
		popTp->code[0]=FKL_POP_TP;
		fklReCodeCat(setTp,t->bc);
		t->l[0]->cpc+=1;
		FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,setTp->size);
		fklCodeCat(t->bc,popTp);
		t->l[t->ls-1]->cpc+=popTp->size;
		fklFreeByteCode(setTp);
		fklFreeByteCode(popTp);
		char* errorType=pErrorType->u.atom->value.str;
		FklSid_t typeid=fklAddSymbolToGlob(errorType)->id;
		FklByteCode* errorTypeByteCode=fklNewByteCode(sizeof(FklSid_t)+sizeof(t->bc->size));
		fklSetSidToByteCode(errorTypeByteCode->code,typeid);
		fklSetU64ToByteCode(errorTypeByteCode->code+sizeof(FklSid_t),t->bc->size);
		fklReCodeCat(errorTypeByteCode,t->bc);
		t->l[0]->cpc+=errorTypeByteCode->size;
		FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,errorTypeByteCode->size);
		fklFreeByteCode(errorTypeByteCode);
		fklPushPtrStack(t,handlerByteCodelntStack);
	}
	FklByteCodelnt* t=expressionByteCodelnt;
	size_t numOfHandlerByteCode=handlerByteCodelntStack->top;
	while(!fklIsPtrStackEmpty(handlerByteCodelntStack))
	{
		FklByteCodelnt* tmp=fklPopPtrStack(handlerByteCodelntStack);
		fklReCodeLntCat(tmp,t);
		fklFreeByteCodelnt(tmp);
	}
	fklFreePtrStack(handlerByteCodelntStack);
	FklByteCode* header=fklNewByteCode(sizeof(FklSid_t)+sizeof(uint32_t)+sizeof(char));
	header->code[0]=FKL_PUSH_TRY;
	fklSetSidToByteCode(header->code+sizeof(char),sid);
	fklSetU32ToByteCode(header->code+sizeof(char)+sizeof(FklSid_t),numOfHandlerByteCode);
	fklReCodeCat(header,t->bc);
	t->l[0]->cpc+=header->size;
	FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,header->size);
	fklFreeByteCode(header);
	FklByteCode* popTry=fklNewByteCode(sizeof(char));
	popTry->code[0]=FKL_POP_TRY;
	fklCodeCat(t->bc,popTry);
	t->l[t->ls-1]->cpc+=popTry->size;
	fklFreeByteCode(popTry);
	return t;
}

void fklPrintCompileError(const FklAstCptr* obj,FklErrorType type,FklInterpreter* inter)
{
	fprintf(stderr,"error of compiling: ");
	switch(type)
	{
		case FKL_SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			fprintf(stderr," is undefined");
			break;
		case FKL_SYNTAXERROR:
			fprintf(stderr,"Invalid syntax ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_INVALIDEXPR:
			fprintf(stderr,"Invalid expression ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_INVALIDTYPEDEF:
			fprintf(stderr,"Invalid type define ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_CIRCULARLOAD:
			fprintf(stderr,"Circular load file ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_INVALIDPATTERN:
			fprintf(stderr,"Invalid string match pattern ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_INVALID_MACRO_PATTERN:
			fprintf(stderr,"Invalid macro pattern ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			fprintf(stderr," undefined");
			break;
		case FKL_CANTDEREFERENCE:
			fprintf(stderr,"cant dereference a non pointer type member ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_CANTGETELEM:
			fprintf(stderr,"cant get element of a non-array or non-pointer type");
			if(obj!=NULL)
			{
				fprintf(stderr," member by path ");
				if(obj->type==FKL_NIL)
					fprintf(stderr,"()");
				else
					fklPrintCptr(obj,stderr);
			}
			break;
		case FKL_INVALIDMEMBER:
			fprintf(stderr,"invalid member ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_NOMEMBERTYPE:
			fprintf(stderr,"cannot get member in a no-member type in ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_NONSCALARTYPE:
			fprintf(stderr,"get the reference of a non-scalar type member by path ");
			if(obj!=NULL)
			{
				if(obj->type==FKL_NIL)
					fprintf(stderr,"()");
				else
					fklPrintCptr(obj,stderr);
			}
			fprintf(stderr," is not allowed");
		case FKL_FILEFAILURE:
			fprintf(stderr,"failed for file:");
			fklPrintCptr(obj,stderr);
			break;
		case FKL_IMPORTFAILED:
			fprintf(stderr,"failed for importing module:");
			fklPrintCptr(obj,stderr);
			break;
		default:
			fprintf(stderr,"unknown compiling error.");
			break;
	}
	if(inter!=NULL)
	{
		if(inter->filename)
			fprintf(stderr," at line %d of file %s\n",(obj==NULL)?inter->curline:obj->curline,inter->filename);
		else
			fprintf(stderr," at line %d\n",(obj==NULL)?inter->curline:obj->curline);
	}
}

FklPreDef* fklAddDefine(const char* symbol,const FklAstCptr* objCptr,FklPreEnv* curEnv)
{
	if(curEnv->symbols==NULL)
	{
		curEnv->symbols=fklNewDefines(symbol);
		fklReplaceCptr(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		FklPreDef* curDef=fklFindDefine(symbol,curEnv);
		if(curDef==NULL)
		{
			curDef=fklNewDefines(symbol);
			curDef->next=curEnv->symbols;
			curEnv->symbols=curDef;
			fklReplaceCptr(&curDef->obj,objCptr);
		}
		else
			fklReplaceCptr(&curDef->obj,objCptr);
		return curDef;
	}
}

FklPreDef* fklNewDefines(const char* name)
{
	FklPreDef* tmp=(FklPreDef*)malloc(sizeof(FklPreDef));
	FKL_ASSERT(tmp,__func__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	FKL_ASSERT(tmp->symbol,__func__);
	strcpy(tmp->symbol,name);
	tmp->obj=(FklAstCptr){NULL,0,FKL_NIL,{NULL}};
	tmp->next=NULL;
	return tmp;
}

FklPreDef* fklFindDefine(const char* name,const FklPreEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		FklPreDef* curDef=curEnv->symbols;
		while(curDef&&strcmp(name,curDef->symbol))
			curDef=curDef->next;
		return curDef;
	}
}

FklCompDef* fklAddCompDef(const char* name,FklCompEnv* curEnv)
{
	if(curEnv->head==NULL)
	{
		FklSymTabNode* node=fklAddSymbolToGlob(name);
		FKL_ASSERT((curEnv->head=(FklCompDef*)malloc(sizeof(FklCompDef))),__func__);
		curEnv->head->next=NULL;
		curEnv->head->id=node->id;
		return curEnv->head;
	}
	else
	{
		FklCompDef* curDef=fklFindCompDef(name,curEnv);
		if(curDef==NULL)
		{
			FklSymTabNode* node=fklAddSymbolToGlob(name);
			FKL_ASSERT((curDef=(FklCompDef*)malloc(sizeof(FklCompDef))),__func__);
			curDef->id=node->id;
			curDef->next=curEnv->head;
			curEnv->head=curDef;
		}
		return curDef;
	}
}

FklCompDef* fklFindCompDefBySid(FklSid_t id,FklCompEnv* curEnv)
{
	if(curEnv->head==NULL)return NULL;
	else
	{
		FklCompDef* curDef=curEnv->head;
		while(curDef&&id!=curDef->id)
			curDef=curDef->next;
		return curDef;
	}
}

FklCompDef* fklAddCompDefBySid(FklSid_t id,FklCompEnv* curEnv)
{
	if(curEnv->head==NULL)
	{
		FKL_ASSERT((curEnv->head=(FklCompDef*)malloc(sizeof(FklCompDef))),__func__);
		curEnv->head->next=NULL;
		curEnv->head->id=id;
		return curEnv->head;
	}
	else
	{
		FklCompDef* curDef=fklFindCompDefBySid(id,curEnv);
		if(curDef==NULL)
		{
			FKL_ASSERT((curDef=(FklCompDef*)malloc(sizeof(FklCompDef))),__func__);
			curDef->id=id;
			curDef->next=curEnv->head;
			curEnv->head=curDef;
		}
		return curDef;
	}
}

FklCompEnv* fklNewCompEnv(FklCompEnv* prev)
{
	FklCompEnv* tmp=(FklCompEnv*)malloc(sizeof(FklCompEnv));
	FKL_ASSERT(tmp,__func__);
	tmp->prev=prev;
	if(prev)
		prev->refcount+=1;
	tmp->head=NULL;
	tmp->prefix=NULL;
	tmp->exp=NULL;
	tmp->n=0;
	tmp->macro=NULL;
	tmp->keyWords=NULL;
	tmp->proc=fklNewByteCodelnt(fklNewByteCode(0));
	tmp->refcount=0;
	return tmp;
}

void fklInitCompEnv(FklCompEnv* curEnv)
{
	int i=0;
	const char** builtInSymbolList=fklGetBuiltInSymbolList();
	for(i=0;i<FKL_NUM_OF_BUILT_IN_SYMBOL;i++)
		fklAddCompDef(builtInSymbolList[i],curEnv);
}

FklPreEnv* fklNewEnv(FklPreEnv* prev)
{
	FklPreEnv* curEnv=NULL;
	FKL_ASSERT((curEnv=(FklPreEnv*)malloc(sizeof(FklPreEnv))),__func__);
	if(prev!=NULL)prev->next=curEnv;
	curEnv->prev=prev;
	curEnv->next=NULL;
	curEnv->symbols=NULL;
	return curEnv;
}

void fklDestroyEnv(FklPreEnv* objEnv)
{
	if(objEnv==NULL)return;
	while(objEnv!=NULL)
	{
		FklPreDef* delsym=objEnv->symbols;
		while(delsym!=NULL)
		{
			free(delsym->symbol);
			fklDeleteCptr(&delsym->obj);
			FklPreDef* prev=delsym;
			delsym=delsym->next;
			free(prev);
		}
		FklPreEnv* prev=objEnv;
		objEnv=objEnv->next;
		free(prev);
	}
}

void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env)
{
	if(env)
	{
		fklFreeAllMacro(env->macro);
		env->macro=NULL;
		fklDestroyCompEnv(env);
	}
}

FklCompDef* fklFindCompDef(const char* name,FklCompEnv* curEnv)
{
	if(curEnv->head==NULL)return NULL;
	else
	{
		FklSymTabNode* node=fklAddSymbolToGlob(name);
		if(node==NULL)
			return NULL;
		FklSid_t id=node->id;
		FklCompDef* curDef=curEnv->head;
		while(curDef&&id!=curDef->id)
			curDef=curDef->next;
		return curDef;
	}
}

FklInterpreter* fklNewIntpr(const char* filename,FILE* file,FklCompEnv* env,FklLineNumberTable* lnt)
{
	FklInterpreter* tmp=NULL;
	FKL_ASSERT((tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter))),__func__)
	tmp->filename=fklCopyStr(filename);
	if(file!=stdin&&filename!=NULL)
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		if(!rp&&!file)
		{
			perror(filename);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=fklGetDir(rp?rp:filename);
		if(rp)
			free(rp);
	}
	else
		tmp->curDir=getcwd(NULL,0);
	tmp->file=file;
	tmp->curline=1;
	tmp->prev=NULL;
	if(lnt)
		tmp->lnt=lnt;
	else
		tmp->lnt=fklNewLineNumTable();
	if(env)
	{
		tmp->glob=env;
		return tmp;
	}
	tmp->glob=fklNewCompEnv(NULL);
	fklInitCompEnv(tmp->glob);
	return tmp;
}

void fklFreeIntpr(FklInterpreter* inter)
{
	if(inter->filename)
		free(inter->filename);
	if(inter->file!=stdin)
		fclose(inter->file);
	free(inter->curDir);
	fklFreeAllMacroThenDestroyCompEnv(inter->glob);
	if(inter->lnt)fklFreeLineNumberTable(inter->lnt);
	free(inter);
}

void fklDestroyCompEnv(FklCompEnv* objEnv)
{
	if(objEnv==NULL)return;
	while(objEnv)
	{
		if(!objEnv->refcount)
		{
			FklCompEnv* curEnv=objEnv;
			objEnv=objEnv->prev;
			FklCompDef* tmpDef=curEnv->head;
			while(tmpDef!=NULL)
			{
				FklCompDef* prev=tmpDef;
				tmpDef=tmpDef->next;
				free(prev);
			}
			fklFreeByteCodeAndLnt(curEnv->proc);
			fklFreeAllMacro(curEnv->macro);
			fklFreeAllKeyWord(curEnv->keyWords);
			free(curEnv);
		}
		else
		{
			objEnv->refcount-=1;
			break;
		}
	}
}

int fklHasLoadSameFile(const char* filename,const FklInterpreter* inter)
{
	while(inter!=NULL)
	{
		if(inter->filename&&!strcmp(inter->filename,filename))
			return 1;
		inter=inter->prev;
	}
	return 0;
}

FklInterpreter* fklGetFirstIntpr(FklInterpreter* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter;
}

void fklFreeAllMacro(FklPreMacro* head)
{
	FklPreMacro* cur=head;
	while(cur!=NULL)
	{
		FklPreMacro* prev=cur;
		cur=cur->next;
		fklDeleteCptr(prev->pattern);
		free(prev->pattern);
		fklDestroyCompEnv(prev->macroEnv);
		fklFreeByteCodeAndLnt(prev->proc);
		free(prev);
	}
}

char* fklGetLastWorkDir(FklInterpreter* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->curDir;
}

void fklFreeAllKeyWord(FklKeyWord* head)
{
	FklKeyWord* cur=head;
	while(cur!=NULL)
	{
		FklKeyWord* prev=cur;
		cur=cur->next;
		free(prev->word);
		free(prev);
	}
}

FklInterpreter* fklNewTmpIntpr(const char* filename,FILE* fp)
{
	FklInterpreter* tmp=NULL;
	FKL_ASSERT((tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter))),__func__);
	tmp->filename=fklCopyStr(filename);
	if(fp!=stdin&&filename)
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		if(rp==NULL)
		{
			perror(filename);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=fklGetDir(rp);
		free(rp);
	}
	else
		tmp->curDir=NULL;
	tmp->file=fp;
	tmp->curline=1;
	tmp->prev=NULL;
	tmp->glob=NULL;
	tmp->lnt=NULL;
	return tmp;
}

void fklSetInterpreterPath(const char* path)
{
	InterpreterPath=fklCopyStr(path);
}

void fklFreeInterpeterPath(void)
{
	free(InterpreterPath);
}

typedef struct
{
	FklCompEnv* env;
	size_t cp;
	FklSid_t sid;
}MayUndefine;

static MayUndefine* newMayUndefine(FklCompEnv* env,size_t cp,FklSid_t sid)
{
	MayUndefine* r=(MayUndefine*)malloc(sizeof(MayUndefine));
	FKL_ASSERT(r,__func__);
	env->refcount+=1;
	r->env=env;
	r->cp=cp;
	r->sid=sid;
	return r;
}

static void freeMayUndefine(MayUndefine* t)
{
	fklDestroyCompEnv(t->env);
	free(t);
}

void fklPrintUndefinedSymbol(FklByteCodelnt* code)
{
	FklUintStack* cpcstack=fklNewUintStack(32,16);
	FklUintStack* scpstack=fklNewUintStack(32,16);
	FklPtrStack* envstack=fklNewPtrStack(32,16);
	FklPtrStack* mayUndefined=fklNewPtrStack(32,16);
	FklByteCode* bc=code->bc;
	fklPushUintStack(0,cpcstack);
	fklPushUintStack(bc->size,scpstack);
	FklCompEnv* globEnv=fklNewCompEnv(NULL);
	fklInitCompEnv(globEnv);
	fklPushPtrStack(globEnv,envstack);
	while((!fklIsUintStackEmpty(cpcstack))&&(!fklIsUintStackEmpty(scpstack)))
	{
		uint64_t i=fklPopUintStack(cpcstack);
		uint64_t end=i+fklPopUintStack(scpstack);
		FklCompEnv* curEnv=fklPopPtrStack(envstack);
		while(i<end)
		{
			int tmplen=0;
			FklOpcode opcode=(FklOpcode)(bc->code[i]);
			switch(fklGetOpcodeArgLen(opcode))
			{
				case -4:
					{
						i+=sizeof(FklSid_t)+sizeof(char);
						int32_t handlerNum=fklGetU32FromByteCode(bc->code+i);
						i+=sizeof(uint32_t);
						int j=0;
						for(;j<handlerNum;j++)
						{
							i+=sizeof(FklSid_t);
							uint32_t pCpc=fklGetU64FromByteCode(bc->code+i);
							i+=sizeof(uint64_t);
							i+=pCpc;
						}
					}
					break;
				case -3:
					{
						int32_t scope=fklGetI32FromByteCode(bc->code+i+sizeof(char));
						FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char)+sizeof(int32_t));
						if(scope!=-1)
						{
							FklCompEnv* env=curEnv;
							for(int32_t i=0;i<scope;i++)
								env=curEnv->prev;
							fklAddCompDefBySid(id,env);
						}
						else
						{
							FklCompDef* def=NULL;
							for(FklCompEnv* e=curEnv;e;e=e->prev)
							{
								def=fklFindCompDefBySid(id,e);
								if(def)
									break;
							}
							if(!def)
								fklPushPtrStack(newMayUndefine(curEnv,i,id),mayUndefined);
						}
					}
					i+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
					break;
				case -2:
					fklPushUintStack(i+sizeof(char)+sizeof(uint64_t),cpcstack);
					{
						fklPushUintStack(fklGetU64FromByteCode(bc->code+i+sizeof(char)),scpstack);
						FklCompEnv* nextEnv=fklNewCompEnv(curEnv);
						fklPushPtrStack(nextEnv,envstack);
					}
					i+=sizeof(char)+sizeof(uint64_t)+fklGetU64FromByteCode(bc->code+i+sizeof(char));
					break;
				case -1:
					tmplen=strlen((char*)bc->code+i+1);
					i+=sizeof(char)+tmplen+1;
					break;
				case 0:
					if(opcode==FKL_PUSH_R_ENV)
						curEnv=fklNewCompEnv(curEnv);
					else if(opcode==FKL_POP_R_ENV)
					{
						FklCompEnv* p=curEnv->prev;
						fklDestroyCompEnv(curEnv);
						curEnv=p;
					}
					i+=sizeof(char);
					break;
				case 1:
					i+=sizeof(char)+sizeof(char);
					break;
				case 4:
					i+=sizeof(char)+sizeof(int32_t);
					break;
				case 8:
					switch(opcode)
					{
						case FKL_PUSH_F64:
							break;
						case FKL_PUSH_I64:
							break;
						case FKL_POP_ARG:
						case FKL_POP_REST_ARG:
						case FKL_PUSH_VAR:
							{
								FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char));
								if(opcode==FKL_POP_ARG||opcode==FKL_POP_REST_ARG)
									fklAddCompDefBySid(id,curEnv);
								else if(opcode==FKL_PUSH_VAR)
								{
									FklCompDef* def=NULL;
									for(FklCompEnv* e=curEnv;e;e=e->prev)
									{
										def=fklFindCompDefBySid(id,e);
										if(def)
											break;
									}
									if(!def)
										fklPushPtrStack(newMayUndefine(curEnv,i,id),mayUndefined);
								}
							}
							break;
						default:break;
					}
					i+=sizeof(char)+sizeof(int64_t);
					break;
			}
		}
		fklDestroyCompEnv(curEnv);
	}
	fklFreeUintStack(cpcstack);
	fklFreeUintStack(scpstack);
	fklFreePtrStack(envstack);
	for(uint32_t i=0;i<mayUndefined->top;i++)
	{
		MayUndefine* cur=mayUndefined->base[i];
		FklCompEnv* curEnv=cur->env;
		FklCompDef* def=NULL;
		for(FklCompEnv* e=curEnv;e;e=e->prev)
		{
			def=fklFindCompDefBySid(cur->sid,e);
			if(def)
				break;
		}
		if(!def)
		{
			FklLineNumberTable table={.list=code->l,.num=code->ls};
			FklLineNumTabNode* node=fklFindLineNumTabNode(cur->cp,&table);
			fprintf(stderr,"warning of compiling: Symbol \"%s\" is undefined at line %d of %s\n"
					,fklGetGlobSymbolWithId(cur->sid)->symbol
					,node->line
					,fklGetGlobSymbolWithId(node->fid)->symbol);
		}
		freeMayUndefine(cur);
	}
	fklFreePtrStack(mayUndefined);
}
