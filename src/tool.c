#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include"tool.h"

char* builtInSymbolList[]=
{
	"nil",
	"else",
	"cons",
	"car",
	"cdr",
	"atom",
	"null",
	"ischr",
	"isint",
	"isdbl",
	"isstr",
	"issym",
	"isprc",
	"eq",
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
	"add",
	"sub",
	"mul",
	"div",
	"mod",
	"gchstr",
	"strlen",
	"strcat",
	"open",
	"close",
	"getc",
	"ungetc",
	"read",
	"write",
	"tell",
	"seek",
	"rewind",
};

char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	int ch;
	int i=0;
	int j;
	int mark=0;
	int braketsNum=0;
	int anotherChar=0;
	while((ch=getc(file))!=EOF)
	{
		mark^=(ch=='\"'&&(tmp==NULL||*(tmp+j)!='\\'));
		if(ch==';'&&!mark&&(tmp==NULL||*(tmp+j)!='\\'))
		{
			while(getc(file)!='\n');
			ungetc('\n',file);
			continue;
		}
		if(ch==')'&&!mark&&(tmp==NULL||*(tmp+j)!='\\'))
		{
			if(braketsNum<=0)continue;
			else braketsNum--;
		}
		if(ch=='\''&&!mark&&(tmp==NULL||*(tmp+j)!='\\'))
		{
			anotherChar=1;
			int numOfSpace=0;
			int lenOfMemory=1;
			char* stringOfSpace=(char*)malloc(sizeof(char)*lenOfMemory);
			if(stringOfSpace==NULL)errors(OUTOFMEMORY);
			stringOfSpace[numOfSpace]='\0';
			char* before=NULL;
			while(isspace((ch=getc(file))))
			{
				numOfSpace++;
				lenOfMemory++;
				before=stringOfSpace;
				if(!(stringOfSpace=(char*)malloc(sizeof(char)*lenOfMemory)))errors(OUTOFMEMORY);
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
			if(!(other=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			strcpy(other,beQuote);
			strcat(other,stringOfSpace);
			strcat(other,tmpList);
			other[len-1]=')';
			i+=len;
			j=i-len;
			before=tmp;
			if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors(OUTOFMEMORY);
			memcpy(tmp,before,j);
			memcpy(tmp+j,other,len);
			if(before!=NULL)free(before);
			free(other);
			free(tmpList);
			continue;
		}
		if(isspace(ch)&&braketsNum<=0&&!mark&&anotherChar)
		{
			ungetc(ch,file);
			break;
		}
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
		if(ch=='('&&!mark&&(tmp==NULL||*(tmp+j-1)!='\\'))braketsNum++;
		if(!isspace(ch))anotherChar=1;
	}
	if(ch==EOF&&braketsNum!=0&&file!=stdin)
	{
		free(tmp);
		return NULL;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}

char* subGetList(FILE* file)
{
	char* tmp=NULL;
	char* before;
	int ch;
	int i=0;
	int j;
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
		if(ch==','&&braketsNum<=0&&!mark&&(tmp==NULL||(*(tmp+j-1)!='\\')))
		{
			ungetc(ch,file);
			break;
		}
		if(isspace(ch)&&braketsNum<=0&&anotherChar){ungetc(ch,file);break;}
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		mark^=(ch=='\"'&&*(tmp+j-1)!='\\');
		if(before!=NULL)free(before);
		if(ch=='('&&!mark&&(tmp==NULL||*(tmp+j-1)!='\\'))braketsNum++;
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
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors(OUTOFMEMORY);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* getStringAfterBackslash(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while(!isspace(*(str+len))&&*(str+len)!='\0')len++;
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors(OUTOFMEMORY);
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
	int i;
	char numString[sizeof(long)*2+3];
	for(i=0;i<sizeof(long)*2+3;i++)numString[i]=0;
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	if(!(tmp=(char*)malloc(lenOfNum*sizeof(char))))errors(OUTOFMEMORY);
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
	char* tmp=NULL;
	char* before;
	int len=0;
	int i=1;
	int j;
	while(*(str+i)!='\"')
	{
		if(*(str+i)=='\n')inter->curline+=1;
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

cptr* createTree(const char* objStr,intpr* inter)
{
	//if(objStr==NULL)return NULL;
	int i=0;
	int braketsNum=0;
	cptr* root=NULL;
	pair* objPair=NULL;
	cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			i++;
			braketsNum++;
			if(root==NULL)
			{
				root=newCptr(inter->curline,objPair);
				root->type=PAR;
				root->value=newPair(inter->curline,NULL);
				objPair=root->value;
				objCptr=&objPair->car;
			}
			else
			{
				objPair=newPair(inter->curline,objPair);
				objCptr->type=PAR;
				objCptr->value=(void*)objPair;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			braketsNum--;
			pair* prev=NULL;
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
			while(isspace(*(tmpStr+j)))j--;
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
				if(*(tmpStr+j)=='\n')inter->curline+=1;
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
				pair* tmp=newPair(inter->curline,objPair);
				objPair->cdr.type=PAR;
				objPair->cdr.value=(void*)tmp;
				objPair=tmp;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)=='\"')
		{
			if(root==NULL)objCptr=root=newCptr(inter->curline,objPair);
			rawString tmp=getStringBetweenMarks(objStr+i,inter);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(STR,tmp.str,objPair);
			i+=tmp.len;
			free(tmp.str);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&isdigit(*(objStr+i+1))))
		{
			if(root==NULL)objCptr=root=newCptr(inter->curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			atom* tmpAtm=NULL;
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
		else if(*(objStr+i)=='#'&&*(objStr+1+i)=='\\')
		{
			if(root==NULL)objCptr=root=newCptr(inter->curline,objPair);
			char* tmp=getStringAfterBackslash(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(CHR,NULL,objPair);
			atom* tmpAtm=objCptr->value;
			if(tmp[2]!='\\')tmpAtm->value.chr=tmp[2];
			else tmpAtm->value.chr=stringToChar(&tmp[3]);
			i+=strlen(tmp);
			free(tmp);
		}
		else
		{
			if(root==NULL)objCptr=root=newCptr(inter->curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(SYM,tmp,objPair);
			i+=strlen(tmp);
			free(tmp);
			continue;
		}
		if(braketsNum<=0&&root!=NULL)break;
	}
	return root;
}

pair* newPair(int curline,pair* prev)
{
	pair* tmp;
	if((tmp=(pair*)malloc(sizeof(pair))))
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
	else errors(OUTOFMEMORY);
	return tmp;
}

cptr* newCptr(int curline,pair* outer)
{
	cptr* tmp=NULL;
	if(!(tmp=(cptr*)malloc(sizeof(cptr))))errors(OUTOFMEMORY);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=NIL;
	tmp->value=NULL;
	return tmp;
}

atom* newAtom(int type,const char* value,pair* prev)
{
	atom* tmp=NULL;
	if(!(tmp=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
	switch(type)
	{
		case SYM:
		case STR:
			if(!(tmp->value.str=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY);
			strcpy(tmp->value.str,value);
			break;
		case CHR:
		case INT:
		case DBL:
			*(int32_t*)(&tmp->value)=0;break;
	}		
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int copyCptr(cptr* objCptr,const cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	pair* objPair=NULL;
	pair* copiedPair=NULL;
	pair* tmpPair=(copiedCptr->type==PAR)?copiedCptr->value:NULL;
	copiedPair=tmpPair;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		if(copiedCptr->type==PAR)
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
			atom* coAtm=copiedCptr->value;
			atom* objAtm=NULL;
			if(coAtm->type==SYM||coAtm->type==STR)
				objAtm=newAtom(coAtm->type,coAtm->value.str,objPair);
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
			pair* objPrev=NULL;
			pair* coPrev=NULL;
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
void replace(cptr* fir,const cptr* sec)
{
	pair* tmp=fir->outer;
	cptr tmpCptr={NULL,0,NIL,NULL};
	tmpCptr.type=fir->type;
	tmpCptr.value=fir->value;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==PAR)((pair*)fir->value)->prev=tmp;
	else if(fir->type==ATM)((atom*)fir->value)->prev=tmp;
}

cptr* destroyCptr(cptr* objCptr)
{
	pair* objPair=NULL;
	if(objCptr->type==PAR)objPair=((pair*)objCptr->value)->prev;
	if(objCptr->type==ATM)objPair=((atom*)objCptr->value)->prev;
	if(objCptr->type==NIL)return objCptr;
	while(objPair!=NULL&&objPair->prev!=NULL)objPair=objPair->prev;
	if(objPair!=NULL)
	{
		deleteCptr(&objPair->car);
		deleteCptr(&objPair->cdr);
	}
	free(objPair);
}
int deleteCptr(cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	pair* tmpPair=(objCptr->type==PAR)?objCptr->value:NULL;
	pair* objPair=tmpPair;
	cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==PAR)
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
				pair* prev=objPair;
				objPair=objPair->prev;
			//	printf("free PAR\n");
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

int cptrcmp(const cptr* first,const cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	pair* firPair=NULL;
	pair* secPair=NULL;
	pair* tmpPair=(first->type==PAR)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAR)
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
				atom* firAtm=first->value;
				atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if((firAtm->type==SYM||firAtm->type==STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(!memcmp(&firAtm->type,&secAtm->type,sizeof(atom)-sizeof(pair*)))return 0;
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
			pair* firPrev=NULL;
			pair* secPrev=NULL;
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

cptr* nextCptr(const cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==PAR)
		return &((pair*)objCptr->outer->cdr.value)->car;
	return NULL;
}

cptr* prevCptr(const cptr* objCptr)
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

void freeAtom(atom* objAtm)
{
	if(objAtm->type==SYM||objAtm->type==STR)free(objAtm->value.str);
	free(objAtm);
}

void printList(const cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	pair* tmpPair=(objCptr->type==PAR)?objCptr->value:NULL;
	pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAR)
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
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==NIL&&objPair->cdr.type!=NIL)
			||(objCptr->outer==NULL&&objCptr->type==NIL))fputs("nil",out);
			if(objCptr->type!=NIL)
			{
				atom* tmpAtm=objCptr->value;
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
			pair* prev=NULL;
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

void exError(const cptr* obj,int type,intpr* inter)
{
	if(inter!=NULL)printf("In file \"%s\",line %d\n",inter->filename,(obj==NULL)?inter->curline:obj->curline);
	printList(obj,stdout);
	switch(type)
	{
		case SYMUNDEFINE:printf(":Symbol is undefined.\n");break;
		case SYNTAXERROR:printf(":Syntax error.\n");break;
	}
}

void printRawChar(char chr,FILE* out)
{
	if(isgraph(chr))
		fprintf(out,"#\\%c",chr);
	else
		fprintf(out,"#\\\\0x%x",(int)chr);
}

env* newEnv(env* prev)
{
	env* curEnv=NULL;
	if(!(curEnv=(env*)malloc(sizeof(env))))errors(OUTOFMEMORY);
	if(prev!=NULL)prev->next=curEnv;
	curEnv->prev=prev;
	curEnv->next=NULL;
	curEnv->symbols=NULL;
	return curEnv;
}

void destroyEnv(env* objEnv)
{
	if(objEnv==NULL)return;
	while(objEnv!=NULL)
	{
		defines* delsym=objEnv->symbols;
		while(delsym!=NULL)
		{
			free(delsym->symName);
			deleteCptr(&delsym->obj);
			defines* prev=delsym;
			delsym=delsym->next;
			free(prev);
		}
		env* prev=objEnv;
		objEnv=objEnv->next;
		free(prev);
	}
}

intpr* newIntpr(const char* filename,FILE* file)
{
	intpr* tmp=NULL;
	if(!(tmp=(intpr*)malloc(sizeof(intpr))))errors(OUTOFMEMORY);
	tmp->filename=copyStr(filename);
	tmp->file=file;
	tmp->curline=1;
	tmp->glob=newCompEnv(NULL);
	initCompEnv(tmp->glob);
	tmp->procs=NULL;
	return tmp;
}

void freeIntpr(intpr* inter)
{
	free(inter->filename);
	fclose(inter->file);
	destroyCompEnv(inter->glob);
	rawproc* tmp=inter->procs;
	while(tmp!=NULL)
	{
		rawproc* prev=tmp;
		tmp=tmp->next;
		freeByteCode(prev->proc);
		free(prev);
	}
	free(inter);
}

compEnv* newCompEnv(compEnv* prev)
{
	compEnv* tmp=(compEnv*)malloc(sizeof(compEnv));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->symbols=NULL;
	return tmp;
}

void destroyCompEnv(compEnv* objEnv)
{
	compDef* tmpDef=objEnv->symbols;
	while(tmpDef!=NULL)
	{
		compDef* prev=tmpDef;
		tmpDef=tmpDef->next;
		free(prev->symName);
		free(prev);
	}
	free(objEnv);
}

compDef* findCompDef(const char* name,compEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		compDef* curDef=curEnv->symbols;
		compDef* prev=NULL;
		while(curDef&&strcmp(name,curDef->symName))
			curDef=curDef->next;
		return curDef;
	}
}

compDef* addCompDef(compEnv* curEnv,const char* name)
{
	if(curEnv->symbols==NULL)
	{
		compEnv* tmpEnv=curEnv->prev;
		if(!(curEnv->symbols=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
		if(!(curEnv->symbols->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
		strcpy(curEnv->symbols->symName,name);
		while(tmpEnv!=NULL&&tmpEnv->symbols==NULL)tmpEnv=tmpEnv->prev;
		if(tmpEnv==NULL)
		{
			curEnv->symbols->count=0;
		}
		else
		{
			compDef* tmpDef=tmpEnv->symbols;
			while(tmpDef->next!=NULL)tmpDef=tmpDef->next;
			curEnv->symbols->count=tmpDef->count+1;
		}
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		compDef* curDef=findCompDef(name,curEnv);
		if(curDef==NULL)
		{
			compDef* prevDef=curEnv->symbols;
			while(prevDef->next!=NULL)prevDef=prevDef->next;
			if(!(curDef=(compDef*)malloc(sizeof(compDef))))errors(OUTOFMEMORY);
			if(!(curDef->symName=(char*)malloc(sizeof(char)*(strlen(name)+1))))errors(OUTOFMEMORY);
			strcpy(curDef->symName,name);
			prevDef->next=curDef;
			curDef->count=prevDef->count+1;
			curDef->next=NULL;
		}
		return curDef;
	}
}

rawproc* newRawProc(int32_t count)
{
	rawproc* tmp=(rawproc*)malloc(sizeof(rawproc));
	if(tmp==NULL)errors(OUTOFMEMORY);
	tmp->count=count;
	tmp->proc=NULL;
	tmp->next=NULL;
	return tmp;
}

rawproc* addRawProc(byteCode* proc,intpr* inter)
{
	byteCode* tmp=createByteCode(proc->size);
	memcpy(tmp->code,proc->code,proc->size);
	rawproc* tmpProc=newRawProc((inter->procs==NULL)?0:inter->procs->count+1);
	tmpProc->proc=tmp;
	tmpProc->next=inter->procs;
	inter->procs=tmpProc;
	return tmpProc;
}

byteCode* createByteCode(unsigned int size)
{
	byteCode* tmp=NULL;
	if(!(tmp=(byteCode*)malloc(sizeof(byteCode))))errors(OUTOFMEMORY);
	tmp->size=size;
	if(!(tmp->code=(char*)malloc(size*sizeof(char))))errors(OUTOFMEMORY);
	int i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
}

void freeByteCode(byteCode* obj)
{
	free(obj->code);
	free(obj);
}

byteCode* codeCat(const byteCode* fir,const byteCode* sec)
{
	byteCode* tmp=createByteCode(fir->size+sec->size);
	memcpy(tmp->code,fir->code,fir->size);
	memcpy(tmp->code+fir->size,sec->code,sec->size);
	return tmp;
}

byteCode* copyByteCode(const byteCode* obj)
{
	byteCode* tmp=createByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

void initCompEnv(compEnv* curEnv)
{
	int i;
	for(i=0;i<41;i++)addCompDef(curEnv,builtInSymbolList[i]);
}

char* copyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	if(tmp==NULL)errors(OUTOFMEMORY);
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
