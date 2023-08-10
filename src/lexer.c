#include<fakeLisp/lexer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<ctype.h>
#include<string.h>

static inline int isCompleteStringBuf(const char* buf,size_t size,char ch,size_t* len)
{
	int mark=0;
	int markN=0;
	size_t i=0;
	for(;i<size&&markN<2
			;i+=(buf[i]=='\\')+1)
		if(buf[i]==ch)
		{
			mark=~mark;
			markN+=1;
		}
	if(len)
		*len=mark?0:i;
	return !mark;
}

static inline FklNastNode* getPartFromState(FklStringMatchState* state,size_t index)
{
	return state->pattern->parts->u.vec->base[index];
}

static inline FklNastNode* getPartFromPattern(FklStringMatchPattern* pattern,size_t index)
{
	return pattern->parts->u.vec->base[index];
}

static inline size_t getSizeFromPattern(FklStringMatchPattern* p)
{
	return p->parts->u.vec->size;
}

static FklStringMatchState* createStringMatchState(FklStringMatchPattern* pattern
		,size_t index
		,FklStringMatchState* next)
{
	FklStringMatchState* r=(FklStringMatchState*)malloc(sizeof(FklStringMatchState));
	FKL_ASSERT(r);
	r->next=next;
	r->index=index;
	r->pattern=pattern;
	return r;
}

static void addStateIntoSyms(FklStringMatchState** syms,FklStringMatchState* state)
{
	state->next=*syms;
	*syms=state;
}

static void addStateIntoStrs(FklStringMatchState** strs,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index)->u.str->size;
	for(;*strs;strs=&(*strs)->next)
	{
		size_t curlen=getPartFromState((*strs),(*strs)->index)->u.str->size;
		if(len>=curlen)
		{
			state->next=*strs;
			*strs=state;
			break;
		}
	}
	if(!*strs)
	{
		state->next=*strs;
		*strs=state;
	}
}

static void addStateIntoBoxes(FklStringMatchState** boxes,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index+1)->u.str->size;
	for(;*boxes;boxes=&(*boxes)->next)
	{
		size_t curlen=getPartFromState((*boxes),(*boxes)->index+1)->u.str->size;
		if(len>=curlen)
		{
			state->next=*boxes;
			*boxes=state;
			break;
		}
	}
	if(!*boxes)
	{
		state->next=*boxes;
		*boxes=state;
	}
}

static void addStateIntoSet(FklStringMatchState* state
		,FklStringMatchState** strs
		,FklStringMatchState** boxes
		,FklStringMatchState** syms)
{
	FklNastNode* part=getPartFromState(state,state->index);
	switch(part->type)
	{
		case FKL_NAST_STR:
			addStateIntoStrs(strs,state);
			break;
		case FKL_NAST_BOX:
			addStateIntoBoxes(boxes,state);
			break;
		case FKL_NAST_SYM:
			addStateIntoSyms(syms,state);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

static FklStringMatchSet* createStringMatchSet(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms
		,FklStringMatchSet* prev)
{
	FklStringMatchSet* r=(FklStringMatchSet*)malloc(sizeof(FklStringMatchSet));
	FKL_ASSERT(r);
	r->str=strs;
	r->box=boxes;
	r->sym=syms;
	r->prev=prev;
	return r;
}

static inline int charbufEqual(const char* buf,size_t n,const char* s,size_t l)
{
	return n>=l&&!memcmp(s,buf,l);
}

static inline int simpleMatch(const char* buf,size_t n,FklString* s)
{
	return n>=s->size&&!memcmp(s->str,buf,s->size);
}

static inline int matchWithStr(size_t pn,const char* buf,size_t n,FklString* s)
{
	return (pn==0||pn==s->size)&&simpleMatch(buf,n,s);
}

static int matchStrInBoxes(const char* buf
		,size_t n
		,FklStringMatchState* boxes)
{
	for(;boxes;boxes=boxes->next)
	{
		FklString* s=getPartFromState(boxes,boxes->index+1)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static int matchStrInStrs(const char* buf
		,size_t n
		,FklStringMatchState* strs)
{
	for(;strs;strs=strs->next)
	{
		FklString* s=getPartFromState(strs,strs->index)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static int matchStrInPatterns(const char* buf
		,size_t n
		,FklStringMatchPattern* patterns)
{
	for(;patterns;patterns=patterns->next)
	{
		FklString* s=getPartFromPattern(patterns,0)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

#define DEFAULT_TOKEN_HEADER_STR       (0)
#define DEFAULT_TOKEN_HEADER_SYM       (1)
#define DEFAULT_TOKEN_HEADER_COMMENT_0 (2)
#define DEFAULT_TOKEN_HEADER_COMMENT_1 (3)
#define DEFAULT_TOKEN_HEADER_CHR       (4)

static const struct DefaultTokenHeader
{
	size_t l;
	const char* s;
}
DefaultTokenHeaders[]=
{
	{1,"\"",},
	{1,"|",},
	{1,";",},
	{2,"#!",},
	{2,"#\\",},
	{0,NULL,},
};

static inline int isComment(const char* buf,size_t n)
{
	return charbufEqual(buf,n
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].s
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].l)
		||charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].l);
}

static inline int matchStrInDefaultHeaders(const char* buf,size_t n)
{
	for(const struct DefaultTokenHeader* cur=&DefaultTokenHeaders[0];cur->l;cur++)
	{
		size_t l=cur->l;
		const char* s=cur->s;
		if(charbufEqual(buf,n,s,l))
			return 1;
	}
	return 0;
}

static size_t getDefaultSymbolLen(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,int* p)
{
	int hasStrs=0;
	size_t i=0;
	for(;i<n
			&&!isspace(buf[i])
			&&!(hasStrs=matchStrInStrs(&buf[i],n-i,strs))
			&&!matchStrInBoxes(&buf[i],n-i,boxes)
			&&!matchStrInPatterns(&buf[i],n-i,patterns)
			&&!matchStrInDefaultHeaders(&buf[i],n-i)
			;i++);
	if(p)
		*p=hasStrs;
	return i;
}

static FklToken* createDefaultToken(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,size_t line
		,size_t* jInc
		,int* err)
{
	if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].l))
	{
		size_t len=getDefaultSymbolLen(&buf[3],n-3,strs,boxes,patterns,NULL)+3;
		*jInc=len;
		return fklCreateToken(FKL_TOKEN_CHAR,fklCreateString(len,buf),line);
	}
	else if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].l))
	{
		size_t len=0;
		int complete=isCompleteStringBuf(buf,n,'"',&len);
		if(complete)
		{
			*jInc=len;
			size_t size=0;
			char* s=fklCastEscapeCharBuf(&buf[1],len-2,&size);
			FklString* str=fklCreateString(size,s);
			free(s);
			return fklCreateToken(FKL_TOKEN_STRING,str,line);
		}
		else
			return FKL_INCOMPLETED_TOKEN;
	}
	else
	{
		int shouldBeSymbol=0;
		size_t i=0;
		FklUintStack lenStack=FKL_STACK_INIT;
		fklInitUintStack(&lenStack,2,4);
		int hasStrs=0;
		while(i<n)
		{
			size_t len=0;
			if(charbufEqual(&buf[i],n-i
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
			{
				shouldBeSymbol=1;
				int complete=isCompleteStringBuf(&buf[i],n-i,'|',&len);
				if(!complete)
				{
					fklUninitUintStack(&lenStack);
					return FKL_INCOMPLETED_TOKEN;
				}
			}
			else
				len=getDefaultSymbolLen(&buf[i],n-i,strs,boxes,patterns,&hasStrs);
			*err=hasStrs&&!len&&!lenStack.top;
			if(!len)
				break;
			i+=len;
			fklPushUintStack(len,&lenStack);
		}
		FklToken* t=NULL;
		if(i)
		{
			i=0;
			FklString* str=fklCreateEmptyString();
			while(!fklIsUintStackEmpty(&lenStack))
			{
				uint64_t len=fklPopUintStack(&lenStack);
				if(charbufEqual(&buf[i],n-i
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
				{
					size_t size=0;
					char* s=fklCastEscapeCharBuf(&buf[i+1],len-2,&size);
					fklStringCharBufCat(&str,s,size);
					free(s);
				}
				else
					fklStringCharBufCat(&str,&buf[i],len);
				i+=len;
			}
			*jInc=i;
			t=fklCreateToken(FKL_TOKEN_SYMBOL,str,line);
			if(!shouldBeSymbol&&fklIsNumberString(t->value))
				t->type=FKL_TOKEN_NUM;
		}
		fklUninitUintStack(&lenStack);
		return t;
	}
}

static FklStringMatchSet* matchInUniversSet(const char* buf
		,size_t n
		,FklStringMatchPattern* pattern
		,FklToken** pt
		,size_t line
		,FklStringMatchSet* prev
		,size_t* jInc
		,int* err)
{
	FklStringMatchSet* r=NULL;
	size_t pn=0;
	FklStringMatchState* strs=NULL;
	FklStringMatchState* boxes=NULL;
	FklStringMatchState* syms=NULL;
	for(FklStringMatchPattern* p=pattern;p;p=p->next)
	{
		FklString* s=p->parts->u.vec->base[0]->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			FklStringMatchState* state=createStringMatchState(p,1,NULL);
			addStateIntoSet(state,&strs,&boxes,&syms);
		}
	}
	if(strs||boxes||syms)
		r=createStringMatchSet(strs,boxes,syms,prev);
	if(!*pt)
	{
		*pt=createDefaultToken(buf
				,n
				,prev?prev->str:NULL
				,prev?prev->box:NULL
				,pattern
				,line
				,jInc
				,err);
	}
	else
		*jInc=(*pt)->value->size;
	return r;
}

static int updateStrState(const char* buf
		,size_t n
		,FklStringMatchState** strs
		,FklStringMatchSet* prev
		,FklToken** pt
		,size_t line
		,FklStringMatchRouteNode* route
		,size_t top)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=strs;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index++;
			r=1;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	if(!r)
	{
		prev->str=*strs;
		*strs=NULL;
	}
	return r;
}

static int updateBoxState(const char* buf
		,size_t n
		,FklStringMatchState** boxes
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line
		,FklStringMatchRouteNode* route
		,size_t top)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=boxes;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index+1)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index+=2;
			r=1;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	if(!r)
	{
		prev->box=*boxes;
		*boxes=NULL;
	}
	return r;
}

static void updateSymState(const char* buf
		,size_t n
		,FklStringMatchState** syms
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line)
{
	for(FklStringMatchState** pstate=syms;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		cur->index++;
		if(cur->index<getSizeFromPattern(cur->pattern))
		{
			*pstate=cur->next;
			addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
		}
		else
			pstate=&(*pstate)->next;
	}
}

static FklStringMatchState* getRollBack(FklStringMatchState** pcur,FklStringMatchState* prev)
{
	FklStringMatchState* r=NULL;
	while(*pcur!=prev)
	{
		FklStringMatchState* cur=*pcur;
		*pcur=cur->next;
		cur->next=r;
		cur->index--;
		r=cur;
	}
	return r;
}

static void rollBack(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState** syms
		,FklStringMatchSet* oset)
{
	FklStringMatchState* ostrs=getRollBack(&oset->str,strs);
	FklStringMatchState* oboxes=getRollBack(&oset->box,boxes);
	for(FklStringMatchState* cur=*syms;cur;cur=cur->next)
		cur->index--;
	FklStringMatchState** psym=&oset->sym;
	for(;*psym;psym=&(*psym)->next)
		(*psym)->index--;
	*psym=*syms;
	*syms=NULL;
	for(FklStringMatchState* cur=ostrs;cur;)
	{
		FklStringMatchState* t=cur->next;
		addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
		cur=t;
	}
	for(FklStringMatchState* cur=oboxes;cur;)
	{
		FklStringMatchState* t=cur->next;
		addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
		cur=t;
	}
}

static inline int isValidToken(FklToken* t)
{
	return t&&t!=FKL_INCOMPLETED_TOKEN;
}

static inline FklStringMatchPattern* getCompleteMatchPattern(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms)
{
	for(FklStringMatchState* cur=strs;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	for(FklStringMatchState* cur=boxes;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	for(FklStringMatchState* cur=syms;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	return NULL;
}

static FklStringMatchSet* updatePreviusSet(FklStringMatchSet* set
		,const char* buf
		,size_t n
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line
		,size_t* jInc
		,FklStringMatchRouteNode** proute
		,size_t top
		,int* err)
{
	FklStringMatchRouteNode* route=*proute;
	if(isComment(buf,n))
	{
		size_t len=0;
		for(;len<n&&buf[len]!='\n';len++);
		*pt=fklCreateToken(FKL_TOKEN_COMMENT,fklCreateString(len,buf),line);
		*jInc=len;
		return set;
	}
	FklStringMatchSet* r=NULL;
	if(set==FKL_STRING_PATTERN_UNIVERSAL_SET)
	{
		r=matchInUniversSet(buf
				,n
				,patterns
				,pt
				,line
				,NULL
				,jInc
				,err);
		if(!isValidToken(*pt))
		{
			route->start=top;
			route->end=top;
			return set;
		}
	}
	else
	{
		FklStringMatchState* strs=set->str;
		FklStringMatchState* boxes=set->box;
		FklStringMatchState* syms=set->sym;
		FklStringMatchState* ostrs=set->str;
		FklStringMatchState* oboxes=set->box;
		set->str=NULL;
		set->box=NULL;
		set->sym=NULL;
		int b=!updateStrState(buf,n,&strs,set,pt,line,route,top)
			&&!updateBoxState(buf,n,&boxes,set,patterns,pt,line,route,top);
		if(b)
			updateSymState(buf,n,&syms,set,patterns,pt,line);
		FklStringMatchSet* nset=NULL;
		FklStringMatchSet* oset=set;
		while(set&&set->str==NULL&&set->box==NULL&&set->sym==NULL)
			set=set->prev;
		if(b)
		{
			nset=matchInUniversSet(buf,n
					,patterns
					,pt
					,line
					,set
					,jInc
					,err);
			if(isValidToken(*pt))
			{
				FklStringMatchRouteNode* node=fklCreateStringMatchRouteNode(NULL
						,top
						,top
						,NULL
						,NULL
						,NULL);
				fklInsertMatchRouteNodeAsFirstChild(route,node);
				if(nset)
				{
					*proute=node;
					nset->prev=oset;
				}
			}
			else
			{
				rollBack(ostrs,oboxes,&syms,oset);
				set=oset;
			}
		}
		else
			*jInc=(*pt)->value->size;
		route->pattern=getCompleteMatchPattern(strs,boxes,syms);
		if(!nset&&isValidToken(*pt))
		{
			set=oset;
			while(set&&set->str==NULL&&set->box==NULL&&set->sym==NULL)
			{
				route->end=top;
				route=route->parent;
				FklStringMatchSet* oset=set;
				set=set->prev;
				free(oset);
			}
			*proute=route;
		}
		fklDestroyStringMatchState(strs);
		fklDestroyStringMatchState(boxes);
		fklDestroyStringMatchState(syms);
		r=(nset==NULL)?set:nset;
	}
	return r;
}

FklStringMatchSet* fklSplitStringIntoTokenWithPattern(const char* buf
		,size_t size
		,size_t line
		,size_t* pline
		,size_t j
		,size_t* pj
		,FklPtrStack* retvalStack
		,FklStringMatchSet* matchSet
		,FklStringMatchPattern* patterns
		,FklStringMatchRouteNode* route
		,FklStringMatchRouteNode** proute
		,int* err)
{
	while(j<size&&matchSet)
	{
		FklToken* token=NULL;
		size_t jInc=0;
		matchSet=updatePreviusSet(matchSet
				,&buf[j]
				,size-j
				,patterns
				,&token
				,line
				,&jInc
				,&route
				,retvalStack->top
				,err);
		if(*err)
			return matchSet;
		if(token==FKL_INCOMPLETED_TOKEN)
			break;
		else if(token==NULL)
		{
			if(buf[j]=='\n')
				line++;
			j++;
		}
		else
		{
			fklPushPtrStack(token,retvalStack);
			line+=fklCountCharInBuf(&buf[j],jInc,'\n');
			j+=jInc;
		}
	}
	*pline=line;
	*pj=j;
	*proute=route;
	return matchSet;
}

FklToken* fklCreateTokenCopyStr(FklTokenType type,const FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=fklCopyString(str);
	return token;
}

FklToken* fklCreateToken(FklTokenType type,FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=str;
	return token;
}

void fklDestroyToken(FklToken* token)
{
	free(token->value);
	free(token);
}

void fklPrintToken(FklPtrStack* tokenStack,FILE* fp)
{
	static const char* tokenTypeName[]=
	{
		"reserve-str",
		"char",
		"num",
		"string",
		"symbol",
		"comment",
	};
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		fprintf(fp,"%lu,%s:",token->line,tokenTypeName[token->type]);
		fklPrintRawString(token->value,fp);
		putc('\n',fp);
	}
}

int fklIsAllComment(FklPtrStack* tokenStack)
{
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		if(token->type!=FKL_TOKEN_COMMENT)
			return 0;
	}
	return 1;
}


// lalr1
// =====

typedef struct
{
	FklSid_t left;
	FklGrammerProduction* prods;
}ProdHashItem;

static void prod_hash_set_key(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

static uintptr_t prod_hash_func(const void* k)
{
	return *(const FklSid_t*)k;
}

static int prod_hash_equal(const void* k0,const void* k1)
{
	return *(const FklSid_t*)k0==*(const FklSid_t*)k1;
}

static void prod_hash_set_val(void* d0,const void* d1)
{
	*(ProdHashItem*)d0=*(const ProdHashItem*)d1;
}

static inline void destroy_prod(FklGrammerProduction* h)
{

	// if(!h->isBuiltin)
	// 	fklDestroyByteCodelnt(h->action.proc);
	free(h->syms);
	free(h);
}

static void prod_hash_uninit(void* d)
{
	ProdHashItem* prod=(ProdHashItem*)d;
	FklGrammerProduction* h=prod->prods;
	while(h)
	{
		FklGrammerProduction* next=h->next;
		destroy_prod(h);
		h=next;
	}
	prod->prods=NULL;
}

static const FklHashTableMetaTable ProdHashMetaTable=
{
	.size=sizeof(ProdHashItem),
	.__setKey=prod_hash_set_key,
	.__setVal=prod_hash_set_val,
	.__hashFunc=prod_hash_func,
	.__uninitItem=prod_hash_uninit,
	.__keyEqual=prod_hash_equal,
	.__getKey=fklHashDefaultGetKey,
};

static inline FklGrammerProduction* create_empty_production(FklSid_t left,size_t len)
{
	FklGrammerProduction* r=(FklGrammerProduction*)calloc(1,sizeof(FklGrammerProduction));
	FKL_ASSERT(r);
	r->left=left;
	r->len=len;
	r->syms=(FklGrammerSym*)calloc(len,sizeof(FklGrammerSym));
	FKL_ASSERT(r->syms);
	return r;
}

static inline FklGrammerProduction* create_grammer_prod_from_cstr(const char* str,FklSymbolTable* symbolTable,FklSymbolTable* termTable)
{
	const char* ss;
	size_t len;
	for(ss=str;isspace(*ss);ss++);
	for(len=0;!isspace(ss[len]);len++);
	FklSid_t left=fklAddSymbolCharBuf(ss,len,symbolTable)->id;
	ss+=len;
	len=0;
	FklPtrStack st=FKL_STACK_INIT;
	fklInitPtrStack(&st,8,8);
	while(*ss)
	{
		for(;isspace(*ss);ss++);
		for(len=0;ss[len]&&!isspace(ss[len]);len++);
		fklPushPtrStack(fklCreateString(len,ss),&st);
		ss+=len;
	}
	size_t prod_len=st.top;
	FklGrammerProduction* prod=create_empty_production(left,prod_len);
	for(uint32_t i=0;i<st.top;i++)
	{
		FklGrammerSym* u=&prod->syms[i];
		FklString* s=st.base[i];
		switch(*(s->str))
		{
			case '#':
				u->isterm=1;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,termTable)->id;
				break;
			case '&':
				u->isterm=0;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,symbolTable)->id;
				break;
		}
		free(s);
	}
	fklUninitPtrStack(&st);
	return prod;
}

static inline FklHashTable* create_prod_hash_table(void)
{
	return fklCreateHashTable(&ProdHashMetaTable);
}

static inline int prod_sym_equal(const FklGrammerSym* u0,const FklGrammerSym* u1)
{
	return u0->isterm==u1->isterm
		&&u0->nt==u1->nt
		&&u0->nodel==u1->nodel
		&&u0->space==u1->space
		&&u0->repeat==u1->repeat;
}

static inline int prod_equal(const FklGrammerProduction* prod0,const FklGrammerProduction* prod1)
{
	if(prod0->len!=prod1->len)
		return 0;
	size_t len=prod0->len;
	FklGrammerSym* u0=prod0->syms;
	FklGrammerSym* u1=prod1->syms;
	for(size_t i=0;i<len;i++)
		if(!prod_sym_equal(&u0[i],&u1[i]))
			return 0;
	return 1;
}

static inline FklGrammerProduction* create_extra_production(FklSid_t start)
{
	FklGrammerProduction* prod=create_empty_production(0,1);
	prod->idx=0;
	FklGrammerSym* u=&prod->syms[0];
	u->isterm=0;
	u->nt=start;
	return prod;
}

static inline void add_prod_to_grammer(FklGrammer* grammer,FklGrammerProduction* prod)
{
	FklSid_t left=prod->left;
	FklHashTable* productions=grammer->productions;
	ProdHashItem* item=fklGetHashItem(&left,productions);
	if(item)
	{
		FklGrammerProduction** pp=&item->prods;
		FklGrammerProduction* cur=NULL;
		for(;*pp;pp=&((*pp)->next),cur=*pp)
		{
			if(prod_equal(*pp,prod))
				break;
		}
		if(cur)
		{
			// prod->next=cur->next;
			// *pp=prod;
			// destroy_prod(cur);
			destroy_prod(prod);
		}
		else
		{
			prod->idx=grammer->prodNum;
			grammer->prodNum++;
			prod->next=NULL;
			*pp=prod;
		}
	}
	else
	{
		if(!grammer->start)
		{
			grammer->start=left;
			FklGrammerProduction* extra_prod=create_extra_production(left);
			fklPutHashItem(&extra_prod->left,grammer->nonterminals);
			extra_prod->next=NULL;
			item=fklPutHashItem(&extra_prod->left,productions);
			item->prods=extra_prod;
			item->prods->idx=grammer->prodNum;
			grammer->prodNum++;
		}
		fklPutHashItem(&left,grammer->nonterminals);
		prod->next=NULL;
		item=fklPutHashItem(&left,productions);
		prod->idx=grammer->prodNum;
		item->prods=prod;
		grammer->prodNum++;
	}
}

static inline FklGrammer* create_empty_lalr1_grammer(void)
{
	FklGrammer* r=(FklGrammer*)calloc(1,sizeof(FklGrammer));
	FKL_ASSERT(r);
	r->terminals=fklCreateSymbolTable();
	r->nonterminals=fklCreateSidSet();
	r->productions=create_prod_hash_table();
	return r;
}

FklGrammer* fklCreateGrammerFromCstr(const char* str[],FklSymbolTable* st)
{
	FklGrammer* grammer=create_empty_lalr1_grammer();
	FklSymbolTable* tt=grammer->terminals;
	for(;*str;str++)
	{
		FklGrammerProduction* prod=create_grammer_prod_from_cstr(*str,st,tt);
		add_prod_to_grammer(grammer,prod);
	}
	return grammer;
}

static inline void print_prod_sym(FILE* fp,const FklGrammerSym* u,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(u->isterm)
	{
		putc('#',fp);
		fklPrintString(fklGetSymbolWithId(u->nt,tt)->symbol,fp);
	}
	else
	{
		putc('&',fp);
		fklPrintString(fklGetSymbolWithId(u->nt,st)->symbol,fp);
	}
}

static inline void print_string_as_dot(const uint8_t* str,char se,size_t size,FILE* out)
{
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
			else if(str[i]=='"')
				fputs("\\\"",out);
			else if(str[i]=='\'')
				fputs("\\'",out);
			else if(str[i]=='\\')
				fputs("\\\\",out);
			else if(isgraph(str[i]))
				putc(str[i],out);
			else if(fklIsSpecialCharAndPrint(str[i],out));
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
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],out);
			i+=l;
		}
	}
}
static inline void print_prod_sym_as_dot(FILE* fp,const FklGrammerSym* u,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(u->isterm)
	{
		const FklString* str=fklGetSymbolWithId(u->nt,tt)->symbol;
		fputs("\\\'",fp);
		print_string_as_dot((const uint8_t*)str->str,'"',str->size,fp);
		fputs("\\\'",fp);
	}
	else
	{
		const FklString* str=fklGetSymbolWithId(u->nt,st)->symbol;
		fputc('|',fp);
		print_string_as_dot((const uint8_t*)str->str,'|',str->size,fp);
		fputc('|',fp);
	}
}

static void lalr_item_set_key(void* d0,const void* d1)
{
	*(FklLalrItem*)d0=*(const FklLalrItem*)d1;
}

static inline int lalr_look_ahead_equal(const FklLalrItemLookAhead* la0,const FklLalrItemLookAhead* la1)
{
	if(la0->t!=la1->t)
		return 0;
	switch(la0->t)
	{
		case FKL_LALR_LOOKAHEAD_NONE:
		case FKL_LALR_LOOKAHEAD_EOF:
			return 1;
			break;
		case FKL_LALR_LOOKAHEAD_STRING:
		case FKL_LALR_LOOKAHEAD_BUILTIN:
			return la0->u.ptr==la1->u.ptr;
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

static int lalr_item_equal(const void* d0,const void* d1)
{
	const FklLalrItem* i0=(const FklLalrItem*)d0;
	const FklLalrItem* i1=(const FklLalrItem*)d1;
	return i0->prod==i1->prod
		&&i0->idx==i1->idx
		&&lalr_look_ahead_equal(&i0->la,&i1->la);
}

static inline uintptr_t lalr_look_ahead_hash_func(const FklLalrItemLookAhead* la)
{
	return (((uintptr_t)la->t)<<1)
		+((uintptr_t)la->u.ptr);
}

static uintptr_t lalr_item_hash_func(const void* d)
{
	const FklLalrItem* i=(const FklLalrItem*)d;
	return (uintptr_t)i->prod+i->idx+lalr_look_ahead_hash_func(&i->la);
}

static const FklHashTableMetaTable LalrItemHashMetaTable=
{
	.size=sizeof(FklLalrItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=lalr_item_set_key,
	.__setVal=lalr_item_set_key,
	.__hashFunc=lalr_item_hash_func,
	.__keyEqual=lalr_item_equal,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline FklHashTable* create_empty_item_set(void)
{
	return fklCreateHashTable(&LalrItemHashMetaTable);
}

static inline FklGrammerSym* get_item_next(const FklLalrItem* item)
{
	if(item->idx>=item->prod->len)
		return NULL;
	return &item->prod->syms[item->idx];
}

static inline FklLalrItem lalr_item_init(FklGrammerProduction* prod,size_t idx,const FklLalrItemLookAhead* la)
{
	FklLalrItem item=
	{
		.prod=prod,
		.idx=idx,
	};
	if(la)
		item.la=*la;
	else
		item.la=FKL_LALR_LOOKAHEAD_NONE_INIT;
	return item;
}

static inline FklLalrItem lalr_item_advance(const FklLalrItem* i)
{
	FklLalrItem item=
	{
		.prod=i->prod,
		.idx=i->idx+1,
		.la=i->la,
	};
	return item;
}

static inline int lalr_look_ahead_cmp(const FklLalrItemLookAhead* la0,const FklLalrItemLookAhead* la1)
{
	if(la0->t==la1->t)
	{
		switch(la0->t)
		{
			case FKL_LALR_LOOKAHEAD_NONE:
			case FKL_LALR_LOOKAHEAD_EOF:
				return 0;
				break;
			case FKL_LALR_LOOKAHEAD_BUILTIN:
				{
					uintptr_t f0=(uintptr_t)la0->u.func;
					uintptr_t f1=(uintptr_t)la1->u.func;
					if(f0>f1)
						return 1;
					else if(f0<f1)
						return -1;
					else
						return 0;
				}
				break;
			case FKL_LALR_LOOKAHEAD_STRING:
				return fklStringCmp(la0->u.s,la1->u.s);
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
	}
	else
	{
		int t0=la0->t;
		int t1=la1->t;
		return t0>t1?1:-1;
	}
}

static inline int lalr_item_cmp(const FklLalrItem* i0,const FklLalrItem* i1)
{
	FklGrammerProduction* p0=i0->prod;
	FklGrammerProduction* p1=i1->prod;
	if(p0==p1)
	{
		if(i0->idx<i1->idx)
			return -1;
		else if(i0->idx>i1->idx)
			return 1;
		else
			return lalr_look_ahead_cmp(&i0->la,&i1->la);
	}
	else if(p0->left<p1->left)
		return -1;
	else if(p0->left>p1->left)
		return 1;
	else if(p0->len>p1->len)
		return -1;
	else if(p0->len<p1->len)
		return 1;
	else
	{
		size_t len=p0->len;
		FklGrammerSym* syms0=p0->syms;
		FklGrammerSym* syms1=p1->syms;
		for(size_t i=0;i<len;i++)
		{
			FklGrammerSym* s0=&syms0[i];
			FklGrammerSym* s1=&syms1[i];
			if(s0->isterm>s1->isterm)
				return -1;
			else if(s0->isterm<s1->isterm)
				return 1;
			else if(s0->nt<s1->nt)
				return -1;
			else if(s0->nt>s1->nt)
				return 1;
			else
			{
				unsigned int f0=(s0->nodel<<2)+(s0->space<<1)+s0->repeat;
				unsigned int f1=(s1->nodel<<2)+(s1->space<<1)+s1->repeat;
				if(f0<f1)
					return -1;
				else if(f0>f1)
					return 1;
			}
		}
		return 0;
	}
}

static int lalr_item_qsort_cmp(const void* i0,const void* i1)
{
	return lalr_item_cmp((const FklLalrItem*)i0,(const FklLalrItem*)i1);
}

static inline void lalr_item_set_sort(FklHashTable* itemSet)
{
	size_t num=itemSet->num;
	FklLalrItem* item_array=(FklLalrItem*)malloc(sizeof(FklLalrItem)*num);
	FKL_ASSERT(item_array);

	size_t i=0;
	for(FklHashTableNodeList* l=itemSet->list;l;l=l->next,i++)
	{
		FklLalrItem* item=(FklLalrItem*)l->node->data;
		item_array[i]=*item;
	}
	qsort(item_array,num,sizeof(FklLalrItem),lalr_item_qsort_cmp);
	fklClearHashTable(itemSet);
	for(i=0;i<num;i++)
		fklPutHashItem(&item_array[i],itemSet);
	free(item_array);
}

static inline void lr0_item_set_closure_and_sort(FklHashTable* itemSet,FklGrammer* g)
{
	int change;
	FklHashTable* sidSet=fklCreateSidSet();
	FklHashTable* changeSet=fklCreateSidSet();
	do
	{
		change=0;
		for(FklHashTableNodeList* l=itemSet->list;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->node->data;
			FklGrammerSym* sym=get_item_next(i);
			if(sym&&!sym->isterm)
			{
				FklSid_t left=sym->nt;
				if(!fklGetHashItem(&left,sidSet))
				{
					change=1;
					fklPutHashItem(&left,sidSet);
					fklPutHashItem(&left,changeSet);
				}
			}
		}

		for(FklHashTableNodeList* lefts=changeSet->list;lefts;lefts=lefts->next)
		{
			FklSid_t left=*((FklSid_t*)lefts->node->data);
			FklGrammerProduction* prod=fklGetGrammerProductions(g,left);
			for(;prod;prod=prod->next)
			{
				FklLalrItem item=lalr_item_init(prod,0,NULL);
				fklPutHashItem(&item,itemSet);
			}
		}
		fklClearHashTable(changeSet);
	}while(change);
	lalr_item_set_sort(itemSet);
	fklDestroyHashTable(sidSet);
	fklDestroyHashTable(changeSet);
}

static inline FklHashTable* create_first_item_set(FklGrammerProduction* prod)
{
	FklLalrItem item=lalr_item_init(prod,0,NULL);
	FklHashTable* itemSet=create_empty_item_set();
	fklPutHashItem(&item,itemSet);
	return itemSet;
}

static inline void print_look_ahead(FILE* fp,const FklLalrItemLookAhead* la)
{
	switch(la->t)
	{
		case FKL_LALR_LOOKAHEAD_STRING:
			putc('#',fp);
			fklPrintString(la->u.s,fp);
			break;
		case FKL_LALR_LOOKAHEAD_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_LOOKAHEAD_BUILTIN:
			fputs("&%",fp);
			break;
		case FKL_LALR_LOOKAHEAD_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_item(FILE* fp
		,const FklLalrItem* item
		,const FklSymbolTable* st
		,const FklSymbolTable* tt)
{
	size_t i=0;
	size_t idx=item->idx;
	FklGrammerProduction* prod=item->prod;
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	if(prod->left)
		fklPrintString(fklGetSymbolWithId(prod->left,st)->symbol,fp);
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	for(;i<idx;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	fputs(" *",fp);
	for(;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	fputs(" , ",fp);
	print_look_ahead(fp,&item->la);
}

void fklPrintItemSet(const FklHashTable* itemSet,const FklGrammer* g,const FklSymbolTable* st,FILE* fp)
{
	FklSymbolTable* tt=g->terminals;
	FklLalrItem* curItem=NULL;
	for(FklHashTableNodeList* list=itemSet->list;list;list=list->next)
	{
		FklLalrItem* item=(FklLalrItem*)list->node->data;
		if(!curItem||item->idx!=curItem->idx
				||item->prod!=curItem->prod)
		{
			if(curItem)
				putc('\n',fp);
			curItem=item;
			print_item(fp,curItem,st,tt);
		}
		else
		{
			fputs(" / ",fp);
			print_look_ahead(fp,&item->la);
		}
	}
	putc('\n',fp);
}

static void hash_grammer_sym_set_key(void* s0,const void* s1)
{
	*(FklGrammerSym*)s0=*(const FklGrammerSym*)s1;
}

static uintptr_t hash_grammer_sym_hash_func(const void* d)
{
	const FklGrammerSym* s=d;
	return (s->isterm<<4)+(s->nt<<3)+(s->nodel<<2)+(s->space<<1)+s->repeat;
}

static inline int grammer_sym_equal(const FklGrammerSym* s0,const FklGrammerSym* s1)
{
	return s0->isterm==s1->isterm
		&&s0->nt==s1->nt
		&&s0->nodel==s1->nodel
		&&s0->space==s1->space
		&&s0->repeat==s1->repeat;
}

static int hash_grammer_sym_equal(const void* d0,const void* d1)
{
	return grammer_sym_equal((const FklGrammerSym*)d0,(const FklGrammerSym*)d1);
}

static const FklHashTableMetaTable GrammerSymMetaTable=
{
	.size=sizeof(FklGrammerSym),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=hash_grammer_sym_set_key,
	.__setVal=hash_grammer_sym_set_key,
	.__hashFunc=hash_grammer_sym_hash_func,
	.__uninitItem=fklDoNothingUnintHashItem,
	.__keyEqual=hash_grammer_sym_equal,
};

static inline FklHashTable* create_grammer_sym_set(void)
{
	return fklCreateHashTable(&GrammerSymMetaTable);
}

static int look_ahead_eqaul(const void* d0,const void* d1)
{
	return lalr_look_ahead_equal((const FklLalrItemLookAhead*)d0
			,(const FklLalrItemLookAhead*)d1);
}

static void look_ahead_set_key(void* d0,const void* d1)
{
	*(FklLalrItemLookAhead*)d0=*(const FklLalrItemLookAhead*)d1;
}

static uintptr_t look_ahead_hash_func(const void* d)
{
	return lalr_look_ahead_hash_func((const FklLalrItemLookAhead*)d);
}

static const FklHashTableMetaTable LookAheadHashMetaTable=
{
	.size=sizeof(FklLalrItemLookAhead),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=look_ahead_set_key,
	.__setVal=look_ahead_set_key,
	.__hashFunc=look_ahead_hash_func,
	.__keyEqual=look_ahead_eqaul,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static void item_set_set_val(void* d0,const void* d1)
{
	FklLalrItemSet* s=(FklLalrItemSet*)d0;
	*s=*(const FklLalrItemSet*)d1;
	s->links=NULL;
	s->spreads=NULL;
}

static uintptr_t item_set_hash_func(const void* d)
{
	const FklHashTable* i=*(const FklHashTable**)d;
	uintptr_t v=0;
	for(FklHashTableNodeList* l=i->list;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->node->data;
		v+=lalr_item_hash_func(i);
	}
	return v;
}

static int item_set_equal(const void* d0,const void* d1)
{
	const FklHashTable* s0=*(const FklHashTable**)d0;
	const FklHashTable* s1=*(const FklHashTable**)d1;
	if(s0->num!=s1->num)
		return 0;
	const FklHashTableNodeList* l0=s0->list;
	const FklHashTableNodeList* l1=s1->list;
	for(;l0;l0=l0->next,l1=l1->next)
		if(!lalr_item_equal(l0->node->data,l1->node->data))
			return 0;
	return 1;
}

static void item_set_uninit(void* d)
{
	FklLalrItemSet* s=(FklLalrItemSet*)d;
	fklDestroyHashTable(s->items);
	FklLalrItemSetLink* l=s->links;
	while(l)
	{
		FklLalrItemSetLink* next=l->next;
		free(l);
		l=next;
	}
	FklLookAheadSpreads* sp=s->spreads;
	while(sp)
	{
		FklLookAheadSpreads* next=sp->next;
		free(l);
		sp=next;
	}
}

static void item_set_add_link(FklLalrItemSet* src,FklGrammerSym* sym,FklLalrItemSet* dst)
{
	FklLalrItemSetLink* l=(FklLalrItemSetLink*)malloc(sizeof(FklLalrItemSetLink));
	FKL_ASSERT(l);
	l->sym=*sym;
	l->dst=dst;
	l->next=src->links;
	src->links=l;
}

static const FklHashTableMetaTable ItemStateSetHashMetaTable=
{
	.size=sizeof(FklLalrItemSet),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklHashDefaultSetPtrKey,
	.__setVal=item_set_set_val,
	.__hashFunc=item_set_hash_func,
	.__keyEqual=item_set_equal,
	.__uninitItem=item_set_uninit,
};

static inline FklHashTable* create_item_state_set(void)
{
	return fklCreateHashTable(&ItemStateSetHashMetaTable);
}

static inline void lalr_item_set_goto(FklLalrItemSet* itemset
		,FklHashTable* itemsetSet
		,FklGrammer* g
		,FklPtrQueue* pending)
{
	FklHashTable* checked=create_grammer_sym_set();
	FklHashTableNodeList* l;
	FklHashTable* items=itemset->items;
	for(l=items->list;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->node->data;
		FklGrammerSym* sym=get_item_next(i);
		if(sym)
			fklPutHashItem(sym,checked);
	}
	for(l=checked->list;l;l=l->next)
	{
		FklHashTable* closure=create_empty_item_set();
		FklGrammerSym* sym=(FklGrammerSym*)l->node->data;
		if(sym)
		{
			for(FklHashTableNodeList* l=items->list;l;l=l->next)
			{
				FklLalrItem* i=(FklLalrItem*)l->node->data;
				FklGrammerSym* next=get_item_next(i);
				if(next&&hash_grammer_sym_equal(sym,next))
				{
					FklLalrItem item=lalr_item_advance(i);
					fklPutHashItem(&item,closure);
				}
			}
			lr0_item_set_closure_and_sort(closure,g);
			FklLalrItemSet* itemsetptr=fklGetHashItem(&closure,itemsetSet);
			if(!itemsetptr)
			{
				FklLalrItemSet itemset={.items=closure,.links=NULL};
				itemsetptr=fklGetOrPutHashItem(&itemset,itemsetSet);
				fklPushPtrQueue(itemsetptr,pending);
			}
			else
				fklDestroyHashTable(closure);
			item_set_add_link(itemset,sym,itemsetptr);
		}
	}
}

FklHashTable* fklGenerateLr0Items(FklGrammer* grammer)
{
	FklHashTable* itemstate_set=create_item_state_set();
	FklSid_t left=0;
	FklGrammerProduction* prod=((ProdHashItem*)fklGetHashItem(&left,grammer->productions))->prods;
	FklHashTable* items=create_first_item_set(prod);
	lr0_item_set_closure_and_sort(items,grammer);
	FklLalrItemSet itemset={.items=items,};
	FklLalrItemSet* itemsetptr=fklGetOrPutHashItem(&itemset,itemstate_set);
	FklPtrQueue pending;
	fklInitPtrQueue(&pending);
	fklPushPtrQueue(itemsetptr,&pending);
	while(!fklIsPtrQueueEmpty(&pending))
	{
		FklLalrItemSet* itemsetptr=fklPopPtrQueue(&pending);
		lalr_item_set_goto(itemsetptr,itemstate_set,grammer,&pending);
	}
	fklUninitPtrQueue(&pending);
	return itemstate_set;
}

static inline int get_first_set(const FklGrammer* g
		,const FklGrammerProduction* prod
		,uint32_t idx
		,FklHashTable* first)
{
#warning recursive
	size_t len=prod->len;
	if(idx>=len)
		return 1;
	size_t lastIdx=len-1;
	FklSymbolTable* tt=g->terminals;
	int hasEpsilon=0;
	for(uint32_t i=idx;i<len;i++)
	{
		FklGrammerSym* sym=&prod->syms[idx];
		if(sym->isterm)
		{
			FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
			if(s->size==0)
			{
				if(idx==lastIdx)
					hasEpsilon=1;
			}
			else
			{
				FklLalrItemLookAhead la={.t=FKL_LALR_LOOKAHEAD_STRING,.u.s=s};
				fklPutHashItem(&la,first);
				break;
			}
		}
		else
		{
			FklGrammerProduction* prods=fklGetGrammerProductions(g,sym->nt);
			int hasEpsilon=0;
			FklHashTable noterminal_first;
			fklInitHashTable(&noterminal_first,&LookAheadHashMetaTable);
			for(;prods;prods=prods->next)
			{
				hasEpsilon|=get_first_set(g,prods,0,&noterminal_first);
				for(FklHashTableNodeList* l=noterminal_first.list;l;l=l->next)
				{
					FklLalrItemLookAhead* la=(FklLalrItemLookAhead*)l->node->data;
					fklPutHashItem(la,first);
				}
				fklClearHashTable(&noterminal_first);
			}
			fklUninitHashTable(&noterminal_first);
			if(!hasEpsilon)
				break;
		}
	}
	return hasEpsilon;
	// FklPtrStack pending;
	// FklPtrStack froms;
	// FklUintStack idxs;
	// fklInitPtrStack(&pending,8,8);
	// fklInitPtrStack(&froms,8,8);
	// fklInitUintStack(&idxs,8,8);
	//
	// fklPushPtrStack(&prod->syms[idx],&pending);
	// fklPushPtrStack(prod,&froms);
	// fklPushUintStack(idx,&idxs);
	//
	// FklSymbolTable* tt=g->terminals;
	// while(!fklIsPtrStackEmpty(&pending))
	// {
	// 	FklGrammerSym* sym=fklPopPtrStack(&pending);
	// 	FklGrammerProduction* from=fklPopPtrStack(&froms);
	// 	uint64_t idx=fklPopUintStack(&idxs);
	// 	if(sym->isterm)
	// 	{
	// 		FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
	// 		FklLalrItemLookAhead la={.t=FKL_LALR_LOOKAHEAD_STRING,.u.s=s};
	// 		fklPutHashItem(&la,t);
	// 	}
	// 	else
	// 	{
	// 		FklSid_t left=sym->nt;
	// 		FklGrammerProduction* prods=fklGetGrammerProductions(g,left);
	// 		for(;prods;prods=prods->next)
	// 		{
	// 			FklGrammerSym* sym=&prods->syms[0];
	// 			if(sym->isterm||sym->nt!=left)
	// 			{
	// 				fklPushPtrStack(sym,&pending);
	// 				fklPushPtrStack(prods,&froms);
	// 				fklPushUintStack(0,&idxs);
	// 			}
	// 		}
	// 	}
	// }
	// fklUninitPtrStack(&pending);
	// return t;
}

static inline void get_la_first_set(const FklGrammer* g
		,const FklGrammerProduction* prod
		,uint32_t beta
		,FklLalrItemLookAhead* a
		,FklHashTable* first)
{
	int hasEpsilon=get_first_set(g,prod,beta,first);
	if(hasEpsilon)
		fklPutHashItem(a,first);
}

static inline void lr1_item_set_closure(FklHashTable* itemSet,FklGrammer* g)
{
	FklHashTable pendingSet;
	FklHashTable changeSet;
	FklHashTable first;
	fklInitHashTable(&pendingSet,&LalrItemHashMetaTable);
	fklInitHashTable(&changeSet,&LalrItemHashMetaTable);
	fklInitHashTable(&first,&LookAheadHashMetaTable);

	for(FklHashTableNodeList* l=itemSet->list;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->node->data;
		fklPutHashItem(i,&pendingSet);
	}

	while(pendingSet.num)
	{
		for(FklHashTableNodeList* l=pendingSet.list;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->node->data;
			FklGrammerSym* next=get_item_next(i);
			if(next&&!next->isterm)
			{
				uint32_t beta=i->idx+1;
				get_la_first_set(g,i->prod,beta,&i->la,&first);
				FklSid_t left=next->nt;
				FklGrammerProduction* prods=fklGetGrammerProductions(g,left);
				for(FklHashTableNodeList* first_list=first.list;first_list;first_list=first_list->next)
				{
					FklLalrItemLookAhead* a=(FklLalrItemLookAhead*)first_list->node->data;
					for(FklGrammerProduction* prod=prods;prod;prod=prod->next)
					{
						FklLalrItem newItem={.prod=prod,.la=*a,.idx=0};
						if(!fklGetHashItem(&newItem,itemSet))
						{
							fklPutHashItem(&newItem,itemSet);
							fklPutHashItem(&newItem,&changeSet);
						}
					}
				}
				fklClearHashTable(&first);
			}
		}

		fklClearHashTable(&pendingSet);
		for(FklHashTableNodeList* l=changeSet.list;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->node->data;
			fklPutHashItem(i,&pendingSet);
		}

		fklClearHashTable(&changeSet);
	}
	fklUninitHashTable(&changeSet);
	fklUninitHashTable(&pendingSet);
	fklUninitHashTable(&first);
}

static inline int is_kernel_item(const FklLalrItem* item)
{
	return item->prod->left==0
		||item->idx!=0;
}

static inline void init_empty_item_set(FklHashTable* t)
{
	fklInitHashTable(t,&LalrItemHashMetaTable);
}

static inline void add_lookahead_spread(FklLalrItemSet* itemset
		,const FklLalrItem* src
		,FklHashTable* dstItems
		,const FklLalrItem* dst)
{
	FklLookAheadSpreads* sp=(FklLookAheadSpreads*)malloc(sizeof(FklLookAheadSpreads));
	FKL_ASSERT(sp);
	sp->next=itemset->spreads;
	sp->dstItems=dstItems;
	sp->src=*src;
	sp->dst=*dst;
	itemset->spreads=sp;
}

static inline void check_lookahead_self_generated_and_spread(FklGrammer* g
		,FklLalrItemSet* itemset
		,const FklLalrItemSetLink* x)
{
	if(x->dst==itemset)
		return;
	FklHashTable* items=itemset->items;
	const FklGrammerSym* xsym=&x->sym;
	FklHashTable closure;
	init_empty_item_set(&closure);
	FklHashTable spread;
	fklInitHashTable(&spread,&LookAheadHashMetaTable);
	for(FklHashTableNodeList* il=items->list;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->node->data;
		if(i->la.t==FKL_LALR_LOOKAHEAD_NONE&&is_kernel_item(i))
		{
			FklLalrItem item={.prod=i->prod,.idx=i->idx,.la=FKL_LALR_LOOKAHEAD_NONE_INIT};
			fklPutHashItem(&item,&closure);
			lr1_item_set_closure(&closure,g);
			for(FklHashTableNodeList* cl=closure.list;cl;cl=cl->next)
			{
				FklLalrItem* i=(FklLalrItem*)cl->node->data;
				const FklGrammerSym* s=get_item_next(i);
				i->idx++;
				if(s&&grammer_sym_equal(s,xsym))
				{
					if(i->la.t==FKL_LALR_LOOKAHEAD_NONE)
						add_lookahead_spread(itemset,&item,x->dst->items,i);
					else
						fklPutHashItem(i,x->dst->items);
				}
			}
			fklClearHashTable(&closure);
		}
	}
}

static inline int lookahead_spread(FklLalrItemSet* itemset)
{
	int change=0;
	FklHashTable* items=itemset->items;
	for(FklLookAheadSpreads* sp=itemset->spreads;sp;sp=sp->next)
	{
		FklLalrItem* srcItem=&sp->src;
		FklLalrItem* dstItem=&sp->dst;
		FklHashTable* dstItems=sp->dstItems;
		for(FklHashTableNodeList* il=items->list;il;il=il->next)
		{
			FklLalrItem* item=(FklLalrItem*)il->node->data;
			if(item->la.t!=FKL_LALR_LOOKAHEAD_NONE
					&&item->prod==srcItem->prod
					&&item->idx==srcItem->idx)
			{
				FklLalrItem newItem=*dstItem;
				newItem.la=item->la;
				if(!fklGetHashItem(&newItem,dstItems))
				{
					change=1;
					fklPutHashItem(&newItem,dstItems);
				}
			}
		}
	}
	return change;
}

static inline void init_lalr_look_ahead(FklHashTable* lr0,FklGrammer* g)
{
	for(FklHashTableNodeList* isl=lr0->list;isl;isl=isl->next)
	{
		FklLalrItemSet* s=(FklLalrItemSet*)isl->node->data;
		for(FklLalrItemSetLink* link=s->links;link;link=link->next)
			check_lookahead_self_generated_and_spread(g,s,link);
	}
	FklHashTableNodeList* isl=lr0->list;
	FklLalrItemSet* s=(FklLalrItemSet*)isl->node->data;
	for(FklHashTableNodeList* il=s->items->list;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->node->data;
		if(i->la.t==FKL_LALR_LOOKAHEAD_NONE&&is_kernel_item(i))
		{
			FklLalrItem item=*i;
			item.la=FKL_LALR_LOOKAHEAD_EOF_INIT;
			fklPutHashItem(&item,s->items);
		}
	}
}

static inline void add_look_ahead_to_items(FklHashTable* items
		,FklGrammer* g)
{
	FklHashTable add;
	init_empty_item_set(&add);
	for(FklHashTableNodeList* il=items->list;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->node->data;
		if(i->la.t!=FKL_LALR_LOOKAHEAD_NONE&&is_kernel_item(i))
			fklPutHashItem(i,&add);
	}
	fklClearHashTable(items);
	for(FklHashTableNodeList* il=add.list;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->node->data;
		fklPutHashItem(i,items);
	}
	fklUninitHashTable(&add);
	lr1_item_set_closure(items,g);
	lalr_item_set_sort(items);
}

static inline void add_look_ahead_for_all_item_set(FklHashTable* lr0,FklGrammer* g)
{
	for(FklHashTableNodeList* isl=lr0->list;isl;isl=isl->next)
	{
		FklLalrItemSet* itemSet=(FklLalrItemSet*)isl->node->data;
		add_look_ahead_to_items(itemSet->items,g);
	}
}

void fklLr0ToLalrItems(FklHashTable* lr0,FklGrammer* g)
{
	init_lalr_look_ahead(lr0,g);
	int change;
	do
	{
		change=0;
		for(FklHashTableNodeList* isl=lr0->list;isl;isl=isl->next)
		{
			FklLalrItemSet* s=(FklLalrItemSet*)isl->node->data;
			change|=lookahead_spread(s);
		}
	}while(change);
	add_look_ahead_for_all_item_set(lr0,g);
}

typedef struct
{
	FklLalrItemSet* set;
	size_t idx;
}ItemStateIdx;

static void itemsetcount_set_val(void* d0,const void* d1)
{
	*(ItemStateIdx*)d0=*(const ItemStateIdx*)d1;
}

static int itemsetcount_equal(const void* d0,const void* d1)
{
	return *(const void**)d0==*(const void**)d1;
}

static uintptr_t itemsetcount_hash_func(const void* d0)
{
	return (uintptr_t)(*(const void**)d0);
}

static const FklHashTableMetaTable ItemStateIdxHashMetaTable=
{
	.size=sizeof(ItemStateIdx),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklHashDefaultSetPtrKey,
	.__setVal=itemsetcount_set_val,
	.__keyEqual=itemsetcount_equal,
	.__hashFunc=itemsetcount_hash_func,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline void print_look_ahead_as_dot(FILE* fp,const FklLalrItemLookAhead* la)
{
	switch(la->t)
	{
		case FKL_LALR_LOOKAHEAD_STRING:
			{
				fputs("\\\'",fp);
				const FklString* str=la->u.s;
				print_string_as_dot((const uint8_t*)str->str,'\'',str->size,fp);
				fputs("\\\'",fp);
			}
			break;
		case FKL_LALR_LOOKAHEAD_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_LOOKAHEAD_BUILTIN:
			fputs("&%",fp);
			break;
		case FKL_LALR_LOOKAHEAD_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_item_as_dot(FILE* fp
		,const FklLalrItem* item
		,const FklSymbolTable* st
		,const FklSymbolTable* tt)
{
	size_t i=0;
	size_t idx=item->idx;
	FklGrammerProduction* prod=item->prod;
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	if(prod->left)
	{
		const FklString* str=fklGetSymbolWithId(prod->left,st)->symbol;
		print_string_as_dot((const uint8_t*)str->str,'"',str->size,fp);
	}
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	for(;i<idx;i++)
	{
		putc(' ',fp);
		print_prod_sym_as_dot(fp,&syms[i],st,tt);
	}
	fputs(" *",fp);
	for(;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym_as_dot(fp,&syms[i],st,tt);
	}
	fputs(" , ",fp);
	print_look_ahead_as_dot(fp,&item->la);
}

static inline void print_item_set_as_dot(const FklHashTable* itemSet,const FklGrammer* g,const FklSymbolTable* st,FILE* fp)
{
	FklSymbolTable* tt=g->terminals;
	FklLalrItem* curItem=NULL;
	for(FklHashTableNodeList* list=itemSet->list;list;list=list->next)
	{
		FklLalrItem* item=(FklLalrItem*)list->node->data;
		if(!curItem||item->idx!=curItem->idx
				||item->prod!=curItem->prod)
		{
			if(curItem)
				fputs("\\l\\\n",fp);
			curItem=item;
			print_item_as_dot(fp,curItem,st,tt);
		}
		else
		{
			fputs(" / ",fp);
			print_look_ahead_as_dot(fp,&item->la);
		}
	}
	fputs("\\l\\\n",fp);
}

void fklPrintItemStateSetAsDot(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	fputs("digraph {\n",fp);
	fputs("\trankdir=\"LR\"\n",fp);
	fputs("\tranksep=1\n",fp);
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableNodeList* l=i->list;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	for(const FklHashTableNodeList* l=i->list;l;l=l->next)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		const FklHashTable* i=s->items;
		ItemStateIdx* c=fklGetHashItem(&s,&idxTable);
		idx=c->idx;
		fprintf(fp,"\t\"I%lu\"[nojustify=true shape=\"box\" label=\"I%lu\\l\\\n"
				,idx
				,idx);
		print_item_set_as_dot(i,g,st,fp);
		fputs("\"]\n",fp);
		for(FklLalrItemSetLink* l=s->links;l;l=l->next)
		{
			FklLalrItemSet* dst=l->dst;
			ItemStateIdx* c=fklGetHashItem(&dst,&idxTable);
			fprintf(fp,"\tI%lu->I%lu[label=\"",idx,c->idx);
			print_prod_sym_as_dot(fp,&l->sym,st,g->terminals);
			fputs("\"]\n",fp);
		}
		putc('\n',fp);
	}
	fklUninitHashTable(&idxTable);
	fputs("}",fp);
}

static inline FklAnalysisStateAction* create_shift_action(const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisState* state)
{
	const FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
	FklAnalysisStateAction* action=(FklAnalysisStateAction*)malloc(sizeof(FklAnalysisStateAction));
	FKL_ASSERT(action);
	action->next=NULL;
	action->action=FKL_ANALYSIS_SHIFT;
	action->u.state=state;
	action->la.t=FKL_LALR_LOOKAHEAD_STRING;
	action->la.u.s=s;
	return action;
}

static inline FklAnalysisStateGoto* create_state_goto(const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisStateGoto* next
		,FklAnalysisState* state)
{
	FklAnalysisStateGoto* gt=(FklAnalysisStateGoto*)malloc(sizeof(FklAnalysisStateGoto));
	FKL_ASSERT(gt);
	gt->next=next;
	gt->state=state;
	gt->nt=sym->nt;
	return gt;
}

static int check_reduce_conflict(const FklAnalysisStateAction* actions,const FklLalrItemLookAhead* la)
{
	for(;actions;actions=actions->next)
		if(lalr_look_ahead_equal(&actions->la,la))
			return 1;
	return 0;
}

static inline int add_reduce_action(FklAnalysisState* curState
		,const FklGrammerProduction* prod
		,const FklLalrItemLookAhead* la)
{
	if(check_reduce_conflict(curState->state.action,la))
		return 1;
	FklAnalysisStateAction* action=(FklAnalysisStateAction*)malloc(sizeof(FklAnalysisStateAction));
	FKL_ASSERT(action);
	action->la=*la;
	if(prod->left==0)
		action->action=FKL_ANALYSIS_ACCEPT;
	else
	{
		action->action=FKL_ANALYSIS_REDUCE;
		action->u.prod=prod;
	}
	FklAnalysisStateAction** pa=&curState->state.action;
	if(la->t==FKL_LALR_LOOKAHEAD_STRING)
	{
		const FklString* s=action->la.u.s;
		for(;*pa;pa=&(*pa)->next)
		{
			FklAnalysisStateAction* curAction=*pa;
			if(curAction->la.t!=FKL_LALR_LOOKAHEAD_STRING
					||(curAction->la.t==FKL_LALR_LOOKAHEAD_STRING
						&&s->size>curAction->la.u.s->size))
				break;
		}
		action->next=*pa;
		*pa=action;
	}
	else
	{
		FklAnalysisStateAction** pa=&curState->state.action;
		for(;*pa;pa=&(*pa)->next)
		{
			FklAnalysisStateAction* curAction=*pa;
			if(curAction->la.t!=FKL_LALR_LOOKAHEAD_STRING)
				break;
		}
		action->next=*pa;
		*pa=action;
	}
	return 0;
}

static inline void clear_analysis_table(FklGrammer* g,size_t last)
{
	size_t end=last+1;
	FklAnalysisState* states=g->aTable.states;
	for(size_t i=0;i<end;i++)
	{
		FklAnalysisState* curState=&states[i];
		FklAnalysisStateAction* actions=curState->state.action;
		while(actions)
		{
			FklAnalysisStateAction* next=actions->next;
			free(actions);
			actions=next;
		}

		FklAnalysisStateGoto* gt=curState->state.gt;
		while(gt)
		{
			FklAnalysisStateGoto* next=gt->next;
			free(gt);
			gt=next;
		}
	}
	free(states);
	g->aTable.states=NULL;
	g->aTable.num=0;
}

static inline void add_shift_action(FklAnalysisState* curState
		,const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisState* dstState)
{
	FklAnalysisStateAction* action=create_shift_action(sym,tt,dstState);
	const FklString* s=action->la.u.s;
	FklAnalysisStateAction** pa=&curState->state.action;
	for(;*pa;pa=&(*pa)->next)
	{
		FklAnalysisStateAction* curAction=*pa;
		if(curAction->la.t!=FKL_LALR_LOOKAHEAD_STRING
				||(curAction->la.t==FKL_LALR_LOOKAHEAD_STRING
					&&s->size>curAction->la.u.s->size))
			break;
	}
	action->next=*pa;
	*pa=action;
}

int fklGenerateLalrAnalyzeTable(FklGrammer* grammer,FklHashTable* states)
{
	int hasConflict=0;
	grammer->aTable.num=states->num;
	FklSymbolTable* tt=grammer->terminals;
	FklAnalysisState* astates=(FklAnalysisState*)malloc(sizeof(FklAnalysisState)*states->num);
	FKL_ASSERT(astates);
	grammer->aTable.states=astates;
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableNodeList* l=states->list;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	idx=0;
	for(const FklHashTableNodeList* l=states->list;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		FklAnalysisState* curState=&astates[idx];
		curState->builtin=0;
		curState->func=NULL;
		curState->state.action=NULL;
		curState->state.gt=NULL;
		FklHashTable* items=s->items;
		for(FklLalrItemSetLink* ll=s->links;ll;ll=ll->next)
		{
			const FklGrammerSym* sym=&ll->sym;
			ItemStateIdx* c=fklGetHashItem(&ll->dst,&idxTable);
			size_t dstIdx=c->idx;
			FklAnalysisState* dstState=&astates[dstIdx];
			if(sym->isterm)
				add_shift_action(curState,sym,tt,dstState);
			else
				curState->state.gt=create_state_goto(sym,tt,curState->state.gt,dstState);
		}
		for(const FklHashTableNodeList* il=items->list;il;il=il->next)
		{
			FklLalrItem* item=(FklLalrItem*)il->node->data;
			if(item->idx>=item->prod->len)
			{
				if(add_reduce_action(curState,item->prod,&item->la))
				{
					hasConflict=1;
					clear_analysis_table(grammer,idx);
					goto break_loop;
				}
			}
		}
	}

break_loop:
	fklUninitHashTable(&idxTable);
	return hasConflict;
}

static inline void print_look_ahead_of_analysis_table(FILE* fp,const FklLalrItemLookAhead* la)
{
	switch(la->t)
	{
		case FKL_LALR_LOOKAHEAD_STRING:
			{
				fputc('\'',fp);
				const FklString* str=la->u.s;
				print_string_as_dot((const uint8_t*)str->str,'\'',str->size,fp);
				fputc('\'',fp);
			}
			break;
		case FKL_LALR_LOOKAHEAD_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_LOOKAHEAD_BUILTIN:
			fputs("&%",fp);
			break;
		case FKL_LALR_LOOKAHEAD_NONE:
			fputs("()",fp);
			break;
	}
}

void fklPrintAnalysisTable(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp)
{
	size_t num=grammer->aTable.num;
	FklAnalysisState* states=grammer->aTable.states;

	for(size_t i=0;i<num;i++)
	{
		fprintf(fp,"%lu: ",i);
		FklAnalysisState* curState=&states[i];
		for(FklAnalysisStateAction* actions=curState->state.action
				;actions
				;actions=actions->next)
		{
			switch(actions->action)
			{
				case FKL_ANALYSIS_SHIFT:
					fputs("S(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->la);
					{
						uintptr_t idx=actions->u.state-states;
						fprintf(fp," , %lu )",idx);
					}
					break;
				case FKL_ANALYSIS_REDUCE:
					fputs("R(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->la);
					fprintf(fp," , %lu )",actions->u.prod->idx);
					break;
				case FKL_ANALYSIS_ACCEPT:
					fputs("acc(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->la);
					fputc(')',fp);
					break;
			}
			fputc('\t',fp);
		}
		fputs("|\t",fp);
		for(FklAnalysisStateGoto* gt=curState->state.gt;gt;gt=gt->next)
		{
			uintptr_t idx=gt->state-states;
			fputc('(',fp);
			fklPrintRawSymbol(fklGetSymbolWithId(gt->nt,st)->symbol,fp);
			fprintf(fp," , %lu)",idx);
			fputc('\t',fp);
		}
		fputc('\n',fp);
	}
}

static inline void init_analysis_table_header(FklHashTable* la
		,FklHashTable* nt
		,FklAnalysisState* states
		,size_t stateNum)
{
	fklInitHashTable(la,&LookAheadHashMetaTable);
	fklInitSidSet(nt);

	for(size_t i=0;i<stateNum;i++)
	{
		FklAnalysisState* curState=&states[i];
		for(FklAnalysisStateAction* action=curState->state.action;action;action=action->next)
			fklPutHashItem(&action->la,la);
		for(FklAnalysisStateGoto* gt=curState->state.gt;gt;gt=gt->next)
			fklPutHashItem(&gt->nt,nt);
	}
}

static inline void print_symbol_for_grapheasy(const FklString* stri,FILE* fp)
{
	size_t size=stri->size;
	const uint8_t* str=(uint8_t*)stri->str;
	size_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(fp,"\\x");
			fprintf(fp,"%X",j/16);
			fprintf(fp,"%X",j%16);
			i++;
		}
		else if(l==1)
		{
			if(str[i]=='\\')
				fputs("\\\\",fp);
			else if(str[i]=='|')
				fputs("\\\\|",fp);
			else if(str[i]==']')
				fputs("\\]",fp);
			else if(isgraph(str[i]))
				putc(str[i],fp);
			else if(fklIsSpecialCharAndPrint(str[i],fp));
			else
			{
				uint8_t j=str[i];
				fprintf(fp,"\\x");
				fprintf(fp,"%X",j/16);
				fprintf(fp,"%X",j%16);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],fp);
			i+=l;
		}
	}
}

static inline void print_string_for_grapheasy(const FklString* stri,FILE* fp)
{
	size_t size=stri->size;
	const uint8_t* str=(uint8_t*)stri->str;
	size_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(fp,"\\x");
			fprintf(fp,"%X",j/16);
			fprintf(fp,"%X",j%16);
			i++;
		}
		else if(l==1)
		{
			if(str[i]=='\\')
				fputs("\\\\",fp);
			else if(str[i]=='|')
				fputs("\\|",fp);
			else if(str[i]==']')
				fputs("\\]",fp);
			else if(isgraph(str[i]))
				putc(str[i],fp);
			else if(fklIsSpecialCharAndPrint(str[i],fp));
			else
			{
				uint8_t j=str[i];
				fprintf(fp,"\\x");
				fprintf(fp,"%X",j/16);
				fprintf(fp,"%X",j%16);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],fp);
			i+=l;
		}
	}
}

static inline void print_look_ahead_for_grapheasy(const FklLalrItemLookAhead* la,FILE* fp)
{
	switch(la->t)
	{
		case FKL_LALR_LOOKAHEAD_STRING:
			{
				fputc('\'',fp);
				const FklString* str=la->u.s;
				print_string_for_grapheasy(str,fp);
				fputc('\'',fp);
			}
			break;
		case FKL_LALR_LOOKAHEAD_EOF:
			fputs("$",fp);
			break;
		case FKL_LALR_LOOKAHEAD_BUILTIN:
			fputs("%",fp);
			break;
		case FKL_LALR_LOOKAHEAD_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_table_header_for_grapheasy(const FklHashTable* la
		,const FklHashTable* sid
		,const FklSymbolTable* st
		,FILE* fp)
{
	fputs("\\n|",fp);
	for(FklHashTableNodeList* al=la->list;al;al=al->next)
	{
		FklLalrItemLookAhead* la=(FklLalrItemLookAhead*)al->node->data;
		print_look_ahead_for_grapheasy(la,fp);
		fputc('|',fp);
	}
	fputs("\\n",fp);
	for(FklHashTableNodeList* sl=sid->list;sl;sl=sl->next)
	{
		fputc('|',fp);
		FklSid_t id=*(FklSid_t*)sl->node->data;
		fputs("\\|",fp);
		print_symbol_for_grapheasy(fklGetSymbolWithId(id,st)->symbol,fp);
		fputs("\\|",fp);
	}
	fputs("||\n",fp);
}

static inline FklAnalysisStateAction* find_action(FklAnalysisStateAction* action
		,const FklLalrItemLookAhead* la)
{
	for(;action;action=action->next)
	{
		if(lalr_look_ahead_equal(la,&action->la))
			return action;
	}
	return NULL;
}

static inline FklAnalysisStateGoto* find_gt(FklAnalysisStateGoto* gt,FklSid_t id)
{
	for(;gt;gt=gt->next)
	{
		if(gt->nt==id)
			return gt;
	}
	return NULL;
}

void fklPrintAnalysisTableForGraphEasy(const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	size_t num=g->aTable.num;
	FklAnalysisState* states=g->aTable.states;

	fputs("[\n",fp);

	FklHashTable laTable;
	FklHashTable sidSet;
	init_analysis_table_header(&laTable,&sidSet,states,num);

	print_table_header_for_grapheasy(&laTable,&sidSet,st,fp);

	FklHashTableNodeList* laList=laTable.list;
	FklHashTableNodeList* sidList=sidSet.list;
	for(size_t i=0;i<num;i++)
	{
		const FklAnalysisState* curState=&states[i];
		fprintf(fp,"%lu: |",i);
		for(FklHashTableNodeList* al=laList;al;al=al->next)
		{
			FklLalrItemLookAhead* la=(FklLalrItemLookAhead*)al->node->data;
			FklAnalysisStateAction* action=find_action(curState->state.action,la);
			if(action)
			{
				switch(action->action)
				{
					case FKL_ANALYSIS_SHIFT:
						{
							uintptr_t idx=action->u.state-states;
							fprintf(fp,"s%lu",idx);
						}
						break;
					case FKL_ANALYSIS_REDUCE:
						fprintf(fp,"r%lu",action->u.prod->idx);
						break;
					case FKL_ANALYSIS_ACCEPT:
						fputs("acc",fp);
						break;
				}
			}
			else
				fputs("\\n",fp);
			fputc('|',fp);
		}
		fputs("\\n",fp);
		for(FklHashTableNodeList* sl=sidList;sl;sl=sl->next)
		{
			fputc('|',fp);
			FklSid_t id=*(FklSid_t*)sl->node->data;
			FklAnalysisStateGoto* gt=find_gt(curState->state.gt,id);
			if(gt)
			{
				uintptr_t idx=gt->state-states;
				fprintf(fp,"%lu",idx);
			}
			else
				fputs("\\n",fp);
		}
		fputs("||\n",fp);
	}
	fputc(']',fp);
	fklUninitHashTable(&laTable);
	fklUninitHashTable(&sidSet);
}

void fklPrintItemStateSet(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableNodeList* l=i->list;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	for(const FklHashTableNodeList* l=i->list;l;l=l->next)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->node->data;
		const FklHashTable* i=s->items;
		ItemStateIdx* c=fklGetHashItem(&s,&idxTable);
		idx=c->idx;
		fprintf(fp,"===\nI%lu: ",idx);
		putc('\n',fp);
		fklPrintItemSet(i,g,st,fp);
		putc('\n',fp);
		for(FklLalrItemSetLink* l=s->links;l;l=l->next)
		{
			FklLalrItemSet* dst=l->dst;
			ItemStateIdx* c=fklGetHashItem(&dst,&idxTable);
			fprintf(fp,"I%lu--{ ",idx);
			print_prod_sym(fp,&l->sym,st,g->terminals);
			fprintf(fp," }-->I%lu\n",c->idx);
		}
		putc('\n',fp);
	}
	fklUninitHashTable(&idxTable);
}

void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(prod->left)
		fklPrintString(fklGetSymbolWithId(prod->left,st)->symbol,fp);
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	for(size_t i=0;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	putc('\n',fp);
}

FklGrammerProduction* fklGetGrammerProductions(const FklGrammer* g,FklSid_t left)
{
	ProdHashItem* item=fklGetHashItem(&left,g->productions);
	if(item)
		return item->prods;
	return NULL;
}

void fklPrintGrammer(FILE* fp,const FklGrammer* grammer,FklSymbolTable* st)
{
	FklSymbolTable* tt=grammer->terminals;
	for(FklHashTableNodeList* list=grammer->productions->list;list;list=list->next)
	{
		FklGrammerProduction* prods=((ProdHashItem*)list->node->data)->prods;
		for(;prods;prods=prods->next)
		{
			fprintf(fp,"(%lu) ",prods->idx);
			fklPrintGrammerProduction(fp,prods,st,tt);
		}
	}
}


typedef struct
{
	int isterm;
	union
	{
		const FklString* s;
		FklSid_t nt;
	}u;
}AnalyzingSymbol;

static inline AnalyzingSymbol* create_term_analyzing_symbol(const FklString* s)
{
	AnalyzingSymbol* sym=(AnalyzingSymbol*)malloc(sizeof(AnalyzingSymbol));
	FKL_ASSERT(sym);
	sym->isterm=1;
	sym->u.s=s;
	return sym;
}

static inline AnalyzingSymbol* create_nonterm_analyzing_symbol(FklSid_t id)
{
	AnalyzingSymbol* sym=(AnalyzingSymbol*)malloc(sizeof(AnalyzingSymbol));
	FKL_ASSERT(sym);
	sym->isterm=0;
	sym->u.nt=id;
	return sym;
}

static inline int do_reduce_action(FklPtrStack* stateStack
		,FklPtrStack* symbolStack
		,const FklGrammerProduction* prod)
{

	size_t len=prod->len;
	stateStack->top-=len;
	FklAnalysisStateGoto* gt=((const FklAnalysisState*)fklTopPtrStack(stateStack))->state.gt;
	const FklAnalysisState* state=NULL;
	FklSid_t left=prod->left;
	for(;gt;gt=gt->next)
		if(gt->nt==left)
			state=gt->state;
	if(!state)
		return 1;
	size_t top=symbolStack->top-1;
	for(size_t i=0;i<len;i++)
		free(symbolStack->base[top-i]);
	symbolStack->top-=len;
	fklPushPtrStack(create_nonterm_analyzing_symbol(left),symbolStack);
	fklPushPtrStack((void*)state,stateStack);
	return 0;
}

int fklParseForCstr(const FklAnalysisTable* t,const char* cstr,FklPtrStack* tokens)
{
	FklPtrStack symbolStack;
	FklPtrStack stateStack;
	fklInitPtrStack(&stateStack,16,8);
	fklInitPtrStack(&symbolStack,16,8);
	fklPushPtrStack(&t->states[0],&stateStack);
	int retval=0;
	FklString* term=NULL;
	for(;;)
	{
		const FklAnalysisState* state=fklTopPtrStack(&stateStack);
		const FklAnalysisStateAction* action=state->state.action;
		for(;action;action=action->next)
		{
			switch(action->la.t)
			{
				case FKL_LALR_LOOKAHEAD_STRING:
					{
						const FklString* laString=action->la.u.s;
						if(fklStringCstrCmp(laString,cstr))
						{
							term=fklCreateString(laString->size,cstr);
							goto process_action;
						}
					}
					break;
				case FKL_LALR_LOOKAHEAD_EOF:
					if(!*cstr)
						goto process_action;
					break;
				case FKL_LALR_LOOKAHEAD_NONE:
				case FKL_LALR_LOOKAHEAD_BUILTIN:
					FKL_ASSERT(0);
					break;
			}
		}
process_action:
		if(action)
		{
			switch(action->action)
			{
				case FKL_ANALYSIS_SHIFT:
					{
						cstr+=term->size;
						fklPushPtrStack((void*)action->u.state,&stateStack);
						fklPushPtrStack(create_term_analyzing_symbol(term),&symbolStack);
						fklPushPtrStack(term,tokens);
					}
					break;
				case FKL_ANALYSIS_ACCEPT:
					goto break_for;
					break;
				case FKL_ANALYSIS_REDUCE:
					if(do_reduce_action(&stateStack
								,&symbolStack
								,action->u.prod))
					{
						retval=1;
						goto break_for;
					}
					break;
			}
		}
		else
		{
			retval=1;
			break;
		}
	}
break_for:
	fklUninitPtrStack(&stateStack);
	while(!fklIsPtrStackEmpty(&symbolStack))
		free(fklPopPtrStack(&symbolStack));
	fklUninitPtrStack(&symbolStack);
	return retval;
}
