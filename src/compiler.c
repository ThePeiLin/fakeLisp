//#include<fakeLisp/compiler.h>
#include<fakeLisp/syntax.h>
#include<fakeLisp/opcode.h>
//#include<fakeLisp/ast.h>
#include<fakeLisp/parser.h>
//#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
//#include<fakeLisp/reader.h>
#include<fakeLisp/mem.h>
#include<stddef.h>
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif
#include<math.h>

//static jmp_buf buf;
//static void errorCallBack(void* a)
//{
//	int* i=(int*)a;
//	longjmp(buf,i[(sizeof(void*)*2)/sizeof(int)]);
//}
//
//static int MacroPatternCmp(const FklAstCptr*,const FklAstCptr*);
//static int fmatcmp(const FklAstCptr*,const FklAstCptr*,FklPreEnv**,FklCompEnv*);
//static int fklAddDefinedMacro(FklPreMacro* macro,FklCompEnv* curEnv);
//static FklErrorState defmacro(FklAstCptr*,FklCompEnv*,FklInterpreter*);
//static FklCompEnv* createPatternCompEnv(FklString* const*,int32_t,FklCompEnv*);
//
//static FklCompEnvHashItem* createCompEnvHashItem(FklSid_t key)
//{
//	FklCompEnvHashItem* r=(FklCompEnvHashItem*)malloc(sizeof(FklCompEnvHashItem));
//	FKL_ASSERT(r);
//	r->id=key;
//	return r;
//}
//
//static size_t _compenv_hashFunc(void* key)
//{
//	FklSid_t sid=*(FklSid_t*)key;
//	return sid;
//}
//
//static void _compenv_freeItem(void* item)
//{
//	free(item);
//}
//
//static int _compenv_keyEqual(void* pkey0,void* pkey1)
//{
//	FklSid_t k0=*(FklSid_t*)pkey0;
//	FklSid_t k1=*(FklSid_t*)pkey1;
//	return k0==k1;
//}
//
//static void* _compenv_getKey(void* item)
//{
//	return &((FklCompEnvHashItem*)item)->id;
//}
//
//static FklHashTableMethodTable CompEnvHashMethodTable=
//{
//	.__hashFunc=_compenv_hashFunc,
//	.__freeItem=_compenv_freeItem,
//	.__keyEqual=_compenv_keyEqual,
//	.__getKey=_compenv_getKey,
//};
//
//static FklVMvalue* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,FklVMgc* gc)
//{
//	FklPtrStack* stack=fklCreatePtrStack(32,16);
//	FklCompEnv* tcEnv=cEnv;
//	for(;tcEnv;tcEnv=tcEnv->prev)
//		fklPushPtrStack(tcEnv,stack);
//	FklVMvalue* preEnv=NULL;
//	FklVMvalue* vEnv=NULL;
//	uint32_t bs=t->bc->size;
//	while(!fklIsPtrStackEmpty(stack))
//	{
//		FklCompEnv* curEnv=fklPopPtrStack(stack);
//		if(!preEnv)
//			vEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(preEnv,gc),gc);//fklInitGlobEnv(vEnv->u.env,gc);
//		else
//			vEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateVMenv(preEnv,gc),gc);
//		preEnv=vEnv;
//		if(curEnv->proc->bc->size)
//		{
//			FklVMnode* prevVMs=fklGetGlobVMs()->h;
//			FklVM* tmpVM=fklCreateTmpVM(NULL,NULL);
//			tmpVM->callback=errorCallBack;
//			fklCodelntCopyCat(t,curEnv->proc);
//			fklInitVMRunningResource(tmpVM,vEnv,gc,t,bs,curEnv->proc->bc->size);
//			bs+=curEnv->proc->bc->size;
//			if(setjmp(buf)==0)
//			{
//				fklRunVM(tmpVM);
//				fklJoinAllThread(prevVMs);
//				fklUninitVMRunningResource(tmpVM,prevVMs);
//			}
//			else
//			{
//				fklJoinAllThread(prevVMs);
//				fklUninitVMRunningResource(tmpVM,prevVMs);
//				return NULL;
//			}
//		}
//	}
//	fklFreePtrStack(stack);
//	return vEnv;
//}
//
//static int cmpString(const void* a,const void* b)
//{
//	return fklStringcmp(*(const FklString**)a,*(const FklString**)b);
//}
//
//FklPreMacro* fklPreMacroMatch(const FklAstCptr* objCptr,FklPreEnv** pmacroEnv,FklCompEnv* curEnv,FklCompEnv** pCEnv)
//{
//	for(;curEnv;curEnv=curEnv->prev)
//	{
//		FklPreMacro* current=curEnv->macro;
//		for(;current;current=current->next)
//			if(fmatcmp(objCptr,current->pattern,pmacroEnv,curEnv))
//			{
//				*pCEnv=curEnv;
//				return current;
//			}
//	}
//	return NULL;
//}
//
//int fklPreMacroExpand(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter)
//{
//	FklPreEnv* macroEnv=NULL;
//	FklByteCodelnt* t=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklPreMacro* tmp=fklPreMacroMatch(objCptr,&macroEnv,curEnv,&curEnv);
//	if(tmp!=NULL)
//	{
//		char* cwd=getcwd(NULL,0);
//		chdir(fklGetCwd());
//		FklVMgc* gc=fklCreateVMgc();
//		FklVMvalue* tmpGlob=genGlobEnv(tmp->macroEnv,t,gc);
//		FklVMnode* prevVMs=fklGetGlobVMs()->h;
//		FklVM* tmpVM=fklCreateTmpVM(NULL,gc);
//		tmpVM->callback=errorCallBack;
//		if(!tmpGlob)
//		{
//			fklDestroyEnv(macroEnv);
//			fklFreeVMstack(tmpVM->stack);
//			fklFreeVMframes(tmpVM->frames);
//			fklFreePtrStack(tmpVM->tstack);
//			fklFreeVMgc(tmpVM->gc);
//			fklFreeByteCodeAndLnt(t);
//			chdir(cwd);
//			free(cwd);
//			free(tmpVM);
//			return 2;
//		}
//		FklHashTable* lineHash=fklCreateLineNumHashTable();
//		FklVMvalue* macroVMenv=fklCastPreEnvToVMenv(macroEnv,tmpGlob,lineHash,tmpVM->gc);
//		fklDestroyEnv(macroEnv);
//		macroEnv=NULL;
//		uint32_t start=t->bc->size;
//		fklCodelntCopyCat(t,tmp->proc);
//		fklInitVMRunningResource(tmpVM,macroVMenv,tmpVM->gc,t,start,tmp->proc->bc->size);
//		FklAstCptr* tmpCptr=NULL;
//		chdir(cwd);
//		free(cwd);
//		if(setjmp(buf)==0)
//		{
//			fklRunVM(tmpVM);
//			tmpCptr=fklCastVMvalueToCptr(fklTopGet(tmpVM->stack),objCptr->curline,lineHash);
//			if(!tmpCptr)
//			{
//				if(inter->filename)
//					fprintf(stderr,"error of compiling: Circular reference occur in expanding macro at line %lu of %s\n",objCptr->curline,inter->filename);
//				else
//					fprintf(stderr,"error of compiling: Circular reference occur in expanding macro at line %lu\n",objCptr->curline);
//				fklFreeByteCodeAndLnt(t);
//				FklVMgc* gc=tmpVM->gc;
//				fklUninitVMRunningResource(tmpVM,prevVMs);
//				fklFreeVMgc(gc);
//				fklFreeHashTable(lineHash);
//				return 2;
//			}
//			fklReplaceCptr(objCptr,tmpCptr);
//			fklDeleteCptr(tmpCptr);
//			free(tmpCptr);
//		}
//		else
//		{
//			fklJoinAllThread(prevVMs);
//			fklFreeByteCodeAndLnt(t);
//			FklVMgc* gc=tmpVM->gc;
//			fklUninitVMRunningResource(tmpVM,prevVMs);
//			fklFreeVMgc(gc);
//			fklFreeHashTable(lineHash);
//			return 2;
//		}
//		fklFreeByteCodeAndLnt(t);
//		gc=tmpVM->gc;
//		fklUninitVMRunningResource(tmpVM,prevVMs);
//		fklFreeVMgc(gc);
//		fklFreeHashTable(lineHash);
//		return 1;
//	}
//	fklFreeByteCodeAndLnt(t);
//	return 0;
//}
//
//static int fklAddDefinedMacro(FklPreMacro* macro,FklCompEnv* curEnv)
//{
//	FklAstCptr* tmpCptr=NULL;
//	FklPreMacro* current=curEnv->macro;
//	FklAstCptr* pattern=macro->pattern;
//	while(current!=NULL&&!MacroPatternCmp(macro->pattern,current->pattern))
//		current=current->next;
//	if(current==NULL)
//	{
//		tmpCptr=&pattern->u.pair->car;
//		fklAddKeyWord(tmpCptr->u.atom->value.str,curEnv);
//		FklPreMacro* current=(FklPreMacro*)malloc(sizeof(FklPreMacro));
//		FKL_ASSERT(current);
//		current->next=curEnv->macro;
//		current->pattern=pattern;
//		current->proc=macro->proc;
//		current->macroEnv=macro->macroEnv;
//		curEnv->macro=current;
//	}
//	else
//	{
//		fklDeleteCptr(current->pattern);
//		fklDestroyCompEnv(current->macroEnv);
//		fklFreeByteCodeAndLnt(current->proc);
//		current->pattern=macro->pattern;
//		current->proc=macro->proc;
//		current->macroEnv=macro->macroEnv;
//	}
//	return 0;
//}
//
//int fklAddMacro(FklAstCptr* pattern,FklByteCodelnt* proc,FklCompEnv* curEnv)
//{
//	if(pattern->type!=FKL_TYPE_PAIR)return FKL_ERR_SYNTAXERROR;
//	FklAstCptr* tmpCptr=NULL;
//	FklPreMacro* current=curEnv->macro;
//	while(current!=NULL&&!MacroPatternCmp(pattern,current->pattern))
//		current=current->next;
//	if(current==NULL)
//	{
//		tmpCptr=&pattern->u.pair->car;
//		fklAddKeyWord(tmpCptr->u.atom->value.str,curEnv);
//		current=(FklPreMacro*)malloc(sizeof(FklPreMacro));
//		FKL_ASSERT(current);
//		current->next=curEnv->macro;
//		current->pattern=pattern;
//		current->proc=proc;
//		current->macroEnv=curEnv;
//		curEnv->refcount+=1;
//		curEnv->macro=current;
//	}
//	else
//	{
//		fklDeleteCptr(current->pattern);
//		free(current->pattern);
//		fklFreeByteCodeAndLnt(current->proc);
//		current->pattern=pattern;
//		current->proc=proc;
//	}
//	return 0;
//}
//
//int MacroPatternCmp(const FklAstCptr* first,const FklAstCptr* second)
//{
//	if(first==NULL&&second==NULL)return 0;
//	FklAstPair* firPair=NULL;
//	FklAstPair* secPair=NULL;
//	FklAstPair* tmpPair=(first->type==FKL_TYPE_PAIR)?first->u.pair:NULL;
//	for(;;)
//	{
//		if(first->type!=second->type)return 0;
//		else if(first->type==FKL_TYPE_PAIR)
//		{
//			firPair=first->u.pair;
//			secPair=second->u.pair;
//			first=&firPair->car;
//			second=&secPair->car;
//			continue;
//		}
//		else if(first->type==FKL_TYPE_ATM||first->type==FKL_TYPE_NIL)
//		{
//			if(first->type==FKL_TYPE_ATM)
//			{
//				FklAstAtom* firAtm=first->u.atom;
//				FklAstAtom* secAtm=second->u.atom;
//				if(firAtm->type!=secAtm->type)return 0;
//				if(firAtm->type==FKL_TYPE_SYM)return 0;
//				if(firAtm->type==FKL_TYPE_STR&&fklStringcmp(firAtm->value.str,secAtm->value.str))return 0;
//				else if(firAtm->type==FKL_TYPE_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
//				else if(firAtm->type==FKL_TYPE_F64&&fabs(firAtm->value.f64-secAtm->value.f64)!=0)return 0;
//				else if(firAtm->type==FKL_TYPE_CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
//			}
//			if(firPair!=NULL&&first==&firPair->car)
//			{ first=&firPair->cdr;
//				second=&secPair->cdr;
//				continue;
//			}
//		}
//		if(firPair!=NULL&&first==&firPair->car)
//		{
//			first=&firPair->cdr;
//			second=&secPair->cdr;
//			continue;
//		}
//		else if(firPair!=NULL&&first==&firPair->cdr)
//		{
//			FklAstPair* firPrev=NULL;
//			if(firPair->prev==NULL)break;
//			while(firPair->prev!=NULL&&firPair!=tmpPair)
//			{
//				firPrev=firPair;
//				firPair=firPair->prev;
//				secPair=secPair->prev;
//				if(firPrev==firPair->car.u.pair)break;
//			}
//			if(firPair!=NULL)
//			{
//				first=&firPair->cdr;
//				second=&secPair->cdr;
//			}
//			if(firPair==tmpPair&&first==&firPair->cdr)break;
//		}
//		if(firPair==NULL&&secPair==NULL)break;
//	}
//	return 1;
//}
//
//int fmatcmp(const FklAstCptr* origin,const FklAstCptr* format,FklPreEnv** pmacroEnv,FklCompEnv* curEnv)
//{
//	FklAstPair* tmpPair=(format->type==FKL_TYPE_PAIR)?format->u.pair:NULL;
//	FklAstPair* forPair=tmpPair;
//	FklAstPair* oriPair=(origin->type==FKL_TYPE_PAIR)?origin->u.pair:NULL;
//	if(tmpPair->car.type!=oriPair->car.type)
//		return 0;
//	if(oriPair->car.u.atom->type==FKL_TYPE_SYM&&fklStringcmp(tmpPair->car.u.atom->value.str,oriPair->car.u.atom->value.str))
//		return 0;
//	format=&forPair->cdr;
//	origin=&oriPair->cdr;
//	FklPreEnv* macroEnv=fklCreateEnv(NULL);
//	while(origin!=NULL)
//	{
//		if(format->type==FKL_TYPE_PAIR&&origin->type==FKL_TYPE_PAIR)
//		{
//			forPair=format->u.pair;
//			oriPair=origin->u.pair;
//			format=&forPair->car;
//			origin=&oriPair->car;
//			continue;
//		}
//		else if(format->type==FKL_TYPE_ATM)
//		{
//			FklAstAtom* tmpAtm=format->u.atom;
//			if(tmpAtm->type==FKL_TYPE_SYM)
//				fklAddDefine(tmpAtm->value.str,origin,macroEnv);
//			forPair=format->outer;
//			oriPair=origin->outer;
//			if(forPair!=NULL&&format==&forPair->car)
//			{
//				origin=&oriPair->cdr;
//				format=&forPair->cdr;
//				continue;
//			}
//		}
//		else if(origin->type!=format->type)
//		{
//			fklDestroyEnv(macroEnv);
//			macroEnv=NULL;
//			return 0;
//		}
//		if(forPair!=NULL&&format==&forPair->car)
//		{
//			origin=&oriPair->cdr;
//			format=&forPair->cdr;
//			continue;
//		}
//		else if(forPair!=NULL&&format==&forPair->cdr)
//		{
//			FklAstPair* oriPrev=NULL;
//			if(oriPair->prev==NULL)break;
//			while(oriPair->prev!=NULL&&forPair!=tmpPair)
//			{
//				oriPrev=oriPair;
//				oriPair=oriPair->prev;
//				forPair=forPair->prev;
//				if(oriPrev==oriPair->car.u.pair)break;
//			}
//			if(oriPair!=NULL)
//			{
//				origin=&oriPair->cdr;
//				format=&forPair->cdr;
//			}
//			if(forPair==tmpPair&&format==&forPair->cdr)break;
//		}
//		if(oriPair==NULL&&forPair==NULL)break;
//	}
//	*pmacroEnv=macroEnv;
//	return 1;
//}
//
//FklCompEnv* fklCreateMacroCompEnv(const FklAstCptr* objCptr,FklCompEnv* prev)
//{
//	if(objCptr==NULL)return NULL;
//	FklCompEnv* tmpEnv=fklCreateCompEnv(prev);
//	FklAstPair* tmpPair=(objCptr->type==FKL_TYPE_PAIR)?objCptr->u.pair:NULL;
//	FklAstPair* objPair=tmpPair;
//	while(objCptr!=NULL)
//	{
//		if(objCptr->type==FKL_TYPE_PAIR)
//		{
//			if(objPair!=NULL&&objCptr==&objPair->cdr)
//			{
//				objPair=objPair->cdr.u.pair;
//				objCptr=&objPair->car;
//			}
//			else
//			{
//				objPair=objCptr->u.pair;
//				objCptr=&objPair->car;
//				continue;
//			}
//		}
//		else if(objCptr->type==FKL_TYPE_ATM||objCptr->type==FKL_TYPE_NIL)
//		{
//			if(objCptr->type!=FKL_TYPE_NIL)
//			{
//				FklAstAtom* tmpAtm=objCptr->u.atom;
//				if(tmpAtm->type==FKL_TYPE_SYM)
//				{
//					const FklString* tmpStr=tmpAtm->value.str;
//					fklAddCompDef(tmpStr,tmpEnv);
//				}
//			}
//			if(objPair!=NULL&&objCptr==&objPair->car)
//			{
//				objCptr=&objPair->cdr;
//				continue;
//			}
//		}
//		if(objPair!=NULL&&objCptr==&objPair->cdr)
//		{
//			FklAstPair* prev=NULL;
//			if(objPair->prev==NULL)break;
//			while(objPair->prev!=NULL&&objPair!=tmpPair)
//			{
//				prev=objPair;
//				objPair=objPair->prev;
//				if(prev==objPair->car.u.pair)break;
//			}
//			if(objPair!=NULL)objCptr=&objPair->cdr;
//			if(objPair==tmpPair&&(prev==objPair->cdr.u.pair||prev==NULL))break;
//		}
//		if(objPair==NULL)break;
//	}
//	return tmpEnv;
//}
//
//int fklRetree(FklAstCptr** fir,FklAstCptr* sec)
//{
//	while(*fir!=sec)
//	{
//		FklAstCptr* preCptr=((*fir)->outer->prev==NULL)?sec:&(*fir)->outer->prev->car;
//		fklReplaceCptr(preCptr,*fir);
//		*fir=preCptr;
//		if(preCptr->outer!=NULL&&preCptr->outer->cdr.type!=FKL_TYPE_NIL)return 0;
//	}
//	return 1;
//}
//
//void fklInitGlobKeyWord(FklCompEnv* glob)
//{
//	fklAddKeyWordCstr("defmacro",glob);
//	fklAddKeyWordCstr("define",glob);
//	fklAddKeyWordCstr("setq",glob);
//	fklAddKeyWordCstr("quote",glob);
//	fklAddKeyWordCstr("cond",glob);
//	fklAddKeyWordCstr("and",glob);
//	fklAddKeyWordCstr("or",glob);
//	fklAddKeyWordCstr("lambda",glob);
////	fklAddKeyWordCstr("setf",glob);
//	fklAddKeyWordCstr("load",glob);
//	fklAddKeyWordCstr("begin",glob);
//	fklAddKeyWordCstr("unquote",glob);
//	fklAddKeyWordCstr("qsquote",glob);
//	fklAddKeyWordCstr("unqtesp",glob);
//	fklAddKeyWordCstr("import",glob);
//	fklAddKeyWordCstr("library",glob);
//	fklAddKeyWordCstr("export",glob);
//	fklAddKeyWordCstr("try",glob);
//	fklAddKeyWordCstr("catch",glob);
//	fklAddKeyWordCstr("macroexpand",glob);
//}
//
//void fklUninitPreprocess()
//{
//	fklFreeAllStringPattern();
//}
//
//FklAstCptr** dealArg(FklAstCptr* argCptr,int num)
//{
//	FklAstCptr** args=(FklAstCptr**)malloc(num*sizeof(FklAstCptr*));
//	FKL_ASSERT(args);
//	int i=0;
//	for(;i<num;i++,argCptr=fklNextCptr(argCptr))
//		args[i]=argCptr;
//	return args;
//}
//
//FklErrorState defmacro(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter)
//{
//	FklErrorState state={0,NULL};
//	FklAstCptr* fir=fklGetFirstCptr(objCptr);
//	FklAstCptr* argCptr=fklNextCptr(fir);
//	FklAstCptr** args=dealArg(argCptr,2);
//	if(args[0]->type!=FKL_TYPE_PAIR)
//	{
//		if(args[0]->type!=FKL_TYPE_NIL)
//		{
//			FklAstAtom* tmpAtom=args[0]->u.atom;
//			if(tmpAtom->type!=FKL_TYPE_STR)
//			{
//				fklPrintCompileError(args[0],FKL_ERR_SYNTAXERROR,inter);
//				free(args);
//				return state;
//			}
//			uint32_t num=0;
//			FklString** parts=fklSplitPattern(tmpAtom->value.str,&num);
//			int isReDef=fklIsReDefStringPattern(parts,num);
//			if(!isReDef&&fklIsInValidStringPattern(parts,num))
//			{
//				fklPrintCompileError(args[0],FKL_ERR_INVALIDEXPR,inter);
//				free(args);
//				return state;
//			}
//			if(isReDef)
//				fklAddReDefStringPattern(parts,num,args[1],inter);
//			else
//				fklAddStringPattern(parts,num,args[1],inter);
////			fklFreeStringArray(parts,num);
//			free(args);
//		}
//		else
//		{
//			state.state=FKL_ERR_SYNTAXERROR;
//			state.place=args[0];
//			free(args);
//			return state;
//		}
//	}
//	else
//	{
//		FklAstCptr* head=fklGetFirstCptr(args[0]);
//		if(head->type!=FKL_TYPE_ATM||head->u.atom->type!=FKL_TYPE_SYM)
//		{
//			state.state=FKL_ERR_INVALID_MACRO_PATTERN;
//			state.place=args[0];
//			free(args);
//			return state;
//		}
//		FklAstCptr* pattern=fklCreateCptr(0,NULL);
//		fklReplaceCptr(pattern,args[0]);
//		head=fklGetFirstCptr(pattern);
//		if(curEnv->prev
//				&&curEnv->prev->exp
//				&&curEnv->prev->prefix)
//		{
//			FklString* t=fklCopyString(curEnv->prev->prefix);
//			fklStringCat(&t,head->u.atom->value.str);
//			free(head->u.atom->value.str);
//			head->u.atom->value.str=t;
//		}
//		FklAstCptr* express=args[1];
//		FklInterpreter* tmpInter=fklCreateTmpIntpr(NULL,NULL);
//		tmpInter->filename=inter->filename;
//		tmpInter->curline=inter->curline;
//		tmpInter->glob=curEnv;
//		tmpInter->curDir=inter->curDir;
//		tmpInter->prev=NULL;
//		tmpInter->lnt=NULL;
//		FklCompEnv* tmpCompEnv=fklCreateMacroCompEnv(&pattern->u.pair->cdr,tmpInter->glob);
//		FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
//		if(!state.state)
//		{
//			fklAddMacro(pattern,tmpByteCodelnt,curEnv);
//			free(args);
//		}
//		else
//		{
//			if(tmpByteCodelnt)
//				fklFreeByteCodeAndLnt(tmpByteCodelnt);
//			fklDeleteCptr(pattern);
//			free(pattern);
//			free(args);
//		}
//		fklDestroyCompEnv(tmpCompEnv);
//		free(tmpInter);
//	}
//	return state;
//}
//
//FklStringMatchPattern* fklAddStringPattern(FklString** parts,uint32_t num,FklAstCptr* express,FklInterpreter* inter)
//{
//	FklStringMatchPattern* tmp=NULL;
//	FklErrorState state={0,NULL};
//	FklInterpreter* tmpInter=fklCreateTmpIntpr(NULL,NULL);
//	tmpInter->filename=inter->filename;
//	tmpInter->curline=inter->curline;
//	tmpInter->glob=inter->glob;
//	tmpInter->curDir=inter->curDir;
//	tmpInter->prev=NULL;
//	tmpInter->lnt=NULL;
//	FklCompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
//	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
//	if(!state.state)
//		tmp=fklCreateStringMatchPattern(num,parts,tmpByteCodelnt);
//	else
//	{
//		if(tmpByteCodelnt)
//			fklFreeByteCodeAndLnt(tmpByteCodelnt);
//		fklPrintCompileError(state.place,state.state,inter);
//		state.place=NULL;
//		state.state=0;
//	}
//	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
//	free(tmpInter);
//	return tmp;
//}
//
//FklStringMatchPattern* fklAddReDefStringPattern(FklString** parts,uint32_t num,FklAstCptr* express,FklInterpreter* inter)
//{
//	FklStringMatchPattern* tmp=fklFindStringPattern(parts[0]);
//	FklErrorState state={0,NULL};
//	FklInterpreter* tmpInter=fklCreateTmpIntpr(NULL,NULL);
//	tmpInter->filename=inter->filename;
//	tmpInter->curline=inter->curline;
//	tmpInter->glob=inter->glob;
//	tmpInter->curDir=inter->curDir;
//	tmpInter->prev=NULL;
//	tmpInter->lnt=NULL;
//	FklCompEnv* tmpCompEnv=createPatternCompEnv(parts,num,inter->glob);
//	FklByteCodelnt* tmpByteCodelnt=fklCompile(express,tmpCompEnv,tmpInter,&state);
//	if(!state.state)
//	{
//		fklFreeStringArray(tmp->parts,num);
//		fklFreeByteCodeAndLnt(tmp->proc);
//		tmp->parts=parts;
//		tmp->proc=tmpByteCodelnt;
//	}
//	else
//	{
//		if(tmpByteCodelnt)
//			fklFreeByteCodeAndLnt(tmpByteCodelnt);
//		fklPrintCompileError(state.place,state.state,inter);
//		state.place=NULL;
//		state.state=0;
//	}
//	fklFreeAllMacroThenDestroyCompEnv(tmpCompEnv);
//	free(tmpInter);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompile(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	for(;;)
//	{
//		if(fklIsLoadExpression(objCptr))return fklCompileLoad(objCptr,curEnv,inter,state);
//		if(fklIsConst(objCptr))return fklCompileConst(objCptr,curEnv,inter,state);
//		if(fklIsUnquoteExpression(objCptr))return fklCompileUnquote(objCptr,curEnv,inter,state);
//		if(fklIsQsquoteExpression(objCptr))return fklCompileQsquote(objCptr,curEnv,inter,state);
//		if(fklIsSymbol(objCptr))return fklCompileSym(objCptr,curEnv,inter,state);
//		if(fklIsDefExpression(objCptr))return fklCompileDef(objCptr,curEnv,inter,state);
//		if(fklIsSetqExpression(objCptr))return fklCompileSetq(objCptr,curEnv,inter,state);
//		if(fklIsCondExpression(objCptr))return fklCompileCond(objCptr,curEnv,inter,state);
//		if(fklIsAndExpression(objCptr))return fklCompileAnd(objCptr,curEnv,inter,state);
//		if(fklIsOrExpression(objCptr))return fklCompileOr(objCptr,curEnv,inter,state);
//		if(fklIsLambdaExpression(objCptr))return fklCompileLambda(objCptr,curEnv,inter,state);
//		if(fklIsBeginExpression(objCptr)) return fklCompileBegin(objCptr,curEnv,inter,state);
//		if(fklIsImportExpression(objCptr))return fklCompileImport(objCptr,curEnv,inter,state);
//		if(fklIsTryExpression(objCptr))return fklCompileTry(objCptr,curEnv,inter,state);
//		if(fklIsLibraryExpression(objCptr))return fklCompileLibrary(objCptr,curEnv,inter,state);
//		if(fklIsCatchExpression(objCptr)||fklIsUnqtespExpression(objCptr)||fklIsExportExpression(objCptr))
//		{
//			state->state=FKL_ERR_INVALIDEXPR;
//			state->place=objCptr;
//			return NULL;
//		}
//		if(fklIsDefmacroExpression(objCptr))
//		{
//			FklErrorState t=defmacro(objCptr,curEnv,inter);
//			if(t.state)
//			{
//				state->state=t.state;
//				state->place=t.place;
//				return NULL;
//			}
//			FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//			tmp->ls=0;
//			tmp->l=NULL;
//			return tmp;
//		}
//		else if(fklIsMacroExpandExpression(objCptr))
//		{
//			FklAstCptr* nextCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//			int i=fklPreMacroExpand(nextCptr,curEnv,inter);
//			if(i==1)
//			{
//				FklByteCode* code=fklCompileQuote(nextCptr);
//				FklByteCodelnt* retval=fklCreateByteCodelnt(code);
//				retval->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//				FKL_ASSERT(retval->l);
//				retval->ls=1;
//				retval->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,code->size,objCptr->curline);
//				return retval;
//			}
//		}
//		int i=fklPreMacroExpand(objCptr,curEnv,inter);
//		if(i==1)
//			continue;
//		else if(i==2)
//		{
//			state->state=FKL_ERR_MACROEXPANDFAILED;
//			state->place=objCptr;
//			return NULL;
//		}
//		else if(!fklIsValid(objCptr)||fklHasKeyWord(objCptr,curEnv))
//		{
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=objCptr;
//			return NULL;
//		}
//		else if(fklIsFuncCall(objCptr,curEnv))return fklCompileFuncCall(objCptr,curEnv,inter,state);
//	}
//}
//
//FklCompEnv* createPatternCompEnv(FklString* const* parts,int32_t num,FklCompEnv* prev)
//{
//	if(parts==NULL)return NULL;
//	FklCompEnv* tmpEnv=fklCreateCompEnv(prev);
//	int32_t i=0;
//	for(;i<num;i++)
//		if(fklIsVar(parts[i]))
//		{
//			FklString* varName=fklGetVarName(parts[i]);
//			fklAddCompDef(varName,tmpEnv);
//			free(varName);
//		}
//	return tmpEnv;
//}
//
//FklByteCode* fklCompileAtom(FklAstCptr* objCptr)
//{
//	FklAstAtom* tmpAtm=objCptr->u.atom;
//	FklByteCode* tmp=NULL;
//	switch(tmpAtm->type)
//	{
//		case FKL_TYPE_SYM:
//			tmp=fklCreatePushSidByteCode(fklAddSymbolToGlob(tmpAtm->value.str)->id);
//			break;
//		case FKL_TYPE_I32:
//			tmp=fklCreatePushI32ByteCode(tmpAtm->value.i32);
//			break;
//		case FKL_TYPE_I64:
//			tmp=fklCreatePushI64ByteCode(tmpAtm->value.i64);
//			break;
//		case FKL_TYPE_F64:
//			tmp=fklCreatePushF64ByteCode(tmpAtm->value.f64);
//			break;
//		case FKL_TYPE_CHR:
//			tmp=fklCreatePushCharByteCode(tmpAtm->value.chr);
//			break;
//		case FKL_TYPE_STR:
//			tmp=fklCreatePushStrByteCode(tmpAtm->value.str);
//			break;
//		case FKL_TYPE_BYTEVECTOR:
//			tmp=fklCreatePushBvecByteCode(tmpAtm->value.bvec);
//			break;
//		case FKL_TYPE_BIG_INT:
//			tmp=fklCreatePushBigIntByteCode(&tmpAtm->value.bigInt);
//			break;
//		default:
//			break;
//	}
//	return tmp;
//}
//
//FklByteCode* fklCompileNil()
//{
//	FklByteCode* tmp=fklCreateByteCode(1);
//	tmp->code[0]=FKL_OP_PUSH_NIL;
//	return tmp;
//}
//
//static FklByteCode* innerCompileConst(FklAstCptr* objCptr)
//{
//	switch(objCptr->type)
//	{
//		case FKL_TYPE_PAIR:
//			return fklCompilePair(objCptr);
//			break;
//		case FKL_TYPE_ATM:
//			if(objCptr->u.atom->type==FKL_TYPE_SYM)
//			{
//				FklByteCode* tmp=fklCreateByteCode(sizeof(char)+sizeof(FklSid_t));
//				const FklString* sym=objCptr->u.atom->value.str;
//				FklSymTabNode* node=fklAddSymbolToGlob(sym);
//				tmp->code[0]=FKL_OP_PUSH_SYM;
//				fklSetSidToByteCode(tmp->code+sizeof(char),node->id);
//				return tmp;
//			}
//			else
//			{
//				switch(objCptr->u.atom->type)
//				{
//					case FKL_TYPE_VECTOR:
//						return fklCompileVector(objCptr);
//						break;
//					case FKL_TYPE_BOX:
//						return fklCompileBox(objCptr);
//						break;
//					case FKL_TYPE_HASHTABLE:
//						return fklCompileHashtable(objCptr);
//						break;
//					default:
//						return fklCompileAtom(objCptr);
//						break;
//				}
//			}
//			break;
//		case FKL_TYPE_NIL:
//			return fklCompileNil();
//			break;
//		default:
//			break;
//	}
//	return NULL;
//}
//
//FklByteCode* fklCompileVector(FklAstCptr* objCptr)
//{
//	FklAstVector* vec=&objCptr->u.atom->value.vec;
//	FklByteCode* pushVector=fklCreateByteCode(sizeof(char)+sizeof(uint64_t));
//	pushVector->code[0]=FKL_OP_PUSH_VECTOR;
//	fklSetU64ToByteCode(pushVector->code+sizeof(char),vec->size);
//	FklByteCode* retval=fklCreateByteCode(0);
//	for(size_t i=0;i<vec->size;i++)
//	{
//		FklByteCode* tmp=innerCompileConst(&vec->base[i]);
//		fklCodeCat(retval,tmp);
//		fklFreeByteCode(tmp);
//	}
//	fklCodeCat(retval,pushVector);
//	fklFreeByteCode(pushVector);
//	return retval;
//}
//
//FklByteCode* fklCompileHashtable(FklAstCptr* objCptr)
//{
//	FklByteCode* pushHash=fklCreateByteCode(sizeof(char)+sizeof(uint64_t));
//	FklAstHashTable* hash=&objCptr->u.atom->value.hash;
//	FklByteCode* retval=fklCreateByteCode(0);
//	pushHash->code[0]=FKL_OP_PUSH_HASHTABLE_EQ+hash->type;
//	for(FklAstCptr* first=fklGetFirstCptr(&hash->items);first;first=fklNextCptr(first))
//	{
//		FklByteCode* key=innerCompileConst(fklGetASTPairCar(first));
//		FklByteCode* value=innerCompileConst(fklGetASTPairCdr(first));
//		fklCodeCat(retval,key);
//		fklCodeCat(retval,value);
//		fklFreeByteCode(key);
//		fklFreeByteCode(value);
//	}
//	fklSetU64ToByteCode(pushHash->code+sizeof(char),hash->num);
//	fklCodeCat(retval,pushHash);
//	fklFreeByteCode(pushHash);
//	return retval;
//}
//
//FklByteCode* fklCompileBox(FklAstCptr* objCptr)
//{
//	FklAstCptr* box=&objCptr->u.atom->value.box;
//	FklByteCode* pushBox=fklCreateByteCode(sizeof(char));
//	pushBox->code[0]=FKL_OP_PUSH_BOX;
//	FklByteCode* tmp=innerCompileConst(box);
//	fklReCodeCat(tmp,pushBox);
//	fklFreeByteCode(tmp);
//	return pushBox;
//}
//
//FklByteCode* fklCompilePair(FklAstCptr* objCptr)
//{
//	FklByteCode* tmp=fklCreateByteCode(0);
//	FklAstPair* objPair=objCptr->u.pair;
//	FklAstPair* tmpPair=objPair;
//	FklByteCode* pushPair=fklCreateByteCode(1);
//	pushPair->code[0]=FKL_OP_PUSH_PAIR;
//	while(objCptr!=NULL)
//	{
//		if(objCptr->type==FKL_TYPE_PAIR)
//		{
//			fklReCodeCat(pushPair,tmp);
//			objPair=objCptr->u.pair;
//			objCptr=&objPair->cdr;
//			continue;
//		}
//		else if(objCptr->type==FKL_TYPE_ATM||objCptr->type==FKL_TYPE_NIL)
//		{
//			FklByteCode* tmp1=(objCptr->type==FKL_TYPE_ATM)?fklCompileAtom(objCptr):fklCompileNil();
//			fklReCodeCat(tmp1,tmp);
//			fklFreeByteCode(tmp1);
//			if(objPair!=NULL&&objCptr==&objPair->cdr)
//			{
//				objCptr=&objPair->car;
//				continue;
//			}
//		}
//		if(objPair!=NULL&&objCptr==&objPair->car)
//		{
//			FklAstPair* prev=NULL;
//			if(objPair->prev==NULL)break;
//			while(objPair->prev!=NULL&&objPair!=tmpPair)
//			{
//				prev=objPair;
//				objPair=objPair->prev;
//				if(prev==objPair->cdr.u.pair)break;
//			}
//			if(objPair!=NULL)objCptr=&objPair->car;
//			if(objPair==tmpPair&&(prev==objPair->car.u.pair||prev==NULL))break;
//		}
//		if(objPair==NULL)break;
//	}
//	fklFreeByteCode(pushPair);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileUnquoteVector(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstVector* vec=&objCptr->u.atom->value.vec;
//	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
//	retval->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//	FKL_ASSERT(retval->l);
//	retval->ls=1;
//	retval->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,retval->bc->size,objCptr->curline);
//	for(size_t i=0;i<vec->size;i++)
//	{
//		if(fklIsUnqtespExpression(&vec->base[i]))
//		{
//			fklFreeByteCodeAndLnt(retval);
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=objCptr;
//			return NULL;
//		}
//		else if(fklIsUnquoteExpression(&vec->base[i]))
//		{
//			FklByteCodelnt* tmp=fklCompileUnquote(&vec->base[i],curEnv,inter,state);
//			if(state->state)
//			{
//				fklFreeByteCodeAndLnt(retval);
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=objCptr;
//				return NULL;
//			}
//			fklCodelntCopyCat(retval,tmp);
//			fklFreeByteCodeAndLnt(tmp);
//		}
//		else
//		{
//			FklByteCode* tmp=innerCompileConst(&vec->base[i]);
//			fklCodeCat(retval->bc,tmp);
//			retval->l[retval->ls-1]->cpc+=tmp->size;
//			fklFreeByteCode(tmp);
//		}
//	}
//	FklByteCode* pushVector=fklCreateByteCode(sizeof(char)+sizeof(uint64_t));
//	pushVector->code[0]=FKL_OP_PUSH_VECTOR;
//	fklSetU64ToByteCode(pushVector->code+sizeof(char),vec->size);
//	fklCodeCat(retval->bc,pushVector);
//	retval->l[retval->ls-1]->cpc+=pushVector->size;
//	fklFreeByteCode(pushVector);
//	return retval;
//}
//
//FklByteCodelnt* fklCompileQsquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//	FklAstCptr* top=objCptr;
//	if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type!=FKL_TYPE_VECTOR&&objCptr->u.atom->type!=FKL_TYPE_BOX)
//		return fklCompileConst(objCptr,curEnv,inter,state);
//	else if(objCptr->type==FKL_TYPE_ATM)
//	{
//		FklAstAtom* tAtom=objCptr->u.atom;
//		if(tAtom->type==FKL_TYPE_VECTOR)
//			return fklCompileUnquoteVector(objCptr,curEnv,inter,state);
//		else
//		{
//			FklAstCptr* box=&objCptr->u.atom->value.box;
//			FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
//			retval->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//			FKL_ASSERT(retval->l);
//			retval->ls=1;
//			retval->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,retval->bc->size,objCptr->curline);
//			if(fklIsUnquoteExpression(box))
//			{
//				FklByteCodelnt* tmp=fklCompileUnquote(box,curEnv,inter,state);
//				if(state->state)
//				{
//					fklFreeByteCodeAndLnt(retval);
//					state->state=FKL_ERR_SYNTAXERROR;
//					state->place=objCptr;
//					return NULL;
//				}
//				fklCodelntCopyCat(retval,tmp);
//				fklFreeByteCodeAndLnt(tmp);
//			}
//			else
//			{
//				FklByteCode* tmp=innerCompileConst(box);
//				fklCodeCat(retval->bc,tmp);
//				retval->l[retval->ls-1]->cpc+=tmp->size;
//				fklFreeByteCode(tmp);
//			}
//			FklByteCode* pushBox=fklCreateByteCode(sizeof(char));
//			pushBox->code[0]=FKL_OP_PUSH_BOX;
//			fklCodeCat(retval->bc,pushBox);
//			retval->l[retval->ls-1]->cpc+=pushBox->size;
//			fklFreeByteCode(pushBox);
//			return retval;
//		}
//	}
//	else if(fklIsUnquoteExpression(objCptr))
//		return fklCompileUnquote(objCptr,curEnv,inter,state);
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklAstPair* objPair=objCptr->u.pair;
//	FklAstPair* tmpPair=objPair;
//	FklByteCode* appd=fklCreateByteCode(1);
//	FklByteCode* pushPair=fklCreateByteCode(1);
//	pushPair->code[0]=FKL_OP_PUSH_PAIR;
//	appd->code[0]=FKL_OP_LIST_APPEND;
//	while(objCptr!=NULL)
//	{
//		if(fklIsUnquoteExpression(objCptr))
//		{
//			FklByteCodelnt* tmp1=fklCompileUnquote(objCptr,curEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(tmp);
//				fklFreeByteCode(appd);
//				fklFreeByteCode(pushPair);
//				return NULL;
//			}
//			fklReCodeLntCat(tmp1,tmp);
//			fklFreeByteCodelnt(tmp1);
//			if(objPair!=NULL&&objCptr==&objPair->cdr)
//			{
//				objCptr=&objPair->car;
//				continue;
//			}
//		}
//		else if(fklIsUnqtespExpression(objCptr))
//		{
//			if(objCptr==&objPair->cdr||objCptr==top)
//			{
//				fklFreeByteCode(pushPair);
//				fklFreeByteCode(appd);
//				fklFreeByteCodeAndLnt(tmp);
//				state->state=FKL_ERR_INVALIDEXPR;
//				state->place=objCptr;
//				return NULL;
//			}
//			FklByteCodelnt* tmp1=fklCompile(fklNextCptr(fklGetFirstCptr(objCptr)),curEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(tmp);
//				fklFreeByteCode(appd);
//				fklFreeByteCode(pushPair);
//				return NULL;
//			}
//			fklReCodeLntCat(tmp1,tmp);
//			fklFreeByteCodelnt(tmp1);
//			if(objPair!=NULL&&objCptr==&objPair->cdr)
//			{
//				objCptr=&objPair->car;
//				continue;
//			}
//		}
//		else if(objCptr->type==FKL_TYPE_PAIR)
//		{
//			FklByteCode* cons=fklIsUnqtespExpression(fklGetFirstCptr(objCptr))?appd:pushPair;
//			fklReCodeCat(cons,tmp->bc);
//			if(!tmp->l)
//			{
//				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//				FKL_ASSERT(tmp->l);
//				tmp->ls=1;
//				tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,cons->size,objCptr->curline);
//			}
//			else
//			{
//				tmp->l[0]->cpc+=cons->size;
//				FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,cons->size);
//			}
//			objPair=objCptr->u.pair;
//			objCptr=&objPair->cdr;
//			continue;
//		}
//		else if(objCptr->type==FKL_TYPE_ATM||objCptr->type==FKL_TYPE_NIL)
//		{
//			FklByteCodelnt* tmp1=NULL;
//			if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type==FKL_TYPE_VECTOR)
//			{
//				tmp1=fklCompileUnquoteVector(objCptr,curEnv,inter,state);
//				if(state->state!=0)
//				{
//					fklFreeByteCodeAndLnt(tmp);
//					fklFreeByteCode(appd);
//					fklFreeByteCode(pushPair);
//					return NULL;
//				}
//			}
//			else
//				tmp1=fklCompileConst(objCptr,curEnv,inter,state);
//			fklReCodeLntCat(tmp1,tmp);
//			fklFreeByteCodelnt(tmp1);
//			if(objPair!=NULL&&objCptr==&objPair->cdr)
//			{
//				objCptr=&objPair->car;
//				continue;
//			}
//		}
//		if(objPair!=NULL&&objCptr==&objPair->car)
//		{
//			FklAstPair* prev=NULL;
//			if(objPair->prev==NULL)break;
//			while(objPair->prev!=NULL&&objPair!=tmpPair)
//			{
//				prev=objPair;
//				objPair=objPair->prev;
//				if(prev==objPair->cdr.u.pair)break;
//			}
//			if(objPair!=NULL)objCptr=&objPair->car;
//			if(objPair==tmpPair&&(prev==objPair->car.u.pair||prev==NULL))break;
//		}
//		if(objPair==NULL)break;
//	}
//	fklFreeByteCode(pushPair);
//	fklFreeByteCode(appd);
//	return tmp;
//}
//
//FklByteCode* fklCompileQuote(FklAstCptr* objCptr)
//{
//	switch(objCptr->type)
//	{
//		case FKL_TYPE_PAIR:
//			return fklCompilePair(objCptr);
//			break;
//		case FKL_TYPE_ATM:
//			if(objCptr->u.atom->type==FKL_TYPE_SYM)
//			{
//				FklByteCode* tmp=fklCreateByteCode(sizeof(char)+sizeof(FklSid_t));
//				FklString* sym=objCptr->u.atom->value.str;
//				FklSymTabNode* node=fklAddSymbolToGlob(sym);
//				tmp->code[0]=FKL_OP_PUSH_SYM;
//				fklSetSidToByteCode(tmp->code+sizeof(char),node->id);
//				return tmp;
//			}
//			else if(objCptr->u.atom->type!=FKL_TYPE_VECTOR&&objCptr->u.atom->type!=FKL_TYPE_BOX)
//				return fklCompileAtom(objCptr);
//			else if(objCptr->u.atom->type==FKL_TYPE_VECTOR)
//				return fklCompileVector(objCptr);
//			else
//				return fklCompileBox(objCptr);
//			break;
//		case FKL_TYPE_NIL:
//			return fklCompileNil();
//			break;
//		default:
//			break;
//	}
//	return NULL;
//}
//
//FklByteCodelnt* fklCompileUnquote(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//	return fklCompile(objCptr,curEnv,inter,state);
//}
//
//FklByteCodelnt* fklCompileConst(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	int32_t line=objCptr->curline;
//	FklByteCode* tmp=NULL;
//	if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type!=FKL_TYPE_VECTOR)tmp=fklCompileAtom(objCptr);
//	if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type==FKL_TYPE_VECTOR)tmp=fklCompileVector(objCptr);
//	else if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type==FKL_TYPE_BOX)tmp=fklCompileBox(objCptr);
//	else if(objCptr->type==FKL_TYPE_ATM&&objCptr->u.atom->type==FKL_TYPE_HASHTABLE)tmp=fklCompileHashtable(objCptr);
//	else if(fklIsNil(objCptr))tmp=fklCompileNil();
//	else if(fklIsQuoteExpression(objCptr))tmp=fklCompileQuote(fklNextCptr(fklGetFirstCptr(objCptr)));
//	if(!tmp)
//	{
//		state->state=FKL_ERR_INVALIDEXPR;
//		state->place=objCptr;
//		return NULL;
//	}
//	FklByteCodelnt* t=fklCreateByteCodelnt(tmp);
//	FklLineNumTabNode* n=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->size,line);
//	t->ls=1;
//	t->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//	FKL_ASSERT(t->l);
//	t->l[0]=n;
//	return t;
//}
//
//FklByteCodelnt* fklCompileFuncCall(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstCptr* headoflist=NULL;
//	FklAstPair* tmpPair=objCptr->u.pair;
//	uint64_t line=objCptr->curline;
//	FklByteCodelnt* tmp1=NULL;
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklByteCode* setBp=fklCreateByteCode(1);
//	FklByteCode* call=fklCreateByteCode(1);
//	setBp->code[0]=FKL_OP_SET_BP;
//	call->code[0]=FKL_OP_CALL;
//	for(;;)
//	{
//		if(objCptr==NULL)
//		{
//			fklCodeCat(tmp->bc,call);
//			if(!tmp->l)
//			{
//				tmp->ls=1;
//				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//				FKL_ASSERT(tmp->l);
//				tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,call->size,line);
//			}
//			else
//				tmp->l[tmp->ls-1]->cpc+=call->size;
//			if(headoflist->outer==tmpPair)break;
//			objCptr=fklPrevCptr(&headoflist->outer->prev->car);
//			for(headoflist=&headoflist->outer->prev->car;fklPrevCptr(headoflist)!=NULL;headoflist=fklPrevCptr(headoflist));
//			continue;
//		}
//		else if(fklIsFuncCall(objCptr,curEnv))
//		{
//			fklCodeCat(tmp->bc,setBp);
//			if(!tmp->l)
//			{
//				tmp->ls=1;
//				tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//				FKL_ASSERT(tmp->l);
//				tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,setBp->size,line);
//			}
//			else
//				tmp->l[tmp->ls-1]->cpc+=setBp->size;
//			headoflist=&objCptr->u.pair->car;
//			for(objCptr=headoflist;fklNextCptr(objCptr)!=NULL;objCptr=fklNextCptr(objCptr));
//		}
//		else
//		{
//			tmp1=fklCompile(objCptr,curEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(tmp);
//				tmp=NULL;
//				break;
//			}
//			fklCodeLntCat(tmp,tmp1);
//			fklFreeByteCodelnt(tmp1);
//			objCptr=fklPrevCptr(objCptr);
//		}
//	}
//	fklFreeByteCode(setBp);
//	fklFreeByteCode(call);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileDef(FklAstCptr* tir,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstPair* tmpPair=tir->u.pair;
//	FklAstCptr* fir=NULL;
//	FklAstCptr* sec=NULL;
//	FklAstCptr* objCptr=NULL;
//	FklByteCodelnt* tmp1=NULL;
//	FklByteCode* pushTop=fklCreateByteCode(sizeof(char));
//	FklByteCode* popVar=fklCreateByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
//	popVar->code[0]=FKL_OP_POP_VAR;
//	pushTop->code[0]=FKL_OP_PUSH_TOP;
//	while(fklIsDefExpression(tir))
//	{
//		fir=&tir->u.pair->car;
//		sec=fklNextCptr(fir);
//		tir=fklNextCptr(sec);
//	}
//	objCptr=tir;
//	tmp1=fklCompile(objCptr,curEnv,inter,state);
//	if(state->state!=0)
//	{
//		fklFreeByteCode(popVar);
//		fklFreeByteCode(pushTop);
//		return NULL;
//	}
//	for(;;)
//	{
//		FklCompEnvHashItem* tmpDef=NULL;
//		FklAstAtom* tmpAtm=sec->u.atom;
//		if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
//		{
//			if(curEnv->prev->prefix)
//			{
//				FklString* symbolWithPrefix=fklStringAppend(curEnv->prev->prefix,tmpAtm->value.str);
//				tmpDef=fklAddCompDef(symbolWithPrefix,curEnv->prev);
//				free(symbolWithPrefix);
//			}
//			else
//				tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv->prev);
//			fklSetU32ToByteCode(popVar->code+sizeof(char),2);
//		}
//		else
//		{
//			fklSetU32ToByteCode(popVar->code+sizeof(char),1);
//			tmpDef=fklAddCompDef(tmpAtm->value.str,curEnv);
//		}
//		fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
//		FklByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
//		fklCodeCat(tmp1->bc,pushTop);
//		fklCodeCat(tmp1->bc,popVar);
//		tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
//		if(fklIsLambdaExpression(objCptr)||fklIsConst(objCptr))
//		{
//			if(curEnv->prev&&fklIsSymbolShouldBeExport(tmpAtm->value.str,curEnv->prev->exp,curEnv->prev->n))
//			{
//				FklByteCode* popVar=fklCreateByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
//				popVar->code[0]=FKL_OP_POP_VAR;
//				fklSetU32ToByteCode(popVar->code+sizeof(char),1);
//				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),tmpDef->id);
//				fklCodeCat(tmp1Copy->bc,pushTop);
//				fklCodeCat(tmp1Copy->bc,popVar);
//				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
//				fklCodelntCopyCat(curEnv->prev->proc,tmp1Copy);
//				fklFreeByteCodeAndLnt(tmp1Copy);
//				tmp1Copy=fklCopyByteCodelnt(tmp1);
//				fklCodeCat(tmp1Copy->bc,pushTop);
//				fklSetU32ToByteCode(popVar->code+sizeof(char),1);
//				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(int32_t),fklAddCompDef(tmpAtm->value.str,curEnv)->id);
//				fklCodeCat(tmp1Copy->bc,popVar);
//				fklCodeCat(tmp1->bc,pushTop);
//				fklCodeCat(tmp1->bc,popVar);
//				tmp1->l[tmp1->ls-1]->cpc+=(popVar->size+pushTop->size);
//				fklCodeCat(tmp1Copy->bc,popVar);
//				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(popVar->size+pushTop->size);
//				fklCodelntCopyCat(curEnv->proc,tmp1Copy);
//				fklFreeByteCode(popVar);
//			}
//			else
//				fklCodelntCopyCat(curEnv->proc,tmp1);
//		}
//		fklFreeByteCodeAndLnt(tmp1Copy);
//		if(fir->outer==tmpPair)
//			break;
//		else
//		{
//			tir=&fir->outer->prev->car;
//			sec=fklPrevCptr(tir);
//			fir=fklPrevCptr(sec);
//		}
//	}
//	fklFreeByteCode(popVar);
//	fklFreeByteCode(pushTop);
//	return tmp1;
//}
//
//FklByteCodelnt* fklCompileSetq(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstPair* tmpPair=objCptr->u.pair;
//	FklAstCptr* fir=&objCptr->u.pair->car;
//	FklAstCptr* sec=fklNextCptr(fir);
//	FklAstCptr* tir=fklNextCptr(sec);
//	FklByteCodelnt* tmp1=NULL;
//	FklByteCode* pushTop=fklCreateByteCode(sizeof(char));
//	FklByteCode* popVar=fklCreateByteCode(sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t));
//	popVar->code[0]=FKL_OP_POP_VAR;
//	pushTop->code[0]=FKL_OP_PUSH_TOP;
//	while(fklIsSetqExpression(tir))
//	{
//		fir=&tir->u.pair->car;
//		sec=fklNextCptr(fir);
//		tir=fklNextCptr(sec);
//	}
//	objCptr=tir;
//	tmp1=fklCompile(objCptr,curEnv,inter,state);
//	if(state->state!=0)
//	{
//		fklFreeByteCode(popVar);
//		fklFreeByteCode(pushTop);
//		return NULL;
//	}
//	for(;;)
//	{
//		uint32_t scope=1;
//		FklSid_t id=0;
//		FklAstAtom* tmpAtm=sec->u.atom;
//		FklCompEnvHashItem* tmpDef=NULL;
//		FklCompEnv* tmpEnv=curEnv;
//		while(tmpEnv!=NULL)
//		{
//			tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//			if(tmpEnv->prefix&&!tmpDef)
//			{
//				FklString* symbolWithPrefix=fklStringAppend(tmpEnv->prefix,tmpAtm->value.str);
//				FKL_ASSERT(symbolWithPrefix);
//				tmpDef=fklFindCompDef(symbolWithPrefix,tmpEnv);
//				free(symbolWithPrefix);
//			}
//			if(tmpDef!=NULL)break;
//			tmpEnv=tmpEnv->prev;
//			scope++;
//		}
//		if(tmpDef==NULL)
//		{
//			FklSymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
//			scope=0;
//			id=node->id;
//		}
//		else
//			id=tmpDef->id;
//		fklSetU32ToByteCode(popVar->code+sizeof(char),scope);
//		fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(scope),id);
//		FklByteCodelnt* tmp1Copy=fklCopyByteCodelnt(tmp1);
//		fklCodeCat(tmp1->bc,pushTop);
//		fklCodeCat(tmp1->bc,popVar);
//		tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
//		if(scope==1&&tmpDef&&(fklIsConst(objCptr)||fklIsLambdaExpression(objCptr)))
//		{
//			fklCodelntCopyCat(curEnv->proc,tmp1);
//			if(tmpEnv->prev
//					&&tmpEnv->prev->exp
//					&&tmpEnv->prev->prefix
//					&&fklIsSymbolShouldBeExport(tmpAtm->value.str
//						,tmpEnv->prev->exp
//						,tmpEnv->prev->n))
//			{
//				FklByteCode* popVar=fklCreateByteCode(sizeof(char)+sizeof(scope)+sizeof(FklSid_t));
//				popVar->code[0]=FKL_OP_POP_VAR;
//				fklSetU32ToByteCode(popVar->code+sizeof(char),scope+1);
//				FklString* symbolWithPrefix=fklStringAppend(tmpEnv->prev->prefix,tmpAtm->value.str);
//				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(scope),fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id);
//				free(symbolWithPrefix);
//				fklCodeCat(tmp1Copy->bc,pushTop);
//				fklCodeCat(tmp1Copy->bc,popVar);
//				tmp1Copy->l[tmp1Copy->ls-1]->cpc+=(pushTop->size+popVar->size);
//				fklFreeByteCode(popVar);
//				fklCodelntCopyCat(tmpEnv->proc,tmp1Copy);
//			}
//		}
//		fklFreeByteCodeAndLnt(tmp1Copy);
//		if(scope&&tmpEnv->prev&&tmpEnv->prev->exp&&fklIsSymbolShouldBeExport(tmpAtm->value.str,tmpEnv->prev->exp,tmpEnv->prev->n))
//		{
//			fklSetU32ToByteCode(popVar->code+sizeof(char),scope+1);
//			if(tmpEnv->prev->prefix)
//			{
//				FklString* symbolWithPrefix=fklStringAppend(tmpEnv->prev->prefix,tmpAtm->value.str);
//				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(scope),fklFindCompDef(symbolWithPrefix,tmpEnv->prev)->id);
//				free(symbolWithPrefix);
//			}
//			else
//				fklSetSidToByteCode(popVar->code+sizeof(char)+sizeof(scope),fklFindCompDef(tmpAtm->value.str,tmpEnv->prev)->id);
//			fklCodeCat(tmp1->bc,pushTop);
//			fklCodeCat(tmp1->bc,popVar);
//			tmp1->l[tmp1->ls-1]->cpc+=(pushTop->size+popVar->size);
//		}
//		if(fir->outer==tmpPair)break;
//		else
//		{
//			tir=&fir->outer->prev->car;
//			sec=fklPrevCptr(tir);
//			fir=fklPrevCptr(sec);
//		}
//	}
//	fklFreeByteCode(pushTop);
//	fklFreeByteCode(popVar);
//	return tmp1;
//}
//
////FklByteCodelnt* fklCompileSetf(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
////{
////	FklAstCptr* fir=&objCptr->u.pair->car;
////	FklAstCptr* sec=fklNextCptr(fir);
////	if(fklIsSymbol(sec))
////		return fklCompileSetq(objCptr,curEnv,inter,state);
////	else
////	{
////		FklAstCptr* tir=fklNextCptr(sec);
////		FklByteCodelnt* tmp1=fklCompile(sec,curEnv,inter,state);
////		if(state->state!=0)
////			return NULL;
////		FklByteCodelnt* tmp2=fklCompile(tir,curEnv,inter,state);
////		if(state->state!=0)
////		{
////			fklFreeByteCodeAndLnt(tmp1);
////			return NULL;
////		}
////		FklByteCode* popRef=fklCreateByteCode(sizeof(char));
////		popRef->code[0]=FKL_OP_POP_REF;
////		fklCodeLntCat(tmp1,tmp2);
////		fklCodeCat(tmp1->bc,popRef);
////		tmp1->l[tmp1->ls-1]->cpc+=popRef->size;
////		fklFreeByteCode(popRef);
////		fklFreeByteCodelnt(tmp2);
////		return tmp1;
////	}
////}
//
//FklByteCodelnt* fklCompileSym(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	int32_t line=objCptr->curline;
//	if(fklHasKeyWord(objCptr,curEnv))
//	{
//		state->state=FKL_ERR_SYNTAXERROR;
//		state->place=objCptr;
//		return NULL;
//	}
//	FklByteCode* pushVar=fklCreateByteCode(sizeof(char)+sizeof(FklSid_t));
//	pushVar->code[0]=FKL_OP_PUSH_VAR;
//	FklAstAtom* tmpAtm=objCptr->u.atom;
//	FklCompEnvHashItem* tmpDef=NULL;
//	FklCompEnv* tmpEnv=curEnv;
//	int32_t id=0;
//	while(tmpEnv!=NULL&&tmpDef==NULL)
//	{
//		tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//		if(tmpEnv->prefix&&!tmpDef)
//		{
//			FklString* symbolWithPrefix=fklStringAppend(tmpEnv->prefix,tmpAtm->value.str);
//			tmpDef=fklFindCompDef(symbolWithPrefix,tmpEnv);
//			free(symbolWithPrefix);
//		}
//		tmpEnv=tmpEnv->prev;
//	}
//	if(tmpDef==NULL)
//	{
//		FklSymTabNode* node=fklAddSymbolToGlob(tmpAtm->value.str);
//		id=node->id;
//	}
//	else
//		id=tmpDef->id;
//	fklSetSidToByteCode(pushVar->code+sizeof(char),id);
//	FklByteCodelnt* bcl=fklCreateByteCodelnt(pushVar);
//	bcl->ls=1;
//	bcl->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//	FKL_ASSERT(bcl->l);
//	bcl->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,pushVar->size,line);
//	return bcl;
//}
//
//FklByteCodelnt* fklCompileAnd(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	int32_t curline=objCptr->curline;
//	FklByteCode* jumpiffalse=fklCreateByteCode(sizeof(char)+sizeof(int64_t));
//	FklByteCode* push1=fklCreateByteCode(sizeof(char)+sizeof(int32_t));
//	FklByteCode* resTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* setTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* popTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* pushREnv=fklCreateByteCode(sizeof(char));
//	FklByteCode* popREnv=fklCreateByteCode(sizeof(char));
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklPtrStack* stack=fklCreatePtrStack(32,16);
//	jumpiffalse->code[0]=FKL_OP_JMP_IF_FALSE;
//	push1->code[0]=FKL_OP_PUSH_I32;
//	fklSetI32ToByteCode(push1->code+sizeof(char),1);
//	resTp->code[0]=FKL_OP_RES_TP;
//	setTp->code[0]=FKL_OP_SET_TP;
//	popTp->code[0]=FKL_OP_POP_TP;
//	pushREnv->code[0]=FKL_OP_PUSH_R_ENV;
//	popREnv->code[0]=FKL_OP_POP_R_ENV;
//	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//	FklCompEnv* andExprEnv=fklCreateCompEnv(curEnv);
//	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
//	{
//		FklByteCodelnt* tmp1=fklCompile(objCptr,andExprEnv,inter,state);
//		if(state->state!=0)
//		{
//			fklFreeByteCode(resTp);
//			fklFreeByteCode(popTp);
//			fklFreeByteCode(setTp);
//			fklFreeByteCodeAndLnt(tmp);
//			fklFreeByteCode(jumpiffalse);
//			fklFreeByteCode(push1);
//			fklFreeByteCode(pushREnv);
//			fklFreeByteCode(popREnv);
//			fklFreePtrStack(stack);
//			fklDestroyCompEnv(andExprEnv);
//			return NULL;
//		}
//		fklPushPtrStack(tmp1,stack);
//	}
//	fklDestroyCompEnv(andExprEnv);
//	while(!fklIsPtrStackEmpty(stack))
//	{
//		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
//		fklSetI64ToByteCode(jumpiffalse->code+sizeof(char),tmp->bc->size);
//		fklCodeCat(tmp1->bc,jumpiffalse);
//		tmp1->l[tmp1->ls-1]->cpc+=jumpiffalse->size;
//		fklReCodeCat(resTp,tmp1->bc);
//		tmp1->l[0]->cpc+=resTp->size;
//		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//		fklReCodeLntCat(tmp1,tmp);
//		fklFreeByteCodelnt(tmp1);
//	}
//	if(tmp->bc->size)
//	{
//		fklReCodeCat(pushREnv,tmp->bc);
//		fklCodeCat(tmp->bc,popREnv);
//		if(!tmp->l)
//		{
//			tmp->ls=1;
//			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//			FKL_ASSERT(tmp->l);
//			tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
//		}
//		else
//		{
//			tmp->l[0]->cpc+=pushREnv->size;
//			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushREnv->size);
//			tmp->l[tmp->ls-1]->cpc+=popREnv->size;
//		}
//	}
//	fklReCodeCat(push1,tmp->bc);
//	if(!tmp->l)
//	{
//		tmp->ls=1;
//		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//		FKL_ASSERT(tmp->l);
//		tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
//	}
//	else
//	{
//		tmp->l[0]->cpc+=push1->size;
//		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,push1->size);
//	}
//	fklReCodeCat(setTp,tmp->bc);
//	tmp->l[0]->cpc+=setTp->size;
//	FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//	fklCodeCat(tmp->bc,popTp);
//	tmp->l[tmp->ls-1]->cpc+=popTp->size;
//	fklFreeByteCode(resTp);
//	fklFreeByteCode(popTp);
//	fklFreeByteCode(setTp);
//	fklFreeByteCode(jumpiffalse);
//	fklFreeByteCode(push1);
//	fklFreeByteCode(pushREnv);
//	fklFreeByteCode(popREnv);
//	fklFreePtrStack(stack);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileOr(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	int32_t curline=objCptr->curline;
//	FklByteCode* setTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* popTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* resTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* jumpifture=fklCreateByteCode(sizeof(char)+sizeof(int64_t));
//	FklByteCode* pushnil=fklCreateByteCode(sizeof(char));
//	FklByteCode* pushREnv=fklCreateByteCode(sizeof(char));
//	FklByteCode* popREnv=fklCreateByteCode(sizeof(char));
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklPtrStack* stack=fklCreatePtrStack(32,16);
//	pushnil->code[0]=FKL_OP_PUSH_NIL;
//	jumpifture->code[0]=FKL_OP_JMP_IF_TRUE;
//	setTp->code[0]=FKL_OP_SET_TP;
//	popTp->code[0]=FKL_OP_POP_TP;
//	resTp->code[0]=FKL_OP_RES_TP;
//	pushREnv->code[0]=FKL_OP_PUSH_R_ENV;
//	popREnv->code[0]=FKL_OP_POP_R_ENV;
//	objCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//	FklCompEnv* orExprEnv=fklCreateCompEnv(curEnv);
//	for(;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
//	{
//		FklByteCodelnt* tmp1=fklCompile(objCptr,orExprEnv,inter,state);
//		if(state->state!=0)
//		{
//			fklFreeByteCode(resTp);
//			fklFreeByteCode(popTp);
//			fklFreeByteCode(setTp);
//			fklFreeByteCodeAndLnt(tmp);
//			fklFreeByteCode(jumpifture);
//			fklFreeByteCode(pushnil);
//			fklFreeByteCode(pushREnv);
//			fklFreeByteCode(popREnv);
//			fklFreePtrStack(stack);
//			fklDestroyCompEnv(orExprEnv);
//			return NULL;
//		}
//		fklPushPtrStack(tmp1,stack);
//	}
//	fklDestroyCompEnv(orExprEnv);
//	while(!fklIsPtrStackEmpty(stack))
//	{
//		FklByteCodelnt* tmp1=fklPopPtrStack(stack);
//		fklSetI64ToByteCode(jumpifture->code+sizeof(char),tmp->bc->size);
//		fklCodeCat(tmp1->bc,jumpifture);
//		tmp1->l[tmp1->ls-1]->cpc+=jumpifture->size;
//		fklReCodeCat(resTp,tmp1->bc);
//		tmp1->l[0]->cpc+=resTp->size;
//		FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//		fklReCodeLntCat(tmp1,tmp);
//		fklFreeByteCodelnt(tmp1);
//	}
//	if(tmp->bc->size)
//	{
//		fklReCodeCat(pushREnv,tmp->bc);
//		fklCodeCat(tmp->bc,popREnv);
//		if(!tmp->l)
//		{
//			tmp->ls=1;
//			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//			FKL_ASSERT(tmp->l);
//			tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
//		}
//		else
//		{
//			tmp->l[0]->cpc+=pushREnv->size;
//			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushREnv->size);
//			tmp->l[tmp->ls-1]->cpc+=popREnv->size;
//		}
//	}
//	fklReCodeCat(pushnil,tmp->bc);
//	if(!tmp->l)
//	{
//		tmp->ls=1;
//		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//		FKL_ASSERT(tmp->l);
//		tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,curline);
//	}
//	else
//	{
//		tmp->l[0]->cpc+=pushnil->size;
//		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushnil->size);
//	}
//	fklReCodeCat(setTp,tmp->bc);
//	tmp->l[0]->cpc+=setTp->size;
//	FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//	fklCodeCat(tmp->bc,popTp);
//	tmp->l[tmp->ls-1]->cpc+=popTp->size;
//	fklFreeByteCode(resTp);
//	fklFreeByteCode(popTp);
//	fklFreeByteCode(setTp);
//	fklFreeByteCode(jumpifture);
//	fklFreeByteCode(pushnil);
//	fklFreeByteCode(pushREnv);
//	fklFreeByteCode(popREnv);
//	fklFreePtrStack(stack);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileBegin(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstCptr* firCptr=fklNextCptr(fklGetFirstCptr(objCptr));
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	if(!firCptr)
//	{
//		tmp->ls=1;
//		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//		FKL_ASSERT(tmp->l);
//		tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,objCptr->curline);
//		FklByteCode* pushnil=fklCreateByteCode(1);
//		pushnil->code[0]=FKL_OP_PUSH_NIL;
//		fklCodeCat(tmp->bc,pushnil);
//		tmp->l[tmp->ls-1]->cpc+=pushnil->size;
//		fklFreeByteCode(pushnil);
//	}
//	else
//	{
//		FklByteCode* resTp=fklCreateByteCode(1);
//		FklByteCode* setTp=fklCreateByteCode(1);
//		FklByteCode* popTp=fklCreateByteCode(1);
//		resTp->code[0]=FKL_OP_RES_TP;
//		setTp->code[0]=FKL_OP_SET_TP;
//		popTp->code[0]=FKL_OP_POP_TP;
//		while(firCptr)
//		{
//			FklByteCodelnt* tmp1=fklCompile(firCptr,curEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(tmp);
//				fklFreeByteCode(resTp);
//				fklFreeByteCode(setTp);
//				fklFreeByteCode(popTp);
//				return NULL;
//			}
//			if(tmp->bc->size&&tmp1->bc->size)
//			{
//				fklReCodeCat(resTp,tmp1->bc);
//				tmp1->l[0]->cpc+=1;
//				FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//			}
//			fklCodeLntCat(tmp,tmp1);
//			fklFreeByteCodelnt(tmp1);
//			firCptr=fklNextCptr(firCptr);
//		}
//		fklReCodeCat(setTp,tmp->bc);
//		if(!tmp->l)
//		{
//			tmp->ls=1;
//			tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//			FKL_ASSERT(tmp->l);
//			tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,objCptr->curline);
//		}
//		else
//		{
//			tmp->l[0]->cpc+=1;
//			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//		}
//		fklCodeCat(tmp->bc,popTp);
//		tmp->l[tmp->ls-1]->cpc+=popTp->size;
//		fklFreeByteCode(setTp);
//		fklFreeByteCode(resTp);
//		fklFreeByteCode(popTp);
//	}
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileLibrary(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstCptr* firCptr=fklNextCptr(fklNextCptr(fklNextCptr(fklGetFirstCptr(objCptr))));
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklByteCode* resTp=fklCreateByteCode(1);
//	FklByteCode* setTp=fklCreateByteCode(1);
//	FklByteCode* popTp=fklCreateByteCode(1);
//	resTp->code[0]=FKL_OP_RES_TP;
//	setTp->code[0]=FKL_OP_SET_TP;
//	popTp->code[0]=FKL_OP_POP_TP;
//	uint32_t i=0;
//	if(!firCptr)
//	{
//		state->state=FKL_ERR_SYNTAXERROR;
//		state->place=objCptr;
//		fklFreeByteCodeAndLnt(tmp);
//		fklFreeByteCode(resTp);
//		fklFreeByteCode(setTp);
//		fklFreeByteCode(popTp);
//		return NULL;
//	}
//	while(firCptr)
//	{
//		FklByteCodelnt* tmp1=fklCompile(firCptr,curEnv,inter,state);
//		if(state->state!=0)
//		{
//			fklFreeByteCodeAndLnt(tmp);
//			fklFreeByteCode(resTp);
//			fklFreeByteCode(setTp);
//			fklFreeByteCode(popTp);
//			return NULL;
//		}
//		i++;
//		if(i==64&&tmp->bc->size&&tmp1->bc->size)
//		{
//			fklReCodeCat(resTp,tmp1->bc);
//			tmp1->l[0]->cpc+=1;
//			FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//			i=0;
//		}
//		fklCodeLntCat(tmp,tmp1);
//		fklFreeByteCodelnt(tmp1);
//		firCptr=fklNextCptr(firCptr);
//	}
//	fklReCodeCat(setTp,tmp->bc);
//	if(!tmp->l)
//	{
//		tmp->ls=1;
//		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//		FKL_ASSERT(tmp->l);
//		tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,tmp->bc->size,objCptr->curline);
//	}
//	else
//	{
//		tmp->l[0]->cpc+=1;
//		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//	}
//	fklCodeCat(tmp->bc,popTp);
//	tmp->l[tmp->ls-1]->cpc+=popTp->size;
//	fklFreeByteCode(setTp);
//	fklFreeByteCode(resTp);
//	fklFreeByteCode(popTp);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileLambda(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	uint32_t line=objCptr->curline;
//	FklAstCptr* tmpCptr=objCptr;
//	FklAstPair* objPair=NULL;
//	FklCompEnv* tmpEnv=fklCreateCompEnv(curEnv);
//	FklByteCode* popArg=fklCreateByteCode(sizeof(char)+sizeof(FklSid_t));
//	FklByteCode* popRestArg=fklCreateByteCode(sizeof(char)+sizeof(FklSid_t));
//	FklByteCode* pArg=fklCreateByteCode(0);
//	popArg->code[0]=FKL_OP_POP_ARG;
//	popRestArg->code[0]=FKL_OP_POP_REST_ARG;
//	objPair=objCptr->u.pair;
//	objCptr=&objPair->car;
//	if(fklNextCptr(objCptr)->type==FKL_TYPE_PAIR)
//	{
//		FklAstCptr* argCptr=&fklNextCptr(objCptr)->u.pair->car;
//		while(argCptr!=NULL&&argCptr->type!=FKL_TYPE_NIL)
//		{
//			FklAstAtom* tmpAtm=(argCptr->type==FKL_TYPE_ATM)?argCptr->u.atom:NULL;
//			if(argCptr->type!=FKL_TYPE_ATM||tmpAtm==NULL||tmpAtm->type!=FKL_TYPE_SYM)
//			{
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=tmpCptr;
//				fklFreeByteCode(popArg);
//				fklFreeByteCode(popRestArg);
//				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//				return NULL;
//			}
//			FklCompEnvHashItem* tmpDef=fklFindCompDef(tmpAtm->value.str,tmpEnv);
//			if(tmpDef)
//			{
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=tmpCptr;
//				fklFreeByteCode(popArg);
//				fklFreeByteCode(popRestArg);
//				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//				return NULL;
//			}
//			else
//				tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
//			fklSetSidToByteCode(popArg->code+sizeof(char),tmpDef->id);
//			fklCodeCat(pArg,popArg);
//			if(fklNextCptr(argCptr)==NULL&&argCptr->outer->cdr.type==FKL_TYPE_ATM)
//			{
//				FklAstAtom* tmpAtom1=(argCptr->outer->cdr.type==FKL_TYPE_ATM)?argCptr->outer->cdr.u.atom:NULL;
//				if(tmpAtom1!=NULL&&tmpAtom1->type!=FKL_TYPE_SYM)
//				{
//					state->state=FKL_ERR_SYNTAXERROR;
//					state->place=tmpCptr;
//					fklFreeByteCode(popArg);
//					fklFreeByteCode(popRestArg);
//					fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//					return NULL;
//				}
//				tmpDef=fklAddCompDef(tmpAtom1->value.str,tmpEnv);
//				fklSetSidToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
//				fklCodeCat(pArg,popRestArg);
//			}
//			argCptr=fklNextCptr(argCptr);
//		}
//	}
//	else if(fklNextCptr(objCptr)->type==FKL_TYPE_ATM)
//	{
//		FklAstAtom* tmpAtm=fklNextCptr(objCptr)->u.atom;
//		if(tmpAtm->type!=FKL_TYPE_SYM)
//		{
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=tmpCptr;
//			fklFreeByteCode(popArg);
//			fklFreeByteCode(popRestArg);
//			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//			return NULL;
//		}
//		FklCompEnvHashItem* tmpDef=fklAddCompDef(tmpAtm->value.str,tmpEnv);
//		fklSetSidToByteCode(popRestArg->code+sizeof(char),tmpDef->id);
//		fklCodeCat(pArg,popRestArg);
//	}
//	fklFreeByteCode(popRestArg);
//	fklFreeByteCode(popArg);
//	FklAstCptr* begin=fklNextCptr(fklNextCptr(objCptr));
//	FklByteCodelnt* codeOfLambda=fklCreateByteCodelnt(pArg);
//	codeOfLambda->ls=1;
//	codeOfLambda->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//	FKL_ASSERT(codeOfLambda->l);
//	if(!begin)
//	{
//		codeOfLambda->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,pArg->size,line);
//		FklByteCode* pushnil=fklCreateByteCode(1);
//		pushnil->code[0]=FKL_OP_PUSH_NIL;
//		fklCodeCat(codeOfLambda->bc,pushnil);
//		codeOfLambda->l[codeOfLambda->ls-1]->cpc+=pushnil->size;
//		fklFreeByteCode(pushnil);
//	}
//	else
//	{
//		FklByteCode* fklResBp=fklCreateByteCode(sizeof(char));
//		fklResBp->code[0]=FKL_OP_RES_BP;
//		FklByteCode* setTp=fklCreateByteCode(sizeof(char));
//		setTp->code[0]=FKL_OP_SET_TP;
//		fklCodeCat(pArg,fklResBp);
//		fklCodeCat(pArg,setTp);
//		codeOfLambda->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,pArg->size,line);
//		fklFreeByteCode(fklResBp);
//		fklFreeByteCode(setTp);
//		objCptr=begin;
//		FklByteCode* resTp=fklCreateByteCode(sizeof(char));
//		resTp->code[0]=FKL_OP_RES_TP;
//		for(;objCptr;objCptr=fklNextCptr(objCptr))
//		{
//			FklByteCodelnt* tmp1=fklCompile(objCptr,tmpEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(codeOfLambda);
//				fklFreeByteCode(resTp);
//				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//				return NULL;
//			}
//			if(fklNextCptr(objCptr)!=NULL)
//			{
//				fklCodeCat(tmp1->bc,resTp);
//				tmp1->l[tmp1->ls-1]->cpc+=resTp->size;
//			}
//			fklCodeLntCat(codeOfLambda,tmp1);
//			fklFreeByteCodelnt(tmp1);
//		}
//		fklFreeByteCode(resTp);
//		FklByteCode* popTp=fklCreateByteCode(sizeof(char));
//		popTp->code[0]=FKL_OP_POP_TP;
//		fklCodeCat(codeOfLambda->bc,popTp);
//		codeOfLambda->l[codeOfLambda->ls-1]->cpc+=popTp->size;
//		fklFreeByteCode(popTp);
//	}
//	FklByteCode* pushProc=fklCreateByteCode(sizeof(char)+sizeof(codeOfLambda->bc->size));
//	pushProc->code[0]=FKL_OP_PUSH_PROC;
//	fklSetU64ToByteCode(pushProc->code+sizeof(char),codeOfLambda->bc->size);
//	FklByteCodelnt* toReturn=fklCreateByteCodelnt(pushProc);
//	toReturn->ls=1;
//	toReturn->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//	FKL_ASSERT(toReturn->l);
//	toReturn->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,pushProc->size,line);
//	fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//	fklScanAndSetTailCall(codeOfLambda->bc);
//	fklCodeLntCat(toReturn,codeOfLambda);
//	fklFreeByteCodelnt(codeOfLambda);
//	return toReturn;
//}
//
//FklByteCodelnt* fklCompileCond(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstCptr* cond=NULL;
//	FklByteCode* pushnil=fklCreateByteCode(sizeof(char));
//	FklByteCode* jumpiffalse=fklCreateByteCode(sizeof(char)+sizeof(int64_t));
//	FklByteCode* jump=fklCreateByteCode(sizeof(char)+sizeof(int64_t));
//	FklByteCode* resTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* setTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* popTp=fklCreateByteCode(sizeof(char));
//	FklByteCode* pushREnv=fklCreateByteCode(sizeof(char));
//	FklByteCode* popREnv=fklCreateByteCode(sizeof(char));
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	FklPtrStack* stack1=fklCreatePtrStack(32,16);
//	FklMemMenager* memMenager=fklCreateMemMenager(32);
//	fklPushMem(pushnil,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(jumpiffalse,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(jump,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(resTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(setTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(popTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(pushREnv,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(popREnv,(FklGenDestructor)fklFreeByteCode,memMenager);
//	fklPushMem(stack1,(FklGenDestructor)fklFreePtrStack,memMenager);
//	setTp->code[0]=FKL_OP_SET_TP;
//	resTp->code[0]=FKL_OP_RES_TP;
//	popTp->code[0]=FKL_OP_POP_TP;
//	pushREnv->code[0]=FKL_OP_PUSH_R_ENV;
//	popREnv->code[0]=FKL_OP_POP_R_ENV;
//	pushnil->code[0]=FKL_OP_PUSH_NIL;
//	jumpiffalse->code[0]=FKL_OP_JMP_IF_FALSE;
//	jump->code[0]=FKL_OP_JMP;
//	for(cond=fklNextCptr(fklGetFirstCptr(objCptr));cond!=NULL;cond=fklNextCptr(cond))
//	{
//		FklCompEnv* conditionEnv=fklCreateCompEnv(curEnv);
//		objCptr=fklGetFirstCptr(cond);
//		FklByteCodelnt* conditionCode=fklCompile(objCptr,conditionEnv,inter,state);
//		FklPtrStack* stack2=fklCreatePtrStack(32,16);
//		fklReCodeCat(pushREnv,conditionCode->bc);
//		conditionCode->l[conditionCode->ls-1]->cpc+=pushREnv->size;
//		for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
//		{
//			FklByteCodelnt* tmp1=fklCompile(objCptr,conditionEnv,inter,state);
//			if(state->state!=0)
//			{
//				fklFreeByteCodeAndLnt(tmp);
//				fklFreeMemMenager(memMenager);
//				fklFreePtrStack(stack2);
//				if(conditionCode)
//				{
//					fklFreeByteCodeAndLnt(conditionCode);
//				}
//				fklDestroyCompEnv(conditionEnv);
//				return NULL;
//			}
//			fklPushPtrStack(tmp1,stack2);
//		}
//		FklByteCodelnt* tmpCond=fklCreateByteCodelnt(fklCreateByteCode(0));
//		while(!fklIsPtrStackEmpty(stack2))
//		{
//			FklByteCodelnt* tmp1=fklPopPtrStack(stack2);
//			fklReCodeCat(resTp,tmp1->bc);
//			tmp1->l[0]->cpc+=resTp->size;
//			FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//			fklReCodeLntCat(tmp1,tmpCond);
//			fklFreeByteCodelnt(tmp1);
//		}
//		fklFreePtrStack(stack2);
//		if(tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size))
//		{
//			fklSetI64ToByteCode(jumpiffalse->code+sizeof(char),tmpCond->bc->size+((fklNextCptr(cond)==NULL)?0:jump->size));
//			fklCodeCat(conditionCode->bc,jumpiffalse);
//			conditionCode->l[conditionCode->ls-1]->cpc+=jumpiffalse->size;
//		}
//		fklReCodeLntCat(conditionCode,tmpCond);
//		fklFreeByteCodelnt(conditionCode);
//		fklPushPtrStack(tmpCond,stack1);
//		fklDestroyCompEnv(conditionEnv);
//	}
//	uint32_t top=stack1->top-1;
//	while(!fklIsPtrStackEmpty(stack1))
//	{
//		FklByteCodelnt* tmpCond=fklPopPtrStack(stack1);
//		if(!fklIsPtrStackEmpty(stack1))
//		{
//			fklReCodeCat(resTp,tmpCond->bc);
//			fklReCodeCat(popREnv,tmpCond->bc);
//			tmpCond->l[0]->cpc+=resTp->size+popREnv->size;
//			FKL_INCREASE_ALL_SCP(tmpCond->l+1,tmpCond->ls-1,resTp->size+popREnv->size);
//		}
//		if(top!=stack1->top)
//		{
//			fklSetI64ToByteCode(jump->code+sizeof(char),tmp->bc->size);
//			fklCodeCat(tmpCond->bc,jump);
//			tmpCond->l[tmpCond->ls-1]->cpc+=jump->size;
//		}
//		fklReCodeLntCat(tmpCond,tmp);
//		fklFreeByteCodelnt(tmpCond);
//	}
//	if(!tmp->l)
//	{
//		fklCodeCat(tmp->bc,pushnil);
//		tmp->ls=1;
//		tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*));
//		FKL_ASSERT(tmp->l);
//		tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,pushnil->size,objCptr->curline);
//	}
//	else
//	{
//		fklReCodeCat(setTp,tmp->bc);
//		tmp->l[0]->cpc+=setTp->size;
//		FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//		fklCodeCat(tmp->bc,popREnv);
//		tmp->l[tmp->ls-1]->cpc+=popREnv->size;
//		fklCodeCat(tmp->bc,popTp);
//		tmp->l[tmp->ls-1]->cpc+=popTp->size;
//	}
//	fklFreeMemMenager(memMenager);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileLoad(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklAstCptr* fir=&objCptr->u.pair->car;
//	FklAstCptr* pFileName=fklNextCptr(fir);
//	FklAstAtom* name=pFileName->u.atom;
//	char* filename=fklCharBufToCstr(name->value.str->str,name->value.str->size);
//	char* rpath=fklRealpath(filename);
//	if(fklHasLoadSameFile(rpath,inter))
//	{
//		state->state=FKL_ERR_CIRCULARLOAD;
//		state->place=pFileName;
//		free(filename);
//		free(rpath);
//		return NULL;
//	}
//	free(filename);
//	FILE* file=NULL;
//	if(rpath)
//		file=fopen(rpath,"r");
//	if(file==NULL)
//	{
//		state->state=FKL_ERR_FILEFAILURE;
//		state->place=pFileName;
//		free(rpath);
//		return NULL;
//	}
//	fklAddSymbolToGlobCstr(rpath);
//	FklInterpreter* tmpIntpr=fklCreateIntpr(rpath,file,curEnv,inter->lnt);
//	tmpIntpr->prev=inter;
//	tmpIntpr->glob=curEnv;
//	FklByteCodelnt* tmp=fklCompileFile(tmpIntpr,NULL);
//	chdir(tmpIntpr->prev->curDir);
//	tmpIntpr->glob=NULL;
//	tmpIntpr->lnt=NULL;
//	fklFreeIntpr(tmpIntpr);
//	if(tmp)
//	{
//		if(!tmp->bc->size)
//		{
//			FklByteCode* pushnil=fklCreateByteCode(1);
//			pushnil->code[0]=FKL_OP_PUSH_NIL;
//			fklCodeCat(tmp->bc,pushnil);
//			tmp->l[tmp->ls-1]->cpc+=pushnil->size;
//			fklFreeByteCode(pushnil);
//		}
//		else
//		{
//			FklByteCode* setTp=fklCreateByteCode(1);
//			FklByteCode* popTp=fklCreateByteCode(1);
//			setTp->code[0]=FKL_OP_SET_TP;
//			popTp->code[0]=FKL_OP_POP_TP;
//			fklReCodeCat(setTp,tmp->bc);
//			fklCodeCat(tmp->bc,popTp);
//			tmp->l[0]->cpc+=setTp->size;
//			FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,setTp->size);
//			tmp->l[tmp->ls-1]->cpc+=popTp->size;
//			fklFreeByteCode(popTp);
//			fklFreeByteCode(setTp);
//		}
//	}
//	free(rpath);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileFile(FklInterpreter* inter,int* exitstate)
//{
//	chdir(inter->curDir);
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	tmp->ls=1;
//	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//	FKL_ASSERT(tmp->l);
//	tmp->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,0,1);
//	FklByteCode* resTp=fklCreateByteCode(1);
//	resTp->code[0]=FKL_OP_RES_TP;
//	char* prev=NULL;
//	size_t prevSize=0;
//	FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
//	for(;;)
//	{
//		FklAstCptr* begin=NULL;
//		int unexpectEOF=0;
//		size_t size=0;
//		char* list=fklReadInStringPattern(inter->file,&prev,&size,&prevSize,inter->curline,&unexpectEOF,tokenStack,NULL);
//		if(list==NULL&&!unexpectEOF&&size)continue;
//		FklErrorState state={0,NULL};
//		if(unexpectEOF)
//		{
//			switch(unexpectEOF)
//			{
//				case 1:
//					fprintf(stderr
//							,"error of reader:Unexpect EOF at line %d of %s\n"
//							,inter->curline
//							,inter->filename);
//					break;
//				case 2:
//					fprintf(stderr
//							,"error of reader:Invalid expression at line %d of %s\n"
//							,inter->curline
//							,inter->filename);
//					break;
//			}
//			fklFreeByteCodeAndLnt(tmp);
//			free(list);
//			if(exitstate)
//				*exitstate=FKL_ERR_UNEXPECTEOF;
//			tmp=NULL;
//			break;
//		}
//		begin=fklCreateAstWithTokens(tokenStack,inter->filename,inter->glob);
//		inter->curline+=fklCountChar(list,'\n',size);
//		if(fklIsAllSpaceBufSize(list,size))
//		{
//			while(!fklIsPtrStackEmpty(tokenStack))
//				fklFreeToken(fklPopPtrStack(tokenStack));
//			free(list);
//			break;
//		}
//		if(begin!=NULL)
//		{
//			FklByteCodelnt* tmpByteCodelnt=fklCompile(begin,inter->glob,inter,&state);
//			if(!tmpByteCodelnt||state.state!=0)
//			{
//				if(state.state)
//				{
//					fklPrintCompileError(state.place,state.state,inter);
//					if(exitstate)
//						*exitstate=state.state;
//					fklDeleteCptr(state.place);
//				}
//				if(tmpByteCodelnt==NULL&&!state.state&&exitstate)
//					*exitstate=FKL_ERR_MACROEXPANDFAILED;
//				fklFreeByteCodeAndLnt(tmp);
//				free(list);
//				fklDeleteCptr(begin);
//				free(begin);
//				tmp=NULL;
//				while(!fklIsPtrStackEmpty(tokenStack))
//					fklFreeToken(fklPopPtrStack(tokenStack));
//				break;
//			}
//			if(tmp->bc->size&&tmpByteCodelnt->bc->size)
//			{
//				fklReCodeCat(resTp,tmpByteCodelnt->bc);
//				tmpByteCodelnt->l[0]->cpc+=resTp->size;
//				FKL_INCREASE_ALL_SCP(tmpByteCodelnt->l+1,tmpByteCodelnt->ls-1,resTp->size);
//			}
//			fklCodeLntCat(tmp,tmpByteCodelnt);
//			fklFreeByteCodelnt(tmpByteCodelnt);
//			fklDeleteCptr(begin);
//			free(begin);
//		}
//		else if(begin==NULL&&!fklIsAllComment(tokenStack))
//		{
//			fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",inter->curline,inter->filename);
//			*exitstate=FKL_ERR_INVALIDEXPR;
//			while(!fklIsPtrStackEmpty(tokenStack))
//				fklFreeToken(fklPopPtrStack(tokenStack));
//			free(list);
//			list=NULL;
//			break;
//		}
//		while(!fklIsPtrStackEmpty(tokenStack))
//			fklFreeToken(fklPopPtrStack(tokenStack));
//		free(list);
//		list=NULL;
//	}
//	if(prev)
//		free(prev);
//	fklFreeByteCode(resTp);
//	fklFreePtrStack(tokenStack);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileImport(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklMemMenager* memMenager=fklCreateMemMenager(32);
//	char postfix[]=".fkl";
//	FklByteCodelnt* tmp=fklCreateByteCodelnt(fklCreateByteCode(0));
//	fklPushMem(tmp,(FklGenDestructor)fklFreeByteCodeAndLnt,memMenager);
//	chdir(inter->curDir);
//	char* prev=NULL;
//	size_t prevSize=0;
//	/* 获取文件路径和文件名
//	 * 然后打开并查找第一个library表达式
//	 */
//	FklAstCptr* plib=fklNextCptr(fklGetFirstCptr(objCptr));
//	for(;plib;plib=fklNextCptr(plib))
//	{
//		FklString* libPrefix=NULL;
//		FklAstCptr* pairOfpPartsOfPath=plib;
//		FklAstCptr* pPartsOfPath=fklGetFirstCptr(plib);
//		if(!pPartsOfPath)
//		{
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=plib;
//			fklFreeMemMenager(memMenager);
//			return NULL;
//		}
//		uint32_t count=0;
//		FklString** partsOfPath=NULL;
//		while(pPartsOfPath)
//		{
//			if(pPartsOfPath->type!=FKL_TYPE_ATM)
//			{
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=plib;
//				fklFreeMemMenager(memMenager);
//				if(count)
//					fklFreeStringArray(partsOfPath,count);
//				return NULL;
//			}
//			FklAstAtom* tmpAtm=pPartsOfPath->u.atom;
//			if(tmpAtm->type!=FKL_TYPE_STR&&tmpAtm->type!=FKL_TYPE_SYM)
//			{
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=plib;
//				fklFreeMemMenager(memMenager);
//				if(count)
//					fklFreeStringArray(partsOfPath,count);
//				return NULL;
//			}
//			if(tmpAtm->type==FKL_TYPE_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"prefix")&&fklNextCptr(pPartsOfPath)->type==FKL_TYPE_PAIR)
//			{
//				FklAstCptr* tmpCptr=fklNextCptr(pPartsOfPath);
//				if(!tmpCptr||tmpCptr->type!=FKL_TYPE_PAIR)
//				{
//					state->state=FKL_ERR_SYNTAXERROR;
//					state->place=objCptr;
//					fklFreeMemMenager(memMenager);
//					if(count)
//						fklFreeStringArray(partsOfPath,count);
//					return NULL;
//				}
//				tmpCptr=fklNextCptr(tmpCptr);
//				if(!tmpCptr||fklNextCptr(tmpCptr)||tmpCptr->type!=FKL_TYPE_ATM)
//				{
//					state->state=FKL_ERR_SYNTAXERROR;
//					state->place=plib;
//					fklFreeMemMenager(memMenager);
//					if(count)
//						fklFreeStringArray(partsOfPath,count);
//					return NULL;
//				}
//				FklAstAtom* prefixAtom=tmpCptr->u.atom;
//				if(prefixAtom->type!=FKL_TYPE_STR&&prefixAtom->type!=FKL_TYPE_SYM)
//				{
//					state->state=FKL_ERR_SYNTAXERROR;
//					state->place=plib;
//					fklFreeMemMenager(memMenager);
//					if(count)
//						fklFreeStringArray(partsOfPath,count);
//					return NULL;
//				}
//				libPrefix=fklCopyString(prefixAtom->value.str);
//				pairOfpPartsOfPath=fklNextCptr(pPartsOfPath);
//				pPartsOfPath=fklGetFirstCptr(pairOfpPartsOfPath);
//				continue;
//			}
//			count++;
//			FklString** tmp=(FklString**)realloc(partsOfPath,sizeof(FklString*)*count);
//			FKL_ASSERT(tmp);
//			partsOfPath=tmp;
//			partsOfPath[count-1]=tmpAtm->type==FKL_TYPE_SYM?fklCopyString(tmpAtm->value.str):fklCopyString(tmpAtm->value.str);
//			pPartsOfPath=fklNextCptr(pPartsOfPath);
//		}
//		uint32_t totalPathLength=0;
//		uint32_t i=0;
//		for(;i<count;i++)
//			totalPathLength+=partsOfPath[i]->size;
//		totalPathLength+=count-1;
//		FklString* path=fklCreateString(totalPathLength,NULL);
//		FKL_ASSERT(path);
//		size_t l=0;
//		for(i=0;i<count;i++)
//		{
//			if(i>0)
//			{
//				memcpy(&path->str[l],FKL_PATH_SEPARATOR_STR,strlen(FKL_PATH_SEPARATOR_STR));
//				l+=strlen(FKL_PATH_SEPARATOR_STR);
//			}
//			memcpy(&path->str[l],partsOfPath[i]->str,partsOfPath[i]->size);
//			l+=partsOfPath[i]->size;
//		}
//		fklStringCstrCat(&path,postfix);
//		char* path_c=fklCharBufToCstr(path->str,path->size);
//		char* rpath=fklRealpath(path_c);
//		free(path_c);
//		free(path);
//		FILE* fp=NULL;
//		if(rpath)
//			fp=fopen(rpath,"r");
//		if(!fp)
//		{
//			state->state=FKL_ERR_IMPORTFAILED;
//			state->place=pairOfpPartsOfPath;
//			free(rpath);
//			fklFreeMemMenager(memMenager);
//			fklFreeStringArray(partsOfPath,count);
//			return NULL;
//		}
//		if(fklHasLoadSameFile(rpath,inter))
//		{
//			state->state=FKL_ERR_CIRCULARLOAD;
//			state->place=objCptr;
//			fklFreeMemMenager(memMenager);
//			fklFreeStringArray(partsOfPath,count);
//			return NULL;
//		}
//		fklAddSymbolToGlobCstr(rpath);
//		FklInterpreter* tmpInter=fklCreateIntpr(rpath,fp,fklCreateGlobCompEnv(NULL),inter->lnt);
//		fklInitGlobKeyWord(tmpInter->glob);
//		free(rpath);
//		tmpInter->prev=inter;
//		FklByteCode* resTp=fklCreateByteCode(1);
//		FklByteCode* setTp=fklCreateByteCode(1);
//		FklByteCode* popTp=fklCreateByteCode(1);
//		resTp->code[0]=FKL_OP_RES_TP;
//		setTp->code[0]=FKL_OP_SET_TP;
//		popTp->code[0]=FKL_OP_POP_TP;
//		fklPushMem(resTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//		fklPushMem(setTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//		fklPushMem(popTp,(FklGenDestructor)fklFreeByteCode,memMenager);
//		FklByteCodelnt* libByteCodelnt=fklCreateByteCodelnt(fklCreateByteCode(0));
//		fklPushMem(libByteCodelnt,(FklGenDestructor)fklFreeByteCodeAndLnt,memMenager);
//		FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
//		for(;;)
//		{
//			FklAstCptr* begin=NULL;
//			int unexpectEOF=0;
//			size_t size=0;
//			char* list=fklReadInStringPattern(tmpInter->file,&prev,&size,&prevSize,tmpInter->curline,&unexpectEOF,tokenStack,NULL);
//			if(list==NULL&&!unexpectEOF&&size)continue;
//			if(unexpectEOF)
//			{
//				switch(unexpectEOF)
//				{
//					case 1:
//						fprintf(stderr,"error of reader:Unexpect EOF at line %d of %s\n",tmpInter->curline,tmpInter->filename);
//						break;
//					case 2:
//						fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",tmpInter->curline,tmpInter->filename);
//						break;
//				}
//				free(list);
//				break;
//			}
//			begin=fklCreateAstWithTokens(tokenStack,tmpInter->filename,tmpInter->glob);
//			tmpInter->curline+=fklCountChar(list,'\n',size);
//			if(fklIsAllSpaceBufSize(list,size))
//			{
//				while(!fklIsPtrStackEmpty(tokenStack))
//					fklFreeToken(fklPopPtrStack(tokenStack));
//				free(list);
//				break;
//			}
//			if(begin!=NULL)
//			{
//				//查找library并编译library
//				if(fklIsLibraryExpression(begin))
//				{
//					free(prev);
//					prev=NULL;
//					FklAstCptr* libName=fklNextCptr(fklGetFirstCptr(begin));
//					FklAstCptr* objLibName=fklGetLastCptr(pairOfpPartsOfPath);
//					if(fklCptrcmp(libName,objLibName))
//					{
//						FklCompEnv* tmpCurEnv=fklCreateCompEnv(tmpInter->glob);
//						FklAstCptr* exportCptr=fklNextCptr(libName);
//						if(!exportCptr||!fklIsExportExpression(exportCptr))
//						{
//							fklPrintCompileError(begin,FKL_ERR_INVALIDEXPR,tmpInter);
//							fklDeleteCptr(begin);
//							free(begin);
//							chdir(tmpInter->prev->curDir);
//							tmpInter->lnt=NULL;
//							fklFreeAllMacroThenDestroyCompEnv(tmpCurEnv);
//							fklFreeIntpr(tmpInter);
//							if(libPrefix)
//								free(libPrefix);
//							fklFreeMemMenager(memMenager);
//							fklFreeStringArray(partsOfPath,count);
//							free(list);
//							while(!fklIsPtrStackEmpty(tokenStack))
//								fklFreeToken(fklPopPtrStack(tokenStack));
//							fklFreePtrStack(tokenStack);
//							return NULL;
//						}
//						else
//						{
//							const FklString** exportSymbols=(const FklString**)malloc(sizeof(const FklString*)*0);
//							fklPushMem(exportSymbols,free,memMenager);
//							FKL_ASSERT(exportSymbols);
//							FklAstCptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
//							uint32_t num=0;
//							for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
//							{
//								if(pExportSymbols->type!=FKL_TYPE_ATM
//										||pExportSymbols->u.atom->type!=FKL_TYPE_SYM)
//								{
//									fklPrintCompileError(exportCptr,FKL_ERR_SYNTAXERROR,tmpInter);
//									fklDeleteCptr(begin);
//									free(begin);
//									chdir(tmpInter->prev->curDir);
//									tmpInter->lnt=NULL;
//									fklFreeAllMacroThenDestroyCompEnv(tmpCurEnv);
//									fklFreeIntpr(tmpInter);
//									if(libPrefix)
//										free(libPrefix);
//									fklFreeMemMenager(memMenager);
//									fklFreeStringArray(partsOfPath,count);
//									free(list);
//									while(!fklIsPtrStackEmpty(tokenStack))
//										fklFreeToken(fklPopPtrStack(tokenStack));
//									fklFreePtrStack(tokenStack);
//									return NULL;
//								}
//								FklAstAtom* pSymbol=pExportSymbols->u.atom;
//								num++;
//								const FklString** t_Cstr=(const FklString**)malloc(sizeof(const FklString*)*num);
//								FKL_ASSERT(t_Cstr);
//								fklReallocMem(exportSymbols,t_Cstr,memMenager);
//								memcpy(t_Cstr,exportSymbols,sizeof(const char**)*(num-1));
//								free(exportSymbols);
//								exportSymbols=t_Cstr;
//								exportSymbols[num-1]=pSymbol->value.str;
//							}
//							mergeSort(exportSymbols,num,sizeof(const char*),cmpString);
//							FklAstCptr* pBody=fklNextCptr(fklNextCptr(libName));
//							tmpInter->glob->prefix=libPrefix;
//							tmpInter->glob->exp=exportSymbols;
//							tmpInter->glob->n=num;
//							uint32_t i=0;
//							for(;pBody;pBody=fklNextCptr(pBody))
//							{
//								FklByteCodelnt* otherByteCodelnt=fklCompile(pBody,tmpCurEnv,tmpInter,state);
//								if(state->state||!otherByteCodelnt)
//								{
//									fklPrintCompileError(state->place,state->state,tmpInter);
//									fklDeleteCptr(begin);
//									free(begin);
//									chdir(tmpInter->prev->curDir);
//									tmpInter->lnt=NULL;
//									fklFreeAllMacroThenDestroyCompEnv(tmpCurEnv);
//									fklFreeIntpr(tmpInter);
//									state->state=0;
//									state->place=NULL;
//									if(libPrefix)
//										free(libPrefix);
//									fklFreeMemMenager(memMenager);
//									fklFreeStringArray(partsOfPath,count);
//									free(list);
//									while(!fklIsPtrStackEmpty(tokenStack))
//										fklFreeToken(fklPopPtrStack(tokenStack));
//									fklFreePtrStack(tokenStack);
//									return NULL;
//								}
//								i++;
//								if(i==FKL_THRESHOLD_SIZE&&libByteCodelnt->bc->size&&otherByteCodelnt&&otherByteCodelnt->bc->size)
//								{
//									fklReCodeCat(resTp,otherByteCodelnt->bc);
//									otherByteCodelnt->l[0]->cpc+=resTp->size;
//									FKL_INCREASE_ALL_SCP(otherByteCodelnt->l+1,otherByteCodelnt->ls-1,resTp->size);
//									i=0;
//								}
//								fklCodeLntCat(libByteCodelnt,otherByteCodelnt);
//								fklFreeByteCodelnt(otherByteCodelnt);
//							}
//							fklReCodeCat(setTp,libByteCodelnt->bc);
//							if(!libByteCodelnt->l)
//							{
//								libByteCodelnt->ls=setTp->size;
//								libByteCodelnt->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//								FKL_ASSERT(libByteCodelnt->l);
//								libByteCodelnt->l[0]=fklCreateLineNumTabNodeWithFilename(tmpInter->filename,0,libByteCodelnt->bc->size,objCptr->curline);
//							}
//							else
//							{
//								libByteCodelnt->l[0]->cpc+=setTp->size;
//								FKL_INCREASE_ALL_SCP(libByteCodelnt->l+1,libByteCodelnt->ls-1,setTp->size);
//							}
//							fklCodeCat(libByteCodelnt->bc,popTp);
//							fklCodeCat(libByteCodelnt->bc,resTp);
//							libByteCodelnt->l[libByteCodelnt->ls-1]->cpc+=popTp->size+resTp->size;
//							fklCodeLntCat(tmp,libByteCodelnt);
//							fklDeleteMem(libByteCodelnt,memMenager);
//							fklFreeByteCodelnt(libByteCodelnt);
//							libByteCodelnt=NULL;
//							FklByteCode* pushProc=fklCreateByteCode(sizeof(char)+sizeof(tmp->bc->size));
//							pushProc->code[0]=FKL_OP_PUSH_PROC;
//							fklSetU64ToByteCode(pushProc->code+sizeof(char),tmp->bc->size);
//							fklReCodeCat(pushProc,tmp->bc);
//							tmp->l[0]->cpc+=pushProc->size;
//							FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,pushProc->size);
//							fklFreeByteCode(pushProc);
//							FklByteCode* call=fklCreateByteCode(sizeof(char));
//							call->code[0]=FKL_OP_CALL;
//							fklCodeCat(tmp->bc,call);
//							tmp->l[tmp->ls-1]->cpc+=call->size;
//							fklFreeByteCode(call);
//						}
//						FklAstCptr* pExportSymbols=fklNextCptr(fklGetFirstCptr(exportCptr));
//						for(;pExportSymbols;pExportSymbols=fklNextCptr(pExportSymbols))
//						{
//							if(pExportSymbols->type!=FKL_TYPE_ATM
//									||pExportSymbols->u.atom->type!=FKL_TYPE_SYM)
//							{
//								fklPrintCompileError(exportCptr,FKL_ERR_SYNTAXERROR,tmpInter);
//								fklDeleteCptr(begin);
//								free(begin);
//								fklFreeByteCodelnt(tmp);
//								chdir(tmpInter->prev->curDir);
//								tmpInter->lnt=NULL;
//								fklFreeAllMacroThenDestroyCompEnv(tmpCurEnv);
//								fklFreeIntpr(tmpInter);
//								if(libPrefix)
//									free(libPrefix);
//								fklFreeMemMenager(memMenager);
//								fklFreeStringArray(partsOfPath,count);
//								free(list);
//								while(!fklIsPtrStackEmpty(tokenStack))
//									fklFreeToken(fklPopPtrStack(tokenStack));
//								fklFreePtrStack(tokenStack);
//								return NULL;
//							}
//							FklAstAtom* pSymbol=pExportSymbols->u.atom;
//							FklCompEnvHashItem* tmpDef=NULL;
//							FklString* symbolWouldExport=NULL;
//							if(libPrefix)
//								symbolWouldExport=fklStringAppend(libPrefix,pSymbol->value.str);
//							else
//								symbolWouldExport=fklCopyString(pSymbol->value.str);
//							tmpDef=fklFindCompDef(symbolWouldExport,tmpInter->glob);
//							if(!tmpDef)
//							{
//								fklPrintCompileError(pExportSymbols,FKL_ERR_SYMUNDEFINE,tmpInter);
//								fklDeleteCptr(begin);
//								free(begin);
//								chdir(tmpInter->prev->curDir);
//								tmpInter->lnt=NULL;
//								fklFreeAllMacroThenDestroyCompEnv(tmpCurEnv);
//								fklFreeIntpr(tmpInter);
//								if(libPrefix)
//									free(libPrefix);
//								free(symbolWouldExport);
//								fklFreeMemMenager(memMenager);
//								fklFreeStringArray(partsOfPath,count);
//								free(list);
//								while(!fklIsPtrStackEmpty(tokenStack))
//									fklFreeToken(fklPopPtrStack(tokenStack));
//								fklFreePtrStack(tokenStack);
//								return NULL;
//							}
//							else
//								fklAddCompDef(symbolWouldExport,curEnv);
//							free(symbolWouldExport);
//						}
//						fklCodelntCopyCat(curEnv->proc,tmp);
//						FklPreMacro* headOfMacroOfImportedFile=tmpCurEnv->macro;
//						tmpCurEnv->macro=NULL;
//						while(headOfMacroOfImportedFile)
//						{
//							fklAddDefinedMacro(headOfMacroOfImportedFile,curEnv);
//							FklPreMacro* prev=headOfMacroOfImportedFile;
//							headOfMacroOfImportedFile=headOfMacroOfImportedFile->next;
//							free(prev);
//						}
//						fklDestroyCompEnv(tmpCurEnv);
//						tmpInter->glob->macro=NULL;
//						fklDeleteCptr(begin);
//						free(begin);
//						free(list);
//						while(!fklIsPtrStackEmpty(tokenStack))
//							fklFreeToken(fklPopPtrStack(tokenStack));
//						break;
//					}
//				}
//				fklDeleteCptr(begin);
//				free(begin);
//			}
//			else if(begin==NULL&&!fklIsAllComment(tokenStack))
//			{
//				fprintf(stderr,"error of reader:Invalid expression at line %d of %s\n",tmpInter->curline,tmpInter->filename);
//				free(prev);
//				while(!fklIsPtrStackEmpty(tokenStack))
//					fklFreeToken(fklPopPtrStack(tokenStack));
//				if(list)
//					free(list);
//				break;
//			}
//			while(!fklIsPtrStackEmpty(tokenStack))
//				fklFreeToken(fklPopPtrStack(tokenStack));
//			free(list);
//			list=NULL;
//		}
//		if(libByteCodelnt&&!libByteCodelnt->bc->size)
//		{
//			state->state=(tmp)?FKL_ERR_LIBUNDEFINED:0;
//			state->place=(tmp)?pairOfpPartsOfPath:NULL;
//			tmpInter->lnt=NULL;
//			fklFreeIntpr(tmpInter);
//			if(libPrefix)
//				free(libPrefix);
//			fklFreeMemMenager(memMenager);
//			fklFreeStringArray(partsOfPath,count);
//			fklFreePtrStack(tokenStack);
//			return NULL;
//		}
//		chdir(tmpInter->prev->curDir);
//		tmpInter->lnt=NULL;
//		fklFreeIntpr(tmpInter);
//		fklFreePtrStack(tokenStack);
//		if(libPrefix)
//			free(libPrefix);
//		fklFreeStringArray(partsOfPath,count);
//	}
//	if(tmp)
//	{
//		fklDeleteMem(tmp,memMenager);
//		fklFreeMemMenager(memMenager);
//	}
//	chdir(inter->curDir);
//	return tmp;
//}
//
//FklByteCodelnt* fklCompileTry(FklAstCptr* objCptr,FklCompEnv* curEnv,FklInterpreter* inter,FklErrorState* state)
//{
//	FklCompEnv* createEnv=fklCreateCompEnv(curEnv);
//	FklAstCptr* pCatchExpression=fklNextCptr(fklGetFirstCptr(objCptr));
//	FklAstCptr* pExpression=fklNextCptr(pCatchExpression);
//	if(!fklIsCatchExpression(pCatchExpression))
//	{
//		state->state=FKL_ERR_SYNTAXERROR;
//		state->place=objCptr;
//		return NULL;
//	}
//	FklByteCodelnt* expressionByteCodelnt=fklCreateByteCodelnt(fklCreateByteCode(0));
//	if(pExpression)
//	{
//		FklByteCode* resTp=fklCreateByteCode(1);
//		FklByteCode* setTp=fklCreateByteCode(1);
//		FklByteCode* popTp=fklCreateByteCode(1);
//		resTp->code[0]=FKL_OP_RES_TP;
//		setTp->code[0]=FKL_OP_SET_TP;
//		popTp->code[0]=FKL_OP_POP_TP;
//		for(;pExpression;pExpression=fklNextCptr(pExpression))
//		{
//			FklByteCodelnt* tmp=fklCompile(pExpression,createEnv,inter,state);
//			if(state->state)
//			{
//				fklFreeByteCodeAndLnt(expressionByteCodelnt);
//				fklFreeByteCode(resTp);
//				fklFreeByteCode(setTp);
//				fklFreeByteCode(popTp);
//				return NULL;
//			}
//			if(tmp->bc->size&&expressionByteCodelnt->bc->size)
//			{
//				fklReCodeCat(resTp,tmp->bc);
//				tmp->l[0]->cpc+=1;
//				FKL_INCREASE_ALL_SCP(tmp->l+1,tmp->ls-1,resTp->size);
//			}
//			fklCodeLntCat(expressionByteCodelnt,tmp);
//			fklFreeByteCodelnt(tmp);
//		}
//		fklReCodeCat(setTp,expressionByteCodelnt->bc);
//		if(!expressionByteCodelnt->l)
//		{
//			expressionByteCodelnt->ls=1;
//			expressionByteCodelnt->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//			FKL_ASSERT(expressionByteCodelnt->l);
//			expressionByteCodelnt->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,expressionByteCodelnt->bc->size,objCptr->curline);
//		}
//		else
//		{
//			expressionByteCodelnt->l[0]->cpc+=1;
//			FKL_INCREASE_ALL_SCP(expressionByteCodelnt->l+1,expressionByteCodelnt->ls-1,setTp->size);
//		}
//		fklCodeCat(expressionByteCodelnt->bc,popTp);
//		expressionByteCodelnt->l[expressionByteCodelnt->ls-1]->cpc+=popTp->size;
//		fklFreeByteCode(resTp);
//		fklFreeByteCode(setTp);
//		fklFreeByteCode(popTp);
//	}
//	else
//	{
//		FklByteCode* pushnil=fklCreateByteCode(1);
//		pushnil->code[0]=FKL_OP_PUSH_NIL;
//		fklCodeCat(expressionByteCodelnt->bc,pushnil);
//		fklFreeByteCode(pushnil);
//		expressionByteCodelnt->ls=1;
//		expressionByteCodelnt->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
//		FKL_ASSERT(expressionByteCodelnt->l);
//		expressionByteCodelnt->l[0]=fklCreateLineNumTabNodeWithFilename(inter->filename,0,expressionByteCodelnt->bc->size,objCptr->curline);
//	}
//	fklFreeAllMacroThenDestroyCompEnv(createEnv);
//	FklByteCode* pushProc=fklCreateByteCode(sizeof(char)+sizeof(expressionByteCodelnt->bc->size));
//	FklByteCode* call=fklCreateByteCode(1);
//	call->code[0]=FKL_OP_CALL;
//	pushProc->code[0]=FKL_OP_PUSH_PROC;
//	fklSetU64ToByteCode(pushProc->code+sizeof(char),expressionByteCodelnt->bc->size);
//	fklReCodeCat(pushProc,expressionByteCodelnt->bc);
//	expressionByteCodelnt->l[0]->cpc+=pushProc->size;
//	FKL_INCREASE_ALL_SCP(expressionByteCodelnt->l+1,expressionByteCodelnt->ls-1,pushProc->size);
//	fklCodeCat(expressionByteCodelnt->bc,call);
//	expressionByteCodelnt->l[expressionByteCodelnt->ls-1]->cpc+=call->size;
//	fklFreeByteCode(pushProc);
//	fklFreeByteCode(call);
//	FklAstCptr* pErrSymbol=fklNextCptr(fklGetFirstCptr(pCatchExpression));
//	FklAstCptr* pHandlerExpression=NULL;
//	if(!pErrSymbol
//			||pErrSymbol->type!=FKL_TYPE_ATM
//			||pErrSymbol->u.atom->type!=FKL_TYPE_SYM
//			||!(pHandlerExpression=fklNextCptr(pErrSymbol)))
//	{
//		state->state=FKL_ERR_SYNTAXERROR;
//		state->place=objCptr;
//		fklFreeByteCodeAndLnt(expressionByteCodelnt);
//		return NULL;
//	}
//	FklString* errSymbol=pErrSymbol->u.atom->value.str;
//	FklSid_t sid=fklAddSymbolToGlob(errSymbol)->id;
//	FklPtrStack* handlerByteCodelntStack=fklCreatePtrStack(32,16);
//	for(;pHandlerExpression;pHandlerExpression=fklNextCptr(pHandlerExpression))
//	{
//		if(pHandlerExpression->type!=FKL_TYPE_PAIR
//				||(fklGetFirstCptr(pHandlerExpression)->type!=FKL_TYPE_NIL&&!fklIsListCptr(fklGetFirstCptr(pHandlerExpression))))
//		{
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=objCptr;
//			fklFreeByteCodeAndLnt(expressionByteCodelnt);
//			return NULL;
//		}
//		FklAstCptr* pErrorTypes=fklGetFirstCptr(pHandlerExpression);
//		uint32_t errTypeNum=fklLengthListCptr(pErrorTypes);
//		FklSid_t* errTypeIds=(FklSid_t*)malloc(sizeof(FklSid_t)*errTypeNum);
//		FKL_ASSERT(errTypeIds);
//		FklAstCptr* firErrSymbol=fklGetFirstCptr(pErrorTypes);
//		for(uint32_t i=0;i<errTypeNum;i++)
//		{
//			if(firErrSymbol->type!=FKL_TYPE_ATM||firErrSymbol->u.atom->type!=FKL_TYPE_SYM)
//			{
//				state->state=FKL_ERR_SYNTAXERROR;
//				state->place=objCptr;
//				free(errTypeIds);
//				fklFreeByteCodeAndLnt(expressionByteCodelnt);
//				fklFreePtrStack(handlerByteCodelntStack);
//				return NULL;
//			}
//			errTypeIds[i]=fklAddSymbolToGlob(firErrSymbol->u.atom->value.str)->id;
//			firErrSymbol=fklNextCptr(firErrSymbol);
//		}
//		FklAstCptr* begin=fklNextCptr(pErrorTypes);
//		//if(!begin||pErrorTypes->type!=FKL_TYPE_ATM||pErrorTypes->u.atom->type!=FKL_TYPE_SYM)
//		//{
//		//	state->state=FKL_ERR_SYNTAXERROR;
//		//	state->place=objCptr;
//		//	fklFreeByteCodeAndLnt(expressionByteCodelnt);
//		//	fklFreePtrStack(handlerByteCodelntStack);
//		//	return NULL;
//		//}
//		FklCompEnv* tmpEnv=fklCreateCompEnv(curEnv);
//		fklAddCompDef(errSymbol,tmpEnv);
//		FklByteCodelnt* t=NULL;
//		for(;begin;begin=fklNextCptr(begin))
//		{
//			FklByteCodelnt* tmp1=fklCompile(begin,tmpEnv,inter,state);
//			if(state->state)
//			{
//				free(errTypeIds);
//				fklFreeByteCodeAndLnt(expressionByteCodelnt);
//				fklFreePtrStack(handlerByteCodelntStack);
//				fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//				return NULL;
//			}
//			if(!t)
//				t=tmp1;
//			else
//			{
//				FklByteCode* resTp=fklCreateByteCode(1);
//				resTp->code[0]=FKL_OP_RES_TP;
//				fklReCodeCat(resTp,tmp1->bc);
//				tmp1->l[0]->cpc+=1;
//				FKL_INCREASE_ALL_SCP(tmp1->l+1,tmp1->ls-1,resTp->size);
//				fklFreeByteCode(resTp);
//			}
//		}
//		if(!t)
//		{
//			state->state=FKL_ERR_SYNTAXERROR;
//			state->place=objCptr;
//			free(errTypeIds);
//			fklFreeByteCodeAndLnt(expressionByteCodelnt);
//			fklFreePtrStack(handlerByteCodelntStack);
//			fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//			return NULL;
//		}
//		fklFreeAllMacroThenDestroyCompEnv(tmpEnv);
//		FklByteCode* setTp=fklCreateByteCode(1);
//		FklByteCode* popTp=fklCreateByteCode(1);
//		setTp->code[0]=FKL_OP_SET_TP;
//		popTp->code[0]=FKL_OP_POP_TP;
//		fklReCodeCat(setTp,t->bc);
//		t->l[0]->cpc+=1;
//		FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,setTp->size);
//		fklCodeCat(t->bc,popTp);
//		t->l[t->ls-1]->cpc+=popTp->size;
//		fklFreeByteCode(setTp);
//		fklFreeByteCode(popTp);
//		//char* errorType=pErrorType->u.atom->value.str;
//		//FklSid_t typeid=fklAddSymbolToGlob(errorType)->id;
//		FklByteCode* errorTypeByteCode=fklCreateByteCode(sizeof(uint32_t)+sizeof(FklSid_t)*errTypeNum+sizeof(t->bc->size));
//		fklSetU32ToByteCode(errorTypeByteCode->code,errTypeNum);
//		for(uint32_t i=0;i<errTypeNum;i++)
//			fklSetSidToByteCode(errorTypeByteCode->code+sizeof(uint32_t)+i*sizeof(FklSid_t),errTypeIds[i]);
//		fklSetU64ToByteCode(errorTypeByteCode->code+sizeof(uint32_t)+errTypeNum*sizeof(FklSid_t),t->bc->size);
//		fklReCodeCat(errorTypeByteCode,t->bc);
//		t->l[0]->cpc+=errorTypeByteCode->size;
//		FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,errorTypeByteCode->size);
//		fklFreeByteCode(errorTypeByteCode);
//		fklPushPtrStack(t,handlerByteCodelntStack);
//		free(errTypeIds);
//	}
//	FklByteCodelnt* t=expressionByteCodelnt;
//	size_t numOfHandlerByteCode=handlerByteCodelntStack->top;
//	while(!fklIsPtrStackEmpty(handlerByteCodelntStack))
//	{
//		FklByteCodelnt* tmp=fklPopPtrStack(handlerByteCodelntStack);
//		fklReCodeLntCat(tmp,t);
//		fklFreeByteCodelnt(tmp);
//	}
//	fklFreePtrStack(handlerByteCodelntStack);
//	FklByteCode* header=fklCreateByteCode(sizeof(FklSid_t)+sizeof(uint32_t)+sizeof(char));
//	header->code[0]=FKL_OP_PUSH_TRY;
//	fklSetSidToByteCode(header->code+sizeof(char),sid);
//	fklSetU32ToByteCode(header->code+sizeof(char)+sizeof(FklSid_t),numOfHandlerByteCode);
//	fklReCodeCat(header,t->bc);
//	t->l[0]->cpc+=header->size;
//	FKL_INCREASE_ALL_SCP(t->l+1,t->ls-1,header->size);
//	fklFreeByteCode(header);
//	FklByteCode* popTry=fklCreateByteCode(sizeof(char));
//	popTry->code[0]=FKL_OP_POP_TRY;
//	fklCodeCat(t->bc,popTry);
//	t->l[t->ls-1]->cpc+=popTry->size;
//	fklFreeByteCode(popTry);
//	return t;
//}
//
//void fklPrintCompileError(const FklAstCptr* obj,FklBuiltInErrorType type,FklInterpreter* inter)
//{
//	if(type==FKL_ERR_DUMMY)
//		return;
//	fprintf(stderr,"error of compiling: ");
//	switch(type)
//	{
//		case FKL_ERR_SYMUNDEFINE:
//			fprintf(stderr,"Symbol ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			fprintf(stderr," is undefined");
//			break;
//		case FKL_ERR_SYNTAXERROR:
//			fprintf(stderr,"Invalid syntax ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_INVALIDEXPR:
//			fprintf(stderr,"Invalid expression ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_CIRCULARLOAD:
//			fprintf(stderr,"Circular load file ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_INVALIDPATTERN:
//			fprintf(stderr,"Invalid string match pattern ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_INVALID_MACRO_PATTERN:
//			fprintf(stderr,"Invalid macro pattern ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_MACROEXPANDFAILED:
//			fprintf(stderr,"Failed to expand macro in ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_LIBUNDEFINED:
//			fprintf(stderr,"Library ");
//			if(obj!=NULL)fklPrintCptr(obj,stderr);
//			fprintf(stderr," undefined");
//			break;
//		case FKL_ERR_FILEFAILURE:
//			fprintf(stderr,"failed for file:");
//			fklPrintCptr(obj,stderr);
//			break;
//		case FKL_ERR_IMPORTFAILED:
//			fprintf(stderr,"failed for importing module:");
//			fklPrintCptr(obj,stderr);
//			break;
//		default:
//			fprintf(stderr,"unknown compiling error.");
//			break;
//	}
//	if(inter!=NULL)
//	{
//		if(inter->filename)
//			fprintf(stderr," at line %lu of file %s\n",(obj==NULL)?inter->curline:obj->curline,inter->filename);
//		else
//			fprintf(stderr," at line %lu\n",(obj==NULL)?inter->curline:obj->curline);
//	}
//}
//
//FklPreDef* fklAddDefine(const FklString* symbol,const FklAstCptr* objCptr,FklPreEnv* curEnv)
//{
//	if(curEnv->symbols==NULL)
//	{
//		curEnv->symbols=fklCreateDefines(symbol);
//		fklReplaceCptr(&curEnv->symbols->obj,objCptr);
//		curEnv->symbols->next=NULL;
//		return curEnv->symbols;
//	}
//	else
//	{
//		FklPreDef* curDef=fklFindDefine(symbol,curEnv);
//		if(curDef==NULL)
//		{
//			curDef=fklCreateDefines(symbol);
//			curDef->next=curEnv->symbols;
//			curEnv->symbols=curDef;
//			fklReplaceCptr(&curDef->obj,objCptr);
//		}
//		else
//			fklReplaceCptr(&curDef->obj,objCptr);
//		return curDef;
//	}
//}
//
//FklPreDef* fklCreateDefines(const FklString* name)
//{
//	FklPreDef* tmp=(FklPreDef*)malloc(sizeof(FklPreDef));
//	FKL_ASSERT(tmp);
//	tmp->symbol=fklCopyString(name);
//	tmp->obj=(FklAstCptr){NULL,0,FKL_TYPE_NIL,{NULL}};
//	tmp->next=NULL;
//	return tmp;
//}
//
//FklPreDef* fklFindDefine(const FklString* name,const FklPreEnv* curEnv)
//{
//	if(curEnv->symbols==NULL)return NULL;
//	else
//	{
//		FklPreDef* curDef=curEnv->symbols;
//		while(curDef&&fklStringcmp(name,curDef->symbol))
//			curDef=curDef->next;
//		return curDef;
//	}
//}
//
//FklCompEnvHashItem* fklAddCompDefCstr(const char* name,FklCompEnv* curEnv)
//{
//	return (FklCompEnvHashItem*)fklPutNoRpHashItem(createCompEnvHashItem(fklAddSymbolToGlobCstr(name)->id),curEnv->defs);
//}
//
//FklCompEnvHashItem* fklAddCompDef(const FklString* name,FklCompEnv* curEnv)
//{
//	return (FklCompEnvHashItem*)fklPutNoRpHashItem(createCompEnvHashItem(fklAddSymbolToGlob(name)->id),curEnv->defs);
//}
//
//FklCompEnvHashItem* fklFindCompDefBySid(FklSid_t id,FklCompEnv* curEnv)
//{
//	return (FklCompEnvHashItem*)fklGetHashItem(&id,curEnv->defs);
//}
//
//FklCompEnvHashItem* fklAddCompDefBySid(FklSid_t id,FklCompEnv* curEnv)
//{
//	return (FklCompEnvHashItem*)fklPutNoRpHashItem(createCompEnvHashItem(id),curEnv->defs);
//}
//
//FklCompEnv* fklCreateGlobCompEnv(FklCompEnv* prev)
//{
//	FklCompEnv* tmp=(FklCompEnv*)malloc(sizeof(FklCompEnv));
//	FKL_ASSERT(tmp);
//	tmp->prev=prev;
//	if(prev)
//		prev->refcount+=1;
//	tmp->prefix=NULL;
//	tmp->exp=NULL;
//	tmp->defs=fklCreateHashTable(512,4,2,0.75,1,&CompEnvHashMethodTable);
//	tmp->n=0;
//	tmp->macro=NULL;
//	tmp->keyWords=NULL;
//	tmp->proc=fklCreateByteCodelnt(fklCreateByteCode(0));
//	tmp->refcount=0;
//	fklInitCompEnv(tmp);
//	return tmp;
//}
//
//FklCompEnv* fklCreateCompEnv(FklCompEnv* prev)
//{
//	FklCompEnv* tmp=(FklCompEnv*)malloc(sizeof(FklCompEnv));
//	FKL_ASSERT(tmp);
//	tmp->prev=prev;
//	if(prev)
//		prev->refcount+=1;
//	tmp->prefix=NULL;
//	tmp->exp=NULL;
//	tmp->defs=fklCreateHashTable(8,4,2,0.75,1,&CompEnvHashMethodTable);
//	tmp->n=0;
//	tmp->macro=NULL;
//	tmp->keyWords=NULL;
//	tmp->proc=fklCreateByteCodelnt(fklCreateByteCode(0));
//	tmp->refcount=0;
//	return tmp;
//}
//
//FklPreEnv* fklCreateEnv(FklPreEnv* prev)
//{
//	FklPreEnv* curEnv=NULL;
//	curEnv=(FklPreEnv*)malloc(sizeof(FklPreEnv));
//	FKL_ASSERT(curEnv);
//	if(prev!=NULL)prev->next=curEnv;
//	curEnv->prev=prev;
//	curEnv->next=NULL;
//	curEnv->symbols=NULL;
//	return curEnv;
//}
//
//void fklDestroyEnv(FklPreEnv* objEnv)
//{
//	if(objEnv==NULL)return;
//	while(objEnv!=NULL)
//	{
//		FklPreDef* delsym=objEnv->symbols;
//		while(delsym!=NULL)
//		{
//			free(delsym->symbol);
//			fklDeleteCptr(&delsym->obj);
//			FklPreDef* prev=delsym;
//			delsym=delsym->next;
//			free(prev);
//		}
//		FklPreEnv* prev=objEnv;
//		objEnv=objEnv->next;
//		free(prev);
//	}
//}
//
//void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env)
//{
//	if(env)
//	{
//		fklFreeAllMacro(env->macro);
//		env->macro=NULL;
//		fklDestroyCompEnv(env);
//	}
//}
//
//FklCompEnvHashItem* fklFindCompDef(const FklString* name,FklCompEnv* curEnv)
//{
//	return (FklCompEnvHashItem*)fklGetHashItem(&fklAddSymbolToGlob(name)->id,curEnv->defs);
//}
//
//FklInterpreter* fklCreateIntpr(const char* filename,FILE* file,FklCompEnv* env,FklLineNumberTable* lnt)
//{
//	FklInterpreter* tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter));
//	FKL_ASSERT(tmp);
//	if(file!=stdin&&filename!=NULL)
//	{
//		char* rp=fklRealpath(filename);
//		if(!rp&&!file)
//		{
//			perror(filename);
//			exit(EXIT_FAILURE);
//		}
//		tmp->curDir=fklGetDir(rp);
//		tmp->filename=fklRelpath(fklGetMainFileRealPath(),rp);
//		tmp->realpath=rp;
//	}
//	else
//	{
//		tmp->curDir=getcwd(NULL,0);
//		tmp->filename=NULL;
//		tmp->realpath=NULL;
//	}
//	tmp->file=file;
//	tmp->curline=1;
//	tmp->prev=NULL;
//	if(lnt)
//		tmp->lnt=lnt;
//	else
//		tmp->lnt=fklCreateLineNumTable();
//	if(env)
//	{
//		tmp->glob=env;
//		return tmp;
//	}
//	tmp->glob=fklCreateGlobCompEnv(NULL);
//	return tmp;
//}
//
//void fklFreeIntpr(FklInterpreter* inter)
//{
//	if(inter->filename)
//		free(inter->filename);
//	if(inter->file!=stdin)
//		fclose(inter->file);
//	if(inter->realpath)
//		free(inter->realpath);
//	free(inter->curDir);
//	fklFreeAllMacroThenDestroyCompEnv(inter->glob);
//	if(inter->lnt)fklFreeLineNumberTable(inter->lnt);
//	free(inter);
//}
//
//void fklDestroyCompEnv(FklCompEnv* objEnv)
//{
//	if(objEnv==NULL)return;
//	while(objEnv)
//	{
//		if(!objEnv->refcount)
//		{
//			FklCompEnv* curEnv=objEnv;
//			objEnv=objEnv->prev;
//			fklFreeHashTable(curEnv->defs);
//			fklFreeByteCodeAndLnt(curEnv->proc);
//			fklFreeAllMacro(curEnv->macro);
//			fklFreeAllKeyWord(curEnv->keyWords);
//			free(curEnv);
//		}
//		else
//		{
//			objEnv->refcount-=1;
//			break;
//		}
//	}
//}
//
//int fklHasLoadSameFile(const char* realpath,const FklInterpreter* inter)
//{
//	while(inter!=NULL)
//	{
//		if(inter->realpath&&!strcmp(inter->realpath,realpath))
//			return 1;
//		inter=inter->prev;
//	}
//	return 0;
//}
//
//FklInterpreter* fklGetFirstIntpr(FklInterpreter* inter)
//{
//	while(inter->prev!=NULL)
//		inter=inter->prev;
//	return inter;
//}
//
//void fklFreeAllMacro(FklPreMacro* head)
//{
//	FklPreMacro* cur=head;
//	while(cur!=NULL)
//	{
//		FklPreMacro* prev=cur;
//		cur=cur->next;
//		fklDeleteCptr(prev->pattern);
//		free(prev->pattern);
//		fklDestroyCompEnv(prev->macroEnv);
//		fklFreeByteCodeAndLnt(prev->proc);
//		free(prev);
//	}
//}
//
//char* fklGetLastWorkDir(FklInterpreter* inter)
//{
//	while(inter->prev!=NULL)
//		inter=inter->prev;
//	return inter->curDir;
//}
//
//void fklFreeAllKeyWord(FklKeyWord* head)
//{
//	FklKeyWord* cur=head;
//	while(cur!=NULL)
//	{
//		FklKeyWord* prev=cur;
//		cur=cur->next;
//		free(prev->word);
//		free(prev);
//	}
//}
//
//FklInterpreter* fklCreateTmpIntpr(const char* filename,FILE* fp)
//{
//	FklInterpreter* tmp=(FklInterpreter*)malloc(sizeof(FklInterpreter));
//	FKL_ASSERT(tmp);
//	if(fp!=stdin&&filename)
//	{
//		char* rp=fklRealpath(filename);
//		if(rp==NULL)
//		{
//			perror(filename);
//			exit(EXIT_FAILURE);
//		}
//		tmp->filename=fklRelpath(fklGetMainFileRealPath(),rp);
//		tmp->curDir=fklGetDir(rp);
//		tmp->realpath=rp;
//	}
//	else
//	{
//		tmp->curDir=NULL;
//		tmp->filename=NULL;
//		tmp->realpath=NULL;
//	}
//	tmp->file=fp;
//	tmp->curline=1;
//	tmp->prev=NULL;
//	tmp->glob=NULL;
//	tmp->lnt=NULL;
//	return tmp;
//}
//
//typedef struct
//{
//	FklCompEnv* env;
//	size_t cp;
//	FklSid_t sid;
//}MayUndefine;
//
//static MayUndefine* createMayUndefine(FklCompEnv* env,size_t cp,FklSid_t sid)
//{
//	MayUndefine* r=(MayUndefine*)malloc(sizeof(MayUndefine));
//	FKL_ASSERT(r);
//	env->refcount+=1;
//	r->env=env;
//	r->cp=cp;
//	r->sid=sid;
//	return r;
//}
//
//static void freeMayUndefine(MayUndefine* t)
//{
//	fklDestroyCompEnv(t->env);
//	free(t);
//}
//
//void fklPrintUndefinedSymbol(FklByteCodelnt* code)
//{
//	FklUintStack* cpcstack=fklCreateUintStack(32,16);
//	FklUintStack* scpstack=fklCreateUintStack(32,16);
//	FklPtrStack* envstack=fklCreatePtrStack(32,16);
//	FklPtrStack* mayUndefined=fklCreatePtrStack(32,16);
//	FklByteCode* bc=code->bc;
//	fklPushUintStack(0,cpcstack);
//	fklPushUintStack(bc->size,scpstack);
//	FklCompEnv* globEnv=fklCreateGlobCompEnv(NULL);
//	fklPushPtrStack(globEnv,envstack);
//	while((!fklIsUintStackEmpty(cpcstack))&&(!fklIsUintStackEmpty(scpstack)))
//	{
//		uint64_t i=fklPopUintStack(cpcstack);
//		uint64_t end=i+fklPopUintStack(scpstack);
//		FklCompEnv* curEnv=fklPopPtrStack(envstack);
//		while(i<end)
//		{
//			FklOpcode opcode=(FklOpcode)(bc->code[i]);
//			switch(fklGetOpcodeArgLen(opcode))
//			{
//				case -4:
//					{
//						i+=sizeof(FklSid_t)+sizeof(char);
//						uint32_t handlerNum=fklGetU32FromByteCode(bc->code+i);
//						i+=sizeof(uint32_t);
//						int j=0;
//						for(;j<handlerNum;j++)
//						{
//							uint32_t num=fklGetU32FromByteCode(bc->code+i);
//							i+=sizeof(num);
//							i+=sizeof(FklSid_t)*num;
//							uint32_t pCpc=fklGetU64FromByteCode(bc->code+i);
//							i+=sizeof(uint64_t);
//							i+=pCpc;
//						}
//					}
//					break;
//				case -3:
//					{
//						uint32_t scope=fklGetU32FromByteCode(bc->code+i+sizeof(char));
//						FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char)+sizeof(scope));
//						if(scope)
//						{
//							FklCompEnv* env=curEnv;
//							for(uint32_t i=1;i<scope;i++)
//								env=curEnv->prev;
//							fklAddCompDefBySid(id,env);
//						}
//						else
//						{
//							FklCompEnvHashItem* def=NULL;
//							for(FklCompEnv* e=curEnv;e;e=e->prev)
//							{
//								def=fklFindCompDefBySid(id,e);
//								if(def)
//									break;
//							}
//							if(!def)
//								fklPushPtrStack(createMayUndefine(curEnv,i,id),mayUndefined);
//						}
//					}
//					i+=sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t);
//					break;
//				case -2:
//					fklPushUintStack(i+sizeof(char)+sizeof(uint64_t),cpcstack);
//					{
//						fklPushUintStack(fklGetU64FromByteCode(bc->code+i+sizeof(char)),scpstack);
//						FklCompEnv* nextEnv=fklCreateCompEnv(curEnv);
//						fklPushPtrStack(nextEnv,envstack);
//					}
//					i+=sizeof(char)+sizeof(uint64_t)+fklGetU64FromByteCode(bc->code+i+sizeof(char));
//					break;
//				case -1:
//					i+=sizeof(char)+sizeof(uint64_t)+fklGetU64FromByteCode(bc->code+i+sizeof(char));
//					break;
//				case 0:
//					if(opcode==FKL_OP_PUSH_R_ENV)
//						curEnv=fklCreateCompEnv(curEnv);
//					else if(opcode==FKL_OP_POP_R_ENV)
//					{
//						FklCompEnv* p=curEnv->prev;
//						fklDestroyCompEnv(curEnv);
//						curEnv=p;
//					}
//					i+=sizeof(char);
//					break;
//				case 1:
//					i+=sizeof(char)+sizeof(char);
//					break;
//				case 4:
//					i+=sizeof(char)+sizeof(int32_t);
//					break;
//				case 8:
//					switch(opcode)
//					{
//						case FKL_OP_PUSH_F64:
//							break;
//						case FKL_OP_PUSH_I64:
//							break;
//						case FKL_OP_POP_ARG:
//						case FKL_OP_POP_REST_ARG:
//						case FKL_OP_PUSH_VAR:
//							{
//								FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char));
//								if(opcode==FKL_OP_POP_ARG||opcode==FKL_OP_POP_REST_ARG)
//									fklAddCompDefBySid(id,curEnv);
//								else if(opcode==FKL_OP_PUSH_VAR)
//								{
//									FklCompEnvHashItem* def=NULL;
//									for(FklCompEnv* e=curEnv;e;e=e->prev)
//									{
//										def=fklFindCompDefBySid(id,e);
//										if(def)
//											break;
//									}
//									if(!def)
//										fklPushPtrStack(createMayUndefine(curEnv,i,id),mayUndefined);
//								}
//							}
//							break;
//						default:break;
//					}
//					i+=sizeof(char)+sizeof(int64_t);
//					break;
//			}
//		}
//		fklDestroyCompEnv(curEnv);
//	}
//	fklFreeUintStack(cpcstack);
//	fklFreeUintStack(scpstack);
//	fklFreePtrStack(envstack);
//	for(uint32_t i=0;i<mayUndefined->top;i++)
//	{
//		MayUndefine* cur=mayUndefined->base[i];
//		FklCompEnv* curEnv=cur->env;
//		FklCompEnvHashItem* def=NULL;
//		for(FklCompEnv* e=curEnv;e;e=e->prev)
//		{
//			def=fklFindCompDefBySid(cur->sid,e);
//			if(def)
//				break;
//		}
//		if(!def)
//		{
//			FklLineNumberTable table={.list=code->l,.num=code->ls};
//			FklLineNumTabNode* node=fklFindLineNumTabNode(cur->cp,&table);
//			fprintf(stderr,"warning of compiling: Symbol \"");
//			fklPrintString(fklGetGlobSymbolWithId(cur->sid)->symbol,stderr);
//			fprintf(stderr,"\" is undefined at line %d of ",node->line);
//			fklPrintString(fklGetGlobSymbolWithId(node->fid)->symbol,stderr);
//			fputc('\n',stderr);
//		}
//		freeMayUndefine(cur);
//	}
//	fklFreePtrStack(mayUndefined);
//}
