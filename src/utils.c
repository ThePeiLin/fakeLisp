#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/common.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include<limits.h>
#include<time.h>
#ifdef WIN32
#include<libloaderapi.h>
#include<io.h>
#include<process.h>
#include<direct.h>
#include<timeapi.h>
#else
#include<unistd.h>
#endif

#ifdef WIN32
char* fklSysgetcwd(void)
{
	return _getcwd(NULL,0);
}

int fklChdir(const char* dir)
{
	return _chdir(dir);
}
#else
char* fklSysgetcwd(void)
{
	return getcwd(NULL,0);
}

int fklChdir(const char* dir)
{
	return chdir(dir);
}
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

double fklStringToDouble(const FklString* str)
{
	return atof(str->str);
}

size_t fklWriteDoubleToBuf(char* buf,size_t max,double f64)
{
	size_t size=snprintf(buf,64,FKL_DOUBLE_FMT,f64);
	if(buf[strspn(buf,"-0123456789")]=='\0')
	{
		buf[size++]='.';
		buf[size++]='0';
	}
	return size;
}

FklString* fklIntToString(int64_t num)
{
	char numString[256]={0};
	int lenOfNum=snprintf(numString,256,"%"FKL_PRT64D"",num);
	FklString* tmp=fklCreateString(lenOfNum,numString);
	return tmp;
}

char* fklIntToCstr(int64_t num)
{
	char numString[256]={0};
	snprintf(numString,256,"%"FKL_PRT64D"",num);
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

int fklIsValidCharBuf(const char* str,size_t len)
{
	if(len==0)
		return 0;
	if(isalpha(str[0])&&len>1)
		return 0;
	if(str[0]=='\\')
	{
		if(len<2)
			return 1;
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

int fklCharBufToChar(const char* buf,size_t len)
{
	int ch=0;
	if(buf[0]!='\\')
		return buf[0];
	if(!(--len))
		return '\\';
	buf++;
	if(toupper(buf[0])=='X'&&isxdigit(buf[1]))
	{
		for(size_t i=1;i<len&&isxdigit(buf[i]);i++)
			ch=ch*16+(isdigit(buf[i])?buf[i]-'0':(toupper(buf[i])-'A'+10));
	}
	else if(fklIsNumberCharBuf(buf,len))
	{
		if(fklIsHexInt(buf,len))
			for(size_t i=2;i<len&&isxdigit(buf[i]);i++)
				ch=ch*16+(isdigit(buf[i])?buf[i]-'0':(toupper(buf[i])-'A'+10));
		else if(fklIsOctInt(buf,len))
			for(size_t i=1;i<len&&isdigit(buf[i])&&buf[i]<'8';i++)
				ch=ch*8+buf[i]-'0';
		else
			for(size_t i=0;i<len&&isdigit(buf[i]);i++)
				ch=ch*10+buf[i]-'0';
	}
	else
	{
		static const char* escapeChars=FKL_ESCAPE_CHARS;
		static const char* escapeCharsTo=FKL_ESCAPE_CHARS_TO;
		char ch=toupper(*(buf));
		for(size_t i=0;escapeChars[i];i++)
			if(ch==escapeChars[i])
				return escapeCharsTo[i];;
		return *buf;
	}
	return ch;
}

int fklIsNumberCharBuf(const char* buf,size_t len)
{
	return fklIsDecInt(buf,len)
		||fklIsOctInt(buf,len)
		||fklIsHexInt(buf,len)
		||fklIsDecFloat(buf,len)
		||fklIsHexFloat(buf,len);
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

int fklIsSpecialCharAndPrint(uint8_t ch,FILE* out)
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
	if(i+1>max)
		return 7;
	else
	{
#define UTF8_M_BITS (0x3F)
		uint32_t sum=((utf8bits[i].b&byte[0])<<(i*6))+(byte[1]&UTF8_M_BITS<<((i-1)*6));
#undef UTF8_M_BITS
		uint32_t min=utf8bits[i].min;
		if(sum<min)
			return 7;
		else
			return i+1;
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
	if(chr==' ')
		fputs("\\s",out);
	else if(chr=='\0')
		fputs("\\0",out);
	else if(fklIsSpecialCharAndPrint(chr,out));
	else if(isgraph(chr))
	{
		if(chr=='\\')
			fprintf(out,"\\");
		else
			fprintf(out,"%c",chr);
	}
	else
	{
		uint8_t j=chr;
		fputs("\\x",out);
		fprintf(out,"%02X",j);
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
		int resultOfCmp=fklStringCmp(pStr[mid],str);
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

static inline int check_file_ext(const char* filename,const char* ext)
{
	size_t i=strlen(filename);
	for(;i>0&&filename[i-1]!='.';i--);
	return i>0&&!strcmp(filename+i-1,ext);
}

int fklIsScriptFile(const char* filename)
{
	return check_file_ext(filename,FKL_SCRIPT_FILE_EXTENSION);
}

int fklIsByteCodeFile(const char* filename)
{
	return check_file_ext(filename,FKL_BYTECODE_FILE_EXTENSION);
}

int fklIsPrecompileFile(const char* filename)
{
	return check_file_ext(filename,FKL_PRE_COMPILE_FILE_EXTENSION);
}

void* fklCopyMemory(const void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	FKL_ASSERT(tmp||!size);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
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

#ifdef _WIN32
char* fklRelpath(const char* start,const char* path)
{
	char divstr[]=FKL_PATH_SEPARATOR_STR;
	char upperDir[]="..\\";
	char rp[MAX_PATH]={0};
	char* c_start=fklCopyCstr(start);
	char* c_path=fklCopyCstr(path);

	size_t start_part_count=0;
	size_t path_part_count=0;

	char** start_parts=fklSplit(c_start,divstr,&start_part_count);
	char** path_parts=fklSplit(c_path,divstr,&path_part_count);

	size_t length=(start_part_count<path_part_count)?start_part_count:path_part_count;
	size_t common_prefix_len=0;

	size_t index=1;
	for(;index<length;index++)
	{
		if(!strcmp(start_parts[index],path_parts[index]))
			common_prefix_len=index+1;
		else
			break;
	}
	char* trp=NULL;
	if(length)
	{
		char* start_drive=start_parts[0];
		char* path_drive=path_drive[0];

		while(*start_drive&&isspace(*start_drive))
			start_drive++;

		while(*path_drive&&isspace(*path_drive))
			path_drive++;

		if(toupper(*start_drive)!=toupper(*path_drive))
			goto exit;
	}

	if(!lastCommonRoot)
		goto exit;

	if(common_prefix_len==(length-1)&&start_part_count==path_part_count)
	{
		trp=fklCopyCstr(".");
		goto exit;
	}
	for(index=common_prefix_len;index<start_part_count;index++)
		if(start_part_count>0)
			strcat(rp,upperDir);
	for(index=common_prefix_len;index<path_part_count-1;index++)
	{
		strcat(rp,path_parts[index]);
		strcat(rp,divstr);
	}
	if(path_parts!=NULL)
		strcat(rp,path_parts[path_part_count-1]);
	trp=fklCopyCstr(rp);
exit:
	free(c_start);
	free(c_path);
	free(start_parts);
	free(path_parts);
	return trp;
}

#else

char* fklRelpath(const char* start,const char* path)
{
	char divstr[]=FKL_PATH_SEPARATOR_STR;
	char rp[PATH_MAX]={0};
	char upperDir[]="../";

	char* c_start=fklCopyCstr(start);
	char* c_path=fklCopyCstr(path);

	size_t start_part_count=0;
	size_t path_part_count=0;

	char** start_parts=fklSplit(c_start,divstr,&start_part_count);
	char** path_parts=fklSplit(c_path,divstr,&path_part_count);

	size_t length=(start_part_count<path_part_count)?start_part_count:path_part_count;
	size_t common_prefix_len=0;

	size_t index=0;
	for(;index<length;index++)
	{
		if(!strcmp(start_parts[index],path_parts[index]))
			common_prefix_len=index+1;
		else
			break;
	}
	char* trp=NULL;

	if(common_prefix_len==length&&start_part_count==path_part_count)
	{
		trp=fklCopyCstr(".");
		goto exit;
	}
	for(index=common_prefix_len;index<start_part_count;index++)
		if(start_part_count>0)
			strcat(rp,upperDir);
	for(index=common_prefix_len;index<path_part_count-1;index++)
	{
		strcat(rp,path_parts[index]);
		strcat(rp,divstr);
	}
	if(path_parts!=NULL)
		strcat(rp,path_parts[path_part_count-1]);
	trp=fklCopyCstr(rp);
exit:
	free(c_start);
	free(c_path);
	free(start_parts);
	free(path_parts);
	return trp;
}

#endif

#ifndef WIN32
char** fklSplit(char* str,const char* divstr,size_t* pcount)
{
	int count=0;
	char* context=NULL;
	char* pNext=NULL;
	char** str_slices=NULL;
	pNext=strtok_r(str,divstr,&context);
	while(pNext!=NULL)
	{
		char** tstrArry=(char**)fklRealloc(str_slices,sizeof(char*)*(count+1));
		FKL_ASSERT(tstrArry);
		str_slices=tstrArry;
		str_slices[count++]=pNext;
		pNext=strtok_r(NULL,divstr,&context);
	}
	*pcount=count;
	return str_slices;
}
#else
char** fklSplit(char* str,const char* divstr,size_t* pcount)
{
	int count=0;
	char* context=NULL;
	char* pNext=NULL;
	char** str_slices=NULL;
	pNext=strtok_s(str,divstr,&context);
	while(pNext!=NULL)
	{
		char** tstrArry=(char**)fklRealloc(str_slices,sizeof(char*)*(count+1));
		FKL_ASSERT(tstrArry);
		str_slices=tstrArry;
		str_slices[count++]=pNext;
		pNext=strtok_s(NULL,divstr,&context);
	}
	*pcount=count;
	return str_slices;
}
#endif

size_t fklCountCharInBuf(const char* buf,size_t n,char c)
{
	size_t num=0;
	for(size_t i=0;i<n;i++)
		if(buf[i]==c)
			num++;
	return num;
}

char* fklStrCat(char* s1,const char* s2)
{
	s1=(char*)fklRealloc(s1,sizeof(char)*(strlen(s1)+strlen(s2)+1));
	FKL_ASSERT(s1);
	strcat(s1,s2);
	return s1;
}

uint8_t* fklCreateByteArry(int32_t size)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp||!size);
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
	char* tstr=(char*)fklRealloc(str,(strlen(str)+1)*sizeof(char));
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
			fprintf(out,"%02X",j);
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
			else if(fklIsSpecialCharAndPrint(str[i],out));
			else
			{
				uint8_t j=str[i];
				fprintf(out,"\\x");
				fprintf(out,"%02X",j);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
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

void fklPrintCharBufInHex(const char* buf,uint32_t len,FILE* fp)
{
	for(const char* end=buf+len;buf<end;buf++)
		fprintf(fp,"%X",(uint8_t)(*buf));
}

int fklIsI64AddOverflow(int64_t a,int64_t b)
{
	int64_t sum=a+b;
	return (a<0&&b<0&&sum>0)||(a>0&&b>0&&sum<0);
}

union Fixu
{
	struct
	{
		int64_t fix:61;
		int8_t o:3;
	};
	int64_t i;
};

int fklIsFixAddOverflow(int64_t a,int64_t b)
{
	union Fixu us={.fix=a+b};
	int64_t sum=us.fix;
	return (a<0&&b<0&&sum>0)||(a>0&&b>0&&sum<0);
}

int fklIsI64MulOverflow(int64_t a,int64_t b)
{
	if(b==0||a==0)
		return 0;
	if(a>=0&&b>=0)
		return (INT64_MAX/a)<b;
	if(a<0&&b<0)
		return (INT64_MAX/a)>b;
	if(a*b==INT64_MIN)
		return 0;
	int64_t t=a*b;
	t/=b;
	return a!=t;
}

int fklIsFixMulOverflow(int64_t a,int64_t b)
{
	if(b==0||a==0)
		return 0;
	if(a>=0&&b>=0)
		return (INT64_MAX/a)<b;
	if(a<0&&b<0)
		return (INT64_MAX/a)>b;
	if(a*b==INT64_MIN)
		return 0;
	union Fixu t={.fix=a*b};
	t.fix/=b;
	return a!=t.fix;
}

char* fklCastEscapeCharBuf(const char* str,size_t size,size_t* psize)
{
	uint64_t strSize=0;
	uint64_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp);
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
			char* ttmp=(char*)fklRealloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp);
			tmp=ttmp;
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	*psize=strSize;
	return tmp;
}

#include<sys/stat.h>
#include<sys/types.h>

#ifdef WIN32
#define S_ISREG(m) ((S_IFMT&m)==S_IFREG)
#define S_ISDIR(m) ((S_IFMT&m)==S_IFDIR)
#endif

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

#ifndef WIN32
int fklIsAccessibleDirectory(const char* p)
{
	return !access(p,R_OK)&&fklIsDirectory(p);
}

int fklIsAccessibleRegFile(const char* p)
{
	return !access(p,R_OK)&&fklIsRegFile(p);
}

int fklMkdir(const char* dir)
{
	return mkdir(dir,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
}


#else
#define R_OK 4
#define W_OK 2
#define F_OK 0

int fklIsAccessibleDirectory(const char* p)
{
	return !_access(p,R_OK)&&fklIsDirectory(p);
}

int fklIsAccessibleRegFile(const char* p)
{
	return !_access(p,R_OK)&&fklIsRegFile(p);
}

int fklMkdir(const char* dir)
{
	return _mkdir(dir);
}

#endif

int fklRewindStream(FILE* fp,const char* buf,ssize_t len)
{
	if(fp==stdin)
	{
		for(size_t i=len;i>0;i--)
			if(ungetc(buf[i-1],fp)==-1)
				return -1;
		return 0;
	}
	return fseek(fp,-len,SEEK_CUR);
}

void* fklRealloc(void* ptr,size_t ns)
{
	return ns?realloc(ptr,ns):(free(ptr),NULL);
}

int fklIsDecInt(const char* cstr,size_t maxLen)
{
	// [-+]?(0|[1-9]\d*)
	if(!maxLen)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(cstr[idx]=='0')
	{
		if(idx+1==maxLen)
			return 1;
		else
			return 0;
	}
	if(isdigit(cstr[idx])&&cstr[idx]!='0')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(!isdigit(cstr[idx]))
			return 0;
	return 1;
}

int fklIsOctInt(const char* cstr,size_t maxLen)
{
	// [-+]?0[0-7]+
	if(maxLen<2)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<2)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(cstr[idx]<'0'||cstr[idx]>'7')
			return 0;
	return 1;
}

int fklIsHexInt(const char* cstr,size_t maxLen)
{
	// [-+]?0[xX][0-9a-fA-F]+
	if(maxLen<3)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<3)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	if(cstr[idx]=='x'||cstr[idx]=='X')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(!isxdigit(cstr[idx]))
			return 0;
	return 1;
}

int fklIsDecFloat(const char* cstr,size_t maxLen)
{
	// [-+]?(\.\d+([eE][-+]?\d+)?|\d+(\.\d*([eE][-+]?\d+)?|[eE][-+]?\d+))
	if(maxLen<2)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<2)
		return 0;
	if(cstr[idx]=='.')
	{
		idx++;
		if(maxLen-idx<1||cstr[idx]=='e'||cstr[idx]=='E')
			return 0;
		goto after_dot;
	}
	else
	{
		for(;idx<maxLen;idx++)
			if(!isdigit(cstr[idx]))
				break;
		if(idx==maxLen)
			return 0;
		if(cstr[idx]=='.')
		{
			idx++;
after_dot:
			for(;idx<maxLen;idx++)
				if(!isdigit(cstr[idx]))
					break;
			if(idx==maxLen)
				return 1;
			else if(cstr[idx]=='e'||cstr[idx]=='E')
				goto after_e;
			else
				return 0;
		}
		else if(cstr[idx]=='e'||cstr[idx]=='E')
		{
after_e:
			idx++;
			if(cstr[idx]=='-'||cstr[idx]=='+')
				idx++;
			if(maxLen-idx<1)
				return 0;
			for(;idx<maxLen;idx++)
				if(!isdigit(cstr[idx]))
					return 0;
		}
		else
			return 0;
	}
	return 1;
}

int fklIsHexFloat(const char* cstr,size_t maxLen)
{
	// [-+]?0[xX](\.[0-9a-fA-F]+[pP][-+]?[0-9a-fA-F]+|[0-9a-fA-F]+(\.[0-9a-fA-F]*[pP][-+]?[0-9a-fA-F]+|[pP][-+]?[0-9a-fA-F]+))
	if(maxLen<5)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<5)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	if(cstr[idx]=='x'||cstr[idx]=='X')
		idx++;
	else
		return 0;
	if(cstr[idx]=='.')
	{
		idx++;
		if(maxLen-idx<3||cstr[idx]=='p'||cstr[idx]=='P')
			return 0;
		goto after_dot;
	}
	else
	{
		for(;idx<maxLen;idx++)
			if(!isxdigit(cstr[idx]))
				break;
		if(idx==maxLen)
			return 0;
		if(cstr[idx]=='.')
		{
			idx++;
after_dot:
			for(;idx<maxLen;idx++)
				if(!isxdigit(cstr[idx]))
					break;
			if(idx==maxLen)
				return 0;
			else if(cstr[idx]=='p'||cstr[idx]=='P')
				goto after_p;
			else
				return 0;
		}
		else if(cstr[idx]=='p'||cstr[idx]=='P')
		{
after_p:
			idx++;
			if(cstr[idx]=='-'||cstr[idx]=='+')
				idx++;
			if(maxLen-idx<1)
				return 0;
			for(;idx<maxLen;idx++)
				if(!isxdigit(cstr[idx]))
					return 0;
		}
		else
			return 0;
	}
	return 1;
}

int fklIsAllDigit(const char* cstr,size_t len)
{
	for(const char* end=cstr+len;cstr<end;cstr++)
		if(!isdigit(*cstr))
			return 0;
	return 1;
}

int fklIsFloat(const char *cstr,size_t maxLen)
{
	return fklIsDecFloat(cstr,maxLen)
		||fklIsHexFloat(cstr,maxLen);
}

int64_t fklStringToInt(const char* cstr,size_t maxLen,int* base)
{
	if(base)
	{
		if(fklIsDecInt(cstr,maxLen))
			return strtoll(cstr,NULL,(*base=10));
		else if(fklIsHexInt(cstr,maxLen))
			return strtoll(cstr,NULL,(*base=16));
		else
			return strtoll(cstr,NULL,(*base=8));
	}
	else
	{
		if(fklIsDecInt(cstr,maxLen))
			return strtoll(cstr,NULL,10);
		else if(fklIsHexInt(cstr,maxLen))
			return strtoll(cstr,NULL,16);
		else
			return strtoll(cstr,NULL,8);
	}
}

int fklGetDelim(FILE *fp,FklStringBuffer *b,char d)
{
	int c;
	while((c=fgetc(fp))!=EOF)
	{
		fklStringBufferPutc(b,c);
		if(c==d)
			break;
	}
	return c;
}

