#include<fakeLisp/syntax.h>
#include<fakeLisp/common.h>
#include<stdlib.h>
#include<string.h>

int fklIsPreprocess(const AST_cptr* objCptr)
{
	if(fklIsDefmacroExpression(objCptr))return 1;
	return 0;
}

int fklIsDefmacroExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"defmacro"))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsDeftypeExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"deftype"))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsDefExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->u.atom;
			AST_atom* second=fklNextCptr(objCptr)->u.atom;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"define"))return 0;
		}
		return 1;
	}
	else return 0;
}

int fklIsSetqExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->u.atom;
			AST_atom* second=fklNextCptr(objCptr)->u.atom;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"setq"))return 0;
		}
		return 1;
	}
	else return 0;
}

int fklIsGetfExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"getf"))return 1;
	return 0;
}


int fklIsSetfExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->u.atom;
			if(first->type!=SYM||strcmp(first->value.str,"setf"))return 0;
		}
		return 1;
	}
	else return 0;
}

int fklIsCondExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		if(objCptr->type==ATM)
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"cond"))
			{
				for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
					if(!fklIsValid(objCptr)||objCptr->type!=PAIR)return 0;
				return 1;
			}
		}
	}
	return 0;
}

int fklIsSymbol(const AST_cptr* objCptr)
{
	AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=SYM)return 0;
	return 1;
}

int fklIsNil(const AST_cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	return 0;
}

int fklIsConst(const AST_cptr* objCptr)
{
	if(fklIsNil(objCptr))return 1;
	AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type!=SYM)return 1;
	if(objCptr->type==PAIR&&(fklIsQuoteExpression(objCptr)||(objCptr->u.pair->car.type==NIL&&objCptr->u.pair->car.type==NIL&&fklIsValid(objCptr))))return 1;
	return 0;
}

int fklIsAndExpression(const AST_cptr* objCptr)
{
	if(!fklIsValid(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->u.atom:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"and"))return 1;
	}
	return 0;
}

int fklIsOrExpression(const AST_cptr* objCptr)
{
	if(!fklIsValid(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->u.atom:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"or"))return 1;
	}
	return 0;
}

int fklIsQuoteExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"quote")&&fklNextCptr(objCptr)&&!fklNextCptr(fklNextCptr(objCptr)))
		return 1;
	return 0;
}

int fklIsSzofExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"szof")&&fklNextCptr(objCptr)&&!fklNextCptr(fklNextCptr(objCptr)))
		return 1;
	return 0;
}

int fklIsUnquoteExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"unquote"))return 1;
	return 0;
}

int fklIsQsquoteExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"qsquote"))return 1;
	return 0;
}

int fklIsUnqtespExpression(const AST_cptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"unqtesp"))return 1;
	return 0;
}

int fklIsLambdaExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"lambda"))return 1;
	return 0;
}

int fklIsBeginExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"begin"))return 1;
	return 0;
}

int fklIsFuncCall(const AST_cptr* objCptr,CompEnv* curEnv)
{
	if(objCptr->type==PAIR&&fklIsValid(objCptr)&&!fklHasKeyWord(objCptr,curEnv)&&!fklIsNil(objCptr))return 1;
	return 0;
}

int fklIsValid(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		objCptr=&objCptr->u.pair->car;
		const AST_cptr* fklPrevCptr=NULL;
		while(objCptr!=NULL)
		{
			fklPrevCptr=objCptr;
			objCptr=fklNextCptr(objCptr);
		}
		if(fklPrevCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

int fklIsLoadExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&objCptr->u.pair->car;
		AST_cptr* sec=fklNextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=ATM||sec->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->u.atom;
		AST_atom* secAtm=sec->u.atom;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"load")&&(secAtm->type==SYM||secAtm->type==STR))
			return 1;
	}
	return 0;
}

int fklIsImportExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&objCptr->u.pair->car;
		AST_cptr* sec=fklNextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->u.atom;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"import"))
			return 1;
	}
	return 0;
}

int fklIsLibraryExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&objCptr->u.pair->car;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->u.atom;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"library"))
			return 1;
	}
	return 0;
}

int fklIsExportExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&objCptr->u.pair->car;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->u.atom;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"export"))
			return 1;
	}
	return 0;
}

int fklIsPrognExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=fklGetFirstCptr(objCptr);
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->u.atom;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"progn"))
			return 1;
	}
	return 0;
}

KeyWord* fklHasKeyWord(const AST_cptr* objCptr,CompEnv* curEnv)
{
	AST_atom* tmpAtm=NULL;
	if(objCptr->type==ATM&&(tmpAtm=objCptr->u.atom)->type==SYM)
	{
		for(;curEnv;curEnv=curEnv->prev)
		{
			KeyWord* tmp=curEnv->keyWords;
			while(tmp!=NULL&&strcmp(tmpAtm->value.str,tmp->word))
				tmp=tmp->next;
			return tmp;
		}
	}
	else if(objCptr->type==PAIR)
	{
		KeyWord* tmp=NULL;
		for(objCptr=&objCptr->u.pair->car;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
		{
			CompEnv* tmpEnv=curEnv;
			tmpAtm=(objCptr->type==ATM)?objCptr->u.atom:NULL;
			AST_atom* cdrAtm=(objCptr->outer->cdr.type==ATM)?objCptr->outer->cdr.u.atom:NULL;
			if((tmpAtm==NULL||tmpAtm->type!=SYM)&&(cdrAtm==NULL||cdrAtm->type!=SYM))
			{
				tmp=NULL;
				continue;
			}
			for(;tmpEnv;tmpEnv=tmpEnv->prev)
			{
				tmp=tmpEnv->keyWords;
				while(tmp!=NULL&&tmpAtm!=NULL&&strcmp(tmpAtm->value.str,tmp->word)&&(cdrAtm==NULL||cdrAtm->type!=SYM||(cdrAtm!=NULL&&strcmp(cdrAtm->value.str,tmp->word))))
					tmp=tmp->next;
				if(tmp!=NULL)return tmp;
			}
		}
		return tmp;
	}
	return NULL;
}

void fklPrintAllKeyWord(KeyWord* head)
{
	KeyWord* tmp=head;
	while(tmp!=NULL)
	{
		puts(tmp->word);
		tmp=tmp->next;
	}
}

void fklAddKeyWord(const char* objStr,CompEnv* curEnv)
{
	if(objStr!=NULL)
	{
		KeyWord* current=curEnv->keyWords;
		KeyWord* prev=NULL;
		while(current!=NULL&&strcmp(current->word,objStr))
		{
			prev=current;
			current=current->next;
		}
		if(current==NULL)
		{
			current=(KeyWord*)malloc(sizeof(KeyWord));
			current->word=(char*)malloc(sizeof(char)*(strlen(objStr)+1));
			FAKE_ASSERT(current&&current->word,"fklAddKeyWord",__FILE__,__LINE__);
			strcpy(current->word,objStr);
			if(prev!=NULL)prev->next=current;
			else
				curEnv->keyWords=current;
			current->next=NULL;
		}
	}
}

int fklIsKeyWord(const char* str,CompEnv* curEnv)
{
	for(;curEnv;curEnv=curEnv->prev)
	{
		KeyWord* cur=curEnv->keyWords;
		for(;cur;cur=cur->next)
			if(!strcmp(str,cur->word))
				return 1;
	}
	return 0;
}

int fklIsAnyExpression(const char* str,const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,str))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsTryExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"try"))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsCatchExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||fklNextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"catch"))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsFlsymExpression(const AST_cptr* objCptr)
{
	return fklIsAnyExpression("flsym",objCptr);
}
