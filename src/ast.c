#include<fakeLisp/ast.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/compiler.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/parser.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif
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

static FklVMvalue* genGlobEnv(FklCompEnv* cEnv,FklByteCodelnt* t,FklVMheap* heap)
{
	FklVMvalue* vEnv=fklNewVMvalue(FKL_ENV,fklNewVMenv(NULL),heap);
	fklInitGlobEnv(vEnv->u.env,heap);
	if(cEnv->proc->bc->size)
	{
		FklVM* tmpVM=fklNewTmpVM(NULL);
		fklInitVMRunningResource(tmpVM,vEnv,heap,cEnv->proc,0,cEnv->proc->bc->size);
		int i=fklRunVM(tmpVM);
		if(i==1)
		{
			fklUninitVMRunningResource(tmpVM);
			return NULL;
		}
		fklCodelntCopyCat(t,cEnv->proc);
		fklUninitVMRunningResource(tmpVM);
	}
	return vEnv;
}

void fklFreeAtom(FklAstAtom* objAtm)
{
	if(objAtm->type==FKL_SYM)
		free(objAtm->value.sym);
	else if(objAtm->type==FKL_STR)
		free(objAtm->value.str.str);
	else if(objAtm->type==FKL_VECTOR)
	{
		FklAstVector* vec=&objAtm->value.vec;
		for(size_t i=0;i<vec->size;i++)
			fklDeleteCptr(&vec->base[i]);
		free(vec->base);
		vec->size=0;
	}
	else if(objAtm->type==FKL_BOX)
	{
		FklAstCptr* c=&objAtm->value.box;
		fklDeleteCptr(c);
		c->curline=0;
		c->outer=NULL;
		c->type=FKL_NIL;
		c->u.all=NULL;
	}
	else if(objAtm->type==FKL_BIG_INT)
	{
		FklBigInt* bi=&objAtm->value.bigInt;
		free(bi->digits);
		bi->digits=NULL;
		bi->neg=0;
		bi->num=0;
		bi->size=0;
	}
	free(objAtm);
}

FklAstPair* fklNewPair(int curline,FklAstPair* prev)
{
	FklAstPair* tmp;
	FKL_ASSERT((tmp=(FklAstPair*)malloc(sizeof(FklAstPair))),__func__);
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
	FKL_ASSERT((tmp=(FklAstCptr*)malloc(sizeof(FklAstCptr))),__func__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=FKL_NIL;
	tmp->u.all=NULL;
	return tmp;
}

FklAstAtom* fklNewAtom(FklValueType type,const char* value,FklAstPair* prev)
{
	FklAstAtom* tmp=NULL;
	FKL_ASSERT((tmp=(FklAstAtom*)malloc(sizeof(FklAstAtom))),__func__);
	switch(type)
	{
		case FKL_SYM:
			if(value!=NULL)
			{
				FKL_ASSERT((tmp->value.sym=(char*)malloc(strlen(value)+1)),__func__);
				strcpy(tmp->value.sym,value);
			}
			else
				tmp->value.sym=NULL;
			break;
		case FKL_CHR:
		case FKL_I32:
		case FKL_F64:
			*(int32_t*)(&tmp->value)=0;
			break;
		case FKL_STR:
			tmp->value.str.size=0;
			tmp->value.str.str=NULL;
			break;
		case FKL_VECTOR:
			tmp->value.vec.size=0;
			tmp->value.vec.base=NULL;
			break;
		case FKL_BIG_INT:
			tmp->value.bigInt.digits=NULL;
			tmp->value.bigInt.num=0;
			tmp->value.bigInt.size=0;
			tmp->value.bigInt.neg=0;
			break;
		case FKL_BOX:
			tmp->value.box.curline=0;
			tmp->value.box.outer=NULL;
			tmp->value.box.type=FKL_NIL;
			tmp->value.box.u.all=NULL;
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
						atom1=fklNewAtom(atom2->type,atom2->value.sym,root1->outer);
						break;
					case FKL_STR:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.str.size=atom2->value.str.size;
						atom1->value.str.str=fklCopyMemory(atom2->value.str.str,atom2->value.str.size);
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
					case FKL_BOX:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						fklPushPtrStack(&atom1->value.box,s1);
						fklPushPtrStack(&atom2->value.box,s2);
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
					case FKL_BIG_INT:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						fklSetBigInt(&atom1->value.bigInt,&atom2->value.bigInt);
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

int fklAstStrcmp(const FklAstString* fir,const FklAstString* sec)
{
	ssize_t size=fir->size<sec->size?fir->size:sec->size;
	int r=memcmp(fir->str,sec->str,size);
	if(!r)
		return fir->size-sec->size;
	return r;
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
				if(firAtm->type==FKL_SYM&&strcmp(firAtm->value.sym,secAtm->value.sym))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_F64&&fabs(firAtm->value.f64-secAtm->value.f64)!=0)return 0;
				else if(firAtm->type==FKL_CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==FKL_STR&&fklAstStrcmp(&firAtm->value.str,&secAtm->value.str))return 0;
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
	if(objCptr&&objCptr->outer!=NULL&&objCptr->outer->cdr.type==FKL_PAIR)
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

static int fklIsValidCharStr(const char* str)
{
	size_t len=strlen(str);
	if(len==0)
		return 0;
	if(isalpha(str[0])&&len>1)
		return 0;
	if(str[0]=='\\')
	{
		if(len<2)
			return 0;
		if(toupper(str[1])=='X')
		{
			if(len<3||len>4)
				return 0;
			for(size_t i=2;i<len;i++)
				if(!isxdigit(str[i]))
					return 0;
		}
		else if(str[1]=='0')
		{
			if(len>5)
				return 0;
			if(len>2)
			{
				for(size_t i=2;i<len;i++)
					if(!isdigit(str[i])||str[i]>'7')
						return 0;
			}
		}
		else if(isdigit(str[1]))
		{
			if(len>4)
				return 0;
			for(size_t i=1;i<len;i++)
				if(!isdigit(str[i]))
					return 0;
		}
	}
	return 1;
}

static FklAstAtom* createChar(const char* oStr,FklAstPair* prev)
{
	if(!fklIsValidCharStr(oStr+2))
		return NULL;
	oStr+=2;
	FklAstAtom* r=fklNewAtom(FKL_CHR,NULL,prev);
	r->value.chr=(oStr[0]=='\\')?
		fklStringToChar(oStr+1):
		oStr[0];
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
		FklBigInt* bInt=fklNewBigIntFromStr(oStr);
		if(fklIsGtLtI64BigInt(bInt))
		{
			r->type=FKL_BIG_INT;
			r->value.bigInt.digits=NULL;
			r->value.bigInt.num=0;
			r->value.bigInt.size=0;
			r->value.bigInt.neg=0;
			fklSetBigInt(&r->value.bigInt,bInt);
		}
		else
		{
			int64_t num=fklBigIntToI64(bInt);
			if(num>INT32_MAX||num<INT32_MIN)
			{
				r->type=FKL_I64;
				r->value.i64=num;
			}
			else
				r->value.i32=num;
		}
		fklFreeBigInt(bInt);
	}
	return r;
}

static char* castEscapeCharaterBuf(const char* str,char end,size_t* size)
{
	uint64_t strSize=0;
	uint64_t memSize=FKL_MAX_STRING_SIZE;
	uint64_t i=0;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	while(str[i]!=end)
	{
		int ch=0;
		if(str[i]=='\\')
		{
			char* backSlashStr=fklGetStringAfterBackslashInStr(str+i+1);
			size_t len=strlen(backSlashStr);
			if(isdigit(backSlashStr[0]))
			{
				if(backSlashStr[0]=='0'&&isdigit(backSlashStr[1]))
					sscanf(backSlashStr,"%4o",&ch);
				else
					sscanf(backSlashStr,"%4d",&ch);
				i+=len+1;
			}
			else if(toupper(backSlashStr[0])=='X')
			{
				ch=fklStringToChar(backSlashStr);
				i+=len+1;
			}
			else if(backSlashStr[0]=='\n')
			{
				i+=2;
				free(backSlashStr);
				continue;
			}
			else
			{
				switch(toupper(backSlashStr[0]))
				{
					case 'A':
						ch=0x07;
						break;
					case 'B':
						ch=0x08;
						break;
					case 'T':
						ch=0x09;
						break;
					case 'N':
						ch=0x0a;
						break;
					case 'V':
						ch=0x0b;
						break;
					case 'F':
						ch=0x0c;
						break;
					case 'R':
						ch=0x0d;
						break;
					case 'S':
						ch=0x20;
						break;
					default:ch=str[i+1];break;
				}
				i+=2;
			}
			free(backSlashStr);
		}
		else ch=str[i++];
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,__func__);
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	tmp=(char*)realloc(tmp,strSize*sizeof(char));
	FKL_ASSERT(!strSize||tmp,__func__);
	*size=strSize;
	return tmp;
}

static FklAstAtom* createString(const char* oStr,FklAstPair* prev)
{
	size_t size=0;
	char* str=castEscapeCharaterBuf(oStr+1,'\"',&size);
	FklAstAtom* r=fklNewAtom(FKL_STR,NULL,prev);
	r->value.str.size=size;
	r->value.str.str=str;
	return r;
}

static FklAstAtom* createSymbol(const char* oStr,FklAstPair* prev)
{
	return fklNewAtom(FKL_SYM,oStr,prev);
}

static FklAstAtom* (* const atomCreators[])(const char* str,FklAstPair* prev)=
{
	createChar,
	createNum,
	createString,
	createSymbol,
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
	FKL_ASSERT(state,__func__);
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
#define BOX ((void*)9)

typedef enum
{
	AST_CAR,
	AST_CDR,
	AST_BOX,
}AstPlace;

typedef struct
{
	AstPlace place;
	FklAstCptr* cptr;
}AstElem;

AstElem* newAstElem(AstPlace place,FklAstCptr* cptr)
{
	AstElem* t=(AstElem*)malloc(sizeof(AstElem));
	FKL_ASSERT(t,__func__);
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
		||pattern==BOX
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
		||!strcmp(str,"~@")
		||!strcmp(str,"#&");
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
	char* cwd=getcwd(NULL,0);
	chdir(fklGetCwd());
	FklByteCodelnt* t=fklNewByteCodelnt(fklNewByteCode(0));
	FklVMvalue* tmpGlobEnv=genGlobEnv(glob,t,tmpVM->heap);
	if(!tmpGlobEnv)
	{
		fklDestroyEnv(tmpEnv);
		fklFreeByteCodelnt(t);
		fklFreeVMheap(tmpVM->heap);
		fklFreeVMstack(tmpVM->stack);
		fklFreeRunnables(tmpVM->rhead);
		fklFreePtrStack(tmpVM->tstack);
		free(tmpVM);
		chdir(cwd);
		free(cwd);
		return NULL;
	}
	FklVMvalue* stringPatternEnv=fklCastPreEnvToVMenv(tmpEnv,tmpGlobEnv,tmpVM->heap);
	uint32_t start=t->bc->size;
	fklCodelntCopyCat(t,pattern->proc);
	fklInitVMRunningResource(tmpVM,stringPatternEnv,tmpVM->heap,t,start,pattern->proc->bc->size);
	int state=fklRunVM(tmpVM);
	if(!state)
		retval=fklCastVMvalueToCptr(fklTopGet(tmpVM->stack),curline);
	else
	{
		fklFreeByteCodeAndLnt(t);
		fklFreeVMheap(tmpVM->heap);
		fklUninitVMRunningResource(tmpVM);
		chdir(cwd);
		free(cwd);
		return NULL;
	}
	if(!retval)
	{
		fprintf(stderr,"error of compiling:Circular reference occur in expanding reader macro at line %d of %s\n",curline,filename);
	}
	fklFreeByteCodeAndLnt(t);
	FklVMheap* h=tmpVM->heap;
	fklUninitVMRunningResource(tmpVM);
	fklFreeVMheap(h);
	chdir(cwd);
	free(cwd);
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
			FklAstAtom* atm=atomCreators[token->type-1](token->value,cur->outer);
			if(!atm)
			{
				fklDeleteCptr(cur);
				free(cur);
				while(!fklIsPtrStackEmpty(stackStack))
				{
					FklPtrStack* cStack=fklPopPtrStack(stackStack);
					while(!fklIsPtrStackEmpty(cStack))
					{
						AstElem* v=fklPopPtrStack(cStack);
						FklAstCptr* cptr=v->cptr;
						fklDeleteCptr(cptr);
						free(cptr);
						free(v);
					}
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
			cur->type=FKL_ATM;
			cur->curline=token->line;
			cur->u.atom=atm;
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
								AstElem* v=fklPopPtrStack(cStack);
								FklAstCptr* cptr=v->cptr;
								fklDeleteCptr(cptr);
								free(cptr);
								free(v);
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
							AstElem* v=fklPopPtrStack(cStack);
							FklAstCptr* cptr=v->cptr;
							fklDeleteCptr(cptr);
							free(cptr);
							free(v);
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
					(strlen(token->value)==2&&token->value[0]=='#'&&token->value[1]=='&')?
					BOX:
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
				else if(state->pattern==BOX)
				{
					AstElem* v=newAstElem(AST_CAR,fklNewCptr(token->line,NULL));
					FklAstCptr* vCptr=v->cptr;
					vCptr->type=FKL_ATM;
					FklAstAtom* atom=fklNewAtom(FKL_BOX,NULL,vCptr->outer);
					atom->value.box=*(postfix->cptr);
					vCptr->u.atom=atom;
					free(postfix->cptr);
					free(postfix);
					fklPushPtrStack(v,cStack);
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
	FKL_ASSERT(!size||vec->base,__func__);
	for(size_t i=0;i<size;i++)
	{
		vec->base[i].outer=NULL;
		vec->base[i].type=FKL_NIL;
		vec->base[i].curline=0;
		vec->base[i].u.all=NULL;
	}
	if(base)
	{
		for(size_t i=0;i<size;i++)
			fklCopyCptr(&vec->base[i],&base[i]);
	}
}

void fklPrintRawAstString(FklAstString* str,FILE* out)
{
	fklPrintRawCharBuf(str->str,str->size,out);
}

void fklPrintCptr(const FklAstCptr* o_cptr,FILE* fp)
{
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* queueStack=fklNewPtrStack(32,16);
	fklPushPtrQueue(newAstElem(AST_CAR,(FklAstCptr*)o_cptr),queue);
	fklPushPtrStack(queue,queueStack);
	while(!fklIsPtrStackEmpty(queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			AstElem* e=fklPopPtrQueue(cQueue);
			FklAstCptr* cptr=e->cptr;
			if(e->place==AST_CDR)
				fputc(',',fp);
			free(e);
			if(cptr->type==FKL_ATM)
			{
				FklAstAtom* tmpAtm=cptr->u.atom;
				switch(tmpAtm->type)
				{
					case FKL_SYM:
						fprintf(fp,"%s",tmpAtm->value.sym);
						break;
					case FKL_STR:
						fklPrintRawAstString(&tmpAtm->value.str,fp);
						break;
					case FKL_I32:
						fprintf(fp,"%d",tmpAtm->value.i32);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",tmpAtm->value.i64);
						break;
					case FKL_F64:
						fprintf(fp,"%lf",tmpAtm->value.f64);
						break;
					case FKL_CHR:
						fklPrintRawChar(tmpAtm->value.chr,fp);
						break;
					case FKL_VECTOR:
						fputs("#(",fp);
						{
							FklPtrQueue* vQueue=fklNewPtrQueue();
							for(size_t i=0;i<tmpAtm->value.vec.size;i++)
								fklPushPtrQueue(newAstElem(AST_CAR,&tmpAtm->value.vec.base[i]),vQueue);
							fklPushPtrStack(vQueue,queueStack);
							cQueue=vQueue;
							continue;
						}
						break;
					case FKL_BIG_INT:
						fklPrintBigInt(&tmpAtm->value.bigInt,fp);
						break;
					case FKL_BOX:
						fputs("#&",fp);
						fklPushPtrQueue(newAstElem(AST_BOX,&tmpAtm->value.box),cQueue);
						break;
					default:
						break;
				}
			}
			else if(cptr->type==FKL_NIL)
				fputs("()",fp);
			else if(cptr->type==FKL_PAIR)
			{
				fputc('(',fp);
				FklPtrQueue* lQueue=fklNewPtrQueue();
				FklAstCptr* c=&cptr->u.pair->car;
				for(;;)
				{
					AstElem* ce=newAstElem(AST_CAR,c);
					fklPushPtrQueue(ce,lQueue);
					FklAstCptr* next=fklNextCptr(c);
					if(!next)
					{
						FklAstCptr* cdr=fklGetCptrCdr(c);
						if(cdr->type!=FKL_NIL)
						{
							AstElem* cdre=newAstElem(AST_CDR,cdr);
							fklPushPtrQueue(cdre,lQueue);
						}
						break;
					}
					c=next;
				}
				fklPushPtrStack(lQueue,queueStack);
				cQueue=lQueue;
				continue;
			}
			if(fklLengthPtrQueue(cQueue)&&((AstElem*)fklFirstPtrQueue(cQueue))->place!=AST_CDR&&((AstElem*)fklFirstPtrQueue(cQueue))->place!=AST_BOX)
				fputc(' ',fp);
		}
		fklPopPtrStack(queueStack);
		fklFreePtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(queueStack);
			if(fklLengthPtrQueue(cQueue)&&((AstElem*)fklFirstPtrQueue(cQueue))->place!=AST_CDR)
				fputc(' ',fp);
		}
	}
	fklFreePtrStack(queueStack);
}
