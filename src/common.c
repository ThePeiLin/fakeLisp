#define USE_CODE_NAME
#include<fakeLisp/common.h>
#include<fakeLisp/opcode.h>
#include"utils.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include<unistd.h>
#include<limits.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif
#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

const char* builtInErrorType[NUM_OF_BUILT_IN_ERROR_TYPE]=
{
	"dummy",
	"symbol-undefined",
	"syntax-error",
	"invalid-expression",
	"invalid-type-def",
	"circular-load",
	"invalid-pattern",
	"wrong-types-of-arguements",
	"stack-error",
	"too-many-arguements",
	"too-few-arguements",
	"cant-create-threads",
	"thread-error",
	"macro-expand-error",
	"invoke-error",
	"load-dll-faild",
	"invalid-symbol",
	"library-undefined",
	"unexpect-eof",
	"div-zero-error",
	"file-failure",
	"cant-dereference",
	"cant-get-element",
	"invalid-member-symbol",
	"no-member-type",
	"non-scalar-type",
	"invalid-assign",
	"invalid-access",
};

char* InterpreterPath=NULL;
const char* builtInSymbolList[]=
{
	"nil",
	"stdin",
	"stdout",
	"stderr",
	"car",
	"cdr",
	"cons",
	"append",
	"atom",
	"null",
	"not",
	"eq",
	"equal",
	"=",
	"+",
	"1+",
	"-",
	"-1+",
	"*",
	"/",
	"%",
	">",
	">=",
	"<",
	"<=",
	"chr",
	"int",
	"dbl",
	"str",
	"sym",
	"byts",
	"type",
	"nth",
	"length",
	"apply",
	"clcc",
	"file",
	"read",
	"getb",
	"prin1",
	"putb",
	"princ",
	"dll",
	"dlsym",
	"argv",
	"go",
	"chanl",
	"send",
	"recv",
	"error",
	"raise",
	"newf",
	"delf",
	"lfdl",
};

FklSymbolTable GlobSymbolTable=STATIC_SYMBOL_INIT;

char* fklGetStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"fklGetStringFromList",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* fklGetStringAfterBackslash(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while(!isspace(*(str+len))&&*(str+len)!='\0')
	{
		len++;
		if(!isalnum(str[len])&&str[len-1]!='\\')break;
	}
	FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"fklGetStringAfterBackslash",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* fklDoubleToString(double num)
{
	char numString[256]={0};
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	FAKE_ASSERT(tmp,"fklDoubleToString",__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);
	return tmp;
}


double fklStringToDouble(const char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}



char* fklIntToString(long num)
{
	char numString[256]={0};
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	FAKE_ASSERT((tmp=(char*)malloc(lenOfNum*sizeof(char))),"fklIntToString",__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);;
	return tmp;
}


int64_t fklStringToInt(const char* str)
{
	int64_t tmp;
	if(fklIsHexNum(str))
		sscanf(str,"%lx",&tmp);
	else if(fklIsOctNum(str))
		sscanf(str,"%lo",&tmp);
	else
		sscanf(str,"%ld",&tmp);
	return tmp;
}


int fklPower(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}

void fklPrintRawString(const char* objStr,FILE* out)
{
	const char* tmpStr=objStr;
	int len=strlen(objStr);
	putc('\"',out);
	for(;tmpStr<objStr+len;tmpStr++)
	{
		if(*tmpStr=='\"')
			fprintf(out,"\\\"");
		else if(isgraph(*tmpStr)||*tmpStr<0)
			putc(*tmpStr,out);
		else if(*tmpStr=='\x20')
			putc(*tmpStr,out);
		else
		{
			uint8_t j=*tmpStr;
			fprintf(out,"\\0x");
			fprintf(out,"%X",j/16);
			fprintf(out,"%X",j%16);
		}
	}
	putc('\"',out);
}

FklAstPair* fklNewPair(int curline,FklAstPair* prev)
{
	FklAstPair* tmp;
	FAKE_ASSERT((tmp=(FklAstPair*)malloc(sizeof(FklAstPair))),"fklNewPair",__FILE__,__LINE__);
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
	FAKE_ASSERT((tmp=(FklAstCptr*)malloc(sizeof(FklAstCptr))),"fklNewCptr",__FILE__,__LINE__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=FKL_NIL;
	tmp->u.all=NULL;
	return tmp;
}

FklAstAtom* fklNewAtom(int type,const char* value,FklAstPair* prev)
{
	FklAstAtom* tmp=NULL;
	FAKE_ASSERT((tmp=(FklAstAtom*)malloc(sizeof(FklAstAtom))),"fklNewAtom",__FILE__,__LINE__);
	switch(type)
	{
		case FKL_SYM:
		case FKL_STR:
			if(value!=NULL)
			{
				FAKE_ASSERT((tmp->value.str=(char*)malloc(strlen(value)+1)),"fklNewAtom",__FILE__,__LINE__);
				strcpy(tmp->value.str,value);
			}
			else
				tmp->value.str=NULL;
			break;
		case FKL_CHR:
		case FKL_I32:
		case FKL_DBL:
			*(int32_t*)(&tmp->value)=0;
			break;
		case FKL_BYTS:
			tmp->value.byts.size=0;
			tmp->value.byts.str=NULL;
			break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int fklCopyCptr(FklAstCptr* objCptr,const FklAstCptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	fklPushComStack(objCptr,s1);
	fklPushComStack((void*)copiedCptr,s2);
	FklAstAtom* atom1=NULL;
	FklAstAtom* atom2=NULL;
	while(!fklIsComStackEmpty(s2))
	{
		FklAstCptr* root1=fklPopComStack(s1);
		FklAstCptr* root2=fklPopComStack(s2);
		root1->type=root2->type;
		root1->curline=root2->curline;
		switch(root1->type)
		{
			case FKL_PAIR:
				root1->u.pair=fklNewPair(0,root1->outer);
				fklPushComStack(fklGetASTPairCar(root1),s1);
				fklPushComStack(fklGetASTPairCdr(root1),s1);
				fklPushComStack(fklGetASTPairCar(root2),s2);
				fklPushComStack(fklGetASTPairCdr(root2),s2);
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
					case FKL_I32:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.i32=atom2->value.i32;
						break;
					case FKL_DBL:
						atom1=fklNewAtom(atom2->type,NULL,root1->outer);
						atom1->value.dbl=atom2->value.dbl;
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
	fklFreeComStack(s1);
	fklFreeComStack(s2);
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

int fklFklAstCptrcmp(const FklAstCptr* first,const FklAstCptr* second)
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
				if((firAtm->type==FKL_SYM||firAtm->type==FKL_STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==FKL_I32&&firAtm->value.i32!=secAtm->value.i32)return 0;
				else if(firAtm->type==FKL_DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
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

int fklIsHexNum(const char* objStr)
{
	int i=(*objStr=='-')?1:0;
	int len=strlen(objStr);
	if(!strncmp(objStr+i,"0x",2)||!strncmp(objStr+i,"0X",2))
	{
		for(i+=2;i<len;i++)
		{
			if(!isxdigit(objStr[i]))
				return 0;
		}
	}
	else
		return 0;
	return 1;
}

int fklIsOctNum(const char* objStr)
{
	int i=(*objStr=='-')?1:0;
	int len=strlen(objStr);
	if(objStr[i]!='0')
		return 0;
	for(;i<len;i++)
	{
		if(!isdigit(objStr[i])||objStr[i]>'7')
			return 0;
	}
	return 1;
}

int fklIsDouble(const char* objStr)
{
	int i=(objStr[0]=='-')?1:0;
	int len=strlen(objStr);
	int isHex=(!strncmp(objStr+i,"0x",2)||!strncmp(objStr+i,"0X",2));
	for(i+=isHex*2;i<len;i++)
	{
		if(objStr[i]=='.'||(i!=0&&toupper(objStr[i])==('E'+isHex*('P'-'E'))&&i<(len-1)))
			return 1;
	}
	return 0;
}

int fklStringToChar(const char* objStr)
{
	int ch=0;
	if(toupper(objStr[0])=='X'&&isxdigit(objStr[1]))
	{
		size_t len=strlen(objStr)+2;
		char* tmpStr=(char*)malloc(sizeof(char)*len);
		sprintf(tmpStr,"0%s",objStr);
		if(fklIsHexNum(tmpStr))
		{
			sscanf(tmpStr+2,"%x",&ch);
		}
		free(tmpStr);
	}
	else if(fklIsNum(objStr))
	{
		if(fklIsHexNum(objStr))
		{
			objStr++;
			sscanf(objStr,"%x",&ch);
		}
		else if(fklIsOctNum(objStr))
			sscanf(objStr,"%o",&ch);
		else
			sscanf(objStr,"%d",&ch);
	}
	else
	{
		switch(toupper(*(objStr)))
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
			default:
				ch=*(objStr);
				break;
		}
	}
	return ch;
}

int fklIsNum(const char* objStr)
{
	int len=strlen(objStr);
	int i=(*objStr=='-')?1:0;
	int hasDot=0;
	int hasExp=0;
	if(objStr[0]=='-'&&!isdigit(objStr[1]))
		return 0;
	else
	{
		if(!strncmp(objStr+i,"0x",2)||!strncmp(objStr+i,"0X",2))
		{
			for(i+=2;i<len;i++)
			{
				if(objStr[i]=='.')
				{
					if(hasDot)
						return 0;
					else
						hasDot=1;
				}
				else if(!isxdigit(objStr[i]))
				{
					if(toupper(objStr[i])=='P')
					{
						if(i<3||hasExp||i>(len-2))
							return 0;
						hasExp=1;
					}
					else
						return 0;
				}
			}
		}
		else
		{
			for(;i<len;i++)
			{
				if(objStr[i]=='.')
				{
					if(hasDot)
						return 0;
					else
						hasDot=1;
				}
				else if(!isdigit(objStr[i]))
				{
					if(toupper(objStr[i])=='E')
					{
						if(i<1||hasExp||i>(len-2))
							return 0;
						hasExp=1;
					}
					else
						return 0;
				}
			}
		}
	}
	return 1;
}

void fklFreeAtom(FklAstAtom* objAtm)
{
	if(objAtm->type==FKL_SYM||objAtm->type==FKL_STR)free(objAtm->value.str);
	else if(objAtm->type==FKL_BYTS)
	{
		objAtm->value.byts.size=0;
		free(objAtm->value.byts.str);
	}
	free(objAtm);
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
				switch((int)tmpAtm->type)
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
					case FKL_DBL:
						fprintf(out,"%lf",tmpAtm->value.dbl);
						break;
					case FKL_CHR:
						fklPrintRawChar(tmpAtm->value.chr,out);
						break;
					case FKL_BYTS:
						fklPrintByteStr(tmpAtm->value.byts.size,tmpAtm->value.byts.str,out,1);
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

void fklExError(const FklAstCptr* obj,int type,FklIntpr* inter)
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

void fklPrintRawChar(char chr,FILE* out)
{
	if(isgraph(chr))
	{
		if(chr=='\\')
			fprintf(out,"#\\\\\\");
		else
			fprintf(out,"#\\%c",chr);
	}
	else
	{
		uint8_t j=chr;
		fprintf(out,"#\\\\x");
		fprintf(out,"%X",j/16);
		fprintf(out,"%X",j%16);
	}
}

FklPreEnv* fklNewEnv(FklPreEnv* prev)
{
	FklPreEnv* curEnv=NULL;
	FAKE_ASSERT((curEnv=(FklPreEnv*)malloc(sizeof(FklPreEnv))),"fklNewEnv",__FILE__,__LINE__);
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

FklIntpr* fklNewIntpr(const char* filename,FILE* file,FklCompEnv* env,LineNumberTable* lnt,FklVMDefTypes* deftypes)
{
	FklIntpr* tmp=NULL;
	FAKE_ASSERT((tmp=(FklIntpr*)malloc(sizeof(FklIntpr))),"fklNewIntpr",__FILE__,__LINE__)
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
		tmp->deftypes=fklNewVMDefTypes();
	if(env)
	{
		tmp->glob=env;
		return tmp;
	}
	tmp->glob=fklNewCompEnv(NULL);
	fklInitCompEnv(tmp->glob);
	return tmp;
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

void fklFreeIntpr(FklIntpr* inter)
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

FklCompEnv* fklNewCompEnv(FklCompEnv* prev)
{
	FklCompEnv* tmp=(FklCompEnv*)malloc(sizeof(FklCompEnv));
	FAKE_ASSERT(tmp,"newComEnv",__FILE__,__LINE__);
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
			FREE_ALL_LINE_NUMBER_TABLE(curEnv->proc->l,curEnv->proc->ls);
			fklFreeByteCodelnt(curEnv->proc);
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

int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n)
{
	int32_t l=0;
	int32_t h=n-1;
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

FklCompDef* fklAddCompDef(const char* name,FklCompEnv* curEnv)
{
	if(curEnv->head==NULL)
	{
		FklSymTabNode* node=fklAddSymbolToGlob(name);
		FAKE_ASSERT((curEnv->head=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
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
			FAKE_ASSERT((curDef=(FklCompDef*)malloc(sizeof(FklCompDef))),"fklAddCompDef",__FILE__,__LINE__);
			curDef->id=node->id;
			curDef->next=curEnv->head;
			curEnv->head=curDef;
		}
		return curDef;
	}
}

FklByteCode* fklNewByteCode(unsigned int size)
{
	FklByteCode* tmp=NULL;
	FAKE_ASSERT((tmp=(FklByteCode*)malloc(sizeof(FklByteCode))),"fklNewByteCode",__FILE__,__LINE__);
	tmp->size=size;
	FAKE_ASSERT((tmp->code=(uint8_t*)malloc(size*sizeof(uint8_t))),"fklNewByteCode",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
}

FklByteCodelnt* fklNewByteCodelnt(FklByteCode* bc)
{
	FklByteCodelnt* t=(FklByteCodelnt*)malloc(sizeof(FklByteCodelnt));
	FAKE_ASSERT(t,"fklNewByteCode",__FILE__,__LINE__);
	t->ls=0;
	t->l=NULL;
	t->bc=bc;
	return t;
}

void fklFreeByteCodelnt(FklByteCodelnt* t)
{
	fklFreeByteCode(t->bc);
	if(t->l)
		free(t->l);
	free(t);
}

void fklFreeByteCode(FklByteCode* obj)
{
	free(obj->code);
	free(obj);
}

void fklCodeCat(FklByteCode* fir,const FklByteCode* sec)
{
	int32_t size=fir->size;
	fir->size=sec->size+fir->size;
	fir->code=(uint8_t*)realloc(fir->code,sizeof(uint8_t)*fir->size);
	FAKE_ASSERT(fir->code||!fir->size,"fklCodeCat",__FILE__,__LINE__);
	memcpy(fir->code+size,sec->code,sec->size);
}

void fklReCodeCat(const FklByteCode* fir,FklByteCode* sec)
{
	int32_t size=fir->size;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*(fir->size+sec->size));
	FAKE_ASSERT(tmp,"fklReCodeCat",__FILE__,__LINE__);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

FklByteCode* fklCopyByteCode(const FklByteCode* obj)
{
	FklByteCode* tmp=fklNewByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt* obj)
{
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklCopyByteCode(obj->bc));
	tmp->ls=obj->ls;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*obj->ls);
	FAKE_ASSERT(tmp->l,"fklCopyByteCodelnt",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<obj->ls;i++)
	{
		LineNumTabNode* t=obj->l[i];
		LineNumTabNode* node=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
		tmp->l[i]=node;
	}
	return tmp;
}

void fklInitCompEnv(FklCompEnv* curEnv)
{
	int i=0;
	for(i=0;i<NUM_OF_BUILT_IN_SYMBOL;i++)
		fklAddCompDef(builtInSymbolList[i],curEnv);
}

char* fklCopyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	FAKE_ASSERT(tmp,"fklCopyStr",__FILE__,__LINE__);
	strcpy(tmp,str);
	return tmp;

}



int fklIsscript(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=4)return 0;
	else return !strcmp(filename+i,".fkl");
}

int fklIscode(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=5)return 0;
	else return !strcmp(filename+i,".fklc");
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

void fklPrintByteCode(const FklByteCode* tmpCode,FILE* fp)
{
	int32_t i=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,codeName[(int)tmpCode->code[i]].codeName);
		switch(codeName[tmpCode->code[i]].len)
		{
			case -4:
				{
					fprintf(fp,"%s ",fklFindSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(FklSid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						FklSid_t type=*(FklSid_t*)(tmpCode->code+i);
						i+=sizeof(FklSid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",fklGetGlobSymbolWithId(type)->symbol,pCpc);
						fklPrintAsByteStr(tmpCode->code+i,pCpc,fp);
						i+=pCpc;
					}
				}
				break;
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				fklPrintAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen((char*)tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				fklPrintRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FAKE_PUSH_SYM)
					fprintf(fp,"%s",fklGetGlobSymbolWithId(*(FklSid_t*)(tmpCode->code+i+1))->symbol);
				else
					fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				switch(tmpCode->code[i])
				{
					case FAKE_PUSH_DBL:
						fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_I64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(FklTypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
				i+=13;
				break;
		}
		putc('\n',fp);
	}
}

uint8_t fklCastCharInt(char ch)
{
	if(isdigit(ch))return ch-'0';
	else if(isxdigit(ch))
	{
		if(ch>='a'&&ch<='f')return 10+ch-'a';
		else if(ch>='A'&&ch<='F')return 10+ch-'A';
	}
	return 0;
}

uint8_t* fklCastStrByteStr(const char* str)
{
	int len=strlen(str);
	int32_t size=(len%2)?(len/2+1):len/2;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"fklCastStrByteStr",__FILE__,__LINE__);
	int32_t i=0;
	int k=0;
	for(;i<size;i++)
	{
		tmp[i]=16*fklCastCharInt(str[k]);
		if(str[k+1]!='\0')tmp[i]+=fklCastCharInt(str[k+1]);
		k+=2;
	}
	return tmp;
}

void fklPrintByteStr(size_t size,const uint8_t* str,FILE* fp,int mode)
{
	if(mode)fputs("#b",fp);
	unsigned int i=0;
	for(;i<size;i++)
	{
		uint8_t j=str[i];
		fprintf(fp,"%X",j/16);
		fprintf(fp,"%X",j%16);
	}
}

void fklPrintAsByteStr(const uint8_t* str,int32_t size,FILE* fp)
{
	fputs("#b",fp);
	for(int i=0;i<size;i++)
	{
		uint8_t j=str[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void* fklCopyMemory(void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	FAKE_ASSERT(tmp,"fklCopyMemory",__FILE__,__LINE__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

int fklHasLoadSameFile(const char* filename,const FklIntpr* inter)
{
	while(inter!=NULL)
	{
		if(!strcmp(inter->filename,filename))
			return 1;
		inter=inter->prev;
	}
	return 0;
}

FklIntpr* fklGetFirstIntpr(FklIntpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter;
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

void fklChangeWorkPath(const char* filename)
{
#ifdef _WIN32
	char* p=_fullpath(NULL,filename,0);
#else
	char* p=realpath(filename,NULL);
#endif
	char* wp=fklGetDir(p);
	if(chdir(wp))
	{
		perror(wp);
		exit(EXIT_FAILURE);
	}
	free(p);
	free(wp);
}

char* fklGetDir(const char* filename)
{
#ifdef _WIN32
	char dp='\\';
#else
	char dp='/';
#endif
	int i=strlen(filename)-1;
	for(;filename[i]!=dp;i--);
	char* tmp=(char*)malloc(sizeof(char)*(i+1));
	FAKE_ASSERT(tmp,"fklGetDir",__FILE__,__LINE__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* fklGetStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	FAKE_ASSERT(tmp,"fklGetStringFromFile",__FILE__,__LINE__);
	tmp[0]='\0';
	char* before;
	int i=0;
	char ch;
	int j;
	while((ch=getc(file))!=EOF&&ch!='\0')
	{
		i++;
		j=i-1;
		before=tmp;
		FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(i+1))),"fklGetStringFromFile",__FILE__,__LINE__);
		if(before!=NULL)
		{
			memcpy(tmp,before,j);
			free(before);
		}
		*(tmp+j)=ch;
	}
	if(tmp!=NULL)tmp[i]='\0';
	return tmp;
}

int fklEqByteString(const FklByteString* fir,const FklByteString* sec)
{
	if(fir->size!=sec->size)return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

char* fklGetLastWorkDir(FklIntpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->curDir;
}

char* fklRelpath(char* abs,char* relto)
{
#ifdef _WIN32
	char divstr[]="\\";
	char upperDir[]="..\\";
#else
	char divstr[]="/";
	char upperDir[]="../";
#endif
	char* cabs=fklCopyStr(abs);
	char* crelto=fklCopyStr(relto);
	int lengthOfAbs=0;
	int lengthOfRelto=0;
	char** absDirs=fklSplit(cabs,divstr,&lengthOfAbs);
	char** reltoDirs=fklSplit(crelto,divstr,&lengthOfRelto);
	int length=(lengthOfAbs<lengthOfRelto)?lengthOfAbs:lengthOfRelto;
	int lastCommonRoot=-1;
	int index;
	for(index=0;index<length;index++)
	{
		if(!strcmp(absDirs[index],reltoDirs[index]))
			lastCommonRoot=index;
		else break;
	}
#ifdef _WIN32
	if(lastCommonRoot==-1)
	{
		fprintf(stderr,"%s:Cant get relative path.\n",abs);
		exit(EXIT_FAILURE);
	}
#endif
	char rp[PATH_MAX]={0};
	for(index=lastCommonRoot+1;index<lengthOfAbs;index++)
		if(lengthOfAbs>0)
			strcat(rp,upperDir);
	for(index=lastCommonRoot+1;index<lengthOfRelto-1;index++)
	{
		strcat(rp,reltoDirs[index]);
#ifdef _WIN32
		strcat(rp,"\\");
#else
		strcat(rp,"/");
#endif
	}
	if(reltoDirs!=NULL)
		strcat(rp,reltoDirs[lengthOfRelto-1]);
	char* trp=fklCopyStr(rp);
	free(cabs);
	free(crelto);
	free(absDirs);
	free(reltoDirs);
	return trp;
}

char** fklSplit(char* str,char* divstr,int* length)
{
	int count=0;
	char* pNext=NULL;
	char** strArry=(char**)malloc(0);
	FAKE_ASSERT(strArry,"fklSplit",__FILE__,__LINE__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		FAKE_ASSERT(strArry,"fklSplit",__FILE__,__LINE__);
		strArry[count-1]=pNext;
		pNext=strtok(NULL,divstr);
	}
	*length=count;
	return strArry;
}

char* fklCastEscapeCharater(const char* str,char end,size_t* len)
{
	int32_t strSize=0;
	int32_t memSize=MAX_STRING_SIZE;
	int32_t i=0;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	while(str[i]!=end)
	{
		int ch=0;
		if(str[i]=='\\')
		{
			if(isdigit(str[i+1]))
			{
				if(str[i+1]=='0')
				{
					if(isdigit(str[i+2]))
					{
						int len=0;
						while((isdigit(str[i+2+len])&&(str[i+2+len]<'8')&&len<4))len++;
						sscanf(str+i+1,"%4o",&ch);
						i+=len+2;
					}
				}
				else
				{
					int len=0;
					while(isdigit(str[i+1+len])&&len<4)len++;
					sscanf(str+i+1,"%4d",&ch);
					i+=len+1;
				}
			}
			else if(toupper(str[i+1])=='X')
			{
				int len=0;
				while(isxdigit(str[i+2+len])&&len<2)len++;
				sscanf(str+i+1,"%4x",&ch);
				i+=len+2;
			}
			else if(str[i+1]=='\n')
			{
				i+=2;
				continue;
			}
			else
			{
				switch(toupper(str[i+1]))
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
					default:ch=str[i+1];break;
				}
				i+=2;
			}
		}
		else ch=str[i++];
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			FAKE_ASSERT(tmp,"castKeyStringToNormalString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	if(tmp)tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,memSize*sizeof(char));
	FAKE_ASSERT(tmp,"castKeyStringToNormalString",__FILE__,__LINE__);
	*len=i+1;
	return tmp;
}

FklIntpr* fklNewTmpIntpr(const char* filename,FILE* fp)
{
	FklIntpr* tmp=NULL;
	FAKE_ASSERT((tmp=(FklIntpr*)malloc(sizeof(FklIntpr))),"fklNewTmpIntpr",__FILE__,__LINE__);
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

int32_t fklCountChar(const char* str,char c,int32_t len)
{
	int32_t num=0;
	int32_t i=0;
	if(len==-1)
	{
		for(;str[i]!='\0';i++)
			if(str[i]==c)
				num++;
	}
	else
	{
		for(;i<len;i++)
			if(str[i]==c)
				num++;
	}
	return num;
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
	FAKE_ASSERT(tmp,"fklNewDefines",__FILE__,__LINE__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	FAKE_ASSERT(tmp->symbol,"fklNewDefines",__FILE__,__LINE__);
	strcpy(tmp->symbol,name);
	tmp->obj=(FklAstCptr){NULL,0,FKL_NIL,{NULL}};
	tmp->next=NULL;
	return tmp;
}

FklSymbolTable* fklNewSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FAKE_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	return tmp;
}

FklSymTabNode* fklNewSymTabNode(const char* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FAKE_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
	tmp->id=0;
	tmp->symbol=fklCopyStr(symbol);
	return tmp;
}

FklSymTabNode* fklAddSymbol(const char* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	if(!table->list)
	{
		node=fklNewSymTabNode(sym);
		table->num=1;
		node->id=table->num-1;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FAKE_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FAKE_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
		table->list[0]=node;
		table->idl[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=table->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int r=strcmp(table->list[mid]->symbol,sym);
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
				return table->list[mid];
		}
		if(strcmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FAKE_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		node=fklNewSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num-1;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FAKE_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
		table->idl[table->num-1]=node;
	}
	return node;
}

FklSymTabNode* fklAddSymbolToGlob(const char* sym)
{
	return fklAddSymbol(sym,&GlobSymbolTable);
}


void fklFreeSymTabNode(FklSymTabNode* node)
{
	free(node->symbol);
	free(node);
}

void fklFreeSymbolTable(FklSymbolTable* table)
{
	int32_t i=0;
	for(;i<table->num;i++)
		fklFreeSymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

void fklFreeGlobSymbolTable()
{
	int32_t i=0;
	for(;i<GlobSymbolTable.num;i++)
		fklFreeSymTabNode(GlobSymbolTable.list[i]);
	free(GlobSymbolTable.list);
	free(GlobSymbolTable.idl);
}

FklSymTabNode* fklFindSymbol(const char* symbol,FklSymbolTable* table)
{
	if(!table->list)
		return NULL;
	int32_t l=0;
	int32_t h=table->num-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int resultOfCmp=strcmp(table->list[mid]->symbol,symbol);
		if(resultOfCmp>0)
			h=mid-1;
		else if(resultOfCmp<0)
			l=mid+1;
		else
			return table->list[mid];
	}
	return NULL;
}

FklSymTabNode* fklFindSymbolInGlob(const char* sym)
{
	return fklFindSymbol(sym,&GlobSymbolTable);
}

FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id)
{
	return GlobSymbolTable.idl[id];
}

void fklPrintSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->num;i++)
	{
		FklSymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:%s id:%d\n",cur->symbol,cur->id);
	}
	fprintf(fp,"size:%d\n",table->num);
}

void fklPrintGlobSymbolTable(FILE* fp)
{
	fklPrintSymbolTable(&GlobSymbolTable,fp);
}

FklAstCptr* fklBaseCreateTree(const char* objStr,FklIntpr* inter)
{
	if(!objStr)
		return NULL;
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	int32_t i=0;
	for(;isspace(objStr[i]);i++)
		if(objStr[i]=='\n')
			inter->curline+=1;
	int32_t curline=(inter)?inter->curline:0;
	FklAstCptr* tmp=fklNewCptr(curline,NULL);
	fklPushComStack(tmp,s1);
	int hasComma=1;
	while(objStr[i]&&!fklIsComStackEmpty(s1))
	{
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		curline=inter->curline;
		FklAstCptr* root=fklPopComStack(s1);
		if(objStr[i]=='(')
		{
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				FklAstCptr* tmp=fklPopComStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushComStack(fklGetASTPairCdr(tmp),s1);
					fklPushComStack(fklGetASTPairCar(tmp),s1);
				}
			}
			int j=0;
			for(;isspace(objStr[i+1+j]);j++);
			if(objStr[i+j+1]==')')
			{
				root->type=FKL_NIL;
				root->u.all=NULL;
				i+=j+2;
			}
			else
			{
				hasComma=0;
				root->type=FKL_PAIR;
				root->u.pair=fklNewPair(curline,root->outer);
				fklPushComStack((void*)s1->top,s2);
				fklPushComStack(fklGetASTPairCdr(root),s1);
				fklPushComStack(fklGetASTPairCar(root),s1);
				i++;
			}
		}
		else if(objStr[i]==',')
		{
			if(hasComma)
			{
				fklDeleteCptr(tmp);
				free(tmp);
				tmp=NULL;
				break;
			}
			else hasComma=1;
			if(root->outer->prev&&root->outer->prev->cdr.u.pair==root->outer)
			{
				//将为下一个部分准备的pair删除并将该pair的前一个pair的cdr部分入栈
				s1->top=(long)fklTopComStack(s2);
				FklAstCptr* tmp=&root->outer->prev->cdr;
				free(tmp->u.pair);
				tmp->type=FKL_NIL;
				tmp->u.all=NULL;
				fklPushComStack(tmp,s1);
			}
			i++;
		}
		else if(objStr[i]==')')
		{
			hasComma=0;
			long t=(long)fklPopComStack(s2);
			FklAstCptr* c=s1->data[t];
			if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
			{
				//如果还有为下一部分准备的pair，则将该pair删除
				FklAstCptr* tmpCptr=s1->data[t];
				tmpCptr=&tmpCptr->outer->prev->cdr;
				tmpCptr->type=FKL_NIL;
				free(tmpCptr->u.pair);
				tmpCptr->u.all=NULL;
			}
			//将栈顶恢复为将pair入栈前的位置
			s1->top=t;
			i++;
		}
		else
		{
			root->type=FKL_ATM;
			char* str=NULL;
			if(objStr[i]=='\"')
			{
				size_t len=0;
				str=fklCastEscapeCharater(objStr+i+1,'\"',&len);
				inter->curline+=fklCountChar(objStr+i,'\n',len);
				root->u.atom=fklNewAtom(FKL_STR,str,root->outer);
				i+=len+1;
				free(str);
			}
			else if(objStr[i]=='#')
			{
				i++;
				FklAstAtom* atom=NULL;
				switch(objStr[i])
				{
					case '\\':
						str=fklGetStringAfterBackslash(objStr+i+1);
						atom=fklNewAtom(FKL_CHR,NULL,root->outer);
						root->u.atom=atom;
						atom->value.chr=(str[0]=='\\')?
							fklStringToChar(str+1):
							str[0];
						i+=strlen(str)+1;
						break;
					case 'b':
						str=fklGetStringAfterBackslash(objStr+i+1);
						atom=fklNewAtom(FKL_BYTS,NULL,root->outer);
						atom->value.byts.size=strlen(str)/2+strlen(str)%2;
						atom->value.byts.str=fklCastStrByteStr(str);
						root->u.atom=atom;
						i+=strlen(str)+1;
						break;
					default:
						str=fklGetStringFromList(objStr+i-1);
						atom=fklNewAtom(FKL_SYM,str,root->outer);
						root->u.atom=atom;
						i+=strlen(str)-1;
						break;
				}
				free(str);
			}
			else
			{
				char* str=fklGetStringFromList(objStr+i);
				if(fklIsNum(str))
				{
					FklAstAtom* atom=NULL;
					if(fklIsDouble(str))
					{
						atom=fklNewAtom(FKL_DBL,NULL,root->outer);
						atom->value.dbl=fklStringToDouble(str);
					}
					else
					{
						atom=fklNewAtom(FKL_I32,NULL,root->outer);
						int64_t num=fklStringToInt(str);
						if(num>=INT32_MAX||num<=INT32_MIN)
						{
							atom->type=FKL_I64;
							atom->value.i64=num;
						}
						else
							atom->value.i32=num;
					}
					root->u.atom=atom;
				}
				else
					root->u.atom=fklNewAtom(FKL_SYM,str,root->outer);
				i+=strlen(str);
				free(str);
			}
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				FklAstCptr* tmp=fklPopComStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushComStack(fklGetASTPairCdr(tmp),s1);
					fklPushComStack(fklGetASTPairCar(tmp),s1);
				}
			}
		}
	}
	fklFreeComStack(s1);
	fklFreeComStack(s2);
	return tmp;
}

void fklWriteSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t size=table->num;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
		fwrite(table->idl[i]->symbol,strlen(table->idl[i]->symbol)+1,1,fp);
}

void fklWriteGlobSymbolTable(FILE* fp)
{
	fklWriteSymbolTable(&GlobSymbolTable,fp);
}

void fklWriteLineNumberTable(LineNumberTable* lnt,FILE* fp)
{
	int32_t size=lnt->num;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
	{
		LineNumTabNode* n=lnt->list[i];
		fwrite(&n->fid,sizeof(n->fid),1,fp);
		fwrite(&n->scp,sizeof(n->scp),1,fp);
		fwrite(&n->cpc,sizeof(n->cpc),1,fp);
		fwrite(&n->line,sizeof(n->line),1,fp);
	}
}

int fklIsAllSpace(const char* str)
{
	for(;*str;str++)
		if(!isspace(*str))
			return 0;
	return 1;
}

LineNumberTable* fklNewLineNumTable()
{
	LineNumberTable* t=(LineNumberTable*)malloc(sizeof(LineNumberTable));
	FAKE_ASSERT(t,"fklNewLineNumTable",__FILE__,__LINE__);
	t->num=0;
	t->list=NULL;
	return t;
}

LineNumTabNode* fklNewLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line)
{
	LineNumTabNode* t=(LineNumTabNode*)malloc(sizeof(LineNumTabNode));
	FAKE_ASSERT(t,"fklNewLineNumTable",__FILE__,__LINE__);
	t->fid=fid;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

void fklFreeLineNumTabNode(LineNumTabNode* n)
{
	free(n);
}

void fklFreeDefTypeTable(FklVMDefTypes* defs)
{
	size_t i=0;
	size_t num=defs->num;
	for(;i<num;i++)
		free(defs->u[i]);
	free(defs->u);
	free(defs);
}

void fklFreeLineNumberTable(LineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->num;i++)
		fklFreeLineNumTabNode(t->list[i]);
	free(t->list);
	free(t);
}

LineNumTabNode* fklFindLineNumTabNode(uint32_t cp,LineNumberTable* t)
{
	int32_t i=0;
	uint32_t size=t->num;
	LineNumTabNode** list=t->list;
	for(;i<size;i++)
	{
		if(list[i]->scp<=cp&&(list[i]->scp+list[i]->cpc)>=cp)
			return list[i];
	}
	return NULL;
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt* o,int32_t size)
{
	int32_t i=0;
	for(;i<o->ls;i++)
		o->l[i]->scp+=size;
}

void fklLntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2)
{
	if(!t->list)
	{
		t->num=s2;
		t->list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s2);
		FAKE_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
		l2[0]->cpc+=bs;
		INCREASE_ALL_SCP(l2+1,s2-1,bs);
		memcpy(t->list,l2,(s2)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(l2,s2,bs);
		if(t->list[t->num-1]->line==l2[0]->line&&t->list[t->num-1]->fid==l2[0]->fid)
		{
			t->list[t->num-1]->cpc+=l2[0]->cpc;
			t->list=(LineNumTabNode**)realloc(t->list,sizeof(LineNumTabNode*)*(t->num+s2-1));
			FAKE_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2+1,(s2-1)*sizeof(LineNumTabNode*));
			t->num+=s2-1;
			fklFreeLineNumTabNode(l2[0]);
		}
		else
		{
			t->list=(LineNumTabNode**)realloc(t->list,sizeof(LineNumTabNode*)*(t->num+s2));
			FAKE_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2,(s2)*sizeof(LineNumTabNode*));
			t->num+=s2;
		}
	}
}

void fklCodefklLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(!f->l)
	{
		f->ls=s->ls;
		f->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
		FAKE_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
		s->l[0]->cpc+=f->bc->size;
		INCREASE_ALL_SCP(s->l+1,s->ls-1,f->bc->size);
		memcpy(f->l,s->l,(s->ls)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
			FAKE_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			f->ls+=s->ls-1;
			fklFreeLineNumTabNode(s->l[0]);
		}
		else
		{
			f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls));
			FAKE_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			f->ls+=s->ls;
		}
	}
	fklCodeCat(f->bc,s->bc);
}

void fklCodelntCopyCat(FklByteCodelnt* f,const FklByteCodelnt* s)
{
	if(s->ls)
	{
		if(!f->l)
		{
			f->ls=s->ls;
			f->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
			FAKE_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
			uint32_t i=0;
			for(;i<f->ls;i++)
			{
				LineNumTabNode* t=s->l[i];
				f->l[i]=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			f->l[0]->cpc+=f->bc->size;
			INCREASE_ALL_SCP(f->l+1,f->ls-1,f->bc->size);
		}
		else
		{
			uint32_t i=0;
			LineNumTabNode** tl=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
			FAKE_ASSERT(tl,"fklCodelntCopyCat",__FILE__,__LINE__);
			for(;i<s->ls;i++)
			{
				LineNumTabNode* t=s->l[i];
				tl[i]=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			INCREASE_ALL_SCP(tl,s->ls,f->bc->size);
			if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
			{
				f->l[f->ls-1]->cpc+=s->l[0]->cpc;
				f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
				FAKE_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl+1,(s->ls-1)*sizeof(LineNumTabNode*));
				f->ls+=s->ls-1;
				fklFreeLineNumTabNode(tl[0]);
			}
			else
			{
				f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls));
				FAKE_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl,(s->ls)*sizeof(LineNumTabNode*));
				f->ls+=s->ls;
			}
			free(tl);
		}
	}
	fklCodeCat(f->bc,s->bc);
}

void reCodefklLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(!s->l)
	{
		s->ls=f->ls;
		s->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*f->ls);
		FAKE_ASSERT(s->l,"reCodefklLntCat",__FILE__,__LINE__);
		f->l[f->ls-1]->cpc+=s->bc->size;
		memcpy(s->l,f->l,(f->ls)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
			FAKE_ASSERT(l,"reCodefklLntCat",__FILE__,__LINE__);
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			fklFreeLineNumTabNode(s->l[0]);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls-1;
		}
		else
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls));
			FAKE_ASSERT(l,"fklReCodeCat",__FILE__,__LINE__);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls;
		}
	}
	fklReCodeCat(f->bc,s->bc);
}

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp)
{
	FklSymbolTable* table=&GlobSymbolTable;
	FklByteCode* tmpCode=obj->bc;
	int32_t i=0;
	int32_t j=0;
	int32_t fid=0;
	int32_t line=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,codeName[(int)tmpCode->code[i]].codeName);
		switch(codeName[(int)tmpCode->code[i]].len)
		{
			case -4:
				{
					fprintf(fp,"%s ",fklFindSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(FklSid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						FklSid_t type=*(FklSid_t*)(tmpCode->code+i);
						i+=sizeof(FklSid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",fklGetGlobSymbolWithId(type)->symbol,pCpc);
						fklPrintAsByteStr(tmpCode->code+i,pCpc,fp);
						i+=pCpc;
					}
				}
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				fklPrintAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen((char*)tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				fklPrintRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FAKE_PUSH_SYM)
					fprintf(fp,"%s",fklGetGlobSymbolWithId(*(FklSid_t*)(tmpCode->code+i+1))->symbol);
				else
					fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				switch(tmpCode->code[i])
				{
					case FAKE_PUSH_DBL:
						fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_I64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
					case FAKE_PUSH_FPROC:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(FklSid_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(FklTypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
				i+=13;
				break;
		}
		if(obj->l[j]->scp+obj->l[j]->cpc<i)
			j++;
		putc('\t',fp);
		if(obj->l[j]->fid!=fid||obj->l[j]->line!=line)
		{
			fid=obj->l[j]->fid;
			line=obj->l[j]->line;
			fprintf(fp,"%s:%d:%d",table->idl[obj->l[j]->fid]->symbol,obj->l[j]->line,obj->l[j]->cpc);
		}
		putc('\n',fp);
	}
}

FklByteCodeLabel* fklNewByteCodeLable(int32_t place,const char* label)
{
	FklByteCodeLabel* tmp=(FklByteCodeLabel*)malloc(sizeof(FklByteCodeLabel));
	FAKE_ASSERT(tmp,"fklNewByteCodeLable",__FILE__,__LINE__);
	tmp->place=place;
	tmp->label=fklCopyStr(label);
	return tmp;
}

FklByteCodeLabel* fklFindByteCodeLabel(const char* label,FklComStack* s)
{
	int32_t l=0;
	int32_t h=s->top-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int cmp=strcmp(((FklByteCodeLabel*)s->data[mid])->label,label);
		if(cmp>0)
			h=mid-1;
		else if(cmp<0)
			l=mid+1;
		else
			return (FklByteCodeLabel*)s->data[mid];
	}
	return NULL;
}


void fklFreeByteCodeLabel(FklByteCodeLabel* obj)
{
	free(obj->label);
	free(obj);
}

int fklIsComStackEmpty(FklComStack* stack)
{
	return stack->top==0;
}

FklComStack* fklNewComStack(uint32_t num)
{
	FklComStack* tmp=(FklComStack*)malloc(sizeof(FklComStack));
	FAKE_ASSERT(tmp,"fklNewComStack",__FILE__,__LINE__);
	tmp->data=(void**)malloc(sizeof(void*)*num);
	FAKE_ASSERT(tmp->data,"fklNewComStack",__FILE__,__LINE__);
	tmp->num=num;
	tmp->top=0;
	return tmp;
}

void fklPushComStack(void* data,FklComStack* stack)
{
	if(stack->top==stack->num)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num+32)*sizeof(void*));
		FAKE_ASSERT(tmpData,"fklPushComStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num+=32;
	}
	stack->data[stack->top]=data;
	stack->top+=1;
}

void* fklPopComStack(FklComStack* stack)
{
	if(fklIsComStackEmpty(stack))
		return NULL;
	stack->top-=1;
	void* tmp=stack->data[stack->top];
	fklRecycleComStack(stack);
	return tmp;
}

void* fklTopComStack(FklComStack* stack)
{
	if(fklIsComStackEmpty(stack))
		return NULL;
	return stack->data[stack->top-1];
}

void fklFreeComStack(FklComStack* stack)
{
	free(stack->data);
	free(stack);
}

void fklRecycleComStack(FklComStack* stack)
{
	if(stack->num-stack->top>32)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num-32)*sizeof(void*));
		FAKE_ASSERT(tmpData,"fklRecycleComStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num-=32;
	}
}

FklMem* fklNewMem(void* block,void (*destructor)(void*))
{
	FklMem* tmp=(FklMem*)malloc(sizeof(FklMem));
	FAKE_ASSERT(tmp,"fklNewMem",__FILE__,__LINE__);
	tmp->block=block;
	tmp->destructor=destructor;
	return tmp;
}

void fklFreeMem(FklMem* mem)
{
	void (*f)(void*)=mem->destructor;
	f(mem->block);
	free(mem);
}

FklMemMenager* fklNewMemMenager(size_t size)
{
	FklMemMenager* tmp=(FklMemMenager*)malloc(sizeof(FklMemMenager));
	FAKE_ASSERT(tmp,"fklNewMemMenager",__FILE__,__LINE__);
	tmp->s=fklNewComStack(size);
	return tmp;
}

void fklFreeMemMenager(FklMemMenager* memMenager)
{
	size_t i=0;
	FklComStack* s=memMenager->s;
	for(;i<s->top;i++)
		fklFreeMem(s->data[i]);
	fklFreeComStack(s);
	free(memMenager);
}

void fklPushMem(void* block,void (*destructor)(void*),FklMemMenager* memMenager)
{
	FklMem* mem=fklNewMem(block,destructor);
	fklPushComStack(mem,memMenager->s);
}

void* popMem(FklMemMenager* memMenager)
{
	return fklPopComStack(memMenager->s);
}

void fklDeleteMem(void* block,FklMemMenager* memMenager)
{
	FklComStack* s=memMenager->s;
	s->top-=1;
	uint32_t i=0;
	uint32_t j=s->top;
	for(;i<s->top;i++)
		if(((FklMem*)s->data[i])->block==block)
			break;
	free(s->data[i]);
	for(;i<j;i++)
		s->data[i]=s->data[i+1];
}

void* fklReallocMem(void* o_block,void* n_block,FklMemMenager* memMenager)
{
	FklComStack* s=memMenager->s;
	uint32_t i=0;
	FklMem* m=NULL;
	for(;i<s->top;i++)
		if(((FklMem*)s->data[i])->block==o_block)
		{
			m=s->data[i];
			break;
		}
	m->block=n_block;
	return n_block;
}

void mergeSort(void* _base,size_t num,size_t size,int (*cmpf)(const void*,const void*))
{
	void* base0=_base;
	void* base1=malloc(size*num);
	FAKE_ASSERT(base1,"mergeSort",__FILE__,__LINE__);
	unsigned int seg=1;
	unsigned int start=0;
	for(;seg<num;seg+=seg)
	{
		for(start=0;start<num;start+=seg*2)
		{
			unsigned int l=start;
			unsigned int mid=MIN(start+seg,num);
			unsigned int h=MIN(start+seg*2,num);
			unsigned int k=l;
			unsigned int start1=l;
			unsigned int end1=mid;
			unsigned int start2=mid;
			unsigned int end2=h;
			while(start1<end1&&start2<end2)
				memcpy(base1+(k++)*size,(cmpf(base0+start1*size,base0+start2*size)<0)?base0+(start1++)*size:base0+(start2++)*size,size);
			while(start1<end1)
				memcpy(base1+(k++)*size,base0+(start1++)*size,size);
			while(start2<end2)
				memcpy(base1+(k++)*size,base0+(start2++)*size,size);
		}
		void* tmp=base0;
		base0=base1;
		base1=tmp;
	}
	if(base0!=_base)
	{
		memcpy(base1,base0,num*size);
		base1=base0;
	}
	free(base1);
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
		FREE_ALL_LINE_NUMBER_TABLE(prev->proc->l,prev->proc->ls);
		fklFreeByteCodelnt(prev->proc);
		free(prev);
	}
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

FklQueueNode* fklNewQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FAKE_ASSERT(tmp,"fklNewQueueNode",__FILE__,__LINE__);
	tmp->data=data;
	tmp->next=NULL;
	return tmp;
}

void fklFreeQueueNode(FklQueueNode* tmp)
{
	free(tmp);
}

FklComQueue* fklNewComQueue()
{
	FklComQueue* tmp=(FklComQueue*)malloc(sizeof(FklComQueue));
	FAKE_ASSERT(tmp,"fklNewComQueue",__FILE__,__LINE__);
	tmp->head=NULL;
	tmp->tail=NULL;
	return tmp;
}

void fklFreeComQueue(FklComQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	while(cur)
	{
		FklQueueNode* prev=cur;
		cur=cur->next;
		fklFreeQueueNode(prev);
	}
	free(tmp);
}

int32_t fklLengthComQueue(FklComQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	int32_t i=0;
	for(;cur;cur=cur->next,i++);
	return i;
}

void* fklPopComQueue(FklComQueue* tmp)
{
	FklQueueNode* head=tmp->head;
	if(!head)
		return NULL;
	void* retval=head->data;
	tmp->head=head->next;
	if(!tmp->head)
		tmp->tail=NULL;
	fklFreeQueueNode(head);
	return retval;
}

void* fklFirstComQueue(FklComQueue* tmp)
{
	FklQueueNode* head=tmp->head;
	if(!head)
		return NULL;
	return head->data;
}

void fklPushComQueue(void* data,FklComQueue* tmp)
{
	FklQueueNode* tmpNode=fklNewQueueNode(data);
	if(!tmp->head)
	{
		tmp->head=tmpNode;
		tmp->tail=tmpNode;
	}
	else
	{
		tmp->tail->next=tmpNode;
		tmp->tail=tmpNode;
	}
}

FklComQueue* fklCopyComQueue(FklComQueue* q)
{
	FklQueueNode* head=q->head;
	FklComQueue* tmp=fklNewComQueue();
	for(;head;head=head->next)
		fklPushComQueue(head->data,tmp);
	return tmp;
}

char* fklStrCat(char* s1,const char* s2)
{
	s1=(char*)realloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FAKE_ASSERT(s1,"fklStrCat",__FILE__,__LINE__);
	strcat(s1,s2);
	return s1;
}

uint8_t* fklCreateByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"fklCreateByteArry",__FILE__,__LINE__);
	return tmp;
}

FklVMDefTypes* fklNewVMDefTypes(void)
{
	FklVMDefTypes* tmp=(FklVMDefTypes*)malloc(sizeof(FklVMDefTypes));
	FAKE_ASSERT(tmp,"fklNewVMDefTypes",__FILE__,__LINE__);
	tmp->num=0;
	tmp->u=NULL;
	return tmp;
}
