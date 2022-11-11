#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/symbol.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include<limits.h>
#ifdef WIN32
#include<io.h>
#include<process.h>
#include<tchar.h>
#else
#include<unistd.h>
#include<dlfcn.h>
#endif

static char* CurWorkDir=NULL;
static char* MainFileRealPath=NULL;

void fklSetCwd(const char* path)
{
	CurWorkDir=fklCopyCstr(path);
}

void fklDestroyCwd(void)
{
	free(CurWorkDir);
	CurWorkDir=NULL;
}

const char* fklGetCwd(void)
{
	return CurWorkDir;
}

void fklSetMainFileRealPath(const char* path)
{
	MainFileRealPath=fklGetDir(path);
}

void fklSetMainFileRealPathWithCwd(void)
{
	MainFileRealPath=fklCopyCstr(CurWorkDir);
}

void fklDestroyMainFileRealPath(void)
{
	free(MainFileRealPath);
	MainFileRealPath=NULL;
}

const char* fklGetMainFileRealPath(void)
{
	return MainFileRealPath;
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
	tmp=(char*)malloc(sizeof(char)*(len+1));
	FKL_ASSERT(tmp);
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
	tmp=(char*)malloc(sizeof(char)*(len+1));
	FKL_ASSERT(tmp);
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
	tmp=(char*)malloc(sizeof(char)*(len+1));
	FKL_ASSERT(tmp);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

FklString* fklDoubleToString(double num)
{
	char numString[256]={0};
	int lenOfNum=sprintf(numString,"%lf",num);
	FklString* tmp=fklCreateString(lenOfNum,numString);
	return tmp;
}


double fklStringToDouble(const FklString* str)
{
	char c_str[str->size+1];
	fklWriteStringToCstr(c_str,str);
	double tmp=atof(c_str);
	return tmp;
}

FklString* fklIntToString(long num)
{
	char numString[256]={0};
	int lenOfNum=sprintf(numString,"%ld",num);
	FklString* tmp=fklCreateString(lenOfNum,numString);
	return tmp;
}

char* fklIntToCstr(long num)
{
	char numString[256]={0};
	sprintf(numString,"%ld",num);
	return fklCopyCstr(numString);
}

int fklPower(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}

void fklPrintRawCstring(const char* objStr,char se,FILE* out)
{
	fklPrintRawCharBuf(objStr,se,strlen(objStr),out);
}

int fklIsHexNumCstr(const char* objStr)
{
	size_t i=(*objStr=='-')?1:0;
	if(!strncmp(objStr+i,"0x",2)||!strncmp(objStr+i,"0X",2))
	{
		for(i+=2;objStr[i]!='\0';i++)
		{
			if(!isxdigit(objStr[i]))
				return 0;
		}
	}
	else
		return 0;
	return 1;
}

int fklIsHexNumCharBuf(const char* buf,size_t len)
{
	if(len>0)
	{
		size_t i=(*buf=='-')?1:0;
		if(len>2&&(!strncmp(buf+i,"0x",2)||!strncmp(buf+i,"0X",2)))
		{
			for(i+=2;i<len;i++)
				if(!isxdigit(buf[i]))
					return 0;
		}
		else
			return 0;
		return 1;
	}
	return 0;
}

int fklIsOctNumCharBuf(const char* buf,size_t len)
{
	if(len>0)
	{
		size_t i=(*buf=='-')?1:0;
		if(len<2||buf[i]!='0')
			return 0;
		for(;i<len;i++)
		{
			if(!isdigit(buf[i])||buf[i]>'7')
				return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsOctNumCstr(const char* objStr)
{
	size_t i=(*objStr=='-')?1:0;
	if(objStr[i]!='0')
		return 0;
	for(;objStr[i]!='\0';i++)
	{
		if(!isdigit(objStr[i])||objStr[i]>'7')
			return 0;
	}
	return 1;
}

int fklIsDoubleString(const FklString* str)
{
	return fklIsDoubleCharBuf(str->str,str->size);
}

int fklIsDoubleCstr(const char* objStr)
{
	size_t i=(objStr[0]=='-')?1:0;
	int isHex=(!strncmp(objStr+i,"0x",2)||!strncmp(objStr+i,"0X",2));
	ssize_t len=strlen(objStr);
	for(i+=isHex*2;objStr[i]!='\0';i++)
	{
		if(objStr[i]=='.'||(i!=0&&toupper(objStr[i])==('E'+isHex*('P'-'E'))&&i<(len-1)))
			return 1;
	}
	return 0;
}

int fklIsDoubleCharBuf(const char* buf,size_t len)
{
	if(len>0)
	{
		size_t i=(buf[0]=='-')?1:0;
		int isHex=len>2&&(!strncmp(buf+i,"0x",2)||!strncmp(buf+i,"0X",2));
		for(i+=isHex*2;i<len;i++)
		{
			if(buf[i]=='.'||(i!=0&&toupper(buf[i])==('E'+isHex*('P'-'E'))&&i<(len-1)))
				return 1;
		}
	}
	return 0;
}

int fklCharBufToChar(const char* buf,size_t len)
{
	int ch=0;
	if(buf[0]!='\\')
		return buf[0];
	buf++;
	len--;
	if(toupper(buf[0])=='X'&&isxdigit(buf[1]))
	{
		for(size_t i=1;i<len&&isxdigit(buf[i]);i++)
			ch=ch*16+isdigit(buf[i])?buf[i]-'0':toupper(buf[i])-'A'+10;
	}
	else if(fklIsNumberCharBuf(buf,len))
	{
		if(fklIsHexNumCharBuf(buf,len))
			for(size_t i=2;i<len&&isxdigit(buf[i]);i++)
				ch=ch*16+isdigit(buf[i])?buf[i]-'0':toupper(buf[i])-'A'+10;
		else if(fklIsOctNumCharBuf(buf,len))
			for(size_t i=1;i<len&&isdigit(buf[i])&&buf[i]<'8';i++)
				ch=ch*8+buf[i]-'0';
		else
			for(size_t i=0;i<len&&isdigit(buf[i]);i++)
				ch=ch*10+buf[i]-'0';
	}
	else
	{
		switch(toupper(*(buf)))
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
				ch=*(buf);
				break;
		}
	}
	return ch;
}

int fklStringToChar(const FklString* str)
{
	return fklCharBufToChar(str->str,str->size);
}

int fklIsNumberCharBuf(const char* buf,size_t len)

{
	if(!len)
		return 0;
	int i=(*buf=='-'||*buf=='+')?1:0;
	int hasDot=0;
	int hasExp=0;
	if(i&&(len<2||!isdigit(buf[1])))
		return 0;
	else if(len==1&&buf[0]=='.')
		return 0;
	else
	{
		if(len>2&&(!strncmp(buf+i,"0x",2)||!strncmp(buf+i,"0X",2)))
		{
			for(i+=2;i<len;i++)
			{
				if(buf[i]=='.')
				{
					if(hasDot)
						return 0;
					else
						hasDot=1;
				}
				else if(!isxdigit(buf[i]))
				{
					if(toupper(buf[i])=='P')
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
				if(buf[i]=='.')
				{
					if(hasDot)
						return 0;
					else
						hasDot=1;
				}
				else if(!isdigit(buf[i]))
				{
					if(toupper(buf[i])=='E')
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

int fklIsNumberString(const FklString* str)
{
	return fklIsNumberCharBuf(str->str,str->size);
}

int fklIsNumberCstr(const char* objStr)
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

int fklIsSymbolShouldBeExport(const FklString* str,const FklString** pStr,uint32_t n)
{
	int32_t l=0;
	int32_t h=n-1;
	int32_t mid=0;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int resultOfCmp=fklStringcmp(pStr[mid],str);
		if(resultOfCmp>0)
			h=mid-1;
		else if(resultOfCmp<0)
			l=mid+1;
		else
			return 1;
	}
	return 0;
}

char* fklCopyCstr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	FKL_ASSERT(tmp);
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
	FKL_ASSERT(tmp);
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
	FKL_ASSERT(tmp);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

int fklChangeWorkPath(const char* filename)
{
	char* p=fklRealpath(filename);
	char* wp=fklGetDir(p);
	int r=chdir(wp);
	free(p);
	free(wp);
	return r;
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
	int i=strlen(filename)-1;
	for(;filename[i]!=FKL_PATH_SEPARATOR;i--);
	char* tmp=(char*)malloc(sizeof(char)*(i+1));
	FKL_ASSERT(tmp);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* fklGetStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	FKL_ASSERT(tmp);
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
		tmp=(char*)malloc(sizeof(char)*(i+1));
		FKL_ASSERT(tmp);
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

char* fklRelpath(const char* abs,const char* relto)
{
	char divstr[]=FKL_PATH_SEPARATOR_STR;
#ifdef _WIN32
	char upperDir[]="..\\";
#else
	char upperDir[]="../";
#endif
	char* cabs=fklCopyCstr(abs);
	char* crelto=fklCopyCstr(relto);
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
		strcat(rp,divstr);
	}
	if(reltoDirs!=NULL)
		strcat(rp,reltoDirs[lengthOfRelto-1]);
	char* trp=fklCopyCstr(rp);
	free(cabs);
	free(crelto);
	free(absDirs);
	free(reltoDirs);
	return trp;
}

char** fklSplit(char* str,char* divstr,int* length)
{
	int count=0;
	char* context=NULL;
	char* pNext=NULL;
	char** strArry=(char**)malloc(0);
	FKL_ASSERT(strArry);
	pNext=strtok_r(str,divstr,&context);
	while(pNext!=NULL)
	{
		count++;
		char** tstrArry=(char**)realloc(strArry,sizeof(char*)*count);
		FKL_ASSERT(tstrArry);
		strArry=tstrArry;
		strArry[count-1]=pNext;
		pNext=strtok_r(NULL,divstr,&context);
	}
	*length=count;
	return strArry;
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
	FKL_ASSERT(base1);
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
	FKL_ASSERT(s1);
	strcat(s1,s2);
	return s1;
}

uint8_t* fklCreateByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp);
	return tmp;
}

char* fklCharBufToCstr(const char* buf,size_t size)
{
	char* str=(char*)malloc(sizeof(char)*(size+1));
	FKL_ASSERT(str);
	size_t len=0;
	for(size_t i=0;i<size;i++)
	{
		if(!buf[i])
			continue;
		str[len]=buf[i];
		len++;
	}
	str[len]='\0';
	char* tstr=(char*)realloc(str,(strlen(str)+1)*sizeof(char));
	FKL_ASSERT(str);
	str=tstr;
	return str;
}

void fklPrintRawCharBuf(const char* str,char se,size_t size,FILE* out)
{
	putc(se,out);
	for(uint64_t i=0;i<size;i++)
	{
		if(str[i]==se)
			fprintf(out,"\\%c",se);
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
	putc(se,out);
}

void fklPrintRawByteBuf(const uint8_t* ptr,size_t size,FILE* out)
{
	fprintf(out,"#vu8(");
	for(size_t i=0;i<size;i++)
	{
		fprintf(out,"0x%X",ptr[i]);
		if(i<size-1)
			fputc(' ',out);
	}
	fprintf(out,")");
}

inline int fklIsI64AddOverflow(int64_t a,int64_t b)
{
	int64_t sum=a+b;
	return (a<0&&b<0&&sum>0)||(a>0&&b>0&&sum<0);
}

inline int fklIsI64MulOverflow(int64_t a,int64_t b)
{
	if(b==0)
		return 0;
	int64_t t=a*b;
	t/=b;
	return a!=t;
}

char* fklCastEscapeCharBuf(const char* str,char end,size_t* size)
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
			const char* backSlashStr=str+i;
			size_t len=1;
			if(isdigit(backSlashStr[len]))
			{
				if(backSlashStr[len]=='0')
				{
					if(toupper(backSlashStr[len+1])=='X')
						for(len++;isxdigit(backSlashStr[len])&&len<5;len++);
					else
						for(;isdigit(backSlashStr[len])&&backSlashStr[len]<'8'&&len<5;len++);
				}
				else
					for(;isdigit(backSlashStr[len+1])&&len<4;len++);
			}
			else if(toupper(backSlashStr[len])=='X')
				for(len++;isxdigit(backSlashStr[len])&&len<4;len++);
			else
				len++;
			ch=fklCharBufToChar(backSlashStr,len);
			i+=len;
		}
		else ch=str[i++];
		strSize++;
		if(strSize>memSize-1)
		{
			char* ttmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp);
			tmp=ttmp;
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	char* ttmp=(char*)realloc(tmp,strSize*sizeof(char));
	FKL_ASSERT(!strSize||ttmp);
	tmp=ttmp;
	*size=strSize;
	return tmp;
}

#include<sys/stat.h>
#include<sys/types.h>
int fklIsRegFile(const char* p)
{
	struct stat buf;
	stat(p,&buf);
	return S_ISREG(buf.st_mode);
}

int fklIsDirectory(const char* p)
{
	struct stat buf;
	stat(p,&buf);
	return S_ISDIR(buf.st_mode);
}

int fklIsAccessableDirectory(const char* p)
{
	return !access(p,R_OK)&&fklIsDirectory(p);
}

int fklIsAccessableScriptFile(const char* p)
{
	return !access(p,R_OK)&&fklIsRegFile(p);
}
