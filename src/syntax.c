#include<fakeLisp/syntax.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<string.h>
#include<fakeLisp/basicADT.h>

int fklIsPreprocess(const FklAstCptr* objCptr)
{
	if(fklIsDefmacroExpression(objCptr))return 1;
	return 0;
}

int fklIsDefmacroExpression(const FklAstCptr* objCptr)
{
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=FKL_ATM||fklNextCptr(objCptr)->type==FKL_NIL)return 0;
		FklAstAtom* tmpAtm=objCptr->u.atom;
		if(tmpAtm->type!=FKL_SYM||fklStringCstrCmp(tmpAtm->value.str,"defmacro"))return 0;
		return 1;
	}
	return 0;
}

int fklIsDefExpression(const FklAstCptr* objCptr)
{
	FklAstCptr* fir=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(fir&&fklIsListCptr(objCptr)&&fklLengthListCptr(objCptr)==3)
	{
		if(fir->type!=FKL_ATM||fklNextCptr(fir)->type!=FKL_ATM)return 0;
		FklAstAtom* first=fir->u.atom;
		FklAstAtom* second=fklNextCptr(fir)->u.atom;
		if(first->type!=FKL_SYM||second->type!=FKL_SYM||fklStringCstrCmp(first->value.str,"define"))return 0;
		return 1;
	}
	else return 0;
}

int fklIsSetqExpression(const FklAstCptr* objCptr)
{
	FklAstCptr* fir=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(fir&&fklIsListCptr(objCptr)&&fklLengthListCptr(objCptr)==3)
	{
		if(fir->type!=FKL_ATM||fklNextCptr(fir)->type!=FKL_ATM)return 0;
		FklAstAtom* first=fir->u.atom;
		FklAstAtom* second=fklNextCptr(fir)->u.atom;
		if(first->type!=FKL_SYM||second->type!=FKL_SYM||fklStringCstrCmp(first->value.str,"setq"))return 0;
		return 1;
	}
	else return 0;
}

//int fklIsSetfExpression(const FklAstCptr* objCptr)
//{
//	FklAstCptr* fir=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
//	if(fir&&fklIsListCptr(objCptr)&&fklLengthListCptr(objCptr)==3)
//	{
//		if(fir->type!=FKL_ATM)
//			return 0;
//		FklAstAtom* first=fir->u.atom;
//		if(first->type!=FKL_SYM||fklStringCstrCmp(first->value.str,"setf"))return 0;
//		return 1;
//	}
//	else return 0;
//}

int fklIsCondExpression(const FklAstCptr* objCptr)
{
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		if(objCptr->type==FKL_ATM)
		{
			FklAstAtom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"cond"))
			{
				for(objCptr=fklNextCptr(objCptr);objCptr!=NULL;objCptr=fklNextCptr(objCptr))
					if(!fklIsValid(objCptr)||objCptr->type!=FKL_PAIR)return 0;
				return 1;
			}
		}
	}
	return 0;
}

int fklIsSymbol(const FklAstCptr* objCptr)
{
	FklAstAtom* tmpAtm=(objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=FKL_SYM)return 0;
	return 1;
}

int fklIsNil(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_NIL)return 1;
	return 0;
}

int fklIsConst(const FklAstCptr* objCptr)
{
	if(fklIsNil(objCptr))return 1;
	FklAstAtom* tmpAtm=(objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type!=FKL_SYM)return 1;
	if(objCptr->type==FKL_PAIR&&(fklIsQuoteExpression(objCptr)||(objCptr->u.pair->car.type==FKL_NIL&&objCptr->u.pair->car.type==FKL_NIL&&fklIsValid(objCptr))))return 1;
	return 0;
}

int fklIsAndExpression(const FklAstCptr* objCptr)
{
	if(!fklIsValid(objCptr))return 0;
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		FklAstAtom* tmpAtm=(objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"and"))return 1;
	}
	return 0;
}

int fklIsOrExpression(const FklAstCptr* objCptr)
{
	if(!fklIsValid(objCptr))return 0;
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL)
	{
		FklAstAtom* tmpAtm=(objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"or"))return 1;
	}
	return 0;
}

int fklIsQuoteExpression(const FklAstCptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"quote")&&fklNextCptr(objCptr)&&!fklNextCptr(fklNextCptr(objCptr)))
		return 1;
	return 0;
}

int fklIsMacroExpandExpression(const FklAstCptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"macroexpand")&&fklNextCptr(objCptr)&&!fklNextCptr(fklNextCptr(objCptr)))
		return 1;
	return 0;
}

int fklIsUnquoteExpression(const FklAstCptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"unquote"))return 1;
	return 0;
}

int fklIsQsquoteExpression(const FklAstCptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"qsquote"))return 1;
	return 0;
}

int fklIsUnqtespExpression(const FklAstCptr* objCptr)
{
	objCptr=(fklIsValid(objCptr)&&objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"unqtesp"))return 1;
	return 0;
}

int fklIsLambdaExpression(const FklAstCptr* objCptr)
{
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"lambda"))return 1;
	return 0;
}

int fklIsBeginExpression(const FklAstCptr* objCptr)
{
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	FklAstAtom* tmpAtm=(objCptr!=NULL&&objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==FKL_SYM&&!fklStringCstrCmp(tmpAtm->value.str,"begin"))return 1;
	return 0;
}

int fklIsFuncCall(const FklAstCptr* objCptr,FklCompEnv* curEnv)
{
	if(objCptr->type==FKL_PAIR&&fklIsValid(objCptr)&&!fklHasKeyWord(objCptr,curEnv)&&!fklIsNil(objCptr))return 1;
	return 0;
}

int fklIsValid(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_PAIR)
	{
		objCptr=&objCptr->u.pair->car;
		const FklAstCptr* fklPrevCptr=NULL;
		while(objCptr!=NULL)
		{
			fklPrevCptr=objCptr;
			objCptr=fklNextCptr(objCptr);
		}
		if(fklPrevCptr->outer->cdr.type!=FKL_NIL)return 0;
	}
	return 1;
}

int fklIsLoadExpression(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* fir=&objCptr->u.pair->car;
		FklAstCptr* sec=fklNextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=FKL_ATM||sec->type!=FKL_ATM)
			return 0;
		FklAstAtom* firAtm=fir->u.atom;
		FklAstAtom* secAtm=sec->u.atom;
		if(firAtm->type==FKL_SYM&&!fklStringCstrCmp(firAtm->value.str,"load")&&(secAtm->type==FKL_SYM||secAtm->type==FKL_STR))
			return 1;
	}
	return 0;
}

int fklIsImportExpression(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* fir=&objCptr->u.pair->car;
		FklAstCptr* sec=fklNextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=FKL_ATM)
			return 0;
		FklAstAtom* firAtm=fir->u.atom;
		if(firAtm->type==FKL_SYM&&!fklStringCstrCmp(firAtm->value.str,"import"))
			return 1;
	}
	return 0;
}

int fklIsLibraryExpression(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* fir=&objCptr->u.pair->car;
		if(fir->type!=FKL_ATM)
			return 0;
		FklAstAtom* firAtm=fir->u.atom;
		if(firAtm->type==FKL_SYM&&!fklStringCstrCmp(firAtm->value.str,"library"))
			return 1;
	}
	return 0;
}

int fklIsExportExpression(const FklAstCptr* objCptr)
{
	if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* fir=&objCptr->u.pair->car;
		if(fir->type!=FKL_ATM)
			return 0;
		FklAstAtom* firAtm=fir->u.atom;
		if(firAtm->type==FKL_SYM&&!fklStringCstrCmp(firAtm->value.str,"export"))
			return 1;
	}
	return 0;
}

FklKeyWord* fklHasKeyWord(const FklAstCptr* objCptr,FklCompEnv* curEnv)
{
	FklAstAtom* tmpAtm=NULL;
	if(objCptr->type==FKL_ATM&&(tmpAtm=objCptr->u.atom)->type==FKL_SYM)
	{
		for(;curEnv;curEnv=curEnv->prev)
		{
			FklKeyWord* tmp=curEnv->keyWords;
			while(tmp!=NULL&&fklStringcmp(tmpAtm->value.str,tmp->word))
				tmp=tmp->next;
			return tmp;
		}
	}
	else if(objCptr->type==FKL_PAIR)
	{
		FklKeyWord* tmp=NULL;
		for(objCptr=&objCptr->u.pair->car;objCptr!=NULL;objCptr=fklNextCptr(objCptr))
		{
			FklCompEnv* tmpEnv=curEnv;
			tmpAtm=(objCptr->type==FKL_ATM)?objCptr->u.atom:NULL;
			FklAstAtom* cdrAtm=(objCptr->outer->cdr.type==FKL_ATM)?objCptr->outer->cdr.u.atom:NULL;
			if((tmpAtm==NULL||tmpAtm->type!=FKL_SYM)&&(cdrAtm==NULL||cdrAtm->type!=FKL_SYM))
			{
				tmp=NULL;
				continue;
			}
			for(;tmpEnv;tmpEnv=tmpEnv->prev)
			{
				tmp=tmpEnv->keyWords;
				while(tmp!=NULL&&tmpAtm!=NULL&&fklStringcmp(tmpAtm->value.str,tmp->word)&&(cdrAtm==NULL||cdrAtm->type!=FKL_SYM||(cdrAtm!=NULL&&fklStringcmp(cdrAtm->value.str,tmp->word))))
					tmp=tmp->next;
				if(tmp!=NULL)return tmp;
			}
		}
		return tmp;
	}
	return NULL;
}

//void fklPrintAllKeyWord(FklKeyWord* head)
//{
//	FklKeyWord* tmp=head;
//	while(tmp!=NULL)
//	{
//		puts(tmp->word);
//		tmp=tmp->next;
//	}
//}

void fklAddKeyWord(const FklString* objStr,FklCompEnv* curEnv)
{
	if(objStr!=NULL)
	{
		FklKeyWord* current=curEnv->keyWords;
		FklKeyWord* prev=NULL;
		while(current!=NULL&&fklStringcmp(current->word,objStr))
		{
			prev=current;
			current=current->next;
		}
		if(current==NULL)
		{
			current=(FklKeyWord*)malloc(sizeof(FklKeyWord));
			current->word=fklCopyString(objStr);
			if(prev!=NULL)prev->next=current;
			else
				curEnv->keyWords=current;
			current->next=NULL;
		}
	}
}

void fklAddKeyWordCstr(const char* objStr,FklCompEnv* curEnv)
{
	if(objStr!=NULL)
	{
		FklKeyWord* current=curEnv->keyWords;
		FklKeyWord* prev=NULL;
		while(current!=NULL&&fklStringCstrCmp(current->word,objStr))
		{
			prev=current;
			current=current->next;
		}
		if(current==NULL)
		{
			current=(FklKeyWord*)malloc(sizeof(FklKeyWord));
			current->word=fklNewStringFromCstr(objStr);
			if(prev!=NULL)prev->next=current;
			else
				curEnv->keyWords=current;
			current->next=NULL;
		}
	}
}

int fklIsKeyWord(const FklString* str,FklCompEnv* curEnv)
{
	for(;curEnv;curEnv=curEnv->prev)
	{
		FklKeyWord* cur=curEnv->keyWords;
		for(;cur;cur=cur->next)
			if(!fklStringcmp(str,cur->word))
				return 1;
	}
	return 0;
}

//int fklIsAnyExpression(const char* str,const FklAstCptr* objCptr)
//{
//	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
//	if(objCptr!=NULL)
//	{
//		if(objCptr->type!=FKL_ATM)return 0;
//		else
//		{
//			FklAstAtom* tmpAtm=objCptr->u.atom;
//			if(tmpAtm->type!=FKL_SYM||fklStringCstrCmp(tmpAtm->value.str,str))return 0;
//		}
//		return 1;
//	}
//	return 0;
//}

int fklIsTryExpression(const FklAstCptr* objCptr)
{
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=FKL_ATM||fklNextCptr(objCptr)->type==FKL_NIL)return 0;
		else
		{
			FklAstAtom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=FKL_SYM||fklStringCstrCmp(tmpAtm->value.str,"try"))return 0;
		}
		return 1;
	}
	return 0;
}

int fklIsCatchExpression(const FklAstCptr* objCptr)
{
	if(!fklIsValid(objCptr))return 0;
	objCptr=(objCptr->type==FKL_PAIR)?&objCptr->u.pair->car:NULL;
	if(objCptr!=NULL&&fklNextCptr(objCptr)!=NULL&&fklNextCptr(fklNextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=FKL_ATM||fklNextCptr(objCptr)->type==FKL_NIL)return 0;
		else
		{
			FklAstAtom* tmpAtm=objCptr->u.atom;
			if(tmpAtm->type!=FKL_SYM||fklStringCstrCmp(tmpAtm->value.str,"catch"))return 0;
		}
		return 1;
	}
	return 0;
}
