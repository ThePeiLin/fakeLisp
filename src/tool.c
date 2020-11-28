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
#endif
#include"tool.h"
#include"opcode.h"
#define NUMOFBUILDINSYMBOL 60
char* builtInSymbolList[]=
{
	"nil",
	"EOF",
	"stdin",
	"stdout",
	"stderr",
	"cons",
	"car",
	"cdr",
	"atom",
	"null",
	"app",
	"ischr",
	"isint",
	"isdbl",
	"isstr",
	"issym",
	"isprc",
	"isbyt",
	"eq",
	"eqn",
	"equal",
	"gt",
	"ge",
	"lt",
	"le",
	"not",
	"dbl",
	"str",
	"sym",
	"chr",
	"int",
	"byt",
	"add",
	"sub",
	"mul",
	"div",
	"mod",
	"rand",
	"nth",
	"length",
	"append",
	"strcat",
	"bytcat",
	"open",
	"close",
	"getc",
	"ungetc",
	"read",
	"readb",
	"write",
	"writeb",
	"princ",
	"tell",
	"seek",
	"rewind",
	"exit",
	"go",
	"send",
	"accept",
	"getid",
};

char* getListFromFile(FILE* file)
{
	char* before=NULL;
	int ch;
	int lenOfStr=0;
	int braketsNum=0;
	int memSize=1;
	int anotherChar=0;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp[0]='\0';
	while((ch=getc(file))!=EOF)
	{
		if(lenOfStr!=0&&tmp[lenOfStr-1]==')'&&ch=='('&&braketsNum==0)
		{
			ungetc(ch,file);
			break;
		}
		if(!isspace(ch))anotherChar=1;
		if(ch==';')
		{
			while(getc(file)!='\n');
			ungetc('\n',file);
			continue;
		}
		else if(ch=='\'')
		{
			int numOfSpace=0;
			int lenOfMemory=1;
			char* stringOfSpace=(char*)malloc(sizeof(char)*lenOfMemory);
			if(stringOfSpace==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			stringOfSpace[numOfSpace]='\0';
			char* before=NULL;
			while(isspace((ch=getc(file))))
			{
				numOfSpace++;
				lenOfMemory++;
				before=stringOfSpace;
				if(!(stringOfSpace=(char*)malloc(sizeof(char)*lenOfMemory)))errors(OUTOFMEMORY,__FILE__,__LINE__);
				if(before!=NULL)
				{
					memcpy(stringOfSpace,before,numOfSpace);
					free(before);
				}
				stringOfSpace[numOfSpace-1]=ch;
			}
			stringOfSpace[numOfSpace]='\0';
			ungetc(ch,file);
			char beQuote[]="(quote ";
			char* tmpList=subGetList(file);
			char* other=NULL;
			int len=strlen(tmpList)+strlen(beQuote)+strlen(stringOfSpace)+1;
			if(!(other=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY,__FILE__,__LINE__);
			strcpy(other,beQuote);
			strcat(other,stringOfSpace);
			strcat(other,tmpList);
			other[len-1]=')';
			lenOfStr+=len;
			before=tmp;
			if(!(tmp=(char*)malloc(sizeof(char)*(lenOfStr+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
			memcpy(tmp,before,memSize);
			strncat(tmp,other,len);
			if(before!=NULL)free(before);
			memSize=lenOfStr+1;
			free(other);
			free(tmpList);
			free(stringOfSpace);
			continue;
		}
		lenOfStr++;
		memSize++;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(memSize))))
			errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(before!=NULL)
		{
			memcpy(tmp,before,lenOfStr-1);
			free(before);
		}
		tmp[lenOfStr-1]=ch;
		tmp[lenOfStr]='\0';
		if(ch=='(')braketsNum++;
		else if(ch==')')braketsNum--;
		else if(isspace(ch)&&braketsNum<=0&&anotherChar)break;
		else if(!isspace(ch)&&ch!=',')
		{
			char* tmpStr=(ch=='\"')?getStringAfterMark(file):getAtomFromFile(file);
			tmp=(char*)realloc(tmp,sizeof(char)*(strlen(tmpStr)+strlen(tmp)+1));
			if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			strcat(tmp,tmpStr);
			lenOfStr=strlen(tmp);
			memSize=lenOfStr+1;
			free(tmpStr);
		}
	}
	if(braketsNum!=0)
	{
		fprintf(stderr,"%s:Syntax error.\n",tmp);
		free(tmp);
		exit(EXIT_FAILURE);
	}
	return tmp;
}

char* getAtomFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp[0]='\0';
	char* before;
	int i=0;
	char ch;
	int j;
	while((ch=getc(file))!=EOF)
	{
		if(isspace(ch)||ch==')'||ch=='('||ch==',')
		{
			ungetc(ch,file);
			break;
		}
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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

char* getStringAfterMark(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp[0]='\0';
	char* before;
	int i=0;
	int j;
	int ch;
	while((ch=getc(file))!=EOF)
	{
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(before!=NULL)
		{
			memcpy(tmp,before,j);
			free(before);
		}
		tmp[j]=ch;
		if(ch=='\"'&&tmp[j-1]!='\\')break;
	}
	if(tmp!=NULL)tmp[i]='\0';
	return tmp;
}

char* subGetList(FILE* file)
{
	char* tmp=NULL;
	char* before;
	int ch;
	int i=0;
	int j=0;
	int mark=0;
	int braketsNum=0;
	int anotherChar=0;
	while((ch=getc(file))!=EOF)
	{
		if(ch==')'&&!mark&&(tmp==NULL||*(tmp+j)!='\\'))
		{
			if(braketsNum<=0)
			{
				ungetc(ch,file);
				break;
			}else braketsNum--;
		}
		if(ch==';'&&!mark&&(tmp==NULL||*(tmp+j)!='\\'))
		{
			while(getc(file)!='\n');
			continue;
		}
		if(ch==','&&braketsNum<=0&&!mark&&(tmp==NULL||((j==0)&&(*(tmp+j-1)!='\\'))))
		{
			ungetc(ch,file);
			break;
		}
		if(isspace(ch)&&braketsNum<=0&&anotherChar){ungetc(ch,file);break;}
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))
			errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(before!=NULL)
		{
			memcpy(tmp,before,j);
			free(before);
		}
		*(tmp+j)=ch;
		mark^=(ch=='\"'&&(j==0||*(tmp+j-1)!='\\'));
		if(ch=='('&&!mark&&(j==0||*(tmp+j-1)!='\\'))braketsNum++;
		else if(!isspace(ch))anotherChar=1;
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
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* doubleToString(double num)
{
	int i;
	char numString[sizeof(double)*2+3];
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
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
	size_t i;
	char numString[sizeof(long)*2+3];
	for(i=0;i<sizeof(long)*2+3;i++)numString[i]=0;
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	if(!(tmp=(char*)malloc(lenOfNum*sizeof(char))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);;
	return tmp;
}


int32_t stringToInt(const char* str)
{
	int32_t tmp;
	char* format=NULL;
	if(isHexNum(str))format="%lx";
	else if(isOctNum(str))format="%lo";
	else format="%ld";
	sscanf(str,format,&tmp);
	return tmp;
}


int power(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}


rawString getStringBetweenMarks(const char* str,intpr* inter)
{
	rawString obj;
	char ch=0;
	char* tmp=(char*)malloc(sizeof(char)*1);
	char* before;
	int len=0;
	int i=1;
	int j;
	while(*(str+i)!='\"')
	{
		if(*(str+i)=='\n'&&inter)inter->curline+=1;
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
			else if(*(str+i+1)=='\n')
			{
				i+=2;
				continue;
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
		if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))
		{
			fprintf(stderr,"Out of memory!\n");
			exit(1);
		}
		if(before!=NULL)memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
	}
	if(tmp!=NULL)*(tmp+len)='\0';
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

void errors(int types,const char* filename,int line)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n",
	};
	fprintf(stderr,"In file \"%s\" line %d\nerror:%s",filename,line,inform[types]);
	exit(1);
}

ANS_cptr* createTree(const char* objStr,intpr* inter)
{
	//if(objStr==NULL)return NULL;
	size_t i=0;
	int braketsNum=0;
	ANS_cptr* root=NULL;
	ANS_pair* objPair=NULL;
	ANS_cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			if(i!=0&&*(objStr+i-1)==')')
			{
				if(objPair!=NULL)
				{
					int curline=(inter)?inter->curline:0;
					ANS_pair* tmp=newPair(curline,objPair);
					objPair->cdr.type=PAIR;
					objPair->cdr.value=(void*)tmp;
					objPair=tmp;
					objCptr=&objPair->car;
				}
			}
			i++;
			braketsNum++;
			if(root==NULL)
			{
				int curline=(inter)?inter->curline:0;
				root=newCptr(curline,objPair);
				root->type=PAIR;
				root->value=newPair(curline,NULL);
				objPair=root->value;
				objCptr=&objPair->car;
			}
			else
			{
				int curline=(inter)?inter->curline:0;
				objPair=newPair(curline,objPair);
				objCptr->type=PAIR;
				objCptr->value=(void*)objPair;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			braketsNum--;
			ANS_pair* prev=NULL;
			if(objPair==NULL)break;
			while(objPair->prev!=NULL)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(objPair->car.value==prev)break;
			}
			if(objPair->car.value==prev)
				objCptr=&objPair->car;
			else if(objPair->cdr.value==prev)
				objCptr=&objPair->cdr;
		}
		else if(*(objStr+i)==',')
		{
			i++;
			objCptr=&objPair->cdr;
		}
		else if(isspace(*(objStr+i)))
		{
			int j=0;
			char* tmpStr=(char*)objStr+i;
			while(j>(int)-i&&isspace(*(tmpStr+j)))j--;
			if(*(tmpStr+j)==','||*(tmpStr+j)=='(')
			{
				j=1;
				while(isspace(*(tmpStr+j)))j++;
				i+=j;
				continue;
			}
			j=0;
			while(isspace(*(tmpStr+j)))
			{
				if(*(tmpStr+j)=='\n')
					if(inter)
						inter->curline+=1;
				j++;
			}
			if(*(tmpStr+j)==','||*(tmpStr+j)==')')
			{
				i+=j;
				continue;
			}
			i+=j;
			if(objPair!=NULL)
			{
				int curline=(inter)?inter->curline:0;
				ANS_pair* tmp=newPair(curline,objPair);
				objPair->cdr.type=PAIR;
				objPair->cdr.value=(void*)tmp;
				objPair=tmp;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)=='\"')
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			rawString tmp=getStringBetweenMarks(objStr+i,inter);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(STR,tmp.str,objPair);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&(/**(objStr+i)&&*/isdigit(*(objStr+i+1)))))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			ANS_atom* tmpAtm=NULL;
			if(!isNum(tmp))
				tmpAtm=newAtom(SYM,tmp,objPair);
			else if(isDouble(tmp))
			{
				tmpAtm=newAtom(DBL,NULL,objPair);
				double num=stringToDouble(tmp);
				tmpAtm->value.dbl=num;
			}
			else
			{
				tmpAtm=newAtom(INT,NULL,objPair);
				int32_t num=stringToInt(tmp);
				tmpAtm->value.num=num;
			}
			objCptr->type=ATM;
			objCptr->value=tmpAtm;
			i+=strlen(tmp);
			free(tmp);
		}
		else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='\\'))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringAfterBackslash(objStr+i+2);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(CHR,NULL,objPair);
			ANS_atom* tmpAtm=objCptr->value;
			if(tmp[0]!='\\')tmpAtm->value.chr=tmp[0];
			else tmpAtm->value.chr=stringToChar(tmp+1);
			i+=strlen(tmp)+2;
			free(tmp);
		}
		else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='b'))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringAfterBackslash(objStr+i+2);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(BYTE,NULL,objPair);
			ANS_atom* tmpAtm=objCptr->value;
			int32_t size=strlen(tmp)/2+strlen(tmp)%2;
			tmpAtm->value.byte.size=size;
			tmpAtm->value.byte.arry=castStrByteArry(tmp);
			i+=strlen(tmp)+2;
			free(tmp);
		}
		else
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(SYM,tmp,objPair);
			i+=strlen(tmp);
			free(tmp);
			continue;
		}
		if(braketsNum<=0&&root!=NULL)break;
	}
	for(;i<strlen(objStr);i++)if(isspace(objStr[i])&&objStr[i]=='\n')
		if(inter)
			inter->curline+=1;
	return root;
}

ANS_pair* newPair(int curline,ANS_pair* prev)
{
	ANS_pair* tmp;
	if((tmp=(ANS_pair*)malloc(sizeof(ANS_pair))))
	{
		tmp->car.outer=tmp;
		tmp->car.type=NIL;
		tmp->car.value=NULL;
		tmp->cdr.outer=tmp;
		tmp->cdr.type=NIL;
		tmp->cdr.value=NULL;
		tmp->prev=prev;
		tmp->car.curline=curline;
		tmp->cdr.curline=curline;
	}
	else errors(OUTOFMEMORY,__FILE__,__LINE__);
	return tmp;
}

ANS_cptr* newCptr(int curline,ANS_pair* outer)
{
	ANS_cptr* tmp=NULL;
	if(!(tmp=(ANS_cptr*)malloc(sizeof(ANS_cptr))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=NIL;
	tmp->value=NULL;
	return tmp;
}

ANS_atom* newAtom(int type,const char* value,ANS_pair* prev)
{
	ANS_atom* tmp=NULL;
	if(!(tmp=(ANS_atom*)malloc(sizeof(ANS_atom))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	switch(type)
	{
		case SYM:
		case STR:
			if(!(tmp->value.str=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY,__FILE__,__LINE__);
			strcpy(tmp->value.str,value);
			break;
		case CHR:
		case INT:
		case DBL:
			*(int32_t*)(&tmp->value)=0;break;
		case BYTE:
			tmp->value.byte.size=0;
			tmp->value.byte.arry=NULL;break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int copyCptr(ANS_cptr* objCptr,const ANS_cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	ANS_pair* objPair=NULL;
	ANS_pair* copiedPair=NULL;
	ANS_pair* tmpPair=(copiedCptr->type==PAIR)?copiedCptr->value:NULL;
	copiedPair=tmpPair;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		objCptr->curline=copiedCptr->curline;
		if(copiedCptr->type==PAIR)
		{
			objPair=newPair(0,objPair);
			objCptr->value=objPair;
			copiedPair=copiedCptr->value;
			copiedCptr=&copiedPair->car;
			copiedCptr=&copiedPair->car;
			objCptr=&objPair->car;
			continue;
		}
		else if(copiedCptr->type==ATM)
		{
			ANS_atom* coAtm=copiedCptr->value;
			ANS_atom* objAtm=NULL;
			if(coAtm->type==SYM||coAtm->type==STR)
				objAtm=newAtom(coAtm->type,coAtm->value.str,objPair);
			else if(coAtm->type==BYTE)
			{
				objAtm=newAtom(coAtm->type,NULL,objPair);
				objAtm->value.byte.size=coAtm->value.byte.size;
				objAtm->value.byte.arry=copyMemory(coAtm->value.byte.arry,coAtm->value.byte.size);
			}
			else
			{
				objAtm=newAtom(coAtm->type,NULL,objPair);
				if(objAtm->type==DBL)objAtm->value.dbl=coAtm->value.dbl;
				else if(objAtm->type==INT)objAtm->value.num=coAtm->value.num;
				else if(objAtm->type==CHR)objAtm->value.chr=coAtm->value.chr;
			}
			objCptr->value=objAtm;
			if(copiedCptr==&copiedPair->car)
			{
				copiedCptr=&copiedPair->cdr;
				objCptr=&objPair->cdr;
				continue;
			}
			
		}
		else if(copiedCptr->type==NIL)
		{
			objCptr->value=NULL;
			if(copiedCptr==&copiedPair->car)
			{
				objCptr=&objPair->cdr;
				copiedCptr=&copiedPair->cdr;
				continue;
			}
		}
		if(copiedPair!=NULL&&copiedCptr==&copiedPair->cdr)
		{
			ANS_pair* objPrev=NULL;
			ANS_pair* coPrev=NULL;
			if(copiedPair->prev==NULL)break;
			while(objPair->prev!=NULL&&copiedPair!=NULL&&copiedPair!=tmpPair)
			{
				coPrev=copiedPair;
				copiedPair=copiedPair->prev;
				objPrev=objPair;
				objPair=objPair->prev;
				if(coPrev==copiedPair->car.value)break;
			}
			if(copiedPair!=NULL)
			{
				copiedCptr=&copiedPair->cdr;
				objCptr=&objPair->cdr;
			}
			if(copiedPair==tmpPair&&copiedCptr->type==objCptr->type)break;
		}
		if(copiedPair==NULL)break;
	}
	return 1;
}
void replace(ANS_cptr* fir,const ANS_cptr* sec)
{
	ANS_pair* tmp=fir->outer;
	ANS_cptr tmpCptr={NULL,0,NIL,NULL};
	tmpCptr.type=fir->type;
	tmpCptr.value=fir->value;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==PAIR)((ANS_pair*)fir->value)->prev=tmp;
	else if(fir->type==ATM)((ANS_atom*)fir->value)->prev=tmp;
}

ANS_cptr* destroyCptr(ANS_cptr* objCptr)
{
	ANS_pair* objPair=NULL;
	if(objCptr->type==PAIR)objPair=((ANS_pair*)objCptr->value)->prev;
	if(objCptr->type==ATM)objPair=((ANS_atom*)objCptr->value)->prev;
	if(objCptr->type==NIL)return objCptr;
	while(objPair!=NULL&&objPair->prev!=NULL)objPair=objPair->prev;
	if(objPair!=NULL)
	{
		deleteCptr(&objPair->car);
		deleteCptr(&objPair->cdr);
	}
	free(objPair);
}
int deleteCptr(ANS_cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	ANS_pair* tmpPair=(objCptr->type==PAIR)?objCptr->value:NULL;
	ANS_pair* objPair=tmpPair;
	ANS_cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==PAIR)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.value;
				tmpCptr=&objPair->car;
				continue;
			}
			else
			{
				objPair=tmpCptr->value;
				tmpCptr=&objPair->car;
				continue;
			}
		}
		else if(tmpCptr->type==ATM)
		{
			freeAtom(tmpCptr->value);
			tmpCptr->type=NIL;
			tmpCptr->value=NULL;
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
				ANS_pair* prev=objPair;
				objPair=objPair->prev;
			//	printf("free PAIR\n");
				free(prev);
				if(objPair==NULL||prev==tmpPair)break;
				if(prev==objPair->car.value)
				{
					objPair->car.type=NIL;
					objPair->car.value=NULL;
				}
				else if(prev==objPair->cdr.value)
				{
					objPair->cdr.type=NIL;
					objPair->cdr.value=NULL;
				}
				tmpCptr=&objPair->cdr;
			}
		}
		if(objPair==NULL)break;
	}
	objCptr->type=NIL;
	objCptr->value=NULL;
	return 0;
}

int ANS_cptrcmp(const ANS_cptr* first,const ANS_cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	ANS_pair* firPair=NULL;
	ANS_pair* secPair=NULL;
	ANS_pair* tmpPair=(first->type==PAIR)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAIR)
		{
			firPair=first->value;
			secPair=second->value;
			first=&firPair->car;
			second=&secPair->car;
			continue;
		}
		else if(first->type==ATM||first->type==NIL)
		{
			if(first->type==ATM)
			{
				ANS_atom* firAtm=first->value;
				ANS_atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if((firAtm->type==SYM||firAtm->type==STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==INT&&firAtm->value.num!=secAtm->value.num)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTE&&!byteArryEq(&firAtm->value.byte,&secAtm->value.byte))return 0;
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
			ANS_pair* firPrev=NULL;
			ANS_pair* secPrev=NULL;
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
				secPrev=secPair;
				firPair=firPair->prev;
				secPair=secPair->prev;
				if(firPrev==firPair->car.value)break;
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

ANS_cptr* nextCptr(const ANS_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==PAIR)
		return &((ANS_pair*)objCptr->outer->cdr.value)->car;
	return NULL;
}

ANS_cptr* prevCptr(const ANS_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->prev!=NULL&&objCptr->outer->prev->cdr.value==objCptr->outer)
		return &objCptr->outer->prev->car;
	return NULL;
}

int isHexNum(const char* objStr)
{
	if(objStr!=NULL&&strlen(objStr)>3&&objStr[0]=='-'&&objStr[1]=='0'&&(objStr[2]=='x'||objStr[2]=='X'))return 1;
	if(objStr!=NULL&&strlen(objStr)>2&&objStr[0]=='0'&&(objStr[1]=='x'||objStr[1]=='X'))return 1;
	return 0;
}

int isOctNum(const char* objStr)
{
	if(objStr!=NULL&&strlen(objStr)>2&&objStr[0]=='-'&&objStr[1]=='0'&&!isalpha(objStr[2]))return 1;
	if(objStr!=NULL&&strlen(objStr)>1&&objStr[0]=='0'&&!isalpha(objStr[1]))return 1;
	return 0;
}

int isDouble(const char* objStr)
{
	int i=(objStr[0]=='-')?1:0;
	int len=strlen(objStr);
	for(;i<len;i++)
		if(objStr[i]=='.')return 1;
	return 0;
}

char stringToChar(const char* objStr)
{
	char* format=NULL;
	char ch=0;
	if(isNum(objStr))
	{
		if(isHexNum(objStr))format="%x";
		else if(isOctNum(objStr))format="%o";
		else format="%d";
		sscanf(objStr,format,&ch);
	}
	else
	{
		switch(*(objStr))
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
			default:ch=*(objStr);break;
		}
	}
	return ch;
}

int isNum(const char* objStr)
{
	if(isHexNum(objStr))return 1;
	if(!isxdigit(*objStr)&&(*objStr!='-'||*objStr!='.'))return 0;
	int len=strlen(objStr);
	int i=(*objStr=='-')?1:0;
	int hasDot=0;
	for(;i<len;i++)
	{
		hasDot+=(objStr[i]=='.')?1:0;
		if(objStr[i]=='.'&&hasDot>1)return 0;
		if(!isxdigit(objStr[i])&&objStr[i]!='.')return 0;
	}
	return 1;
}

void freeAtom(ANS_atom* objAtm)
{
	if(objAtm->type==SYM||objAtm->type==STR)free(objAtm->value.str);
	else if(objAtm->type==BYTE)
	{
		objAtm->value.byte.size=0;
		free(objAtm->value.byte.arry);
	}
	free(objAtm);
}

void printList(const ANS_cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	ANS_pair* tmpPair=(objCptr->type==PAIR)?objCptr->value:NULL;
	ANS_pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				putc(' ',out);
				objPair=objPair->cdr.value;
				objCptr=&objPair->car;
			}
			else
			{
				putc('(',out);
				objPair=objCptr->value;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr&&objCptr->type==ATM)putc(',',out);
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==NIL&&objPair->prev!=NULL)
			||(objCptr->outer==NULL&&objCptr->type==NIL))fputs("nil",out);
			if(objCptr->type!=NIL)
			{
				ANS_atom* tmpAtm=objCptr->value;
				switch(tmpAtm->type)
				{
					case SYM:
						fprintf(out,"%s",tmpAtm->value.str);
						break;
					case STR:
						printRawString(tmpAtm->value.str,out);
						break;
					case INT:
						fprintf(out,"%ld",tmpAtm->value.num);
						break;
					case DBL:
						fprintf(out,"%lf",tmpAtm->value.dbl);
						break;
					case CHR:
						printRawChar(tmpAtm->value.chr,out);
						break;
					case BYTE:
						printByteArry(&tmpAtm->value.byte,out,1);
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
			ANS_pair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.value||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
}

void exError(const ANS_cptr* obj,int type,intpr* inter)
{
	if(inter!=NULL)fprintf(stderr,"In file \"%s\",line %d\n",inter->filename,(obj==NULL)?inter->curline:obj->curline);
	if(obj!=NULL)printList(obj,stderr);
	switch(type)
	{
		case SYMUNDEFINE:fprintf(stderr,":Symbol is undefined.\n");break;
		case SYNTAXERROR:fprintf(stderr,":Syntax error.\n");break;
		case ILLEGALEXPR:fprintf(stderr,":Invalid expression here.\n");break;
		case CIRCULARLOAD:fprintf(stderr,":Circular load file.\n");break;
	}
}

void printRawChar(char chr,FILE* out)
{
	if(isgraph(chr))
		fprintf(out,"#\\%c",chr);
	else
		fprintf(out,"#\\\\0x%x",(int)chr);
}

PreEnv* newEnv(PreEnv* prev)
{
	PreEnv* curEnv=NULL;
	if(!(curEnv=(PreEnv*)malloc(sizeof(PreEnv))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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
			free(delsym->symName);
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

intpr* newIntpr(const char* filename,FILE* file,CompEnv* env)
{
	intpr* tmp=NULL;
	if(!(tmp=(intpr*)malloc(sizeof(intpr))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->filename=copyStr(filename);
	if(file!=stdin)
	{
		char* rp=realpath(filename,0);
		if(rp==NULL)
		{
			perror(rp);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=getDir(rp);
		free(rp);
	}
	else
		tmp->curDir=getcwd(NULL,0);
	tmp->file=file;
	tmp->curline=1;
	tmp->procs=NULL;
	tmp->prev=NULL;
	tmp->modules=NULL;
	tmp->head=NULL;
	tmp->tail=NULL;
	if(env)
	{
		tmp->glob=env;
		return tmp;
	}
	tmp->glob=newCompEnv(NULL);
	initCompEnv(tmp->glob);
	return tmp;
}

void freeIntpr(intpr* inter)
{
	free(inter->filename);
	if(inter->file!=stdin)
		fclose(inter->file);
	free(inter->curDir);
	destroyCompEnv(inter->glob);
	deleteAllDll(inter->modules);
	freeModlist(inter->head);
	RawProc* tmp=inter->procs;
	while(tmp!=NULL)
	{
		RawProc* prev=tmp;
		tmp=tmp->next;
		freeByteCode(prev->proc);
		free(prev);
	}
	free(inter);
}

CompEnv* newCompEnv(CompEnv* prev)
{
	CompEnv* tmp=(CompEnv*)malloc(sizeof(CompEnv));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->prev=prev;
	tmp->symbols=NULL;
	return tmp;
}

void destroyCompEnv(CompEnv* objEnv)
{
	if(objEnv==NULL)return;
	CompDef* tmpDef=objEnv->symbols;
	while(tmpDef!=NULL)
	{
		CompDef* prev=tmpDef;
		tmpDef=tmpDef->next;
		free(prev->symName);
		free(prev);
	}
	free(objEnv);
}

CompDef* findCompDef(const char* name,CompEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		CompDef* curDef=curEnv->symbols;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}

CompDef* addCompDef(CompEnv* curEnv,const char* name)
{
	if(curEnv->symbols==NULL)
	{
		CompEnv* tmpEnv=curEnv->prev;
		if(!(curEnv->symbols=(CompDef*)malloc(sizeof(CompDef))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		if(!(curEnv->symbols->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
		strcpy(curEnv->symbols->symName,name);
		while(tmpEnv!=NULL&&tmpEnv->symbols==NULL)tmpEnv=tmpEnv->prev;
		if(tmpEnv==NULL)
			curEnv->symbols->count=0;
		else
		{
			CompDef* tmpDef=tmpEnv->symbols;
			while(tmpDef->next!=NULL)tmpDef=tmpDef->next;
			curEnv->symbols->count=tmpDef->count+1;
		}
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		CompDef* curDef=findCompDef(name,curEnv);
		if(curDef==NULL)
		{
			CompDef* prevDef=curEnv->symbols;
			while(prevDef->next!=NULL)prevDef=prevDef->next;
			if(!(curDef=(CompDef*)malloc(sizeof(CompDef))))errors(OUTOFMEMORY,__FILE__,__LINE__);
			if(!(curDef->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
			strcpy(curDef->symName,name);
			prevDef->next=curDef;
			curDef->count=prevDef->count+1;
			curDef->next=NULL;
		}
		return curDef;
	}
}

RawProc* newRawProc(int32_t count)
{
	RawProc* tmp=(RawProc*)malloc(sizeof(RawProc));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->count=count;
	tmp->proc=NULL;
	tmp->next=NULL;
	return tmp;
}

RawProc* addRawProc(ByteCode* proc,intpr* inter)
{
	while(inter->prev!=NULL)inter=inter->prev;
	ByteCode* tmp=createByteCode(proc->size);
	memcpy(tmp->code,proc->code,proc->size);
	RawProc* tmpProc=newRawProc((inter->procs==NULL)?0:inter->procs->count+1);
	tmpProc->proc=tmp;
	tmpProc->next=inter->procs;
	inter->procs=tmpProc;
	return tmpProc;
}

ByteCode* createByteCode(unsigned int size)
{
	ByteCode* tmp=NULL;
	if(!(tmp=(ByteCode*)malloc(sizeof(ByteCode))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=size;
	if(!(tmp->code=(char*)malloc(size*sizeof(char))))errors(OUTOFMEMORY,__FILE__,__LINE__);
	int32_t i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
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
	fir->code=(char*)realloc(fir->code,sizeof(char)*fir->size);
	if(!fir->code)errors(OUTOFMEMORY,__FILE__,__LINE__);
	memcpy(fir->code+size,sec->code,sec->size);
}

void reCodeCat(const ByteCode* fir,ByteCode* sec)
{
	int32_t size=fir->size;
	char* tmp=(char*)malloc(sizeof(char)*(fir->size+sec->size));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

ByteCode* copyByteCode(const ByteCode* obj)
{
	ByteCode* tmp=createByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

void initCompEnv(CompEnv* curEnv)
{
	int i=0;
	for(;i<NUMOFBUILDINSYMBOL;i++)
		addCompDef(curEnv,builtInSymbolList[i]);
}

char* copyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
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

ANS_cptr* getLast(const ANS_cptr* objList)
{
	ANS_pair* objPair=objList->value;
	ANS_cptr* first=&objPair->car;
	for(;nextCptr(first)!=NULL;first=nextCptr(first));
	return first;
}

ANS_cptr* getFirst(const ANS_cptr* objList)
{
	ANS_pair* objPair=objList->value;
	ANS_cptr* first=&objPair->car;
	return first;
}

void printByteCode(const ByteCode* tmpCode,FILE* fp)
{
	int32_t i=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,codeName[tmpCode->code[i]].codeName);
		switch(codeName[tmpCode->code[i]].len)
		{
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				printAsByteArry((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen(tmpCode->code+i+1);
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
				fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
				i+=9;
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

uint8_t* castStrByteArry(const char* str)
{
	int len=strlen(str);
	int32_t size=(len%2)?(len/2+1):len/2;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	int32_t i=0;
	int k=0;
	for(;i<size;i++)
	{
		tmp[i]=castCharInt(str[k]);
		if(str[k+1]!='\0')tmp[i]+=16*castCharInt(str[k+1]);
		k+=2;
	}
	return tmp;
}

void printByteArry(const ByteArry* obj,FILE* fp,int mode)
{
	if(mode)fputs("#b",fp);
	for(int i=0;i<obj->size;i++)
	{
		uint8_t j=obj->arry[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void printAsByteArry(const uint8_t* arry,int32_t size,FILE* fp)
{
	fputs("@\\",fp);
	for(int i=0;i<size;i++)
	{
		uint8_t j=arry[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void* copyMemory(void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

int hasLoadSameFile(const char* filename,const intpr* inter)
{
	while(inter!=NULL)
	{
		if(!strcmp(inter->filename,filename))
			return 1;
		inter=inter->prev;
	}
	return 0;
}

RawProc* getHeadRawProc(const intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->procs;
}

ByteCode* newDllFuncProc(const char* name)
{
	ByteCode* callProc=createByteCode(sizeof(char)+strlen(name)+1);
	ByteCode* endProc=createByteCode(1);
	endProc->code[0]=FAKE_END_PROC;
	callProc->code[0]=FAKE_CALL_PROC;
	strcpy(callProc->code+sizeof(char),name);
	codeCat(callProc,endProc);
	freeByteCode(endProc);
	return callProc;
}

ANS_cptr* getANSPairCar(const ANS_cptr* obj)
{
	ANS_pair* tmpPair=obj->value;
	return &tmpPair->car;
}

ANS_cptr* getANSPairCdr(const ANS_cptr* obj)
{
	ANS_pair* tmpPair=obj->value;
	return &tmpPair->cdr;
}

Dlls* newDll(DllHandle handle)
{
	Dlls* tmp=(Dlls*)malloc(sizeof(Dlls));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->handle=handle;
	return tmp;
}

Dlls* loadDll(const char* rpath,Dlls** Dhead,const char* modname,Modlist** tail)
{
#ifdef _WIN32
	DllHandle handle=LoadLibrary(rpath);
#else
	DllHandle handle=dlopen(rpath,RTLD_LAZY);
#endif
	if(handle==NULL)
	{
		perror(dlerror());
		exit(EXIT_FAILURE);
	}
	Dlls* tmp=newDll(handle);
	tmp->count=(*Dhead)?(*Dhead)->count+1:0;
	tmp->next=*Dhead;
	*Dhead=tmp;
	if(modname!=NULL&&tail!=NULL)
	{
		Modlist* tmpL=newModList(modname);
		tmpL->count=(*tail)?(*tail)->count+1:0;
		tmpL->next=NULL;
		if(*tail)(*tail)->next=tmpL;
		*tail=tmpL;
	}
	return tmp;
}

void* getAddress(const char* funcname,Dlls* head)
{
	Dlls* cur=head;
	void* pfunc=NULL;
	while(cur!=NULL)
	{
#ifdef _WIN32
		pfunc=GetProcAddress(cur->handle,funcname);
#else
		pfunc=dlsym(cur->handle,funcname);
#endif
		if(pfunc!=NULL)return pfunc;
		cur=cur->next;
	}
	return NULL;
}

void deleteAllDll(Dlls* head)
{
	Dlls* cur=head;
	while(cur)
	{
		Dlls* prev=cur;
#ifdef _WIN32
		FreeLibrary(prev->handle);
#else
		dlclose(prev->handle);
#endif
		cur=cur->next;
		free(prev);
	}
}

Dlls** getHeadOfMods(intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return &inter->modules;
}

Modlist* newModList(const char* libname)
{
	Modlist* tmp=(Modlist*)malloc(sizeof(Modlist));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->name=(char*)malloc(sizeof(char)*(strlen(libname)+1));
	if(tmp->name==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	strcpy(tmp->name,libname);
	return tmp;
}

Dlls* loadAllModules(FILE* fp,Dlls** mods)
{
#ifdef _WIN32
	char* filetype=".dll";
#else
	char* filetype=".so";
#endif
	int32_t num=0;
	int i=0;
	fread(&num,sizeof(int32_t),1,fp);
	for(;i<num;i++)
	{
		char* modname=getStringFromFile(fp);
		char* realModname=(char*)malloc(sizeof(char)*(strlen(modname)+strlen(filetype)+1));
		if(realModname==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
		strcpy(realModname,modname);
		strcat(realModname,filetype);
		char* rpath=realpath(realModname,0);
		if(rpath==NULL)
		{
			perror(rpath);
			exit(EXIT_FAILURE);
		}
		loadDll(rpath,mods,NULL,NULL);
		free(modname);
		free(rpath);
		free(realModname);
	}
	return *mods;
}

void changeWorkPath(const char* filename)
{
	char* p=realpath(filename,NULL);
	char* wp=getDir(p);
	int32_t len=strlen(p);
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
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* getStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	if(tmp==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
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
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors(OUTOFMEMORY,__FILE__,__LINE__);
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

void writeAllDll(intpr* inter,FILE* fp)
{
	int num=(inter->head==NULL)?0:inter->tail->count+1;
	fwrite(&num,sizeof(int32_t),1,fp);
	int i=0;
	Modlist* cur=inter->head;
	for(;i<num;i++)
	{
		fwrite(cur->name,strlen(cur->name)+1,1,fp);
		cur=cur->next;
	}
}

void freeAllRawProc(RawProc* cur)
{
	while(cur!=NULL)
	{
		freeByteCode(cur->proc);
		RawProc* prev=cur;
		cur=cur->next;
		free(prev);
	}
}

int byteArryEq(ByteArry* fir,ByteArry* sec)
{
	if(fir->size!=sec->size)return 0;
	else return !memcmp(fir->arry,sec->arry,sec->size);
}

int ModHasLoad(const char* name,Modlist* head)
{
	while(head)
	{
		if(!strcmp(name,head->name))return 1;
		head=head->next;
	}
	return 0;
}

Dlls** getpDlls(intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->modules;
}

Modlist** getpTail(intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->tail;
}

Modlist** getpHead(intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->head;
}

char* getLastWorkDir(intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->curDir;
}

char* relpath(char* abs,char* relto)
{
#ifdef _WIN32
	char* divstr="\\";
	char* upperDir="..\\";
#else
	char* divstr="/";
	char* upperDir="../";
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
	if(lastCommonRoot==-1)
	{
		fprintf(stderr,"%s:Cant get relative path.\n",abs);
		exit(EXIT_FAILURE);
	}
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
	int i=0;
	char* pNext=NULL;
	char** strArry=(char**)malloc(0);
	if(strArry==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		strArry[count-1]=pNext;
		pNext=strtok(NULL,divstr);
	}
	*length=count;
	return strArry;
}

void freeModlist(Modlist* cur)
{
	while(cur)
	{
		Modlist* prev=cur;
		cur=cur->next;
		free(prev->name);
		free(prev);
	}
}
