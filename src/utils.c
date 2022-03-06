#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/ast.h>
#include<fakeLisp/symbol.h>
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

char* fklGetStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"fklGetStringFromList",__FILE__,__LINE__);
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
	FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),"fklGetStringAfterBackslash",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"fklDoubleToString",__FILE__,__LINE__);
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
	FKL_ASSERT((tmp=(char*)malloc(lenOfNum*sizeof(char))),"fklIntToString",__FILE__,__LINE__);
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
	int i=(*objStr=='-'||*objStr=='+')?1:0;
	int hasDot=0;
	int hasExp=0;
	if(i&&!isdigit(objStr[1]))
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

void fklExError(const FklAstCptr* obj,int type,FklInterpreter* inter)
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

FklInterpreter* fklNewIntpr(const char* filename,FILE* file,FklCompEnv* env,LineNumberTable* lnt,FklVMDefTypes* deftypes)
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
	FKL_ASSERT(tmp,"fklCopyStr",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"fklCastStrByteStr",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"fklCopyMemory",__FILE__,__LINE__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
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
	FKL_ASSERT(tmp,"fklGetDir",__FILE__,__LINE__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* fklGetStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	FKL_ASSERT(tmp,"fklGetStringFromFile",__FILE__,__LINE__);
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
		FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(i+1))),"fklGetStringFromFile",__FILE__,__LINE__);
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

char* fklGetLastWorkDir(FklInterpreter* inter)
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
	FKL_ASSERT(strArry,"fklSplit",__FILE__,__LINE__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		FKL_ASSERT(strArry,"fklSplit",__FILE__,__LINE__);
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
			FKL_ASSERT(tmp,"castKeyStringToNormalString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	if(tmp)tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,memSize*sizeof(char));
	FKL_ASSERT(tmp,"castKeyStringToNormalString",__FILE__,__LINE__);
	*len=i+1;
	return tmp;
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
	FKL_ASSERT(tmp,"fklNewDefines",__FILE__,__LINE__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	FKL_ASSERT(tmp->symbol,"fklNewDefines",__FILE__,__LINE__);
	strcpy(tmp->symbol,name);
	tmp->obj=(FklAstCptr){NULL,0,FKL_NIL,{NULL}};
	tmp->next=NULL;
	return tmp;
}

FklSymbolTable* fklNewSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FKL_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	return tmp;
}

FklSymTabNode* fklNewSymTabNode(const char* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
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
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
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
		FKL_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		node=fklNewSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
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
	if(id==0)
		return NULL;
	return GlobSymbolTable.idl[id-1];
}

void fklPrintSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->num;i++)
	{
		FklSymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:%s id:%d\n",cur->symbol,cur->id+1);
	}
	fprintf(fp,"size:%d\n",table->num);
}

void fklPrintGlobSymbolTable(FILE* fp)
{
	fklPrintSymbolTable(&GlobSymbolTable,fp);
}

FklAstCptr* fklBaseCreateTree(const char* objStr,FklInterpreter* inter)
{
	if(!objStr)
		return NULL;
	FklPtrStack* s1=fklNewPtrStack(32);
	FklPtrStack* s2=fklNewPtrStack(32);
	int32_t i=0;
	for(;isspace(objStr[i]);i++)
		if(objStr[i]=='\n')
			inter->curline+=1;
	int32_t curline=(inter)?inter->curline:0;
	FklAstCptr* tmp=fklNewCptr(curline,NULL);
	fklPushPtrStack(tmp,s1);
	int hasComma=1;
	while(objStr[i]&&!fklIsPtrStackEmpty(s1))
	{
		for(;isspace(objStr[i]);i++)
			if(objStr[i]=='\n')
				inter->curline+=1;
		curline=inter->curline;
		FklAstCptr* root=fklPopPtrStack(s1);
		if(objStr[i]=='(')
		{
			if(&root->outer->car==root)
			{
				//如果root是root所在pair的car部分，
				//则在对应的pair后追加一个pair为下一个部分准备
				FklAstCptr* tmp=fklPopPtrStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
					fklPushPtrStack(fklGetASTPairCar(tmp),s1);
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
				fklPushPtrStack((void*)s1->top,s2);
				fklPushPtrStack(fklGetASTPairCdr(root),s1);
				fklPushPtrStack(fklGetASTPairCar(root),s1);
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
				s1->top=(long)fklTopPtrStack(s2);
				FklAstCptr* tmp=&root->outer->prev->cdr;
				free(tmp->u.pair);
				tmp->type=FKL_NIL;
				tmp->u.all=NULL;
				fklPushPtrStack(tmp,s1);
			}
			i++;
		}
		else if(objStr[i]==')')
		{
			hasComma=0;
			long t=(long)fklPopPtrStack(s2);
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
						if(num>INT32_MAX||num<INT32_MIN)
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
				FklAstCptr* tmp=fklPopPtrStack(s1);
				if(tmp)
				{
					tmp->type=FKL_PAIR;
					tmp->u.pair=fklNewPair(curline,tmp->outer);
					fklPushPtrStack(fklGetASTPairCdr(tmp),s1);
					fklPushPtrStack(fklGetASTPairCar(tmp),s1);
				}
			}
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
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

int fklIsAllSpace(const char* str)
{
	for(;*str;str++)
		if(!isspace(*str))
			return 0;
	return 1;
}

void mergeSort(void* _base,size_t num,size_t size,int (*cmpf)(const void*,const void*))
{
	void* base0=_base;
	void* base1=malloc(size*num);
	FKL_ASSERT(base1,"mergeSort",__FILE__,__LINE__);
	unsigned int seg=1;
	unsigned int start=0;
	for(;seg<num;seg+=seg)
	{
		for(start=0;start<num;start+=seg*2)
		{
			unsigned int l=start;
			unsigned int mid=FKL_MIN(start+seg,num);
			unsigned int h=FKL_MIN(start+seg*2,num);
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

char* fklStrCat(char* s1,const char* s2)
{
	s1=(char*)realloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FKL_ASSERT(s1,"fklStrCat",__FILE__,__LINE__);
	strcat(s1,s2);
	return s1;
}

uint8_t* fklCreateByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp,"fklCreateByteArry",__FILE__,__LINE__);
	return tmp;
}
