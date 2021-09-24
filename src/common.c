#define USE_CODE_NAME
#include"common.h"
#include"opcode.h"
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
	freeLineNumTabNode((l)[i]);\
}

const char* builtInErrorType[NUMOFBUILTINERRORTYPE]=
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
};

SymbolTable GlobSymbolTable=STATIC_SYMBOL_INIT;

char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"getStringFromList",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* getStringAfterBackslash(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while(!isspace(*(str+len))&&*(str+len)!='\0')
	{
		len++;
		if(!isalnum(str[len])&&str[len-1]!='\\')break;
	}
	FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"getStringAfterBackslash",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* doubleToString(double num)
{
	char numString[256]={0};
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	FAKE_ASSERT(tmp,"doubleToString",__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);
	return tmp;
}


double stringToDouble(const char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}



char* intToString(long num)
{
	char numString[256]={0};
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	FAKE_ASSERT((tmp=(char*)malloc(lenOfNum*sizeof(char))),"intToString",__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);;
	return tmp;
}


int64_t stringToInt(const char* str)
{
	int64_t tmp;
	if(isHexNum(str))
		sscanf(str,"%lx",&tmp);
	else if(isOctNum(str))
		sscanf(str,"%lo",&tmp);
	else
		sscanf(str,"%ld",&tmp);
	return tmp;
}


int power(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}

void printRawString(const char* objStr,FILE* out)
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

AST_pair* newPair(int curline,AST_pair* prev)
{
	AST_pair* tmp;
	FAKE_ASSERT((tmp=(AST_pair*)malloc(sizeof(AST_pair))),"newPair",__FILE__,__LINE__);
	tmp->car.outer=tmp;
	tmp->car.type=NIL;
	tmp->car.u.all=NULL;
	tmp->cdr.outer=tmp;
	tmp->cdr.type=NIL;
	tmp->cdr.u.all=NULL;
	tmp->prev=prev;
	tmp->car.curline=curline;
	tmp->cdr.curline=curline;
	return tmp;
}

AST_cptr* newCptr(int curline,AST_pair* outer)
{
	AST_cptr* tmp=NULL;
	FAKE_ASSERT((tmp=(AST_cptr*)malloc(sizeof(AST_cptr))),"newCptr",__FILE__,__LINE__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=NIL;
	tmp->u.all=NULL;
	return tmp;
}

AST_atom* newAtom(int type,const char* value,AST_pair* prev)
{
	AST_atom* tmp=NULL;
	FAKE_ASSERT((tmp=(AST_atom*)malloc(sizeof(AST_atom))),"newAtom",__FILE__,__LINE__);
	switch(type)
	{
		case SYM:
		case STR:
			if(value!=NULL)
			{
				FAKE_ASSERT((tmp->value.str=(char*)malloc(strlen(value)+1)),"newAtom",__FILE__,__LINE__);
				strcpy(tmp->value.str,value);
			}
			else
				tmp->value.str=NULL;
			break;
		case CHR:
		case IN32:
		case DBL:
			*(int32_t*)(&tmp->value)=0;
			break;
		case BYTS:
			tmp->value.byts.size=0;
			tmp->value.byts.str=NULL;
			break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int copyCptr(AST_cptr* objCptr,const AST_cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	pushComStack(objCptr,s1);
	pushComStack((void*)copiedCptr,s2);
	AST_atom* atom1=NULL;
	AST_atom* atom2=NULL;
	while(!isComStackEmpty(s2))
	{
		AST_cptr* root1=popComStack(s1);
		AST_cptr* root2=popComStack(s2);
		root1->type=root2->type;
		root1->curline=root2->curline;
		switch(root1->type)
		{
			case PAIR:
				root1->u.pair=newPair(0,root1->outer);
				pushComStack(getASTPairCar(root1),s1);
				pushComStack(getASTPairCdr(root1),s1);
				pushComStack(getASTPairCar(root2),s2);
				pushComStack(getASTPairCdr(root2),s2);
				break;
			case ATM:
				atom1=NULL;
				atom2=root2->u.atom;
				switch(atom2->type)
				{
					case SYM:
					case STR:
						atom1=newAtom(atom2->type,atom2->value.str,root1->outer);
						break;
					case BYTS:
						atom1=newAtom(atom2->type,NULL,root1->outer);
						atom1->value.byts.size=atom2->value.byts.size;
						atom1->value.byts.str=copyMemory(atom2->value.byts.str,atom2->value.byts.size);
						break;
					case IN32:
						atom1=newAtom(atom2->type,NULL,root1->outer);
						atom1->value.in32=atom2->value.in32;
						break;
					case DBL:
						atom1=newAtom(atom2->type,NULL,root1->outer);
						atom1->value.dbl=atom2->value.dbl;
						break;
					case CHR:
						atom1=newAtom(atom2->type,NULL,root1->outer);
						atom1->value.chr=atom2->value.chr;
						break;
					default:
						atom1=newAtom(atom2->type,NULL,root1->outer);
						break;
				}
				root1->u.atom=atom1;
				break;
			default:
				root1->u.all=NULL;
				break;
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return 1;
}
void replaceCptr(AST_cptr* fir,const AST_cptr* sec)
{
	AST_pair* tmp=fir->outer;
	AST_cptr tmpCptr={NULL,0,NIL,{NULL}};
	tmpCptr.type=fir->type;
	tmpCptr.u.all=fir->u.all;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==PAIR)
		fir->u.pair->prev=tmp;
	else if(fir->type==ATM)
		fir->u.atom->prev=tmp;
}

int deleteCptr(AST_cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->u.pair:NULL;
	AST_pair* objPair=tmpPair;
	AST_cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==PAIR)
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
		else if(tmpCptr->type==ATM)
		{
			freeAtom(tmpCptr->u.atom);
			tmpCptr->type=NIL;
			tmpCptr->u.all=NULL;
			continue;
		}
		else if(tmpCptr->type==NIL)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->car)
			{
				tmpCptr=&objPair->cdr;
				continue;
			}
			else if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				AST_pair* prev=objPair;
				objPair=objPair->prev;
				free(prev);
				if(objPair==NULL||prev==tmpPair)break;
				if(prev==objPair->car.u.pair)
				{
					objPair->car.type=NIL;
					objPair->car.u.all=NULL;
				}
				else if(prev==objPair->cdr.u.pair)
				{
					objPair->cdr.type=NIL;
					objPair->cdr.u.all=NULL;
				}
				tmpCptr=&objPair->cdr;
			}
		}
		if(objPair==NULL)break;
	}
	objCptr->type=NIL;
	objCptr->u.all=NULL;
	return 0;
}

int AST_cptrcmp(const AST_cptr* first,const AST_cptr* second)
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
				if((firAtm->type==SYM||firAtm->type==STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==IN32&&firAtm->value.in32!=secAtm->value.in32)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTS&&!eqByteString(&firAtm->value.byts,&secAtm->value.byts))return 0;
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

AST_cptr* nextCptr(const AST_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==PAIR)
		return &objCptr->outer->cdr.u.pair->car;
	return NULL;
}

AST_cptr* prevCptr(const AST_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->prev!=NULL&&objCptr->outer->prev->cdr.u.pair==objCptr->outer)
		return &objCptr->outer->prev->car;
	return NULL;
}

int isHexNum(const char* objStr)
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

int isOctNum(const char* objStr)
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

int isDouble(const char* objStr)
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

int stringToChar(const char* objStr)
{
	int ch=0;
	if(toupper(objStr[0])=='X'&&isxdigit(objStr[1]))
	{
		size_t len=strlen(objStr)+2;
		char* tmpStr=(char*)malloc(sizeof(char)*len);
		sprintf(tmpStr,"0%s",objStr);
		if(isHexNum(tmpStr))
		{
			sscanf(tmpStr+2,"%x",&ch);
		}
		free(tmpStr);
	}
	else if(isNum(objStr))
	{
		if(isHexNum(objStr))
		{
			objStr++;
			sscanf(objStr,"%x",&ch);
		}
		else if(isOctNum(objStr))
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

int isNum(const char* objStr)
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

void freeAtom(AST_atom* objAtm)
{
	if(objAtm->type==SYM||objAtm->type==STR)free(objAtm->value.str);
	else if(objAtm->type==BYTS)
	{
		objAtm->value.byts.size=0;
		free(objAtm->value.byts.str);
	}
	free(objAtm);
}

void printCptr(const AST_cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->u.pair:NULL;
	AST_pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
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
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr&&objCptr->type==ATM)putc(',',out);
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==NIL)
			||(objCptr->outer==NULL&&objCptr->type==NIL))fputs("()",out);
			if(objCptr->type!=NIL)
			{
				AST_atom* tmpAtm=objCptr->u.atom;
				switch((int)tmpAtm->type)
				{
					case SYM:
						fprintf(out,"%s",tmpAtm->value.str);
						break;
					case STR:
						printRawString(tmpAtm->value.str,out);
						break;
					case IN32:
						fprintf(out,"%d",tmpAtm->value.in32);
						break;
					case DBL:
						fprintf(out,"%lf",tmpAtm->value.dbl);
						break;
					case CHR:
						printRawChar(tmpAtm->value.chr,out);
						break;
					case BYTS:
						printByteStr(tmpAtm->value.byts.size,tmpAtm->value.byts.str,out,1);
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
}

void exError(const AST_cptr* obj,int type,Intpr* inter)
{
	fprintf(stderr,"error of compiling: ");
	switch(type)
	{
		case SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)printCptr(obj,stderr);
			fprintf(stderr," is undefined ");
			break;
		case SYNTAXERROR:
			fprintf(stderr,"Invalid syntax ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case INVALIDEXPR:
			fprintf(stderr,"Invalid expression ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case INVALIDTYPEDEF:
			fprintf(stderr,"Invalid type define ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case CIRCULARLOAD:
			fprintf(stderr,"Circular load file ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case INVALIDPATTERN:
			fprintf(stderr,"Invalid string match pattern ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)printCptr(obj,stderr);
			fprintf(stderr," undefined ");
			break;
		case CANTDEREFERENCE:
			fprintf(stderr,"cant dereference a non pointer type member ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case CANTGETELEM:
			fprintf(stderr,"cant get element of a non-array or non-pointer type");
			if(obj!=NULL)
			{
				fprintf(stderr," member by path ");
				if(obj->type==NIL)
					fprintf(stderr,"()");
				else
					printCptr(obj,stderr);
			}
			break;
		case INVALIDMEMBER:
			fprintf(stderr,"invalid member ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case NOMEMBERTYPE:
			fprintf(stderr,"cannot get member in a no-member type in ");
			if(obj!=NULL)printCptr(obj,stderr);
			break;
		case NONSCALARTYPE:
			fprintf(stderr,"get the reference of a non-scalar type member by path ");
			if(obj!=NULL)
			{
				if(obj->type==NIL)
					fprintf(stderr,"()");
				else
					printCptr(obj,stderr);
			}
			fprintf(stderr," is not allowed");
			break;

	}
	if(inter!=NULL)fprintf(stderr," at line %d of file %s\n",(obj==NULL)?inter->curline:obj->curline,inter->filename);
}

void printRawChar(char chr,FILE* out)
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

PreEnv* newEnv(PreEnv* prev)
{
	PreEnv* curEnv=NULL;
	FAKE_ASSERT((curEnv=(PreEnv*)malloc(sizeof(PreEnv))),"newEnv",__FILE__,__LINE__);
	if(prev!=NULL)prev->next=curEnv;
	curEnv->prev=prev;
	curEnv->next=NULL;
	curEnv->symbols=NULL;
	return curEnv;
}

void destroyEnv(PreEnv* objEnv)
{
	if(objEnv==NULL)return;
	while(objEnv!=NULL)
	{
		PreDef* delsym=objEnv->symbols;
		while(delsym!=NULL)
		{
			free(delsym->symbol);
			deleteCptr(&delsym->obj);
			PreDef* prev=delsym;
			delsym=delsym->next;
			free(prev);
		}
		PreEnv* prev=objEnv;
		objEnv=objEnv->next;
		free(prev);
	}
}

Intpr* newIntpr(const char* filename,FILE* file,CompEnv* env,LineNumberTable* lnt,VMDefTypes* deftypes)
{
	Intpr* tmp=NULL;
	FAKE_ASSERT((tmp=(Intpr*)malloc(sizeof(Intpr))),"newIntpr",__FILE__,__LINE__)
	tmp->filename=copyStr(filename);
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
		tmp->curDir=getDir(rp?rp:filename);
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
		tmp->lnt=newLineNumTable();
	if(deftypes)
		tmp->deftypes=deftypes;
	else
		tmp->deftypes=newVMDefTypes();
	if(env)
	{
		tmp->glob=env;
		return tmp;
	}
	tmp->glob=newCompEnv(NULL);
	initCompEnv(tmp->glob);
	return tmp;
}

void freeIntpr(Intpr* inter)
{
	if(inter->filename)
		free(inter->filename);
	if(inter->file!=stdin)
		fclose(inter->file);
	free(inter->curDir);
	destroyCompEnv(inter->glob);
	if(inter->lnt)freeLineNumberTable(inter->lnt);
	if(inter->deftypes)freeDefTypeTable(inter->deftypes);
	free(inter);
}

CompEnv* newCompEnv(CompEnv* prev)
{
	CompEnv* tmp=(CompEnv*)malloc(sizeof(CompEnv));
	FAKE_ASSERT(tmp,"newComEnv",__FILE__,__LINE__);
	tmp->prev=prev;
	tmp->head=NULL;
	tmp->prefix=NULL;
	tmp->exp=NULL;
	tmp->n=0;
	tmp->macro=NULL;
	tmp->keyWords=NULL;
	tmp->proc=newByteCodelnt(newByteCode(0));
	return tmp;
}

void destroyCompEnv(CompEnv* objEnv)
{
	if(objEnv==NULL)return;
	CompDef* tmpDef=objEnv->head;
	while(tmpDef!=NULL)
	{
		CompDef* prev=tmpDef;
		tmpDef=tmpDef->next;
		free(prev);
	}
	FREE_ALL_LINE_NUMBER_TABLE(objEnv->proc->l,objEnv->proc->ls);
	freeByteCodelnt(objEnv->proc);
	freeAllMacro(objEnv->macro);
	freeAllKeyWord(objEnv->keyWords);
	free(objEnv);
}

CompDef* findCompDef(const char* name,CompEnv* curEnv)
{
	if(curEnv->head==NULL)return NULL;
	else
	{
		SymTabNode* node=addSymbolToGlob(name);
		if(node==NULL)
			return NULL;
		Sid_t id=node->id;
		CompDef* curDef=curEnv->head;
		while(curDef&&id!=curDef->id)
			curDef=curDef->next;
		return curDef;
	}
}

CompDef* addCompDef(const char* name,CompEnv* curEnv)
{
	if(curEnv->head==NULL)
	{
		SymTabNode* node=addSymbolToGlob(name);
		FAKE_ASSERT((curEnv->head=(CompDef*)malloc(sizeof(CompDef))),"addCompDef",__FILE__,__LINE__);
		curEnv->head->next=NULL;
		curEnv->head->id=node->id;
		return curEnv->head;
	}
	else
	{
		CompDef* curDef=findCompDef(name,curEnv);
		if(curDef==NULL)
		{
			SymTabNode* node=addSymbolToGlob(name);
			FAKE_ASSERT((curDef=(CompDef*)malloc(sizeof(CompDef))),"addCompDef",__FILE__,__LINE__);
			curDef->id=node->id;
			curDef->next=curEnv->head;
			curEnv->head=curDef;
		}
		return curDef;
	}
}

ByteCode* newByteCode(unsigned int size)
{
	ByteCode* tmp=NULL;
	FAKE_ASSERT((tmp=(ByteCode*)malloc(sizeof(ByteCode))),"newByteCode",__FILE__,__LINE__);
	tmp->size=size;
	FAKE_ASSERT((tmp->code=(uint8_t*)malloc(size*sizeof(uint8_t))),"newByteCode",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
}

ByteCodelnt* newByteCodelnt(ByteCode* bc)
{
	ByteCodelnt* t=(ByteCodelnt*)malloc(sizeof(ByteCodelnt));
	FAKE_ASSERT(t,"newByteCode",__FILE__,__LINE__);
	t->ls=0;
	t->l=NULL;
	t->bc=bc;
	return t;
}

void freeByteCodelnt(ByteCodelnt* t)
{
	freeByteCode(t->bc);
	if(t->l)
		free(t->l);
	free(t);
}

void freeByteCode(ByteCode* obj)
{
	free(obj->code);
	free(obj);
}

void codeCat(ByteCode* fir,const ByteCode* sec)
{
	int32_t size=fir->size;
	fir->size=sec->size+fir->size;
	fir->code=(uint8_t*)realloc(fir->code,sizeof(uint8_t)*fir->size);
	FAKE_ASSERT(fir->code||!fir->size,"codeCat",__FILE__,__LINE__);
	memcpy(fir->code+size,sec->code,sec->size);
}

void reCodeCat(const ByteCode* fir,ByteCode* sec)
{
	int32_t size=fir->size;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*(fir->size+sec->size));
	FAKE_ASSERT(tmp,"reCodeCat",__FILE__,__LINE__);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

ByteCode* copyByteCode(const ByteCode* obj)
{
	ByteCode* tmp=newByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

ByteCodelnt* copyByteCodelnt(const ByteCodelnt* obj)
{
	ByteCodelnt* tmp=newByteCodelnt(copyByteCode(obj->bc));
	tmp->ls=obj->ls;
	tmp->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*obj->ls);
	FAKE_ASSERT(tmp->l,"copyByteCodelnt",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<obj->ls;i++)
	{
		LineNumTabNode* t=obj->l[i];
		LineNumTabNode* node=newLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
		tmp->l[i]=node;
	}
	return tmp;
}

void initCompEnv(CompEnv* curEnv)
{
	int i=0;
	for(i=0;i<NUMOFBUILTINSYMBOL;i++)
		addCompDef(builtInSymbolList[i],curEnv);
}

char* copyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	FAKE_ASSERT(tmp,"copyStr",__FILE__,__LINE__);
	strcpy(tmp,str);
	return tmp;

}



int isscript(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=4)return 0;
	else return !strcmp(filename+i,".fkl");
}

int iscode(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=5)return 0;
	else return !strcmp(filename+i,".fklc");
}

AST_cptr* getLastCptr(const AST_cptr* objList)
{
	if(objList->type!=PAIR)
		return NULL;
	AST_pair* objPair=objList->u.pair;
	AST_cptr* first=&objPair->car;
	for(;nextCptr(first)!=NULL;first=nextCptr(first));
	return first;
}

AST_cptr* getFirstCptr(const AST_cptr* objList)
{
	if(objList->type!=PAIR)
		return NULL;
	AST_pair* objPair=objList->u.pair;
	AST_cptr* first=&objPair->car;
	return first;
}

void printByteCode(const ByteCode* tmpCode,FILE* fp)
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
					fprintf(fp,"%s ",findSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(Sid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						Sid_t type=*(Sid_t*)(tmpCode->code+i);
						i+=sizeof(Sid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",getGlobSymbolWithId(type)->symbol,pCpc);
						printAsByteStr(tmpCode->code+i,pCpc,fp);
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
				printAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
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
				printRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FAKE_PUSH_SYM)
					fprintf(fp,"%s",getGlobSymbolWithId(*(Sid_t*)(tmpCode->code+i+1))->symbol);
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
					case FAKE_PUSH_IN64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(TypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(TypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(TypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
				i+=13;
				break;
		}
		putc('\n',fp);
	}
}

uint8_t castCharInt(char ch)
{
	if(isdigit(ch))return ch-'0';
	else if(isxdigit(ch))
	{
		if(ch>='a'&&ch<='f')return 10+ch-'a';
		else if(ch>='A'&&ch<='F')return 10+ch-'A';
	}
	return 0;
}

uint8_t* castStrByteStr(const char* str)
{
	int len=strlen(str);
	int32_t size=(len%2)?(len/2+1):len/2;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"castStrByteStr",__FILE__,__LINE__);
	int32_t i=0;
	int k=0;
	for(;i<size;i++)
	{
		tmp[i]=16*castCharInt(str[k]);
		if(str[k+1]!='\0')tmp[i]+=castCharInt(str[k+1]);
		k+=2;
	}
	return tmp;
}

void printByteStr(size_t size,const uint8_t* str,FILE* fp,int mode)
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

void printAsByteStr(const uint8_t* str,int32_t size,FILE* fp)
{
	fputs("#b",fp);
	for(int i=0;i<size;i++)
	{
		uint8_t j=str[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void* copyMemory(void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	FAKE_ASSERT(tmp,"copyMemory",__FILE__,__LINE__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

int hasLoadSameFile(const char* filename,const Intpr* inter)
{
	while(inter!=NULL)
	{
		if(!strcmp(inter->filename,filename))
			return 1;
		inter=inter->prev;
	}
	return 0;
}

Intpr* getFirstIntpr(Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter;
}

AST_cptr* getASTPairCar(const AST_cptr* obj)
{
	return &obj->u.pair->car;
}

AST_cptr* getASTPairCdr(const AST_cptr* obj)
{
	return &obj->u.pair->cdr;
}

AST_cptr* getCptrCar(const AST_cptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->car;
	return NULL;
}

AST_cptr* getCptrCdr(const AST_cptr* obj)
{
	if(obj&&obj->outer!=NULL)
		return &obj->outer->cdr;
	return NULL;
}

void changeWorkPath(const char* filename)
{
#ifdef _WIN32
	char* p=_fullpath(NULL,filename,0);
#else
	char* p=realpath(filename,NULL);
#endif
	char* wp=getDir(p);
	if(chdir(wp))
	{
		perror(wp);
		exit(EXIT_FAILURE);
	}
	free(p);
	free(wp);
}

char* getDir(const char* filename)
{
#ifdef _WIN32
	char dp='\\';
#else
	char dp='/';
#endif
	int i=strlen(filename)-1;
	for(;filename[i]!=dp;i--);
	char* tmp=(char*)malloc(sizeof(char)*(i+1));
	FAKE_ASSERT(tmp,"getDir",__FILE__,__LINE__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* getStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	FAKE_ASSERT(tmp,"getStringFromFile",__FILE__,__LINE__);
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
		FAKE_ASSERT((tmp=(char*)malloc(sizeof(char)*(i+1))),"getStringFromFile",__FILE__,__LINE__);
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

int eqByteString(const ByteString* fir,const ByteString* sec)
{
	if(fir->size!=sec->size)return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

char* getLastWorkDir(Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->curDir;
}

char* relpath(char* abs,char* relto)
{
#ifdef _WIN32
	char divstr[]="\\";
	char upperDir[]="..\\";
#else
	char divstr[]="/";
	char upperDir[]="../";
#endif
	char* cabs=copyStr(abs);
	char* crelto=copyStr(relto);
	int lengthOfAbs=0;
	int lengthOfRelto=0;
	char** absDirs=split(cabs,divstr,&lengthOfAbs);
	char** reltoDirs=split(crelto,divstr,&lengthOfRelto);
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
	char* trp=copyStr(rp);
	free(cabs);
	free(crelto);
	free(absDirs);
	free(reltoDirs);
	return trp;
}

char** split(char* str,char* divstr,int* length)
{
	int count=0;
	char* pNext=NULL;
	char** strArry=(char**)malloc(0);
	FAKE_ASSERT(strArry,"split",__FILE__,__LINE__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		FAKE_ASSERT(strArry,"split",__FILE__,__LINE__);
		strArry[count-1]=pNext;
		pNext=strtok(NULL,divstr);
	}
	*length=count;
	return strArry;
}

char* castEscapeCharater(const char* str,char end,size_t* len)
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

Intpr* newTmpIntpr(const char* filename,FILE* fp)
{
	Intpr* tmp=NULL;
	FAKE_ASSERT((tmp=(Intpr*)malloc(sizeof(Intpr))),"newTmpIntpr",__FILE__,__LINE__);
	tmp->filename=copyStr(filename);
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
		tmp->curDir=getDir(rp);
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

int32_t countChar(const char* str,char c,int32_t len)
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

PreDef* findDefine(const char* name,const PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		PreDef* curDef=curEnv->symbols;
		while(curDef&&strcmp(name,curDef->symbol))
			curDef=curDef->next;
		return curDef;
	}
}

PreDef* addDefine(const char* symbol,const AST_cptr* objCptr,PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)
	{
		curEnv->symbols=newDefines(symbol);
		replaceCptr(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		PreDef* curDef=findDefine(symbol,curEnv);
		if(curDef==NULL)
		{
			curDef=newDefines(symbol);
			curDef->next=curEnv->symbols;
			curEnv->symbols=curDef;
			replaceCptr(&curDef->obj,objCptr);
		}
		else
			replaceCptr(&curDef->obj,objCptr);
		return curDef;
	}
}

PreDef* newDefines(const char* name)
{
	PreDef* tmp=(PreDef*)malloc(sizeof(PreDef));
	FAKE_ASSERT(tmp,"newDefines",__FILE__,__LINE__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	FAKE_ASSERT(tmp->symbol,"newDefines",__FILE__,__LINE__);
	strcpy(tmp->symbol,name);
	tmp->obj=(AST_cptr){NULL,0,NIL,{NULL}};
	tmp->next=NULL;
	return tmp;
}

SymbolTable* newSymbolTable()
{
	SymbolTable* tmp=(SymbolTable*)malloc(sizeof(SymbolTable));
	FAKE_ASSERT(tmp,"newSymbolTable",__FILE__,__LINE__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	return tmp;
}

SymTabNode* newSymTabNode(const char* symbol)
{
	SymTabNode* tmp=(SymTabNode*)malloc(sizeof(SymTabNode));
	FAKE_ASSERT(tmp,"newSymbolTable",__FILE__,__LINE__);
	tmp->id=0;
	tmp->symbol=copyStr(symbol);
	return tmp;
}

SymTabNode* addSymbol(const char* sym,SymbolTable* table)
{
	SymTabNode* node=NULL;
	if(!table->list)
	{
		node=newSymTabNode(sym);
		table->num=1;
		node->id=table->num-1;
		table->list=(SymTabNode**)malloc(sizeof(SymTabNode*)*1);
		FAKE_ASSERT(table->list,"addSymbol",__FILE__,__LINE__);
		table->idl=(SymTabNode**)malloc(sizeof(SymTabNode*)*1);
		FAKE_ASSERT(table->idl,"addSymbol",__FILE__,__LINE__);
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
		table->list=(SymTabNode**)realloc(table->list,sizeof(SymTabNode*)*table->num);
		FAKE_ASSERT(table->list,"addSymbol",__FILE__,__LINE__);
		node=newSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num-1;
		table->idl=(SymTabNode**)realloc(table->idl,sizeof(SymTabNode*)*table->num);
		FAKE_ASSERT(table->idl,"addSymbol",__FILE__,__LINE__);
		table->idl[table->num-1]=node;
	}
	return node;
}

SymTabNode* addSymbolToGlob(const char* sym)
{
	return addSymbol(sym,&GlobSymbolTable);
}


void freeSymTabNode(SymTabNode* node)
{
	free(node->symbol);
	free(node);
}

void freeSymbolTable(SymbolTable* table)
{
	int32_t i=0;
	for(;i<table->num;i++)
		freeSymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

void freeGlobSymbolTable()
{
	int32_t i=0;
	for(;i<GlobSymbolTable.num;i++)
		freeSymTabNode(GlobSymbolTable.list[i]);
	free(GlobSymbolTable.list);
	free(GlobSymbolTable.idl);
}

SymTabNode* findSymbol(const char* symbol,SymbolTable* table)
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

SymTabNode* findSymbolInGlob(const char* sym)
{
	return findSymbol(sym,&GlobSymbolTable);
}

SymTabNode* getGlobSymbolWithId(Sid_t id)
{
	return GlobSymbolTable.idl[id];
}

void printSymbolTable(SymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->num;i++)
	{
		SymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:%s id:%d\n",cur->symbol,cur->id);
	}
	fprintf(fp,"size:%d\n",table->num);
}

void printGlobSymbolTable(FILE* fp)
{
	printSymbolTable(&GlobSymbolTable,fp);
}

AST_cptr* baseCreateTree(const char* objStr,Intpr* inter)
{
	if(!objStr)
		return NULL;
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	int32_t i=0;
	for(;isspace(objStr[i]);i++)
		if(objStr[i]=='\n')
			inter->curline+=1;
	int32_t curline=(inter)?inter->curline:0;
	AST_cptr* tmp=newCptr(curline,NULL);
	pushComStack(tmp,s1);
	int hasComma=1;
	while(objStr[i]&&!isComStackEmpty(s1))
	{
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		curline=inter->curline;
		AST_cptr* root=popComStack(s1);
		if(objStr[i]=='(')
		{
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				AST_cptr* tmp=popComStack(s1);
				if(tmp)
				{
					tmp->type=PAIR;
					tmp->u.pair=newPair(curline,tmp->outer);
					pushComStack(getASTPairCdr(tmp),s1);
					pushComStack(getASTPairCar(tmp),s1);
				}
			}
			int j=0;
			for(;isspace(objStr[i+1+j]);j++);
			if(objStr[i+j+1]==')')
			{
				root->type=NIL;
				root->u.all=NULL;
				i+=j+2;
			}
			else
			{
				hasComma=0;
				root->type=PAIR;
				root->u.pair=newPair(curline,root->outer);
				pushComStack((void*)s1->top,s2);
				pushComStack(getASTPairCdr(root),s1);
				pushComStack(getASTPairCar(root),s1);
				i++;
			}
		}
		else if(objStr[i]==',')
		{
			if(hasComma)
			{
				deleteCptr(tmp);
				free(tmp);
				tmp=NULL;
				break;
			}
			else hasComma=1;
			if(root->outer->prev&&root->outer->prev->cdr.u.pair==root->outer)
			{
				//将为下一个部分准备的pair删除并将该pair的前一个pair的cdr部分入栈
				s1->top=(long)topComStack(s2);
				AST_cptr* tmp=&root->outer->prev->cdr;
				free(tmp->u.pair);
				tmp->type=NIL;
				tmp->u.all=NULL;
				pushComStack(tmp,s1);
			}
			i++;
		}
		else if(objStr[i]==')')
		{
			hasComma=0;
			long t=(long)popComStack(s2);
			AST_cptr* c=s1->data[t];
			if(s1->top-t>0&&c->outer->prev&&c->outer->prev->cdr.u.pair==c->outer)
			{
				//如果还有为下一部分准备的pair，则将该pair删除
				AST_cptr* tmpCptr=s1->data[t];
				tmpCptr=&tmpCptr->outer->prev->cdr;
				tmpCptr->type=NIL;
				free(tmpCptr->u.pair);
				tmpCptr->u.all=NULL;
			}
			//将栈顶恢复为将pair入栈前的位置
			s1->top=t;
			i++;
		}
		else
		{
			root->type=ATM;
			char* str=NULL;
			if(objStr[i]=='\"')
			{
				size_t len=0;
				str=castEscapeCharater(objStr+i+1,'\"',&len);
				inter->curline+=countChar(objStr+i,'\n',len);
				root->u.atom=newAtom(STR,str,root->outer);
				i+=len+1;
				free(str);
			}
			else if(objStr[i]=='#')
			{
				i++;
				AST_atom* atom=NULL;
				switch(objStr[i])
				{
					case '\\':
						str=getStringAfterBackslash(objStr+i+1);
						atom=newAtom(CHR,NULL,root->outer);
						root->u.atom=atom;
						atom->value.chr=(str[0]=='\\')?
							stringToChar(str+1):
							str[0];
						i+=strlen(str)+1;
						break;
					case 'b':
						str=getStringAfterBackslash(objStr+i+1);
						atom=newAtom(BYTS,NULL,root->outer);
						atom->value.byts.size=strlen(str)/2+strlen(str)%2;
						atom->value.byts.str=castStrByteStr(str);
						root->u.atom=atom;
						i+=strlen(str)+1;
						break;
					default:
						str=getStringFromList(objStr+i-1);
						atom=newAtom(SYM,str,root->outer);
						root->u.atom=atom;
						i+=strlen(str)-1;
						break;
				}
				free(str);
			}
			else
			{
				char* str=getStringFromList(objStr+i);
				if(isNum(str))
				{
					AST_atom* atom=NULL;
					if(isDouble(str))
					{
						atom=newAtom(DBL,NULL,root->outer);
						atom->value.dbl=stringToDouble(str);
					}
					else
					{
						atom=newAtom(IN32,NULL,root->outer);
						int64_t num=stringToInt(str);
						if(num>=INT32_MAX||num<=INT32_MIN)
						{
							atom->type=IN64;
							atom->value.in64=num;
						}
						else
							atom->value.in32=num;
					}
					root->u.atom=atom;
				}
				else
					root->u.atom=newAtom(SYM,str,root->outer);
				i+=strlen(str);
				free(str);
			}
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				AST_cptr* tmp=popComStack(s1);
				if(tmp)
				{
					tmp->type=PAIR;
					tmp->u.pair=newPair(curline,tmp->outer);
					pushComStack(getASTPairCdr(tmp),s1);
					pushComStack(getASTPairCar(tmp),s1);
				}
			}
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

void writeSymbolTable(SymbolTable* table,FILE* fp)
{
	int32_t size=table->num;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
		fwrite(table->idl[i]->symbol,strlen(table->idl[i]->symbol)+1,1,fp);
}

void writeGlobSymbolTable(FILE* fp)
{
	writeSymbolTable(&GlobSymbolTable,fp);
}

void writeLineNumberTable(LineNumberTable* lnt,FILE* fp)
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

int isAllSpace(const char* str)
{
	for(;*str;str++)
		if(!isspace(*str))
			return 0;
	return 1;
}

LineNumberTable* newLineNumTable()
{
	LineNumberTable* t=(LineNumberTable*)malloc(sizeof(LineNumberTable));
	FAKE_ASSERT(t,"newLineNumTable",__FILE__,__LINE__);
	t->num=0;
	t->list=NULL;
	return t;
}

LineNumTabNode* newLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line)
{
	LineNumTabNode* t=(LineNumTabNode*)malloc(sizeof(LineNumTabNode));
	FAKE_ASSERT(t,"newLineNumTable",__FILE__,__LINE__);
	t->fid=fid;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

void freeLineNumTabNode(LineNumTabNode* n)
{
	free(n);
}

void freeDefTypeTable(VMDefTypes* defs)
{
	size_t i=0;
	size_t num=defs->num;
	for(;i<num;i++)
		free(defs->u[i]);
	free(defs->u);
	free(defs);
}

void freeLineNumberTable(LineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->num;i++)
		freeLineNumTabNode(t->list[i]);
	free(t->list);
	free(t);
}

LineNumTabNode* findLineNumTabNode(uint32_t cp,LineNumberTable* t)
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

void increaseScpOfByteCodelnt(ByteCodelnt* o,int32_t size)
{
	int32_t i=0;
	for(;i<o->ls;i++)
		o->l[i]->scp+=size;
}

void lntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2)
{
	if(!t->list)
	{
		t->num=s2;
		t->list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s2);
		FAKE_ASSERT(t->list,"lntCat",__FILE__,__LINE__);
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
			FAKE_ASSERT(t->list,"lntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2+1,(s2-1)*sizeof(LineNumTabNode*));
			t->num+=s2-1;
			freeLineNumTabNode(l2[0]);
		}
		else
		{
			t->list=(LineNumTabNode**)realloc(t->list,sizeof(LineNumTabNode*)*(t->num+s2));
			FAKE_ASSERT(t->list,"lntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2,(s2)*sizeof(LineNumTabNode*));
			t->num+=s2;
		}
	}
}

void codelntCat(ByteCodelnt* f,ByteCodelnt* s)
{
	if(!f->l)
	{
		f->ls=s->ls;
		f->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
		FAKE_ASSERT(f->l,"codelntCat",__FILE__,__LINE__);
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
			FAKE_ASSERT(f->l,"codelntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			f->ls+=s->ls-1;
			freeLineNumTabNode(s->l[0]);
		}
		else
		{
			f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls));
			FAKE_ASSERT(f->l,"codelntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			f->ls+=s->ls;
		}
	}
	codeCat(f->bc,s->bc);
}

void codelntCopyCat(ByteCodelnt* f,const ByteCodelnt* s)
{
	if(s->ls)
	{
		if(!f->l)
		{
			f->ls=s->ls;
			f->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
			FAKE_ASSERT(f->l,"codelntCopyCat",__FILE__,__LINE__);
			uint32_t i=0;
			for(;i<f->ls;i++)
			{
				LineNumTabNode* t=s->l[i];
				f->l[i]=newLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			f->l[0]->cpc+=f->bc->size;
			INCREASE_ALL_SCP(f->l+1,f->ls-1,f->bc->size);
		}
		else
		{
			uint32_t i=0;
			LineNumTabNode** tl=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
			FAKE_ASSERT(tl,"codelntCopyCat",__FILE__,__LINE__);
			for(;i<s->ls;i++)
			{
				LineNumTabNode* t=s->l[i];
				tl[i]=newLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			INCREASE_ALL_SCP(tl,s->ls,f->bc->size);
			if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
			{
				f->l[f->ls-1]->cpc+=s->l[0]->cpc;
				f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
				FAKE_ASSERT(f->l,"codelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl+1,(s->ls-1)*sizeof(LineNumTabNode*));
				f->ls+=s->ls-1;
				freeLineNumTabNode(tl[0]);
			}
			else
			{
				f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls));
				FAKE_ASSERT(f->l,"codelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl,(s->ls)*sizeof(LineNumTabNode*));
				f->ls+=s->ls;
			}
			free(tl);
		}
	}
	codeCat(f->bc,s->bc);
}

void reCodelntCat(ByteCodelnt* f,ByteCodelnt* s)
{
	if(!s->l)
	{
		s->ls=f->ls;
		s->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*f->ls);
		FAKE_ASSERT(s->l,"reCodelntCat",__FILE__,__LINE__);
		f->l[f->ls-1]->cpc+=s->bc->size;
		memcpy(s->l,f->l,(f->ls)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
			FAKE_ASSERT(l,"reCodelntCat",__FILE__,__LINE__);
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			freeLineNumTabNode(s->l[0]);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls-1;
		}
		else
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls));
			FAKE_ASSERT(l,"reCodeCat",__FILE__,__LINE__);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls;
		}
	}
	reCodeCat(f->bc,s->bc);
}

void printByteCodelnt(ByteCodelnt* obj,FILE* fp)
{
	SymbolTable* table=&GlobSymbolTable;
	ByteCode* tmpCode=obj->bc;
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
					fprintf(fp,"%s ",findSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(Sid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						Sid_t type=*(Sid_t*)(tmpCode->code+i);
						i+=sizeof(Sid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",getGlobSymbolWithId(type)->symbol,pCpc);
						printAsByteStr(tmpCode->code+i,pCpc,fp);
						i+=pCpc;
					}
				}
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				printAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
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
				printRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FAKE_PUSH_SYM)
					fprintf(fp,"%s",getGlobSymbolWithId(*(Sid_t*)(tmpCode->code+i+1))->symbol);
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
					case FAKE_PUSH_IN64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FAKE_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(TypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(TypeId_t)));
						break;
					case FAKE_PUSH_FPROC:
						fprintf(fp,"%d %u",*(TypeId_t*)(tmpCode->code+i+1),*(Sid_t*)(tmpCode->code+i+1+sizeof(TypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(TypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
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

ByteCodeLabel* newByteCodeLable(int32_t place,const char* label)
{
	ByteCodeLabel* tmp=(ByteCodeLabel*)malloc(sizeof(ByteCodeLabel));
	FAKE_ASSERT(tmp,"newByteCodeLable",__FILE__,__LINE__);
	tmp->place=place;
	tmp->label=copyStr(label);
	return tmp;
}

ByteCodeLabel* findByteCodeLabel(const char* label,ComStack* s)
{
	int32_t l=0;
	int32_t h=s->top-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int cmp=strcmp(((ByteCodeLabel*)s->data[mid])->label,label);
		if(cmp>0)
			h=mid-1;
		else if(cmp<0)
			l=mid+1;
		else
			return (ByteCodeLabel*)s->data[mid];
	}
	return NULL;
}


void freeByteCodeLabel(ByteCodeLabel* obj)
{
	free(obj->label);
	free(obj);
}

int isComStackEmpty(ComStack* stack)
{
	return stack->top==0;
}

ComStack* newComStack(uint32_t num)
{
	ComStack* tmp=(ComStack*)malloc(sizeof(ComStack));
	FAKE_ASSERT(tmp,"newComStack",__FILE__,__LINE__);
	tmp->data=(void**)malloc(sizeof(void*)*num);
	FAKE_ASSERT(tmp->data,"newComStack",__FILE__,__LINE__);
	tmp->num=num;
	tmp->top=0;
	return tmp;
}

void pushComStack(void* data,ComStack* stack)
{
	if(stack->top==stack->num)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num+32)*sizeof(void*));
		FAKE_ASSERT(tmpData,"pushComStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num+=32;
	}
	stack->data[stack->top]=data;
	stack->top+=1;
}

void* popComStack(ComStack* stack)
{
	if(isComStackEmpty(stack))
		return NULL;
	stack->top-=1;
	void* tmp=stack->data[stack->top];
	recycleComStack(stack);
	return tmp;
}

void* topComStack(ComStack* stack)
{
	if(isComStackEmpty(stack))
		return NULL;
	return stack->data[stack->top-1];
}

void freeComStack(ComStack* stack)
{
	free(stack->data);
	free(stack);
}

void recycleComStack(ComStack* stack)
{
	if(stack->num-stack->top>32)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num-32)*sizeof(void*));
		FAKE_ASSERT(tmpData,"recycleComStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num-=32;
	}
}

FakeMem* newFakeMem(void* block,void (*destructor)(void*))
{
	FakeMem* tmp=(FakeMem*)malloc(sizeof(FakeMem));
	FAKE_ASSERT(tmp,"newFakeMem",__FILE__,__LINE__);
	tmp->block=block;
	tmp->destructor=destructor;
	return tmp;
}

void freeFakeMem(FakeMem* mem)
{
	void (*f)(void*)=mem->destructor;
	f(mem->block);
	free(mem);
}

FakeMemMenager* newFakeMemMenager(size_t size)
{
	FakeMemMenager* tmp=(FakeMemMenager*)malloc(sizeof(FakeMemMenager));
	FAKE_ASSERT(tmp,"newFakeMemMenager",__FILE__,__LINE__);
	tmp->s=newComStack(size);
	return tmp;
}

void freeFakeMemMenager(FakeMemMenager* memMenager)
{
	size_t i=0;
	ComStack* s=memMenager->s;
	for(;i<s->top;i++)
		freeFakeMem(s->data[i]);
	freeComStack(s);
	free(memMenager);
}

void pushFakeMem(void* block,void (*destructor)(void*),FakeMemMenager* memMenager)
{
	FakeMem* mem=newFakeMem(block,destructor);
	pushComStack(mem,memMenager->s);
}

void* popFakeMem(FakeMemMenager* memMenager)
{
	return popComStack(memMenager->s);
}

void deleteFakeMem(void* block,FakeMemMenager* memMenager)
{
	ComStack* s=memMenager->s;
	s->top-=1;
	uint32_t i=0;
	uint32_t j=s->top;
	for(;i<s->top;i++)
		if(((FakeMem*)s->data[i])->block==block)
			break;
	free(s->data[i]);
	for(;i<j;i++)
		s->data[i]=s->data[i+1];
}

void* reallocFakeMem(void* o_block,void* n_block,FakeMemMenager* memMenager)
{
	ComStack* s=memMenager->s;
	uint32_t i=0;
	FakeMem* m=NULL;
	for(;i<s->top;i++)
		if(((FakeMem*)s->data[i])->block==o_block)
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

void freeAllMacro(PreMacro* head)
{
	PreMacro* cur=head;
	while(cur!=NULL)
	{
		PreMacro* prev=cur;
		cur=cur->next;
		deleteCptr(prev->pattern);
		free(prev->pattern);
		FREE_ALL_LINE_NUMBER_TABLE(prev->proc->l,prev->proc->ls);
		freeByteCodelnt(prev->proc);
		free(prev);
	}
}

void freeAllKeyWord(KeyWord* head)
{
	KeyWord* cur=head;
	while(cur!=NULL)
	{
		KeyWord* prev=cur;
		cur=cur->next;
		free(prev->word);
		free(prev);
	}
}

QueueNode* newQueueNode(void* data)
{
	QueueNode* tmp=(QueueNode*)malloc(sizeof(QueueNode));
	FAKE_ASSERT(tmp,"newQueueNode",__FILE__,__LINE__);
	tmp->data=data;
	tmp->next=NULL;
	return tmp;
}

void freeQueueNode(QueueNode* tmp)
{
	free(tmp);
}

ComQueue* newComQueue()
{
	ComQueue* tmp=(ComQueue*)malloc(sizeof(ComQueue));
	FAKE_ASSERT(tmp,"newComQueue",__FILE__,__LINE__);
	tmp->head=NULL;
	tmp->tail=NULL;
	return tmp;
}

void freeComQueue(ComQueue* tmp)
{
	QueueNode* cur=tmp->head;
	while(cur)
	{
		QueueNode* prev=cur;
		cur=cur->next;
		freeQueueNode(prev);
	}
	free(tmp);
}

int32_t lengthComQueue(ComQueue* tmp)
{
	QueueNode* cur=tmp->head;
	int32_t i=0;
	for(;cur;cur=cur->next,i++);
	return i;
}

void* popComQueue(ComQueue* tmp)
{
	QueueNode* head=tmp->head;
	if(!head)
		return NULL;
	void* retval=head->data;
	tmp->head=head->next;
	if(!tmp->head)
		tmp->tail=NULL;
	freeQueueNode(head);
	return retval;
}

void* firstComQueue(ComQueue* tmp)
{
	QueueNode* head=tmp->head;
	if(!head)
		return NULL;
	return head->data;
}

void pushComQueue(void* data,ComQueue* tmp)
{
	QueueNode* tmpNode=newQueueNode(data);
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

ComQueue* copyComQueue(ComQueue* q)
{
	QueueNode* head=q->head;
	ComQueue* tmp=newComQueue();
	for(;head;head=head->next)
		pushComQueue(head->data,tmp);
	return tmp;
}

char* strCat(char* s1,const char* s2)
{
	s1=(char*)realloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FAKE_ASSERT(s1,"strCat",__FILE__,__LINE__);
	strcat(s1,s2);
	return s1;
}

uint8_t* createByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"createByteArry",__FILE__,__LINE__);
	return tmp;
}

VMDefTypes* newVMDefTypes(void)
{
	VMDefTypes* tmp=(VMDefTypes*)malloc(sizeof(VMDefTypes));
	FAKE_ASSERT(tmp,"newVMDefTypes",__FILE__,__LINE__);
	tmp->num=0;
	tmp->u=NULL;
	return tmp;
}
