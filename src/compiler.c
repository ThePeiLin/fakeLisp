#include<fakeLisp/compiler.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
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
		if(curEnv->proc->bc->size)
		{
			FklVM* tmpVM=fklNewTmpVM(NULL);
			vEnv=fklNewVMenv(preEnv);
			preEnv=vEnv;
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

static inline void setI32ToByteCode(uint8_t* code,int32_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
}

static inline void setU32ToByteCode(uint8_t* code,uint32_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
}

static inline void setI64ToByteCode(uint8_t* code,int64_t i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
}

static inline void setF64ToByteCode(uint8_t* code,double i)
{
	code[0]=((uint8_t*)&i)[0];
	code[1]=((uint8_t*)&i)[1];
	code[2]=((uint8_t*)&i)[2];
	code[3]=((uint8_t*)&i)[3];
	code[4]=((uint8_t*)&i)[4];
	code[5]=((uint8_t*)&i)[5];
	code[6]=((uint8_t*)&i)[6];
	code[7]=((uint8_t*)&i)[7];
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
			fklFreeVMheap(tmpVM->heap);
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
			tmpCptr=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),objCptr->curline);
			fklReplaceCptr(objCptr,tmpCptr);
			fklDeleteCptr(tmpCptr);
			free(tmpCptr);
		}
		else
		{
			fklFreeByteCodeAndLnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklUninitVMRunningResource(tmpVM);
			return 2;
		}
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
		FKL_ASSERT(current,"fklAddDefinedMacro",__FILE__,__LINE__);
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
		FKL_ASSERT((current=(FklPreMacro*)malloc(sizeof(FklPreMacro))),"fklAddMacro",__FILE__,__LINE__);
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
				if(firAtm->type==FKL_SYM)return 0;
				if(firAtm->type==FKL_STR&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_DBL&&fabs(firAtm->value.f64-secAtm->value.f64)!=0)return 0;
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
	if(strcmp(tmpPair->car.u.atom->value.str,oriPair->car.u.atom->value.str))
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
	fklAddKeyWord("deftype",glob);
	fklAddKeyWord("getf",glob);
	fklAddKeyWord("szof",glob);
	fklAddKeyWord("flsym",glob);
}

void fklUninitPreprocess()
{
	fklFreeAllStringPattern();
}

FklAstCptr** dealArg(FklAstCptr* argCptr,int num)
{
	FklAstCptr** args=NULL;
	FKL_ASSERT((args=(FklAstCptr**)malloc(num*sizeof(FklAstCptr*))),"dealArg",__FILE__,__LINE__);
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
			state.state=FKL_INVALIDPATTERN;
			state.place=args[0];
			free(args);
			return state;
		}
		FklAstCptr* pattern=fklNewCptr(0,NULL);
		fklReplaceCptr(pattern,args[0]);
		FklAstCptr* express=args[1];
		FklInterpreter* tmpInter=fklNewTmpIntpr(NULL,NULL);
		tmpInter->filename=inter->filename;
		tmpInter->curline=inter->curline;
		tmpInter->glob=/*(curEnv->prev&&curEnv->prev->exp)?curEnv->prev:*/curEnv;
		tmpInter->curDir=inter->curDir;
		tmpInter->prev=NULL;
		tmpInter->lnt=NULL;
		FklCompEnv* tmpCompEnv=fklCreateMacroCompEnv(&pattern->u.pair->cdr,tmpInter->glob);
		FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
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
	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FKL_ASSERT(tmParts,"fklAddStringPattern",__FILE__,__LINE__);
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
	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state,1);
	if(!state.state)
	{
		char** tmParts=(char**)malloc(sizeof(char*)*num);
		FKL_ASSERT(tmParts,"fklAddStringPattern",__FILE__,__LINE__);
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

FklByteCodelnt* fklCompile(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
		if(fklIsImportExpression(objCptr))return fklCompileImport(objCptr,curEnv,inter,state,evalIm);
		if(fklIsTryExpression(objCptr))return fklCompileTry(objCptr,curEnv,inter,state,evalIm);
		if(fklIsFlsymExpression(objCptr))return fklCompileFlsym(objCptr,curEnv,inter,state,evalIm);
		if(fklIsLibraryExpression(objCptr))
		{
			FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
			FKL_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
			tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,0,inter->curline);
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
			FKL_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
			tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id
					,0
					,0
					,objCptr->curline);
			return tmp;
		}
		else if(fklIsDeftypeExpression(objCptr))
		{
			FklSid_t typeName=0;
			FklTypeId_t type=fklGenDefTypes(objCptr,inter->deftypes,&typeName);
			//FklDefTypeUnion type={.all=fklGenDefTypes(objCptr,inter->deftypes,&typeName).all};
			if(!type)
			{
				state->state=FKL_INVALIDTYPEDEF;
				state->place=objCptr;
				return NULL;
			}
			if(fklAddDefTypes(inter->deftypes,typeName,type))
			{
				state->state=FKL_INVALIDTYPEDEF;
				state->place=objCptr;
				return NULL;
			}
			FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
			tmp->ls=1;
			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
			FKL_ASSERT(tmp->l,"fklCompile",__FILE__,__LINE__);
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
		else if(fklIsFuncCall(objCptr,curEnv))return fklCompileFuncCall(objCptr,curEnv,inter,state,evalIm);
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
	switch((int)tmpAtm->type)
	{
		case FKL_SYM:
			tmp=fklNewByteCode(sizeof(char)+sizeof(FklSid_t));
			tmp->code[0]=FKL_PUSH_SYM;
			setU32ToByteCode(tmp->code+sizeof(char),fklAddSymbolToGlob(tmpAtm->value.str)->id);
			break;
		case FKL_STR:
			tmp=fklNewByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FKL_PUSH_STR;
			strcpy((char*)tmp->code+1,tmpAtm->value.str);
			break;
		case FKL_I32:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t));
			tmp->code[0]=FKL_PUSH_I32;
			setI32ToByteCode(tmp->code+sizeof(char),tmpAtm->value.i32);
			break;
		case FKL_I64:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int64_t));
			tmp->code[0]=FKL_PUSH_I64;
			setI64ToByteCode(tmp->code+sizeof(char),tmpAtm->value.i64);
			break;
		case FKL_DBL:
			tmp=fklNewByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FKL_PUSH_F64;
			setF64ToByteCode(tmp->code+sizeof(char),tmpAtm->value.f64);
			break;
		case FKL_CHR:
			tmp=fklNewByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FKL_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
		case FKL_BYTS:
			tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t)+tmpAtm->value.byts.size);
			tmp->code[0]=FKL_PUSH_BYTS;
			setU32ToByteCode(tmp->code+sizeof(char),tmpAtm->value.byts.size);
			memcpy(tmp->code+sizeof(char)+sizeof(uint32_t)
					,tmpAtm->value.byts.str
					,tmpAtm->value.byts.size);
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

FklByteCodelnt* fklCompileQsquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	if(objCptr->type==FKL_ATM)
		return fklCompileConst(objCptr,curEnv,inter,state,evalIm);
	else if(fklIsUnquoteExpression(objCptr))
		return fklCompileUnquote(objCptr,curEnv,inter,state,evalIm);
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
			FklByteCodelnt* tmp1=fklCompileUnquote(objCptr,curEnv,inter,state,evalIm);
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
			if(objCptr==&objPair->cdr)
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
			FklByteCodelnt* tmp1=fklCompile(fklNextCptr(fklGetFirstCptr(objCptr)),curEnv,inter,state,evalIm);
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
					FKL_ASSERT(tmp->l,"fklCompileQsquote",__FILE__,__LINE__);
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
		else if((objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)&&(!fklIsUnqtespExpression(&objPair->car)))
		{
			FklByteCodelnt* tmp1=fklCompileConst(objCptr,curEnv,inter,state,evalIm);
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
	switch((int)objCptr->type)
	{
		case FKL_PAIR:
			return fklCompilePair(objCptr);
			break;
		case FKL_ATM:
			if(objCptr->u.atom->type==FKL_SYM)
			{
				FklByteCode* tmp=fklNewByteCode(sizeof(char)+sizeof(int32_t));
				char* sym=objCptr->u.atom->value.str;
				FklSymTabNode* node=fklAddSymbolToGlob(sym);
				tmp->code[0]=FKL_PUSH_SYM;
				setU32ToByteCode(tmp->code+sizeof(char),node->id);
				return tmp;
			}
			else
				return fklCompileAtom(objCptr);
			break;
		case FKL_NIL:
			return fklCompileNil();
			break;
	}
	return NULL;
}

FklByteCodelnt* fklCompileUnquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	return fklCompile(objCptr,curEnv,inter,state,evalIm);
}

FklByteCodelnt* fklCompileConst(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	int32_t line=objCptr->curline;
	FklByteCode* tmp=NULL;
	if(objCptr->type==FKL_ATM)tmp=fklCompileAtom(objCptr);
	if(fklIsNil(objCptr))tmp=fklCompileNil();
	if(fklIsQuoteExpression(objCptr))tmp=fklCompileQuote(objCptr);
	if(!tmp)
	{
		state->state=FKL_INVALIDEXPR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCodelnt* t=fklNewByteCodelnt(tmp);
	FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->size,line);
	t->ls=1;
	t->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
	FKL_ASSERT(t->l,"fklCompileConst",__FILE__,__LINE__);
	t->l[0]=n;
	return t;
}

FklByteCodelnt* fklCompileFuncCall(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
				FKL_ASSERT(tmp->l,"fklCompileFuncCall",__FILE__,__LINE__);
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
				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
				FKL_ASSERT(tmp->l,"fklCompileFuncCall",__FILE__,__LINE__);
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

FklByteCodelnt* fklCompileDef(FklAstCptr* tir,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
		FklCompDef* tmpDef=NULL;
		FklAstAtom* tmpAtm=sec->u.atom;
		if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
		{
			if(curEnv->prev->prefix)
			{
				size_t len=strlen(tmpAtm->value.str)+strlen(curEnv->prev->prefix)+1;
				char* symbolWithPrefix=(char*)malloc(sizeof(char)*len);
				FKL_ASSERT(symbolWithPrefix,"fklCompileDef",__FILE__,__LINE__);
				sprintf(symbolWithPrefix,"%s%s",curEnv->prev->prefix,tmpAtm->value.str);
				tmpDef=fklAddCompDef(symbolWithPrefix,curEnv->prev);
				free(symbolWithPrefix);
			}
			else
				tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv->prev);
			setI32ToByteCode(popVar->code+sizeof(char),1);
		}
		else
		{
			setI32ToByteCode(popVar->code+sizeof(char),0);
			tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv);
		}
		setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
		FklByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
		fklCodeCat(tmp1->bc,pushTop);
		fklCodeCat(tmp1->bc,popVar);
		tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
		if(fklIsLambdaExpression(objCptr)||fklIsConst(objCptr))
		{
			if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
			{
				FklByteCode* popVar=fklNewByteCode(sizeof(char)+sizeof(int32_t)+sizeof(int32_t));
				popVar->code[0]=FKL_POP_VAR;
				setI32ToByteCode(popVar->code+sizeof(char),0);
				setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
				fklCodeCat(tmp1Copy->bc,pushTop);
				fklCodeCat(tmp1Copy->bc,popVar);
				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
				fklCodelntCopyCat(curEnv->prev->proc,tmp1Copy);
				fklFreeByteCodeAndLnt(tmp1Copy);
				tmp1Copy=fklCopyByteCodelnt(tmp1);
				fklCodeCat(tmp1Copy->bc,pushTop);
				setI32ToByteCode(popVar->code+sizeof(char),0);
				setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklAddCompDef(tmpAtm->value.str,curEnv)->id);
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

FklByteCodelnt* fklCompileSetq(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
				FKL_ASSERT(symbolWithPrefix,"fklCompileSetq",__FILE__,__LINE__);
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
		setI32ToByteCode(popVar->code+sizeof(char),scope);
		setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),id);
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
				setI32ToByteCode(popVar->code+sizeof(char),scope+1);
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
		if(tmpEnv->prev&&tmpEnv->prev->exp&&scope!=-1&&fklIsSymbolShouldBeExport(tmpAtm->value.str,tmpEnv->prev->exp,tmpEnv->prev->n))
		{
			setI32ToByteCode(popVar->code+sizeof(char),scope+1);
			if(tmpEnv->prev->prefix)
			{
				char* symbolWithPrefix=fklCopyStr(tmpEnv->prev->prefix);
				symbolWithPrefix=fklStrCat(symbolWithPrefix,tmpAtm->value.str);
				setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id);
				free(symbolWithPrefix);
			}
			else
				setU32ToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklFindCompDef(tmpAtm->value.str,tmpEnv->prev)->id);
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

FklByteCodelnt* fklCompileSetf(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* fir=&objCptr->u.pair->car;
	FklAstCptr* sec=fklNextCptr(fir);
	if(fklIsSymbol(sec))
		return fklCompileSetq(objCptr,curEnv,inter,state,0);
	else
	{
		FklAstCptr* tir=fklNextCptr(sec);
		FklByteCodelnt* tmp1=fklCompile(sec,curEnv,inter,state,evalIm);
		if(state->state!=0)
			return NULL;
		FklByteCodelnt* tmp2=(fklIsLambdaExpression(tir))?
			fklCompile(tir,curEnv,inter,state,0):
			fklCompile(tir,curEnv,inter,state,evalIm);
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

FklByteCodelnt* fklCompileGetf(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* fir=fklGetFirstCptr(objCptr);
	FklAstCptr* typeCptr=fklNextCptr(fir);
	FklAstCptr* pathCptr=fklNextCptr(typeCptr);
	FklAstCptr* expressionCptr=fklNextCptr(pathCptr);
	FklAstCptr* indexCptr=fklNextCptr(expressionCptr);
	FklTypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!(expressionCptr&&type!=0))
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklDefTypeUnion typeUnion=fklGetVMTypeUnion(type);
	if(FKL_GET_TYPES_TAG(typeUnion.all)!=FKL_DEF_FUNC_TYPE_TAG)
	{
		FklTypeId_t memberType=type;
		ssize_t offset=0;
		FklByteCodelnt* tmp=NULL;
		for(pathCptr=fklGetFirstCptr(pathCptr);pathCptr;pathCptr=fklNextCptr(pathCptr))
		{
			if(pathCptr->type!=FKL_ATM||(pathCptr->u.atom->type!=FKL_SYM&&pathCptr->u.atom->type!=FKL_I32&&pathCptr->u.atom->type!=FKL_I64))
			{
				state->state=FKL_SYNTAXERROR;
				state->place=pathCptr;
				return NULL;
			}
			else if(pathCptr->u.atom->type==FKL_I32||pathCptr->u.atom->type==FKL_I64)
			{
				size_t typeSize=fklGetVMTypeSizeWithTypeId(memberType);
				offset+=typeSize*((pathCptr->u.atom->type==FKL_I32)?pathCptr->u.atom->value.i32:pathCptr->u.atom->value.i64);
			}
			else if(!strcmp(pathCptr->u.atom->value.str,"*"))
			{
				FklDefTypeUnion curTypeUnion=fklGetVMTypeUnion(memberType);
				void* tmpPtr=FKL_GET_TYPES_PTR(curTypeUnion.all);
				FklDefTypeTag tag=FKL_GET_TYPES_TAG(curTypeUnion.all);
				FklTypeId_t defTypeId=0;
				switch(tag)
				{
					case FKL_DEF_ARRAY_TYPE_TAG:
						defTypeId=((FklDefArrayType*)tmpPtr)->etype;
						break;
					case FKL_DEF_PTR_TYPE_TAG:
						defTypeId=((FklDefPtrType*)tmpPtr)->ptype;
						break;
					default:
						state->state=FKL_CANTDEREFERENCE;
						state->place=pathCptr;
						if(tmp)
							fklFreeByteCodeAndLnt(tmp);
						return NULL;
						break;
				}
				FklByteCodelnt* pushDefRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t)));
				pushDefRef->bc->code[0]=FKL_PUSH_DEF_REF;
				setI64ToByteCode(pushDefRef->bc->code+sizeof(char),offset);
				setU32ToByteCode(pushDefRef->bc->code+sizeof(char)+sizeof(ssize_t),defTypeId);
				FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushDefRef->bc->size,pathCptr->curline);
				pushDefRef->ls=1;
				pushDefRef->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
				FKL_ASSERT(pushDefRef->l,"fklCompileGetf",__FILE__,__LINE__);
				pushDefRef->l[0]=n;
				offset=0;
				memberType=defTypeId;
				if(!tmp)
					tmp=pushDefRef;
				else
				{
					fklCodeLntCat(tmp,pushDefRef);
					fklFreeByteCodelnt(pushDefRef);
				}
			}
			else if(!strcmp(pathCptr->u.atom->value.str,"&"))
			{
				FklByteCodelnt* pushPtrRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t)));
				pushPtrRef->bc->code[0]=FKL_PUSH_PTR_REF;
				FklTypeId_t ptrId=fklNewVMPtrType(memberType);
				setI64ToByteCode(pushPtrRef->bc->code+sizeof(char),offset);
				setU32ToByteCode(pushPtrRef->bc->code+sizeof(char)+sizeof(ssize_t),ptrId);
				FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushPtrRef->bc->size,pathCptr->curline);
				pushPtrRef->ls=1;
				pushPtrRef->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
				FKL_ASSERT(pushPtrRef->l,"fklCompileGetf",__FILE__,__LINE__);
				pushPtrRef->l[0]=n;
				offset=0;
				memberType=ptrId;
				if(!tmp)
					tmp=pushPtrRef;
				else
				{
					fklCodeLntCat(tmp,pushPtrRef);
					fklFreeByteCodelnt(pushPtrRef);
				}
			}
			else
			{
				if(FKL_GET_TYPES_TAG(typeUnion.all)!=FKL_DEF_STRUCT_TYPE_TAG&&FKL_GET_TYPES_TAG(typeUnion.all)!=FKL_DEF_UNION_TYPE_TAG)
				{
					if(tmp)
						fklFreeByteCodeAndLnt(tmp);
					state->state=FKL_NOMEMBERTYPE;
					state->place=typeCptr;
					return NULL;
				}
				FklSid_t curPathNode=fklAddSymbolToGlob(pathCptr->u.atom->value.str)->id;
				FklDefTypeUnion typeUnion=fklGetVMTypeUnion(memberType);
				if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_DEF_STRUCT_TYPE_TAG)
				{
					FklDefStructType* structType=FKL_GET_TYPES_PTR(typeUnion.st);
					uint32_t i=0;
					uint32_t num=structType->num;
					FklTypeId_t tmpType=0;
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
						state->state=FKL_INVALIDMEMBER;
						state->place=pathCptr;
						if(tmp)
							fklFreeByteCodeAndLnt(tmp);
						return NULL;
					}
					FklAstCptr* next=fklNextCptr(pathCptr);
					if(!next||(next->type==FKL_ATM&&next->u.atom->type==FKL_SYM&&(!strcmp(next->u.atom->value.str,"&")||!strcmp(next->u.atom->value.str,"*"))))
					{
						FklByteCodelnt* pushRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t)));
						pushRef->bc->code[0]=FKL_PUSH_REF;
						setI64ToByteCode(pushRef->bc->code+sizeof(char),offset);
						setU32ToByteCode(pushRef->bc->code+sizeof(char)+sizeof(ssize_t),memberType);
						FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushRef->bc->size,pathCptr->curline);
						pushRef->ls=1;
						pushRef->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
						FKL_ASSERT(pushRef->l,"fklCompileGetf",__FILE__,__LINE__);
						pushRef->l[0]=n;
						if(!tmp)
							tmp=pushRef;
						else
						{
							fklCodeLntCat(tmp,pushRef);
							fklFreeByteCodelnt(pushRef);
						}
						offset=0;
					}
				}
				else
				{
					FklDefUnionType* unionType=FKL_GET_TYPES_PTR(typeUnion.st);
					uint32_t i=0;
					uint32_t num=unionType->num;
					FklTypeId_t tmpType=0;
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
						state->state=FKL_INVALIDMEMBER;
						state->place=pathCptr;
						if(tmp)
							fklFreeByteCodeAndLnt(tmp);
						return NULL;
					}
					FklAstCptr* next=fklNextCptr(pathCptr);
					if(!next||(next->type==FKL_ATM&&next->u.atom->type==FKL_SYM&&(!strcmp(next->u.atom->value.str,"&")||!strcmp(next->u.atom->value.str,"*"))))
					{
						FklByteCodelnt* pushRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t)));
						pushRef->bc->code[0]=FKL_PUSH_REF;
						setI64ToByteCode(pushRef->bc->code+sizeof(char),offset);
						setU32ToByteCode(pushRef->bc->code+sizeof(char)+sizeof(ssize_t),memberType);
						FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushRef->bc->size,pathCptr->curline);
						pushRef->ls=1;
						pushRef->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
						FKL_ASSERT(pushRef->l,"fklCompileGetf",__FILE__,__LINE__);
						pushRef->l[0]=n;
						if(!tmp)
							tmp=pushRef;
						else
						{
							fklCodeLntCat(tmp,pushRef);
							fklFreeByteCodelnt(pushRef);
						}
						offset=0;
					}
				}
			}
		}
		FklByteCodelnt* expression=fklCompile(expressionCptr,curEnv,inter,state,evalIm);
		if(state->state)
		{
			if(tmp)
				fklFreeByteCodeAndLnt(tmp);
			return NULL;
		}
		if(!tmp)
			tmp=expression;
		else
		{
			fklReCodeLntCat(expression,tmp);
			fklFreeByteCodelnt(expression);
		}
		FklByteCodelnt* index=NULL;
		if(indexCptr!=NULL)
		{
			index=fklCompile(indexCptr,curEnv,inter,state,evalIm);
			if(state->state)
			{
				fklFreeByteCodeAndLnt(tmp);
				return NULL;
			}
			FklTypeId_t elemType=0;
			FklDefTypeUnion outerType=fklGetVMTypeUnion(memberType);
			void* tmpPtr=FKL_GET_TYPES_PTR(outerType.all);
			FklDefTypeTag tag=FKL_GET_TYPES_TAG(outerType.all);
			switch(tag)
			{
				case FKL_DEF_PTR_TYPE_TAG:
					elemType=((FklDefPtrType*)tmpPtr)->ptype;
					break;
				case FKL_DEF_ARRAY_TYPE_TAG:
					elemType=((FklDefArrayType*)tmpPtr)->etype;
					break;
				default:
					state->state=FKL_CANTGETELEM;
					state->place=pathCptr;
					fklFreeByteCodeAndLnt(index);
					fklFreeByteCodeAndLnt(tmp);
					return NULL;
					break;
			}
			FklByteCodelnt* pushIndRef=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(FklTypeId_t)+sizeof(uint32_t)));
			pushIndRef->bc->code[0]=FKL_PUSH_IND_REF;
			setU32ToByteCode(pushIndRef->bc->code+sizeof(char),elemType);
			setU32ToByteCode(pushIndRef->bc->code+sizeof(char)+sizeof(FklTypeId_t),fklGetVMTypeSizeWithTypeId(elemType));
			FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushIndRef->bc->size,indexCptr->curline);
			pushIndRef->ls=1;
			pushIndRef->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
			FKL_ASSERT(pushIndRef->l,"fklCompileGetf",__FILE__,__LINE__);
			pushIndRef->l[0]=n;
			fklCodeLntCat(tmp,index);
			fklFreeByteCodelnt(index);
			fklCodeLntCat(tmp,pushIndRef);
			fklFreeByteCodelnt(pushIndRef);
		}
		else
		{
			FklDefTypeUnion typeUnion=fklGetVMTypeUnion(memberType);
			FklDefTypeTag tag=FKL_GET_TYPES_TAG(typeUnion.all);
			if(tag!=FKL_DEF_NATIVE_TYPE_TAG&&tag!=FKL_DEF_PTR_TYPE_TAG&&tag!=FKL_DEF_ARRAY_TYPE_TAG&&tag!=FKL_DEF_FUNC_TYPE_TAG)
			{
				fklFreeByteCodeAndLnt(tmp);
				state->state=FKL_NONSCALARTYPE;
				state->place=fklNextCptr(typeCptr);
				return NULL;
			}
		}
		return tmp;
	}
	else
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
}

FklByteCodelnt* fklCompileFlsym(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* fir=fklGetFirstCptr(objCptr);
	FklAstCptr* typeCptr=fklNextCptr(fir);
	FklTypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!type)
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklDefTypeUnion typeUnion=fklGetVMTypeUnion(type);
	if(FKL_GET_TYPES_TAG(typeUnion.all)!=FKL_DEF_FUNC_TYPE_TAG)
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklAstCptr* nameCptr=fklNextCptr(typeCptr);
	if(fklNextCptr(nameCptr)!=NULL)
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	if(!nameCptr)
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCodelnt* name=fklCompile(nameCptr,curEnv,inter,state,evalIm);
	if(state->state)
		return NULL;
	if(state->state)
	{
		fklFreeByteCodeAndLnt(name);
		return NULL;
	}
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(FklTypeId_t)));
	tmp->bc->code[0]=FKL_PUSH_FPROC;
	setU32ToByteCode(tmp->bc->code+sizeof(char),type);
	FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
	tmp->ls=1;
	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
	FKL_ASSERT(tmp->l,"fklCompileGetf",__FILE__,__LINE__);
	tmp->l[0]=n;
	fklReCodeLntCat(name,tmp);
	fklFreeByteCodelnt(name);
	return tmp;
}
FklByteCodelnt* fklCompileSzof(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* typeCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	FklTypeId_t type=fklGenDefTypesUnion(typeCptr,inter->deftypes);
	if(!type)
	{
		state->state=FKL_INVALIDTYPEDEF;
		state->place=typeCptr;
		return NULL;
	}
	size_t size=fklGetVMTypeSizeWithTypeId(type);
	FklByteCodelnt* tmp=NULL;
	if(size>INT32_MAX)
	{
		tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(int64_t)));
		tmp->bc->code[0]=FKL_PUSH_I64;
		setI64ToByteCode(tmp->bc->code+sizeof(char),size);
	}
	else
	{
		tmp=fklNewByteCodelnt(fklNewByteCode(sizeof(char)+sizeof(int32_t)));
		tmp->bc->code[0]=FKL_PUSH_I32;
		setI32ToByteCode(tmp->bc->code+sizeof(char),size);
	}
	FklLineNumTabNode* n=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
	tmp->ls=1;
	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
	FKL_ASSERT(tmp->l,"fklCompileSzof",__FILE__,__LINE__);
	tmp->l[0]=n;
	return tmp;
}

FklByteCodelnt* fklCompileSym(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
			FKL_ASSERT(symbolWithPrefix,"fklCompileSym",__FILE__,__LINE__);
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
		if(evalIm)
		{
			state->state=FKL_SYMUNDEFINE;
			state->place=objCptr;
			fklFreeByteCode(pushVar);
			return NULL;
		}
	}
	else
		id=tmpDef->id;
	setU32ToByteCode(pushVar->code+sizeof(char),id);
	FklByteCodelnt* bcl=fklNewByteCodelnt(pushVar);
	bcl->ls=1;
	bcl->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(bcl->l,"fklCompileSym",__FILE__,__LINE__);
	bcl->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushVar->size,line);
	return bcl;
}

FklByteCodelnt* fklCompileAnd(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	int32_t curline=objCptr->curline;
	FklByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* push1=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack=fklNewPtrStack(32,16);
	jumpiffalse->code[0]=FKL_JMP_IF_FALSE;
	push1->code[0]=FKL_PUSH_I32;
	setI32ToByteCode(push1->code+sizeof(char),1);
	resTp->code[0]=FKL_RES_TP;
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(jumpiffalse);
			fklFreeByteCode(push1);
			fklFreePtrStack(stack);
			return NULL;
		}
		fklPushPtrStack(tmp1,stack);
	}
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
		setI32ToByteCode(jumpiffalse->code+sizeof(char),tmp->bc->size);
		fklCodeCat(tmp1->bc,jumpiffalse);
		tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		fklReCodeLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);

	}
	fklReCodeCat(push1,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,"fklCompileAnd",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,curline);
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
	fklFreePtrStack(stack);
	//fklPrintByteCodelnt(tmp,inter->table,stderr);
	return tmp;
}

FklByteCodelnt* fklCompileOr(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	int32_t curline=objCptr->curline;
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* jumpifture=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* pushnil=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack=fklNewPtrStack(32,16);
	pushnil->code[0]=FKL_PUSH_NIL;
	jumpifture->code[0]=FKL_JMP_IF_TRUE;
	setTp->code[0]=FKL_SET_TP;
	popTp->code[0]=FKL_POP_TP;
	resTp->code[0]=FKL_RES_TP;
	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
		if(state->state!=0)
		{
			fklFreeByteCode(resTp);
			fklFreeByteCode(popTp);
			fklFreeByteCode(setTp);
			fklFreeByteCodeAndLnt(tmp);
			fklFreeByteCode(jumpifture);
			fklFreeByteCode(pushnil);
			fklFreePtrStack(stack);
			return NULL;
		}
		fklPushPtrStack(tmp1,stack);
	}
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
		setI32ToByteCode(jumpifture->code+sizeof(char),tmp->bc->size);
		fklCodeCat(tmp1->bc,jumpifture);
		tmp1->l[tmp1->ls-1]->cpc+=jumpifture->size;
		fklReCodeCat(resTp,tmp1->bc);
		tmp1->l[0]->cpc+=resTp->size;
		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
		fklReCodeLntCat(tmp1,tmp);
		fklFreeByteCodelnt(tmp1);
	}
	fklReCodeCat(pushnil,tmp->bc);
	if(!tmp->l)
	{
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,"fklCompileOr",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,curline);
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
	fklFreePtrStack(stack);
	return tmp;
}

FklByteCodelnt* fklCompileBegin(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
		FklByteCodelnt* tmp1=fklCompile(firCptr,curEnv,inter,state,evalIm);
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
		FKL_ASSERT(tmp->l,"fklCompileBegin",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,tmp->bc->size,objCptr->curline);
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

FklByteCodelnt* fklCompileLambda(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
			setU32ToByteCode(popArg->code+sizeof(char),tmpDef->id);
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
				setU32ToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
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
		setU32ToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
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
	FKL_ASSERT(codeOfLambda->l,"fklCompileLambda",__FILE__,__LINE__);
	codeOfLambda->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pArg->size,line);
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	resTp->code[0]=FKL_RES_TP;
	for(;objCptr;objCptr=fklNextCptr(objCptr))
	{
		FklByteCodelnt* tmp1=fklCompile(objCptr,tmpEnv,inter,state,evalIm);
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
	FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	pushProc->code[0]=FKL_PUSH_PROC;
	setU32ToByteCode(pushProc->code+sizeof(char),codeOfLambda->bc->size);
	FklByteCodelnt* toReturn=fklNewByteCodelnt(pushProc);
	toReturn->ls=1;
	toReturn->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(toReturn->l,"fklCompileLambda",__FILE__,__LINE__);
	toReturn->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushProc->size,line);
	fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
	fklCodeLntCat(toReturn,codeOfLambda);
	fklFreeByteCodelnt(codeOfLambda);
	return toReturn;
}

FklByteCodelnt* fklCompileCond(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* cond=NULL;
	FklByteCode* pushnil=fklNewByteCode(sizeof(char));
	FklByteCode* jumpiffalse=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* jump=fklNewByteCode(sizeof(char)+sizeof(int32_t));
	FklByteCode* resTp=fklNewByteCode(sizeof(char));
	FklByteCode* setTp=fklNewByteCode(sizeof(char));
	FklByteCode* popTp=fklNewByteCode(sizeof(char));
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	FklPtrStack* stack1=fklNewPtrStack(32,16);
	setTp->code[0]=FKL_SET_TP;
	resTp->code[0]=FKL_RES_TP;
	popTp->code[0]=FKL_POP_TP;
	pushnil->code[0]=FKL_PUSH_NIL;
	jumpiffalse->code[0]=FKL_JMP_IF_FALSE;
	jump->code[0]=FKL_JMP;
	for(cond=fklNextCptr(fklGetFirstCptr(objCptr));cond!=NULL;cond=fklNextCptr(cond))
	{
		objCptr=fklGetFirstCptr(cond);
		FklByteCodelnt* conditionCode=fklCompile(objCptr,curEnv,inter,state,evalIm);
		FklPtrStack* stack2=fklNewPtrStack(32,16);
		for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
		{
			FklByteCodelnt* tmp1=fklCompile(objCptr,curEnv,inter,state,evalIm);
			if(state->state!=0)
			{
				fklFreeByteCodeAndLnt(tmp);
				fklFreeByteCode(jumpiffalse);
				fklFreeByteCode(jump);
				fklFreeByteCode(resTp);
				fklFreeByteCode(setTp);
				fklFreeByteCode(popTp);
				fklFreeByteCode(pushnil);
				fklFreePtrStack(stack1);
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
			setI32ToByteCode(jumpiffalse->code+sizeof(char),tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size));
			fklCodeCat(conditionCode->bc,jumpiffalse);
			conditionCode->l[conditionCode->ls-1]->cpc+=jumpiffalse->size;
		}
		fklReCodeLntCat(conditionCode,tmpCond);
		fklFreeByteCodelnt(conditionCode);
		fklPushPtrStack(tmpCond,stack1);
	}
	uint32_t top=stack1->top-1;
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklByteCodelnt* tmpCond=fklPopPtrStack(stack1);
		if(!fklIsPtrStackEmpty(stack1))
		{
			fklReCodeCat(resTp,tmpCond->bc);
			tmpCond->l[0]->cpc+=1;
			FKL_INCREASE_ALL_SCP(tmpCond->l+1,tmpCond->ls-1,resTp->size);
		}
		if(top!=stack1->top)
		{
			setI32ToByteCode(jump->code+sizeof(char),tmp->bc->size);
			fklCodeCat(tmpCond->bc,jump);
			tmpCond->l[tmpCond->ls-1]->cpc+=jump->size;
		}
		fklReCodeLntCat(tmpCond,tmp);
		fklFreeByteCodelnt(tmpCond);
	}
	fklFreePtrStack(stack1);
	if(!tmp->l)
	{
		fklCodeCat(tmp->bc,pushnil);
		tmp->ls=1;
		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
		FKL_ASSERT(tmp->l,"fklCompileCond",__FILE__,__LINE__);
		tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,pushnil->size,objCptr->curline);
	}
	fklReCodeCat(setTp,tmp->bc);
	tmp->l[0]->cpc+=setTp->size;
	FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
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

FklByteCodelnt* fklCompileLoad(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
		perror(name->value.str);
		return NULL;
	}
	fklAddSymbolToGlob(name->value.str);
	FklInterpreter* tmpIntpr=fklNewIntpr(name->value.str,file,curEnv,inter->lnt,inter->deftypes);
	tmpIntpr->prev=inter;
	tmpIntpr->glob=curEnv;
	FklByteCodelnt* tmp=fklCompileFile(tmpIntpr,evalIm,NULL);
	chdir(tmpIntpr->prev->curDir);
	tmpIntpr->glob=NULL;
	tmpIntpr->lnt=NULL;
	tmpIntpr->deftypes=NULL;
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

FklByteCodelnt* fklCompileFile(FklInterpreter* inter,int evalIm,int* exitstate)
{
	chdir(inter->curDir);
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklNewByteCode(0));
	tmp->ls=1;
	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(tmp->l,"fklCompileFile",__FILE__,__LINE__);
	tmp->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(inter->filename)->id,0,0,1);
	FklByteCode* resTp=fklNewByteCode(1);
	resTp->code[0]=FKL_RES_TP;
	char* prev=NULL;
	for(;;)
	{
		FklAstCptr* begin=NULL;
		int unexpectEOF=0;
		char* list=fklReadInPattern(inter->file,&prev,&unexpectEOF);
		if(list==NULL)continue;
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
			*exitstate=FKL_UNEXPECTEOF;
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
			FklByteCodelnt* tmpByteCodelnt=fklCompile(begin,inter->glob,inter,&state,!fklIsLambdaExpression(begin));
			if(!tmpByteCodelnt||state.state!=0)
			{
				if(state.state)
				{
					fklPrintCompileError(state.place,state.state,inter);
					if(exitstate)*exitstate=state.state;
					fklDeleteCptr(state.place);
				}
				if(tmpByteCodelnt==NULL&&!state.state&&exitstate)*exitstate=FKL_MACROEXPANDFAILED;
				fklFreeByteCodeAndLnt(tmp);
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
				FKL_INCREASE_ALL_SCP(tmpByteCodelnt->l+1,tmpByteCodelnt->ls-1,resTp->size);
			}
			fklCodeLntCat(tmp,tmpByteCodelnt);
			fklFreeByteCodelnt(tmpByteCodelnt);
			fklDeleteCptr(begin);
			free(begin);
		}
		else
		{
			free(list);
			list=NULL;
			fklFreeByteCodeAndLnt(tmp);
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

FklByteCodelnt* fklCompileImport(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
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
		FKL_ASSERT(partsOfPath,"fklCompileImport",__FILE__,__LINE__);
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
			FKL_ASSERT(pPartsOfPath,"fklCompileImport",__FILE__,__LINE__);
			partsOfPath[count-1]=tmpAtm->value.str;
			pPartsOfPath=fklNextCptr(pPartsOfPath);
		}
		uint32_t totalPathLength=0;
		uint32_t i=0;
		for(;i<count;i++)
			totalPathLength+=strlen(partsOfPath[i]);
		totalPathLength+=count+strlen(postfix);
		char* path=(char*)malloc(sizeof(char)*totalPathLength);
		FKL_ASSERT(path,"fklCompileImport",__FILE__,__LINE__);
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
			FKL_ASSERT(pathWithInterpreterPath,"fklCompileImport",__FILE__,__LINE__);
			sprintf(pathWithInterpreterPath,"%s/%s%s",InterpreterPath,t,path);
			fp=fopen(pathWithInterpreterPath,"r");
			if(!fp)
			{
				fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);
				perror(path);
				free(pathWithInterpreterPath);
				free(path);
				fklFreeMemMenager(memMenager);
				return NULL;
			}
			sprintf(pathWithInterpreterPath,"%s%s",t,path);
			free(path);
			path=pathWithInterpreterPath;
		}
		if(fklHasLoadSameFile(path,inter))
		{
			state->state=FKL_CIRCULARLOAD;
			state->place=objCptr;
			fklFreeMemMenager(memMenager);
			return NULL;
		}
		fklAddSymbolToGlob(path);
		FklInterpreter* tmpInter=fklNewIntpr(path,fp,fklNewCompEnv(NULL),inter->lnt,inter->deftypes);
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
		for(;;)
		{
			FklAstCptr* begin=NULL;
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
					FklAstCptr* libName=fklNextCptr(fklGetFirstCptr(begin));
					if(fklCptrcmp(libName,pairOfpPartsOfPath))
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
							tmpInter->deftypes=NULL;
							fklFreeIntpr(tmpInter);
							if(libPrefix)
								free(libPrefix);
							fklFreeMemMenager(memMenager);
							free(list);
							return NULL;
						}
						else
						{
							const char** exportSymbols=(const char**)malloc(sizeof(const char*)*0);
							fklPushMem(exportSymbols,free,memMenager);
							FKL_ASSERT(exportSymbols,"fklCompileImport",__FILE__,__LINE__);
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
									tmpInter->deftypes=NULL;
									fklFreeIntpr(tmpInter);
									if(libPrefix)
										free(libPrefix);
									fklFreeMemMenager(memMenager);
									free(list);
									return NULL;
								}
								FklAstAtom* pSymbol=pExportSymbols->u.atom;
								num++;
								exportSymbols=(const char**)fklReallocMem(exportSymbols,realloc(exportSymbols,sizeof(const char*)*num),memMenager);
								FKL_ASSERT(exportSymbols,"fklCompileImport",__FILE__,__LINE__);
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
								FklByteCodelnt* otherByteCodelnt=fklCompile(pBody,tmpCurEnv,tmpInter,state,1);
								if(state->state||!otherByteCodelnt)
								{
									fklPrintCompileError(state->place,state->state,tmpInter);
									fklDeleteCptr(begin);
									free(begin);
									chdir(tmpInter->prev->curDir);
									tmpInter->lnt=NULL;
									tmpInter->deftypes=NULL;
									fklFreeIntpr(tmpInter);
									state->state=0;
									state->place=NULL;
									if(libPrefix)
										free(libPrefix);
									fklFreeMemMenager(memMenager);
									free(list);
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
								FKL_ASSERT(libByteCodelnt->l,"fklCompileImport",__FILE__,__LINE__);
								libByteCodelnt->l[0]=fklNewLineNumTabNode(fklAddSymbolToGlob(tmpInter->filename)->id,0,libByteCodelnt->bc->size,objCptr->curline);
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
							FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(int32_t));
							pushProc->code[0]=FKL_PUSH_PROC;
							setU32ToByteCode(pushProc->code+sizeof(char),tmp->bc->size);
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
								tmpInter->deftypes=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								fklFreeMemMenager(memMenager);
								free(list);
								return NULL;
							}
							FklAstAtom* pSymbol=pExportSymbols->u.atom;
							FklCompDef* tmpDef=NULL;
							char* symbolWouldExport=NULL;
							if(libPrefix)
							{
								size_t len=strlen(libPrefix)+strlen(pSymbol->value.str)+1;
								symbolWouldExport=(char*)malloc(sizeof(char)*len);
								FKL_ASSERT(symbolWouldExport,"fklCompileImport",__FILE__,__LINE__);
								sprintf(symbolWouldExport,"%s%s",libPrefix,pSymbol->value.str);
							}
							else
							{
								symbolWouldExport=(char*)malloc(sizeof(char)*(strlen(pSymbol->value.str)+1));
								FKL_ASSERT(symbolWouldExport,"fklCompileImport",__FILE__,__LINE__);
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
								tmpInter->deftypes=NULL;
								fklFreeIntpr(tmpInter);
								if(libPrefix)
									free(libPrefix);
								free(symbolWouldExport);
								fklFreeMemMenager(memMenager);
								free(list);
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
						break;
					}
				}
				fklDeleteCptr(begin);
				free(begin);
			}
			else
			{
				if(libPrefix)
					free(libPrefix);
				fklFreeMemMenager(memMenager);
				return NULL;
			}
			free(list);
			list=NULL;
		}

		if(libByteCodelnt&&!libByteCodelnt->bc->size)
		{
			state->state=(tmp)?FKL_LIBUNDEFINED:0;
			state->place=(tmp)?pairOfpPartsOfPath:NULL;
			tmpInter->lnt=NULL;
			tmpInter->deftypes=NULL;
			fklFreeIntpr(tmpInter);
			if(libPrefix)
				free(libPrefix);
			fklFreeMemMenager(memMenager);
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
		fklDeleteMem(tmp,memMenager);
		fklFreeMemMenager(memMenager);
	}
	chdir(inter->curDir);
	return tmp;
}

FklByteCodelnt* fklCompileTry(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state,int evalIm)
{
	FklAstCptr* pExpression=fklNextCptr(fklGetFirstCptr(objCptr));
	FklAstCptr* pCatchExpression=NULL;
	if(!pExpression||!(pCatchExpression=fklNextCptr(pExpression)))
	{
		state->state=FKL_SYNTAXERROR;
		state->place=objCptr;
		return NULL;
	}
	FklByteCodelnt* expressionByteCodelnt=fklCompile(pExpression,curEnv,inter,state,evalIm);
	if(state->state)
		return NULL;
	else
	{
		FklByteCode* pushProc=fklNewByteCode(sizeof(char)+sizeof(uint32_t));
		FklByteCode* invoke=fklNewByteCode(1);
		invoke->code[0]=FKL_INVOKE;
		pushProc->code[0]=FKL_PUSH_PROC;
		setU32ToByteCode(pushProc->code+sizeof(char),expressionByteCodelnt->bc->size);
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
			FklByteCodelnt* tmp1=fklCompile(begin,tmpEnv,inter,state,evalIm);
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
		FklByteCode* errorTypeByteCode=fklNewByteCode(sizeof(FklSid_t)+sizeof(uint32_t));
		*(FklSid_t*)(errorTypeByteCode->code)=typeid;
		*(uint32_t*)(errorTypeByteCode->code+sizeof(FklSid_t))=t->bc->size;
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
	FklByteCode* header=fklNewByteCode(sizeof(FklSid_t)+sizeof(int32_t)+sizeof(char));
	header->code[0]=FKL_PUSH_TRY;
	setU32ToByteCode(header->code+sizeof(char),sid);
	setU32ToByteCode(header->code+sizeof(char)+sizeof(FklSid_t),numOfHandlerByteCode);
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

void fklPrintCompileError(const FklAstCptr* obj,int type,FklInterpreter* inter)
{
	fprintf(stderr,"error of compiling: ");
	switch(type)
	{
		case FKL_SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			fprintf(stderr," is undefined ");
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
		case FKL_MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			break;
		case FKL_LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)fklPrintCptr(obj,stderr);
			fprintf(stderr," undefined ");
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
			break;

	}
	if(inter!=NULL)fprintf(stderr," at line %d of file %s\n",(obj==NULL)?inter->curline:obj->curline,inter->filename);
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
	FKL_ASSERT(tmp,"fklNewDefines",__FILE__,__LINE__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	FKL_ASSERT(tmp->symbol,"fklNewDefines",__FILE__,__LINE__);
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
		FKL_ASSERT((curEnv->head=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
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
			FKL_ASSERT((curDef=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
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
		FKL_ASSERT((curEnv->head=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
		curEnv->head->next=NULL;
		curEnv->head->id=id;
		return curEnv->head;
	}
	else
	{
		FklCompDef* curDef=fklFindCompDefBySid(id,curEnv);
		if(curDef==NULL)
		{
			FKL_ASSERT((curDef=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"newComEnv",__FILE__,__LINE__);
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
	FKL_ASSERT((curEnv=(FklPreEnv*)malloc(sizeof(FklPreEnv))),"fklNewEnv",__FILE__,__LINE__);
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

FklInterpreter* fklNewIntpr(const char* filename,FILE* file,FklCompEnv* env,FklLineNumberTable* lnt,FklDefTypes* deftypes)
{
	FklInterpreter* tmp=NULL;
	FKL_ASSERT((tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter))),"fklNewIntpr",__FILE__,__LINE__)
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
	if(deftypes)
		tmp->deftypes=deftypes;
	else
		tmp->deftypes=fklNewDefTypes();
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
	if(inter->deftypes)fklFreeDefTypeTable(inter->deftypes);
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
		if(!strcmp(inter->filename,filename))
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
	FKL_ASSERT((tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter))),"fklNewTmpIntpr",__FILE__,__LINE__);
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

void fklPrintUndefinedSymbol(FklByteCodelnt* code)
{
	FklUintStack* cpcstack=fklNewUintStack(32,16);
	FklUintStack* scpstack=fklNewUintStack(32,16);
	FklPtrStack* envstack=fklNewPtrStack(32,16);
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
						i+=sizeof(FklSid_t);
						int32_t handlerNum=*(int32_t*)(bc->code+i);
						i+=sizeof(int32_t);
						int j=0;
						for(;j<handlerNum;j++)
						{
							i+=sizeof(FklSid_t);
							uint32_t pCpc=*(uint32_t*)(bc->code+i);
							i+=sizeof(uint32_t);
							i+=pCpc;
						}
					}
					break;
				case -3:
					{
						int32_t scope=0;
						FklSid_t id=0;
						memcpy(&scope,bc->code+i+sizeof(char),sizeof(int32_t));
						memcpy(&id,bc->code+i+sizeof(char)+sizeof(int32_t),sizeof(FklSid_t));
						if(scope==0)
							fklAddCompDefBySid(id,curEnv);
						else if(scope==-1)
						{
							FklCompDef* def=NULL;
							for(FklCompEnv* e=curEnv;e;e=e->prev)
							{
								def=fklFindCompDefBySid(id,e);
								if(def)
									break;
							}
							if(!def)
							{
								FklLineNumberTable table={.list=code->l,.num=code->ls};
								FklLineNumTabNode* node=fklFindLineNumTabNode(i,&table);
								fprintf(stderr,"warnnig of compiling:symbol \"%s\" is undefined at line %d of %s\n"
										,fklGetGlobSymbolWithId(id)->symbol
										,node->line
										,fklGetGlobSymbolWithId(node->fid)->symbol);
							}
						}
					}
					i+=9;
					break;
				case -2:
					fklPushUintStack(i+5,cpcstack);
					{
						uint32_t size=0;
						memcpy(&size,bc->code+i+sizeof(char),sizeof(uint32_t));
						fklPushUintStack(size,scpstack);
						FklCompEnv* nextEnv=fklNewCompEnv(curEnv);
						fklPushPtrStack(nextEnv,envstack);
					}
					i+=5+*(uint32_t*)(bc->code+i+sizeof(char));
					break;
				case -1:
					tmplen=strlen((char*)bc->code+i+1);
					i+=tmplen+2;
					break;
				case 0:
					i+=1;
					break;
				case 1:
					i+=2;
					break;
				case 4:
					{
						FklSid_t id=0;
						if(opcode==FKL_POP_ARG||opcode==FKL_POP_REST_ARG)
						{
							memcpy(&id,bc->code+i+sizeof(char),sizeof(FklSid_t));
							fklAddCompDefBySid(id,curEnv);
						}
						else if(opcode==FKL_PUSH_VAR)
						{
							memcpy(&id,bc->code+i+sizeof(char),sizeof(FklSid_t));
							FklCompDef* def=NULL;
							for(FklCompEnv* e=curEnv;e;e=e->prev)
							{
								def=fklFindCompDefBySid(id,e);
								if(def)
									break;
							}
							if(!def)
							{
								FklLineNumberTable table={.list=code->l,.num=code->ls};
								FklLineNumTabNode* node=fklFindLineNumTabNode(i,&table);
								fprintf(stderr,"warnnig of compiling:symbol \"%s\" is undefined at line %d of %s\n"
										,fklGetGlobSymbolWithId(id)->symbol
										,node->line
										,fklGetGlobSymbolWithId(node->fid)->symbol);
							}
						}
					}
					i+=5;
					break;
				case 8:
					switch(opcode)
					{
						case FKL_PUSH_F64:
							break;
						case FKL_PUSH_I64:
							break;
						case FKL_PUSH_IND_REF:
							break;
						default:break;
					}
					i+=9;
					break;
				case 12:
					i+=13;
					break;
			}
		}
		fklDestroyCompEnv(curEnv);
	}
	fklFreeUintStack(cpcstack);
	fklFreeUintStack(scpstack);
	fklFreePtrStack(envstack);
}
