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

FklString* fklIntToString(int64_t num)
{
	char numString[256]={0};
	int lenOfNum=sprintf(numString,"%ld",num);
	FklString* tmp=fklCreateString(lenOfNum,numString);
	return tmp;
}

char* fklIntToCstr(int64_t num)
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
	fklPrintRawCharBuf((const uint8_t*)objStr,se,strlen(objStr),out);
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
			ch=ch*16+(isdigit(buf[i])?buf[i]-'0':(toupper(buf[i])-'A'+10));
	}
	else if(fklIsNumberCharBuf(buf,len))
	{
		if(fklIsHexNumCharBuf(buf,len))
			for(size_t i=2;i<len&&isxdigit(buf[i]);i++)
				ch=ch*16+(isdigit(buf[i])?buf[i]-'0':(toupper(buf[i])-'A'+10));
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

inline int static isSpecialCharAndPrint(uint8_t ch,FILE* out)
{
	int r=0;
	if((r=ch=='\n'))
		fputs("\\n",out);
	else if((r=ch=='\t'))
		fputs("\\t",out);
	else if((r=ch=='\v'))
		fputs("\\v",out);
	else if((r=ch=='\a'))
		fputs("\\a",out);
	else if((r=ch=='\b'))
		fputs("\\b",out);
	else if((r=ch=='\f'))
		fputs("\\f",out);
	else if((r=ch=='\r'))
		fputs("\\r",out);
	else if((r=ch=='\x20'))
		putc(ch,out);
	return r;
}

unsigned int fklGetByteNumOfUtf8(const uint8_t* byte,size_t max)
{
#define UTF8_ASCII (0x80)
#define UTF8_M (0xC0)
	if(byte[0]<UTF8_ASCII)
		return 1;
	else if(byte[0]<UTF8_M)
		return 7;
#undef UTF8_ASCII
#undef UTF8_M
	static struct{uint8_t a;uint8_t b;uint32_t min;} utf8bits[]=
	{
		{0x7F,0x7F,0x00000000,},
		{0xDF,0x1F,0x00000080,},
		{0xEF,0x0F,0x00000800,},
		{0xF7,0x07,0x00010000,},
		{0xFB,0x03,0x00200000,},
		{0xFD,0x01,0x04000000,},
	};
	size_t i=0;
	for(;i<6;i++)
		if((byte[0]|utf8bits[i].a)==utf8bits[i].a)
			break;
	i++;
	if(i>max)
		return 7;
	else
	{
		uint32_t sum=0;
		uint32_t bitmove=0;
		uint32_t min=utf8bits[i-1].min;
#define UTF8_M_BITS (0x3F)
		for(size_t j=i;j>1;j--)
		{
			uint32_t cur=byte[j-1]&UTF8_M_BITS;
			sum+=cur<<bitmove;
			bitmove+=6;
		}
		sum+=(utf8bits[i-1].b&byte[0])<<bitmove;
#undef UTF8_M_BITS
		if(sum<min)
			return 7;
		else
			return i;
	}
}

int fklWriteCharAsCstr(char chr,char* buf,size_t s)
{
	if(s<=3)
		return 1;
	buf[0]='#';
	buf[1]='\\';
	if(isgraph(chr))
	{
		if(chr=='\\')
		{
			if(s<=4)
				return 1;
			buf[2]='\\';
			buf[3]='\\';
			buf[4]='\0';
		}
		else
		{
			buf[2]=chr;
			buf[3]='\0';
		}
	}
	else
	{
		if(s<=6)
			return 1;
		uint8_t j=chr;
		uint8_t h=j/16;
		uint8_t l=j%16;
		buf[2]='\\';
		buf[3]='x';
		buf[4]=h<10?'0'+h:'A'+(h-10);
		buf[5]=l<10?'0'+l:'A'+(l-10);
		buf[6]='\0';
	}
	return 0;
}

void fklPrintRawChar(char chr,FILE* out)
{
	fprintf(out,"#\\");
	if(isgraph(chr))
	{
		if(chr=='\\')
			fprintf(out,"\\\\");
		else
			fprintf(out,"%c",chr);
	}
	else if(isSpecialCharAndPrint(chr,out));
	else
	{
		uint8_t j=chr;
		fprintf(out,"x");
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

size_t fklCountCharInBuf(const char* buf,size_t n,char c)
{
	size_t num=0;
	for(size_t i=0;i<n;i++)
		if(buf[i]==c)
			num++;
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

void fklPrintRawCharBuf(const uint8_t* str,char se,size_t size,FILE* out)
{
	putc(se,out);
	uint64_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(out,"\\x");
			fprintf(out,"%X",j/16);
			fprintf(out,"%X",j%16);
			i++;
		}
		else if(l==1)
		{
			if(str[i]==se)
				fprintf(out,"\\%c",se);
			else if(str[i]=='\\')
				fprintf(out,"\\\\");
			else if(isgraph(str[i]))
				putc(str[i],out);
			else if(isSpecialCharAndPrint(str[i],out));
			else
			{
				uint8_t j=str[i];
				fprintf(out,"\\x");
				fprintf(out,"%X",j/16);
				fprintf(out,"%X",j%16);
			}
			i++;
		}
		else
		{
			for(int j=0;j<l;j++)
				putc(str[i+j],out);
			i+=l;
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

char* fklCastEscapeCharBuf(const char* str,size_t size,size_t* psize)
{
	uint64_t strSize=0;
	uint64_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	for(size_t i=0;i<size;)
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
	*psize=strSize;
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
