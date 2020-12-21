#include"ast.h"
#include"tool.h"
#include<string.h>
#include<ctype.h>

AST_cptr* createTree(const char* objStr,intpr* inter)
{
	//if(objStr==NULL)return NULL;
	size_t i=0;
	int braketsNum=0;
	AST_cptr* root=NULL;
	AST_pair* objPair=NULL;
	AST_cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			if(i!=0&&*(objStr+i-1)==')')
			{
				if(objPair!=NULL)
				{
					int curline=(inter)?inter->curline:0;
					AST_pair* tmp=newPair(curline,objPair);
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
			AST_pair* prev=NULL;
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
				AST_pair* tmp=newPair(curline,objPair);
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
			AST_atom* tmpAtm=NULL;
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
				tmpAtm=newAtom(IN32,NULL,objPair);
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
			AST_atom* tmpAtm=objCptr->value;
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
			objCptr->value=(void*)newAtom(BYTA,NULL,objPair);
			AST_atom* tmpAtm=objCptr->value;
			int32_t size=strlen(tmp)/2+strlen(tmp)%2;
			tmpAtm->value.byta.size=size;
			tmpAtm->value.byta.arry=castStrByteArry(tmp);
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

AST_cptr* castVMvalueToCptr(VMvalue* value,int32_t curline,AST_pair* prev)
{
	ValueType cptrType=0;
	if(value->type==NIL)
		cptrType=NIL;
	else if(value->type==PAIR)
		cptrType=PAIR;
	else
		cptrType=ATM;
	AST_cptr* tmp=newCptr(cptrType,NULL);
	if(cptrType==ATM)
	{
		AST_atom* tmpAtm=newAtom(value->type,NULL,prev);
		switch(value->type)
		{
			case SYM:
			case STR:
				tmpAtm->value.str=copyStr(value->u.str->str);
				break;
			case IN32:
				tmpAtm->value.num=*value->u.num;
				break;
			case DBL:
				tmpAtm->value.dbl=*value->u.dbl;
				break;
			case CHR:
				tmpAtm->value.chr=*value->u.chr;
				break;
			case BYTA:
				tmpAtm->value.byta.size=value->u.byta->size;
				tmpAtm->value.byta.arry=copyMemory(value->u.byta->arry,value->u.byta->size);
				break;
		}
		tmp->value=tmpAtm;
	}
	else
	{
		AST_pair* tmpPair=newPair(curline,prev);
		AST_cptr* astCar=castVMvalueToCptr(value->u.pair->car,curline,tmpPair);
		AST_cptr* astCdr=castVMvalueToCptr(value->u.pair->cdr,curline,tmpPair);
		tmpPair->car.outer=tmpPair;
		tmpPair->car.value=astCar->value;
		tmpPair->car.type=astCar->type;
		tmpPair->cdr.outer=tmpPair;
		tmpPair->cdr.value=astCdr->value;
		tmpPair->cdr.type=astCdr->type;
		free(astCdr);
		free(astCar);
		tmp->value=tmpPair;
	}
	return tmp;
}
