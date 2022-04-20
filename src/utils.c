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

char* fklGetStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),__func__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* fklGetStringAfterBackslash(const char* str)
{
	char* tmp=NULL;
	size_t len=0;
	while(!isspace(*(str+len))&&*(str+len)!='\0')
	{
		len++;
		if(!isalnum(str[len])&&str[len-1]!='\\')
			break;
	}
	FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),__func__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* fklGetStringAfterBackslashInStr(const char* str)
{
	char* tmp=NULL;
	size_t len=0;
	if(str[0])
	{
		if(isdigit(str[0]))
		{
			len++;
			if(str[1]&&isdigit(str[1]))
				len++;
			if(str[2]&&isdigit(str[2]))
				len++;
		}
		else if(toupper(str[0])=='X')
		{
			len++;
			if(str[1]&&isxdigit(str[1]))
				len++;
			if(str[2]&&isxdigit(str[2]))
				len++;
		}
		else
			len++;
	}
	FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(len+1))),__func__);
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
	FKL_ASSERT(tmp,__func__);
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
	FKL_ASSERT((tmp=(char*)malloc(lenOfNum*sizeof(char))),__func__);
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
		else if(isgraph(*tmpStr))
			putc(*tmpStr,out);
		else if(*tmpStr=='\x20')
			putc(*tmpStr,out);
		else
		{
			uint8_t j=*tmpStr;
			fprintf(out,"\\x");
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
			case 'S':
				ch=0x20;
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
	if(!len)
		return 0;
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

char* fklCopyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	FKL_ASSERT(tmp,__func__);
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
	FKL_ASSERT(tmp,__func__);
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

void* fklCopyMemory(const void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	FKL_ASSERT(tmp,__func__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

void fklChangeWorkPath(const char* filename)
{
	char* p=fklRealpath(filename);
	char* wp=fklGetDir(p);
	if(chdir(wp))
	{
		perror(wp);
		exit(EXIT_FAILURE);
	}
	free(p);
	free(wp);
}

char* fklRealpath(const char* filename)
{
#ifdef _WIN32
	return _fullpath(NULL,filename,0);
#else
	return realpath(filename,NULL);
#endif
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
	FKL_ASSERT(tmp,__func__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* fklGetStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	FKL_ASSERT(tmp,__func__);
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
		FKL_ASSERT((tmp=(char*)malloc(sizeof(char)*(i+1))),__func__);
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
	FKL_ASSERT(strArry,__func__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		FKL_ASSERT(strArry,__func__);
		strArry[count-1]=pNext;
		pNext=strtok(NULL,divstr);
	}
	*length=count;
	return strArry;
}

char* fklCastEscapeCharater(const char* str,char end,size_t* len)
{
	int32_t strSize=0;
	int32_t memSize=FKL_MAX_STRING_SIZE;
	int32_t i=0;
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
	if(tmp)tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,memSize*sizeof(char));
	FKL_ASSERT(tmp,__func__);
	*len=i+1;
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
	FKL_ASSERT(base1,__func__);
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

char* fklStrCat(char* s1,const char* s2)
{
	s1=(char*)realloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FKL_ASSERT(s1,__func__);
	strcat(s1,s2);
	return s1;
}

uint8_t* fklCreateByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp,__func__);
	return tmp;
}

char* fklCharBufToStr(const char* buf,size_t size)
{
	char* str=(char*)malloc(sizeof(char)*(size+1));
	FKL_ASSERT(str,__func__);
	size_t len=0;
	for(size_t i=0;i<size;i++)
	{
		if(!buf[i])
			continue;
		str[len]=buf[i];
		len++;
	}
	str[len]='\0';
	str=(char*)realloc(str,(strlen(str)+1)*sizeof(char));
	FKL_ASSERT(str,__func__);
	return str;
}

void fklPrintRawCharBuf(const char* str,size_t size,FILE* out)
{
	putc('\"',out);
	for(uint64_t i=0;i<size;i++)
	{
		if(str[i]=='\"')
			fprintf(out,"\\\"");
		else if(isgraph(str[i]))
			putc(str[i],out);
		else if(str[i]=='\x20')
			putc(str[i],out);
		else
		{
			uint8_t j=str[i];
			fprintf(out,"\\x");
			fprintf(out,"%X",j/16);
			fprintf(out,"%X",j%16);
		}
	}
	putc('\"',out);
}
