#include<fakeLisp/ast.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/parser.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<ctype.h>

static int copyAndAddToTail(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type==FKL_PAIR)fir=&fir->u.pair->cdr;
	if(fir->type!=FKL_NIL)
		return 1;
	fklReplaceCptr(fir,sec);
	return 0;
}

static int copyAndAddToList(FklAstCptr* fir,const FklAstCptr* sec)
{
	while(fir->type==FKL_PAIR)fir=&fir->u.pair->cdr;
	if(fir->type!=FKL_NIL)
		return 1;
	fir->type=FKL_PAIR;
	fir->u.pair=fklNewPair(sec->curline,fir->outer);
	fklReplaceCptr(&fir->u.pair->car,sec);
	return 0;
}

static FklVMenv* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,FklVMheap* heap)
{
	FklVMenv* vEnv=fklNewVMenv(NULL);
	fklInitGlobEnv(vEnv,heap);
	if(cEnv->proc->bc->size)
	{
		FklVM* tmpVM=fklNewTmpVM(NULL);
		fklIncreaseVMenvRefcount(vEnv);
		fklInitVMRunningResource(tmpVM,vEnv,heap,cEnv->proc,0,cEnv->proc->bc->size);
		int i=fklRunVM(tmpVM);
		if(i==1)
		{
			fklFreeVMenv(vEnv);
			fklUninitVMRunningResource(tmpVM);
			return NULL;
		}
		fklCodelntCopyCat(t,cEnv->proc);
		fklUninitVMRunningResource(tmpVM);
	}
	return vEnv;
}

int fklEqByteString(const FklAstByteString* fir,const FklAstByteString* sec)
{
	if(fir->size!=sec->size)return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

void fklPrintCptr(const FklAstCptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	FklAstPair* tmpPair=(objCptr->type==FKL_PAIR)?objCptr->u.pair:NULL;
	FklAstPair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==FKL_PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				putc(' ',out);
				objPair=objPair->cdr.u.pair;
				objCptr=&objPair->car;
			}
			else
			{
				putc('(',out);
				objPair=objCptr->u.pair;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==FKL_ATM||objCptr->type==FKL_NIL)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr&&objCptr->type==FKL_ATM)putc(',',out);
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==FKL_NIL)
					||(objCptr->outer==NULL&&objCptr->type==FKL_NIL))fputs("()",out);
			if(objCptr->type!=FKL_NIL)
			{
				FklAstAtom* tmpAtm=objCptr->u.atom;
				switch(tmpAtm->type)
				{
					case FKL_SYM:
						fprintf(out,"%s",tmpAtm->value.str);
						break;
					case FKL_STR:
						fklPrintRawString(tmpAtm->value.str,out);
						break;
					case FKL_I32:
						fprintf(out,"%d",tmpAtm->value.i32);
						break;
					case FKL_I64:
						fprintf(out,"%ld",tmpAtm->value.i64);
						break;
					case FKL_F64:
						fprintf(out,"%lf",tmpAtm->value.f64);
						break;
					case FKL_CHR:
						fklPrintRawChar(tmpAtm->value.chr,out);
						break;
					case FKL_BYTS:
						fklPrintByteStr(tmpAtm->value.byts.size,tmpAtm->value.byts.str,out,1);
						break;
					case FKL_VECTOR:
						fprintf(out,"#(");
						for(size_t i=0;i<tmpAtm->value.vec.size;i++)
							fklPrintCptr(&tmpAtm->value.vec.base[i],out);
						fprintf(out,")");
						break;
					default:
							break;
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
			putc(')',out);
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
}

void fklFreeAtom(FklAstAtom* objAtm)
{
	if(objAtm->type==FKL_SYM||objAtm->type==FKL_STR)free(objAtm->value.str);
	else if(objAtm->type==FKL_BYTS)
	{
		objAtm->value.byts.size=0;
		free(objAtm->value.byts.str);
	}
	else if(objAtm->type==FKL_VECTOR)
	{
		FklAstVector* vec=&objAtm->value.vec;
		for(size_t i=0;i<vec->size;i++)
			fklDeleteCptr(&vec->base[i]);
		free(vec->base);
		vec->size=0;
	}
	free(objAtm);
}

FklAstPair* fklNewPair(int curline,FklAstPair* prev)
{
	FklAstPair* tmp;
	FKL_ASSERT((tmp=(FklAstPair*)malloc(sizeof(FklAstPair))),"fklNewPair");
	tmp->car.outer=tmp;
	tmp->car.type=FKL_NIL;
	tmp->car.u.all=NULL;
	tmp->cdr.outer=tmp;
	tmp->cdr.type=FKL_NIL;
	tmp->cdr.u.all=NULL;
	tmp->prev=prev;
	tmp->car.curline=curline;
	tmp->cdr.curline=curline;
	return tmp;
}

FklAstCptr* fklNewCptr(int curline,FklAstPair* outer)
{
	FklAstCptr* tmp=NULL;
	FKL_ASSERT((tmp=(FklAstCptr*)malloc(sizeof(FklAstCptr))),"fklNewCptr");
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=FKL_NIL;
	tmp->u.all=NULL;
	return tmp;
}

FklAstAtom* fklNewAtom(FklValueType type,const char* value,FklAstPair* prev)
{
	FklAstAtom* tmp=NULL;
	FKL_ASSERT((tmp=(FklAstAtom*)malloc(sizeof(FklAstAtom))),"fklNewAtom");
	switch(type)
	{
		case FKL_SYM:
		case FKL_STR:
			if(value!=NULL)
			{
				FKL_ASSERT((tmp->value.str=(char*)malloc(strlen(value)+1)),"fklNewAtom");
				strcpy(tmp->value.str,value);
			}
			else
				tmp->value.str=NULL;
			break;
		case FKL_CHR:
		case FKL_I32:
		case FKL_F64:
			*(int32_t*)(&tmp->value)=0;
			break;
		case FKL_BYTS:
			tmp->value.byts.size=0;
			tmp->value.byts.str=NULL;
			break;
		case FKL_VECTOR:
			tmp->value.vec.size=0;
			tmp->value.vec.base=NULL;
			break;
		default:
			break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int fklCopyCptr(FklAstCptr* objCptr,const FklAstCptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	fklPushPtrStack(objCptr,s1);
	fklPushPtrStack((void*)copiedCptr,s2);
	FklAstAtom* atom1=NULL;
	FklAstAtom* atom2=NULL;
	while(!fklIsPtrStackEmpty(s2))
	{
		FklAstCptr* root1=fklPopPtrStack(s1);
		FklAstCptr* root2=fklPopPtrStack(s2);
		root1->type=root2->type;
		root1->curline=root2->curline;
		switch(root1->type)
		{
			case FKL_PAIR:
				root1->u.pair=fklNewPair(0,root1->outer);
				fklPushPtrStack(fklGetASTPairCar(root1),s1);
				fklPushPtrStack(fklGetASTPairCdr(root1),s1);
				fklPushPtrStack(fklGetASTPairCar(root2),s2);
				fklPushPtrStack(fklGetASTPairCdr(root2),s2);
				break;
			case FKL_ATM:
				atom1=NULL;
				atom2=root2->u.atom;
				switch(atom2->type)
				{
					case FKL_SYM:
					case FKL_STR:
						atom1=fklNewAtom(atom2->type,atom2->value.str,root1->outer);
						break;
					case FKL_BYTS:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.byts.size=atom2->value.byts.size;
						atom1->value.byts.str=fklCopyMemory(atom2->value.byts.str,atom2->value.byts.size);
						break;
					case FKL_VECTOR:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						fklMakeAstVector(&atom1->value.vec,atom2->value.vec.size,NULL);
						for(size_t i=0;i<atom2->value.vec.size;i++)
						{
							fklPushPtrStack(&atom1->value.vec.base[i],s1);
							fklPushPtrStack(&atom2->value.vec.base[i],s2);
						}
						break;
					case FKL_I32:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.i32=atom2->value.i32;
						break;
					case FKL_I64:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.i64=atom2->value.i64;
						break;
					case FKL_F64:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.f64=atom2->value.f64;
						break;
					case FKL_CHR:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.chr=atom2->value.chr;
						break;
					default:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						break;
				}
				root1->u.atom=atom1;
				break;
			default:
				root1->u.all=NULL;
				break;
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return 1;
}
void fklReplaceCptr(FklAstCptr* fir,const FklAstCptr* sec)
{
	FklAstPair* tmp=fir->outer;
	FklAstCptr tmpCptr={NULL,0,FKL_NIL,{NULL}};
	tmpCptr.type=fir->type;
	tmpCptr.u.all=fir->u.all;
	fklCopyCptr(fir,sec);
	fklDeleteCptr(&tmpCptr);
	if(fir->type==FKL_PAIR)
		fir->u.pair->prev=tmp;
	else if(fir->type==FKL_ATM)
		fir->u.atom->prev=tmp;
}

int fklDeleteCptr(FklAstCptr* objCptr)
{
	if(objCptr==NULL)return 0;
	FklAstPair* tmpPair=(objCptr->type==FKL_PAIR)?objCptr->u.pair:NULL;
	FklAstPair* objPair=tmpPair;
	FklAstCptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==FKL_PAIR)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.u.pair;
				tmpCptr=&objPair->car;
				continue;
			}
			else
			{
				objPair=tmpCptr->u.pair;
				tmpCptr=&objPair->car;
				continue;
			}
		}
		else if(tmpCptr->type==FKL_ATM)
		{
			fklFreeAtom(tmpCptr->u.atom);
			tmpCptr->type=FKL_NIL;
			tmpCptr->u.all=NULL;
			continue;
		}
		else if(tmpCptr->type==FKL_NIL)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->car)
			{
				tmpCptr=&objPair->cdr;
				continue;
			}
			else if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				FklAstPair* prev=objPair;
				objPair=objPair->prev;
				free(prev);
				if(objPair==NULL||prev==tmpPair)break;
				if(prev==objPair->car.u.pair)
				{
					objPair->car.type=FKL_NIL;
					objPair->car.u.all=NULL;
				}
				else if(prev==objPair->cdr.u.pair)
				{
					objPair->cdr.type=FKL_NIL;
					objPair->cdr.u.all=NULL;
				}
				tmpCptr=&objPair->cdr;
			}
		}
		if(objPair==NULL)break;
	}
	objCptr->type=FKL_NIL;
	objCptr->u.all=NULL;
	return 0;
}

int fklCptrcmp(const FklAstCptr* first,const FklAstCptr* second)
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
				if((firAtm->type==FKL_SYM||firAtm->type==FKL_STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_F64&&fabs(firAtm->value.f64-secAtm->value.f64)!=0)return 0;
				else if(firAtm->type==FKL_CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==FKL_BYTS&&!fklEqByteString(&firAtm->value.byts,&secAtm->value.byts))return 0;
			}
			if(firPair!=NULL&&first==&firPair->car)
			{
				first=&firPair->cdr;
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

FklAstCptr* fklNextCptr(const FklAstCptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==FKL_PAIR)
		return &objCptr->outer->cdr.u.pair->car;
	return NULL;
}

FklAstCptr* fklPrevCptr(const FklAstCptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->prev!=NULL&&objCptr->outer->prev->cdr.u.pair==objCptr->outer)
		return &objCptr->outer->prev->car;
	return NULL;
}

FklAstCptr* fklGetLastCptr(const FklAstCptr* objList)
{
	if(objList->type!=FKL_PAIR)
		return NULL;
	FklAstPair* objPair=objList->u.pair;
	FklAstCptr* first=&objPair->car;
	for(;fklNextCptr(first)!=NULL;first=fklNextCptr(first));
	return first;
}

FklAstCptr* fklGetFirstCptr(const FklAstCptr* objList)
{
	if(objList->type!=FKL_PAIR)
		return NULL;
	FklAstPair* objPair=objList->u.pair;
	FklAstCptr* first=&objPair->car;
	return first;
}

FklAstCptr* fklGetASTPairCar(const FklAstCptr* obj)
{
	return &obj->u.pair->car;
}

FklAstCptr* fklGetASTPairCdr(const FklAstCptr* obj)
{
	return &obj->u.pair->cdr;
}

FklAstCptr* fklGetCptrCar(const FklAstCptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->car;
	return NULL;
}

FklAstCptr* fklGetCptrCdr(const FklAstCptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->cdr;
	return NULL;
}

int fklIsListCptr(const FklAstCptr* list)
{
	FklAstCptr* last=NULL;
	for(last=(list->type==FKL_PAIR)?&list->u.pair->car:NULL
			;fklGetCptrCdr(last)->type==FKL_PAIR
			;last=fklNextCptr(last));
	return fklGetCptrCdr(last)->type!=FKL_ATM;
}

unsigned int fklLengthListCptr(const FklAstCptr* list)
{
	unsigned int n=0;
	for(FklAstCptr* fir=(list->type==FKL_PAIR)?&list->u.pair->car:NULL
			;fir
			;fir=fklNextCptr(fir))
		n++;
	return n;
}

static FklAstAtom* createChar(const char* oStr,FklAstPair* prev)
{
	char* str=fklGetStringAfterBackslash(oStr+2);
	FklAstAtom* r=fklNewAtom(FKL_CHR,NULL,prev);
	r->value.chr=(str[0]=='\\')?
		fklStringToChar(str+1):
		str[0];
	free(str);
	return r;
}

static FklAstAtom* createNum(const char* oStr,FklAstPair* prev)
{
	FklAstAtom* r=NULL;
	if(fklIsDouble(oStr))
	{
		r=fklNewAtom(FKL_F64,NULL,prev);
		r->value.f64=fklStringToDouble(oStr);
	}
	else
	{
		r=fklNewAtom(FKL_I32,NULL,prev);
		int64_t num=fklStringToInt(oStr);
		if(num>INT32_MAX||num<INT32_MIN)
		{
			r->type=FKL_I64;
			r->value.i64=num;
		}
		else
			r->value.i32=num;
	}
	return r;
}

static FklAstAtom* createByts(const char* oStr,FklAstPair* prev)
{
	char* str=fklGetStringAfterBackslash(oStr+2);
	FklAstAtom* r=fklNewAtom(FKL_BYTS,NULL,prev);
	r->value.byts.size=strlen(str)/2+strlen(str)%2;
	r->value.byts.str=fklCastStrByteStr(str);
	free(str);
	return r;
}

static FklAstAtom* createString(const char* oStr,FklAstPair* prev)
{
	size_t len=0;
	char* str=fklCastEscapeCharater(oStr+1,'\"',&len);
	FklAstAtom* r=fklNewAtom(FKL_STR,str,prev);
	free(str);
	return r;
}

static FklAstAtom* createSymbol(const char* oStr,FklAstPair* prev)
{
	return fklNewAtom(FKL_SYM,oStr,prev);
}

static FklAstAtom* (* const atomCreators[])(const char* str,FklAstPair* prev)=
{
	createChar, //char
	createNum, //num
	createByts, //byts
	createString,
	createSymbol, //symbol
};

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

static MatchState* newMatchState(FklStringMatchPattern* pattern,uint32_t line,uint32_t cindex)
{
	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
	FKL_ASSERT(state,"newMatchState");
	state->pattern=pattern;
	state->line=line;
	state->cindex=cindex;
	return state;
}

static void freeMatchState(MatchState* state)
{
	free(state);
}

#define PARENTHESE_0 ((void*)0)
#define PARENTHESE_1 ((void*)1)
#define QUOTE ((void*)2)
#define QSQUOTE ((void*)3)
#define UNQUOTE ((void*)4)
#define UNQTESP ((void*)5)
#define DOTTED ((void*)6)
#define VECTOR_0 ((void*)7)
#define VECTOR_1 ((void*)8)

typedef enum
{
	AST_CAR,
	AST_CDR,
}AstPlace;

typedef struct
{
	AstPlace place;
	FklAstCptr* cptr;
}AstElem;

AstElem* newAstElem(AstPlace place,FklAstCptr* cptr)
{
	AstElem* t=(AstElem*)malloc(sizeof(AstElem));
	FKL_ASSERT(t,"newAstElem");
	t->place=place;
	t->cptr=cptr;
	return t;
}

static int isBuiltInSingleStrPattern(FklStringMatchPattern* pattern)
{
	return pattern==QUOTE
		||pattern==QSQUOTE
		||pattern==UNQUOTE
		||pattern==UNQTESP
		||pattern==DOTTED
		;
}

static int isBuiltInParenthese(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1
		;
}

static int isBuiltInVector(FklStringMatchPattern* pattern)
{
	return pattern==VECTOR_0
		||pattern==VECTOR_1
		;
}

static int isLeftParenthese(const char* str)
{
	return strlen(str)==1&&(str[0]=='('||str[0]=='[');
}

static int isVectorStart(const char* str)
{
	return strlen(str)==2&&str[0]=='#'&&(str[1]=='('||str[1]=='[');
}

static int isRightParenthese(const char* str)
{
	return strlen(str)==1&&(str[0]==')'||str[0]==']');
}

static int isBuiltSingleStr(const char* str)
{
	return (strlen(str)==1&&(str[0]=='\''
				||str[0]=='`'
				||str[0]=='~'
				||str[0]==','))
		||!strcmp(str,"~@");
}

static MatchState* searchReverseStringCharMatchState(const char* str,FklPtrStack* matchStateStack)
{
	MatchState* top=fklTopPtrStack(matchStateStack);
	uint32_t i=0;
	for(;top&&!isBuiltInSingleStrPattern(top->pattern)&&!isBuiltInParenthese(top->pattern)&&!isBuiltInVector(top->pattern)&&top->cindex+i<top->pattern->num;)
	{
		const char* cpart=fklGetNthPartOfStringMatchPattern(top->pattern,top->cindex+i);
		if(!fklIsVar(cpart)&&!strcmp(str,cpart))
		{
			top->cindex+=i;
			return top;
		}
		i++;
	}
	return NULL;
}

static FklAstCptr* expandReaderMacroWithTreeStack(FklStringMatchPattern* pattern,const char* filename,FklCompEnv* glob,FklPtrStack* treeStack,uint32_t curline)
{
	FklPreEnv* tmpEnv=fklNewEnv(NULL);
	for(uint32_t j=0;j<treeStack->top;)
	{
		for(uint32_t i=0;i<pattern->num;i++)
		{
			if(fklIsVar(pattern->parts[i]))
			{
				char* varName=fklGetVarName(pattern->parts[i]);
				AstElem* elem=treeStack->base[j];
				fklAddDefine(varName,elem->cptr,tmpEnv);
				free(varName);
				j++;
			}
		}
	}
	FklVM* tmpVM=fklNewTmpVM(NULL);
	FklAstCptr* retval=NULL;
	if(pattern->type==FKL_BYTS)
	{
		FklByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
		FklVMenv* tmpGlobEnv=genGlobEnv(glob,t,tmpVM->heap);
		if(!tmpGlobEnv)
		{
			fklDestroyEnv(tmpEnv);
			fklFreeByteCodelnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklFreeVMstack(tmpVM->stack);
			fklFreePtrStack(tmpVM->rstack);
			fklFreePtrStack(tmpVM->tstack);
			free(tmpVM);
			return NULL;
		}
		FklVMenv* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap);
		uint32_t start=t->bc->size;
		fklCodelntCopyCat(t,pattern->u.bProc);
		fklInitVMRunningResource(tmpVM,stringPatternEnv,tmpVM->heap,t,start,pattern->u.bProc->bc->size);
		int state=fklRunVM(tmpVM);
		if(!state)
			retval=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),curline);
		else
		{
			fklFreeVMenv(tmpGlobEnv);
			fklFreeByteCodeAndLnt(t);
			fklFreeVMheap(tmpVM->heap);
			fklUninitVMRunningResource(tmpVM);
			return NULL;
		}
		if(!retval)
		{
			fprintf(stderr,"error of compiling:Circular reference occur in expanding reader macro at line %d of %s\n",curline,filename);
		}
		fklFreeVMenv(tmpGlobEnv);
		fklFreeByteCodeAndLnt(t);
		fklFreeVMheap(tmpVM->heap);
		fklUninitVMRunningResource(tmpVM);
	}
	else if(pattern->type==FKL_FLPROC)
	{
		FklVMenv* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,NULL,tmpVM->heap);
		FklVMrunnable* mainrunnable=fklNewVMrunnable(NULL);
		mainrunnable->localenv=stringPatternEnv;
		fklPushPtrStack(mainrunnable,tmpVM->rstack);
		pattern->u.fProc(tmpVM);
		retval=fklCastVMvalueToCptr(fklGET_VAL(tmpVM->stack->values[0],tmpVM->heap),curline);
		free(mainrunnable);
		fklFreeVMenv(stringPatternEnv);
		fklFreeVMheap(tmpVM->heap);
		fklFreeVMstack(tmpVM->stack);
		fklFreePtrStack(tmpVM->rstack);
		fklFreePtrStack(tmpVM->tstack);
		free(tmpVM);
	}
	fklDestroyEnv(tmpEnv);
	return retval;
}

FklAstCptr* fklCreateAstWithTokens(FklPtrStack* tokenStack,const char* filename,FklCompEnv* glob)
{
	if(fklIsPtrStackEmpty(tokenStack))
		return NULL;
	FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
	FklPtrStack* stackStack=fklNewPtrStack(32,16);
	FklPtrStack* elemStack=fklNewPtrStack(32,16);
	fklPushPtrStack(elemStack,stackStack);
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		FklPtrStack* cStack=fklTopPtrStack(stackStack);
		if(token->type>FKL_TOKEN_RESERVE_STR&&token->type<FKL_TOKEN_COMMENT)
		{
			FklAstCptr* cur=fklNewCptr(token->line,NULL);
			cur->type=FKL_ATM;
			cur->curline=token->line;
			cur->u.atom=atomCreators[token->type-1](token->value,cur->outer);
			AstElem* elem=newAstElem(AST_CAR,cur);
			fklPushPtrStack(elem,cStack);
		}
		else if(token->type==FKL_TOKEN_RESERVE_STR)
		{
			MatchState* state=searchReverseStringCharMatchState(token->value,matchStateStack);
			FklStringMatchPattern* pattern=fklFindStringPattern(token->value);
			if(state)
			{
				FklStringMatchPattern* pattern=state->pattern;
				const char* part=fklGetNthPartOfStringMatchPattern(pattern,state->cindex-1);
				if(part&&fklIsVar(part)&&fklIsMustList(part))
				{
					fklPopPtrStack(stackStack);
					AstElem* v=newAstElem(AST_CAR,fklNewCptr(state->line,NULL));
					int r=0;
					uint32_t i=0;
					for(;i<cStack->top;i++)
					{
						AstElem* c=cStack->base[i];
						if(!r)
						{
							if(c->place==AST_CAR)
								r=copyAndAddToList(v->cptr,c->cptr);
							else
								r=copyAndAddToTail(v->cptr,c->cptr);
						}
						fklDeleteCptr(c->cptr);
						free(c->cptr);
						free(c);
					}
					fklFreePtrStack(cStack);
					cStack=fklTopPtrStack(stackStack);
					if(!r)
						fklPushPtrStack(v,cStack);
					if(r)
					{
						fklDeleteCptr(v->cptr);
						free(v->cptr);
						free(v);
						while(!fklIsPtrStackEmpty(stackStack))
						{
							FklPtrStack* cStack=fklPopPtrStack(stackStack);
							while(!fklIsPtrStackEmpty(cStack))
							{
								FklAstCptr* cptr=fklPopPtrStack(cStack);
								fklDeleteCptr(cptr);
								free(cptr);
							}
							fklFreePtrStack(cStack);
						}
						fklFreePtrStack(stackStack);
						while(!fklIsPtrStackEmpty(matchStateStack))
						{
							MatchState* state=fklPopPtrStack(matchStateStack);
							free(state);
						}
						fklFreePtrStack(matchStateStack);
						return NULL;
					}
				}
				state->cindex+=1;
				part=fklGetNthPartOfStringMatchPattern(pattern,state->cindex);
				if(part&&fklIsVar(part)&&fklIsMustList(part))
				{
					cStack=fklNewPtrStack(32,16);
					fklPushPtrStack(cStack,stackStack);
				}
				if(state->cindex<state->pattern->num)
					continue;
			}
			else if(pattern)
			{
				MatchState* state=newMatchState(pattern,token->line,1);
				fklPushPtrStack(state,matchStateStack);
				cStack=fklNewPtrStack(32,16);
				fklPushPtrStack(cStack,stackStack);
				const char* part=fklGetNthPartOfStringMatchPattern(pattern,state->cindex);
				if(fklIsVar(part)&&fklIsMustList(part))
				{
					cStack=fklNewPtrStack(32,16);
					fklPushPtrStack(cStack,stackStack);
				}
				continue;
			}
			else if(isLeftParenthese(token->value))
			{
				FklStringMatchPattern* pattern=token->value[0]=='('?PARENTHESE_0:PARENTHESE_1;
				MatchState* state=newMatchState(pattern,token->line,0);
				fklPushPtrStack(state,matchStateStack);
				cStack=fklNewPtrStack(32,16);
				fklPushPtrStack(cStack,stackStack);
				continue;
			}
			else if(isVectorStart(token->value))
			{
				FklStringMatchPattern* pattern=token->value[1]=='('?VECTOR_0:VECTOR_1;
				MatchState* state=newMatchState(pattern,token->line,0);
				fklPushPtrStack(state,matchStateStack);
				cStack=fklNewPtrStack(32,16);
				fklPushPtrStack(cStack,stackStack);
				continue;
			}
			else if(isRightParenthese(token->value))
			{
				fklPopPtrStack(stackStack);
				MatchState* cState=fklPopPtrStack(matchStateStack);
				AstElem* v=newAstElem(AST_CAR,fklNewCptr(cState->line,NULL));
				int r=0;
				if(isBuiltInParenthese(cState->pattern))
				{
					for(uint32_t i=0;i<cStack->top;i++)
					{
						AstElem* c=cStack->base[i];
						if(!r)
						{
							if(c->place==AST_CAR)
								r=copyAndAddToList(v->cptr,c->cptr);
							else
								r=copyAndAddToTail(v->cptr,c->cptr);
						}
						fklDeleteCptr(c->cptr);
						free(c->cptr);
						free(c);
					}
				}
				else
				{
					FklAstAtom* vec=fklNewAtom(FKL_VECTOR,NULL,v->cptr->outer);
					fklMakeAstVector(&vec->value.vec,cStack->top,NULL);
					v->cptr->type=FKL_ATM;
					v->cptr->u.atom=vec;
					for(uint32_t i=0;i<cStack->top;i++)
					{
						AstElem* c=cStack->base[i];
						if(!r)
						{
							if(c->place==AST_CAR)
								fklCopyCptr(&vec->value.vec.base[i],c->cptr);
							else
								r=1;
							
						}
						fklDeleteCptr(c->cptr);
						free(c->cptr);
						free(c);
					}
				}
				fklFreePtrStack(cStack);
				cStack=fklTopPtrStack(stackStack);
				if(!r)
					fklPushPtrStack(v,cStack);
				freeMatchState(cState);
				if(r)
				{
					fklDeleteCptr(v->cptr);
					free(v->cptr);
					free(v);
					while(!fklIsPtrStackEmpty(stackStack))
					{
						FklPtrStack* cStack=fklPopPtrStack(stackStack);
						while(!fklIsPtrStackEmpty(cStack))
						{
							FklAstCptr* cptr=fklPopPtrStack(cStack);
							fklDeleteCptr(cptr);
							free(cptr);
						}
						fklFreePtrStack(cStack);
					}
					fklFreePtrStack(stackStack);
					while(!fklIsPtrStackEmpty(matchStateStack))
					{
						MatchState* state=fklPopPtrStack(matchStateStack);
						free(state);
					}
					fklFreePtrStack(matchStateStack);
					return NULL;
				}
			}
			else if(isBuiltSingleStr(token->value))
			{
				FklStringMatchPattern* pattern=token->value[0]=='\''?
					QUOTE:
					token->value[0]=='`'?
					QSQUOTE:
					token->value[0]==','?
					DOTTED:
					token->value[0]=='~'?
					(strlen(token->value)==1?
					 UNQUOTE:
					 token->value[1]=='@'?
					 UNQTESP:NULL):
					NULL;
				MatchState* state=newMatchState(pattern,token->line,0);
				fklPushPtrStack(state,matchStateStack);
				cStack=fklNewPtrStack(1,1);
				fklPushPtrStack(cStack,stackStack);
				continue;
			}
		}
		for(MatchState* state=fklTopPtrStack(matchStateStack);
				state&&(isBuiltInSingleStrPattern(state->pattern)||(!isBuiltInParenthese(state->pattern)&&!isBuiltInVector(state->pattern)));
				state=fklTopPtrStack(matchStateStack))
		{
			if(isBuiltInSingleStrPattern(state->pattern))
			{
				fklPopPtrStack(stackStack);
				MatchState* cState=fklPopPtrStack(matchStateStack);
				AstElem* postfix=fklPopPtrStack(cStack);
				fklFreePtrStack(cStack);
				cStack=fklTopPtrStack(stackStack);
				if(state->pattern==DOTTED)
				{
					postfix->place=AST_CDR;
					fklPushPtrStack(postfix,cStack);
				}
				else
				{
					AstElem* v=newAstElem(AST_CAR,fklNewCptr(token->line,NULL));
					FklAstCptr* prefix=fklNewCptr(token->line,NULL);
					FklStringMatchPattern* pattern=cState->pattern;
					const char* prefixValue=pattern==QUOTE?
						"quote":
						pattern==QSQUOTE?
						"qsquote":
						pattern==UNQUOTE?
						"unquote":
						pattern==UNQTESP?
						"unqtesp":
						NULL;
					fklPushPtrStack(v,cStack);
					prefix->type=FKL_ATM;
					prefix->u.atom=fklNewAtom(FKL_SYM,prefixValue,v->cptr->outer);
					copyAndAddToList(v->cptr,prefix);
					copyAndAddToList(v->cptr,postfix->cptr);
					fklDeleteCptr(prefix);
					free(prefix);
					fklDeleteCptr(postfix->cptr);
					free(postfix->cptr);
					free(postfix);
				}
				freeMatchState(cState);
			}
			else
			{
				const char* cpart=fklGetNthPartOfStringMatchPattern(state->pattern,state->cindex);
				if(cpart&&fklIsVar(cpart)&&!fklIsMustList(cpart))
				{
					state->cindex++;
					cpart=fklGetNthPartOfStringMatchPattern(state->pattern,state->cindex);
					if(cpart&&fklIsVar(cpart)&&fklIsMustList(cpart))
					{
						cStack=fklNewPtrStack(32,16);
						fklPushPtrStack(cStack,stackStack);
					}
				}
				if(state->cindex>=state->pattern->num)
				{
					fklPopPtrStack(stackStack);
					FklAstCptr* r=expandReaderMacroWithTreeStack(state->pattern,filename,glob,cStack,state->line);
					freeMatchState(fklPopPtrStack(matchStateStack));
					for(uint32_t i=0;i<cStack->top;i++)
					{
						AstElem* elem=cStack->base[i];
						fklDeleteCptr(elem->cptr);
						free(elem->cptr);
						free(elem);
					}
					fklFreePtrStack(cStack);
					cStack=fklTopPtrStack(stackStack);
					fklPushPtrStack(newAstElem(AST_CAR,r),cStack);
				}
				else
					break;
			}
		}
	}
	AstElem* retvalElem=fklTopPtrStack(elemStack);
	FklAstCptr* retval=(retvalElem)?retvalElem->cptr:NULL;
	free(retvalElem);
	fklFreePtrStack(matchStateStack);
	fklFreePtrStack(stackStack);
	fklFreePtrStack(elemStack);
	return retval;
}

void fklMakeAstVector(FklAstVector* vec,size_t size,const FklAstCptr* base)
{
	vec->size=size;
	vec->base=(FklAstCptr*)malloc(sizeof(FklAstCptr)*size);
	FKL_ASSERT(!size||vec->base,"fklMakeAstVector");
	if(base)
	{
		for(size_t i=0;i<size;i++)
			fklCopyCptr(&vec->base[i],&base[i]);
	}
}
