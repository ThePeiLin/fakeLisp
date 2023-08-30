#ifndef FKL_PARSER_H
#define FKL_PARSER_H

#include"symbol.h"
#include"nast.h"
#include"base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FklStringMatchRouteNode;
struct FklCodegen;
FklNastNode* fklCreateNastNodeFromTokenStackAndMatchRoute(FklPtrStack*
		,struct FklStringMatchRouteNode*
		,size_t* errorLine
		,const FklSid_t t[4]
		,struct FklCodegen*
		,FklSymbolTable*);


struct FklStringMatchPattern;
FklNastNode* fklCreateNastNodeFromCstr(const char*
		,const FklSid_t t[4]
		,struct FklStringMatchPattern* pattern
		,FklSymbolTable* publicSymbolTable);
#ifdef __cplusplus
}
#endif
#endif
