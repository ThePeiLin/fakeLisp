#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include"tool.h"

char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	char ch;
	int i=0;
	int j;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		if(ch=='\n'&&braketsNum==0)break;
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*i)))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
		if(ch=='(')braketsNum++;
		if(ch==')')braketsNum--;
		if(isalnum(ch))continue;
		if(braketsNum==0)break;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}

char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	if(!(tmp=(char*)malloc(sizeof(char)*len+1)))errors(OUTOFMEMORY);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* floatToString(double num)
{
	int i;
	char numString[sizeof(double)*2+3];
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}


double stringToFloat(const char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}



char* intToString(long num)
{
	int i;
	char numString[sizeof(long)*2+3];
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}


long stringToInt(const char* str)
{
	long tmp;
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


rawString getStringBetweenMarks(const char* str)
{
	rawString obj;
	char ch=0;
	char* tmp=NULL;
	char* before;
	int len=0;
	int i=1;
	int j;
	while(*(str+i)!='\"')
	{
		if(*(str+i)=='\\')
		{
			if(isdigit(*(str+i+1)))
			{
				if(*(str+i+1)=='0')
				{
					if(isdigit(*(str+i+2)))
					{
						ch=0;
						int k=0;
						int len;
						while((isdigit(*(str+i+2+k))&&*(str+i+2+k)<'8')&&k<4)k++;
						len=k;
						while(k>0)ch=ch+(*(str+i+2+len-k)-'0')*power(8,--k);
						i+=len+2;
					}
					else if((*(str+i+2))=='x'||(*(str+i+2))=='X')
					{
						ch=0;
						int k=0;
						int len;
						while(isxdigit(*(str+i+3)))k++;
						len=k;
						while(k>0)
						{
							int result;
							switch(*(str+i+3+len-k))
							{
								case 'a':
								case 'A':result=10;break;
								case 'b':
								case 'B':result=11;break;
								case 'c':
								case 'C':result=12;break;
								case 'd':
								case 'D':result=13;break;
								case 'e':
								case 'E':result=14;break;
								case 'f':
								case 'F':result=15;break;
								default:if(isdigit(*(str+i+3+len-k)))result=*(str+i+3+len-k)-'0';
							}
							ch=ch+result*power(16,--k);
						}
						i+=len+3;
					}
				}
				else if(isdigit(*(str+i+1))&&*(str+i+1)!='0')
				{
					ch=0;
					int k=0;
					int len;
					while(isdigit(*(str+i+1+k))&&k<4)k++;
					len=k;
					while(k>0)ch=ch+(*(str+i+1+len-k)-'0')*power(10,--k);
					i+=len+1;
				}
			}
			else
			{
				switch(*(str+i+1))
				{
					case 'A':
					case 'a':ch=0x07;break;
					case 'b':
					case 'B':ch=0x08;break;
					case 't':
					case 'T':ch=0x09;break;
					case 'n':
					case 'N':ch=0x0a;break;
					case 'v':
					case 'V':ch=0x0b;break;
					case 'f':
					case 'F':ch=0x0c;break;
					case 'r':
					case 'R':ch=0x0d;break;
					case '\\':ch=0x5c;break;
					default:ch=*(str+i+1);break;
				}
				i+=2;
			}
		}
		else ch=*(str+(i++));
		len++;
		j=len-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*len)))
		{
			fprintf(stderr,"Out of memory!\n");
			exit(1);
		}
		if(before!=NULL)memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	obj.len=i+1;
	obj.str=tmp;
	return obj;
}
void printRawString(const char* objStr,FILE* out)
{
	const char* tmpStr=objStr;
	int len=strlen(objStr);
	putc('\"',out);
	for(;tmpStr<objStr+len;tmpStr++)
	{
		if(isgraph(*tmpStr))
			putc(*tmpStr,out);
		else
			fprintf(out,"\\0x%0x",*tmpStr);
	}
	putc('\"',out);
}

void errors(int types)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n",
	};
	fprintf(stderr,"error:%s",inform[types]);
	exit(1);
}
