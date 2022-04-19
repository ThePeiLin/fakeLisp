#include<fakeLisp/utils.h>
#include<string.h>
#include<stdlib.h>
#include"deftype.h"

static FklTypeId_t LastNativeTypeId=0;
static FklTypeId_t CharTypeId=0;
static FklTypeId_t StringTypeId=0;
static FklTypeId_t FILEpTypeId=0;

static int isNativeType(FklSid_t typeName,FklDefTypes* otherTypes);
struct GlobTypeUnionListStruct
{
	FklTypeId_t num;
	FklDefTypeUnion* ul;
} GlobTypeUnionList={0,NULL};

static void initVMStructTypeId(FklTypeId_t id,const char* structName,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t totalSize=0;
	size_t prevAllign=1;
	for(uint32_t i=0
			;i<num
			;totalSize+=totalSize%prevAllign
			,totalSize+=fklGetVMTypeSize(fklGetVMTypeUnion(memberTypes[i]))
			,i++)
		prevAllign=fklGetVMTypeAlign(fklGetVMTypeUnion(memberTypes[i]));
	FklDefTypeUnion* pst=&GlobTypeUnionList.ul[id-1];
	FklDefStructType* ost=(FklDefStructType*)FKL_GET_TYPES_PTR(pst->st);
	ost=(FklDefStructType*)realloc(ost,sizeof(FklDefStructType)+num*sizeof(FklDefStructMember));
	FKL_ASSERT(ost,__func__);
	if(structName)
		ost->type=fklAddSymbolToGlob(structName)->id;
	else
		ost->type=0;
	ost->num=num;
	ost->totalSize=totalSize;
	for(uint32_t i=0;i<num;i++)
	{
		ost->layout[i].memberSymbol=symbols[i];
		ost->layout[i].type=memberTypes[i];
	}
	pst->st=FKL_MAKE_STRUCT_TYPE(ost);
}

static void initVMUnionTypeId(FklTypeId_t id,const char* unionName,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t maxSize=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklGetVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	FklDefTypeUnion* put=&GlobTypeUnionList.ul[id-1];
	FklDefUnionType* out=(FklDefUnionType*)FKL_GET_TYPES_PTR(put->ut);
	out=(FklDefUnionType*)realloc(out,sizeof(FklDefUnionType)+num*sizeof(FklDefStructMember));
	FKL_ASSERT(out,__func__);
	if(unionName)
		out->type=fklAddSymbolToGlob(unionName)->id;
	else
		out->type=0;
	out->num=num;
	out->maxSize=maxSize;
	for(uint32_t i=0;i<num;i++)
	{
		out->layout[i].memberSymbol=symbols[i];
		out->layout[i].type=memberTypes[i];
	}
	put->ut=FKL_MAKE_UNION_TYPE(out);
}

static FklTypeId_t addToGlobTypeUnionList(FklDefTypeUnion type)
{
	GlobTypeUnionList.num+=1;
	size_t num=GlobTypeUnionList.num;
	GlobTypeUnionList.ul=(FklDefTypeUnion*)realloc(GlobTypeUnionList.ul,sizeof(FklDefTypeUnion)*num);
	FKL_ASSERT(GlobTypeUnionList.ul,__func__);
	GlobTypeUnionList.ul[num-1]=type;
	return num;
}

/*genTypeId functions list*/
static FklTypeId_t genArrayTypeId(FklAstCptr* compositeDataHead,FklDefTypes* otherTypes)
{
	FklAstCptr* numCptr=fklNextCptr(compositeDataHead);
	if(!numCptr||numCptr->type!=FKL_ATM||numCptr->u.atom->type!=FKL_I32)
		return 0;
	FklAstCptr* typeCptr=fklNextCptr(numCptr);
	if(!typeCptr)
		return 0;
	FklTypeId_t type=fklGenDefTypesUnion(typeCptr,otherTypes);
	if(!type)
		return type;
	if(fklGetCptrCdr(typeCptr)->type!=FKL_NIL)
		return 0;
	return fklNewVMArrayType(type,numCptr->u.atom->value.i32);
}

static FklTypeId_t genPtrTypeId(FklAstCptr* compositeDataHead,FklDefTypes* otherTypes)
{
	FklAstCptr* ptrTypeCptr=fklNextCptr(compositeDataHead);
	if(ptrTypeCptr->type==FKL_ATM&&ptrTypeCptr->u.atom->type!=FKL_SYM)
		return 0;
	FklTypeId_t pType=fklGenDefTypesUnion(ptrTypeCptr,otherTypes);
	if(!pType)
		return pType;
	if(fklGetCptrCdr(ptrTypeCptr)->type!=FKL_NIL)
		return 0;
	return fklNewVMPtrType(pType);
}

static int isInPtrDeclare(FklAstCptr* compositeDataHead)
{
	FklAstPair* outerPair=compositeDataHead->outer;
	FklAstPair* outerPrevPair=outerPair->prev;
	if(outerPrevPair)
	{
		FklAstCptr* outerTypeHead=fklPrevCptr(&outerPrevPair->car);
		if(outerTypeHead)
			return outerTypeHead->type==FKL_ATM&&outerTypeHead->u.atom->type==FKL_SYM&&!strcmp(outerTypeHead->u.atom->value.str,"ptr");
	}
	return 0;
}

static FklTypeId_t genStructTypeId(FklAstCptr* compositeDataHead,FklDefTypes* otherTypes)
{
	char* structName=NULL;
	FklAstCptr* structNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	FklAstCptr* memberCptr=NULL;
	if(structNameCptr->type==FKL_ATM)
	{
		if(structNameCptr->u.atom->type==FKL_SYM)
			structName=structNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(structNameCptr);
		structNameCptr=memberCptr;
	}
	else
		memberCptr=structNameCptr;
	FklTypeId_t retval=0;
	if(memberCptr==NULL&&structName!=NULL)
	{
		FklTypeId_t i=0;
		FklTypeId_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_DEF_STRUCT_TYPE_TAG)
			{
				FklDefStructType* st=(FklDefStructType*)FKL_GET_TYPES_PTR(typeUnion.st);
				if(st->type==fklAddSymbolToGlob(structName)->id)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval&&!isInPtrDeclare(compositeDataHead))
			return retval;
	}
	else if(structName!=NULL)
		retval=fklNewVMStructType(structName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=fklNextCptr(memberCptr));
	memberCptr=structNameCptr;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberCptr,__func__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			FklAstCptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklAstCptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=FKL_ATM||memberName->u.atom->type!=FKL_SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=FKL_NIL)
			{
				free(memberSymbolList);
				free(memberTypeList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
		}
	}
	if(retval&&num)
		initVMStructTypeId(retval,structName,num,memberSymbolList,memberTypeList);
	else if(!retval)
		retval=fklNewVMStructType(structName,num,memberSymbolList,memberTypeList);
	free(memberSymbolList);
	free(memberTypeList);
	return retval;
}

static FklTypeId_t genUnionTypeId(FklAstCptr* compositeDataHead,FklDefTypes* otherTypes)
{
	char* unionName=NULL;
	FklAstCptr* unionNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	FklAstCptr* memberCptr=NULL;
	if(unionNameCptr->type==FKL_ATM)
	{
		if(unionNameCptr->u.atom->type==FKL_SYM)
			unionName=unionNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(unionNameCptr);
		unionNameCptr=memberCptr;
	}
	else
		memberCptr=unionNameCptr;
	FklTypeId_t retval=0;
	if(memberCptr==NULL&&unionName!=NULL)
	{
		FklTypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_DEF_UNION_TYPE_TAG)
			{
				FklDefUnionType* ut=(FklDefUnionType*)FKL_GET_TYPES_PTR(typeUnion.st);
				if(ut->type==fklAddSymbolToGlob(unionName)->id)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval)
			return retval;
	}
	else if(unionName!=NULL)
		retval=fklNewVMUnionType(unionName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=fklNextCptr(memberCptr));
	memberCptr=unionNameCptr;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberCptr,__func__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			FklAstCptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklAstCptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=FKL_ATM||memberName->u.atom->type!=FKL_SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=FKL_NIL)
			{
				free(memberSymbolList);
				free(memberTypeList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
		}
	}
	if(retval&&num)
	{
		initVMUnionTypeId(retval,unionName,num,memberTypeList,memberTypeList);
	}
	else if(!retval)
	{
		retval=fklNewVMUnionType(unionName,num,memberSymbolList,memberTypeList);
		if(memberTypeList&&memberSymbolList)
		{
			free(memberSymbolList);
			free(memberTypeList);
		}
	}
	return retval;
}

static FklTypeId_t genFuncTypeId(FklAstCptr* compositeDataHead,FklDefTypes* otherTypes)
{
	FklTypeId_t rtype=0;
	FklAstCptr* argCptr=fklNextCptr(compositeDataHead);
	if(!argCptr||(argCptr->type!=FKL_PAIR&&argCptr->type!=FKL_NIL))
		return 0;
	FklAstCptr* rtypeCptr=fklNextCptr(argCptr);
	if(rtypeCptr)
	{
		FklTypeId_t tmp=fklGenDefTypesUnion(rtypeCptr,otherTypes);
		if(!tmp)
			return 0;
		FklDefTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_NATIVE_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_PTR_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_ARRAY_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_FUNC_TYPE_TAG)
			return 0;
		rtype=tmp;
	}
	uint32_t i=0;
	FklAstCptr* firArgCptr=fklGetFirstCptr(argCptr);
	for(;firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++);
	FklTypeId_t* atypes=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*i);
	FKL_ASSERT(atypes,__func__);
	for(i=0,firArgCptr=fklGetFirstCptr(argCptr);firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++)
	{
		FklTypeId_t tmp=fklGenDefTypesUnion(firArgCptr,otherTypes);
		if(!tmp)
		{
			free(atypes);
			return 0;
		}
		FklDefTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_NATIVE_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_PTR_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_ARRAY_TYPE_TAG&&FKL_GET_TYPES_TAG(tu.all)!=FKL_DEF_FUNC_TYPE_TAG)
		{
			free(atypes);
			return 0;
		}
		FklAstCptr* cdr=fklGetCptrCdr(firArgCptr);
		if(!tmp||(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL))
		{
			free(atypes);
			return 0;
		}
		atypes[i]=tmp;
	}
	FklTypeId_t retval=fklNewVMFuncType(rtype,i,atypes);
	free(atypes);
	return retval;
}

/*------------------------*/


FklDefTypes* fklNewDefTypes(void)
{
	FklDefTypes* tmp=(FklDefTypes*)malloc(sizeof(FklDefTypes));
	FKL_ASSERT(tmp,__func__);
	tmp->num=0;
	tmp->u=NULL;
	return tmp;
}

void fklFreeDefTypeTable(FklDefTypes* defs)
{
	size_t i=0;
	size_t num=defs->num;
	for(;i<num;i++)
		free(defs->u[i]);
	free(defs->u);
	free(defs);
}

FklTypeId_t fklGenDefTypes(FklAstCptr* objCptr,FklDefTypes* otherTypes,FklSid_t* typeName)
{
	FklAstCptr* fir=fklNextCptr(fklGetFirstCptr(objCptr));
	if(fir->type!=FKL_ATM||fir->u.atom->type!=FKL_SYM)
		return 0;
	FklSid_t typeId=fklAddSymbolToGlob(fir->u.atom->value.str)->id;
	if(isNativeType(typeId,otherTypes))
		return 0;
	*typeName=typeId;
	fir=fklNextCptr(fir);
	if(fir->type!=FKL_ATM&&fir->type!=FKL_PAIR)
		return 0;
	return fklGenDefTypesUnion(fir,otherTypes);
}

FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklDefTypes* otherTypes)
{
	if(!otherTypes)
		return 0;
	if(objCptr->type==FKL_ATM&&objCptr->u.atom->type==FKL_SYM)
	{
		FklDefTypesNode* n=fklFindDefTypesNode(fklAddSymbolToGlob(objCptr->u.atom->value.str)->id,otherTypes);
		if(!n)
			return 0;
		else
			return n->type;
	}
	else if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* compositeDataHead=fklGetFirstCptr(objCptr);
		if(compositeDataHead->type!=FKL_ATM||compositeDataHead->u.atom->type!=FKL_SYM)
			return 0;
		if(!strcmp(compositeDataHead->u.atom->value.str,"array"))
			return genArrayTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"ptr"))
			return genPtrTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"struct"))
			return genStructTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"union"))
			return genUnionTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"function"))
			return genFuncTypeId(compositeDataHead,otherTypes);
		else
			return 0;
	}
	else
		return 0;
}

FklTypeId_t fklNewVMNativeType(FklSid_t type,size_t size,size_t align)
{
	FklDefNativeType* tmp=(FklDefNativeType*)malloc(sizeof(FklDefNativeType));
	FKL_ASSERT(tmp,__func__);
	tmp->type=type;
	tmp->align=align;
	tmp->size=size;
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_NATIVE_TYPE(tmp));
}

void fklFreeVMNativeType(FklDefNativeType* obj)
{
	free(obj);
}

FklDefTypeUnion fklGetVMTypeUnion(FklTypeId_t t)
{
	return GlobTypeUnionList.ul[t-1];
}

size_t fklGetVMTypeSizeWithTypeId(FklTypeId_t t)
{
	return fklGetVMTypeSize(fklGetVMTypeUnion(t));
}

size_t fklGetVMTypeSize(FklDefTypeUnion t)
{
	FklDefTypeTag tag=FKL_GET_TYPES_TAG(t.all);
	t.all=FKL_GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_DEF_NATIVE_TYPE_TAG:
			return t.nt->size;
			break;
		case FKL_DEF_ARRAY_TYPE_TAG:
			return t.at->totalSize;
			break;
		case FKL_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_DEF_STRUCT_TYPE_TAG:
			return t.st->totalSize;
			break;
		case FKL_DEF_UNION_TYPE_TAG:
			return t.ut->maxSize;
			break;
		case FKL_DEF_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

size_t fklGetVMTypeAlign(FklDefTypeUnion t)
{
	FklDefTypeTag tag=FKL_GET_TYPES_TAG(t.all);
	t.all=FKL_GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_DEF_NATIVE_TYPE_TAG:
			return t.nt->align;
			break;
		case FKL_DEF_ARRAY_TYPE_TAG:
			return t.at->align;
			break;
		case FKL_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_DEF_STRUCT_TYPE_TAG:
			return t.st->align;
			break;
		case FKL_DEF_UNION_TYPE_TAG:
			return t.ut->align;
			break;
		case FKL_DEF_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

FklTypeId_t fklNewVMArrayType(FklTypeId_t type,size_t num)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_ARRAY_TYPE_TAG)
		{
			FklDefArrayType* arrayType=(FklDefTypeUnion){.at=FKL_GET_TYPES_PTR(tmpType.all)}.at;
			if(arrayType->etype==type&&arrayType->num==num)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefArrayType* tmp=(FklDefArrayType*)malloc(sizeof(FklDefArrayType));
		FKL_ASSERT(tmp,__func__);
		tmp->etype=type;
		tmp->num=num;
		tmp->totalSize=num*fklGetVMTypeSize(fklGetVMTypeUnion(type));
		tmp->align=fklGetVMTypeAlign(fklGetVMTypeUnion(type));
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_ARRAY_TYPE(tmp));
	}
	else
		return id;
}

void fklFreeVMArrayType(FklDefArrayType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMPtrType(FklTypeId_t type)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_PTR_TYPE_TAG)
		{
			FklDefPtrType* ptrType=(FklDefTypeUnion){.pt=FKL_GET_TYPES_PTR(tmpType.all)}.pt;
			if(ptrType->ptype==type)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefPtrType* tmp=(FklDefPtrType*)malloc(sizeof(FklDefPtrType));
		FKL_ASSERT(tmp,__func__);
		tmp->ptype=type;
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_PTR_TYPE(tmp));
	}
	return id;
}

void fklFreeVMPtrType(FklDefPtrType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMStructType(const char* structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	FklTypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=GlobTypeUnionList.num;
		FklSid_t stype=fklAddSymbolToGlob(structName)->id;
		for(;i<num;i++)
		{
			FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_STRUCT_TYPE_TAG)
			{
				FklDefStructType* structType=(FklDefTypeUnion){.st=FKL_GET_TYPES_PTR(tmpType.all)}.st;
				if(structType->type==stype)
				{
					id=i+1;
					break;
				}
			}
		}
	}
	if(!id)
	{
		size_t totalSize=0;
		size_t maxSize=0;
		size_t structAlign=0;
		for(uint32_t i=0;i<num;i++)
		{
			FklDefTypeUnion tu=fklGetVMTypeUnion(memberTypes[i]);
			size_t align=fklGetVMTypeAlign(tu);
			size_t size=fklGetVMTypeSize(tu);
			totalSize+=(totalSize%align)?align-totalSize%align:0;
			totalSize+=size;
			if(maxSize<size)
			{
				maxSize=size;
				structAlign=align;
			}
		}
		totalSize+=(maxSize&&totalSize%maxSize)?maxSize-totalSize%maxSize:0;
		FklDefStructType* tmp=(FklDefStructType*)malloc(sizeof(FklDefStructType)+sizeof(FklDefStructMember)*num);
		FKL_ASSERT(tmp,__func__);
		if(structName)
			tmp->type=fklAddSymbolToGlob(structName)->id;
		else
			tmp->type=0;
		tmp->num=num;
		tmp->totalSize=totalSize;
		tmp->align=structAlign;
		for(uint32_t i=0;i<num;i++)
		{
			tmp->layout[i].memberSymbol=symbols[i];
			tmp->layout[i].type=memberTypes[i];
		}
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_STRUCT_TYPE(tmp));
	}
	return id;
}

FklTypeId_t fklNewVMUnionType(const char* unionName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	size_t maxSize=0;
	size_t align=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklGetVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
		{
			maxSize=curSize;
			align=fklGetVMTypeAlign(fklGetVMTypeUnion(memberTypes[i]));
		}
	}
	FklDefUnionType* tmp=(FklDefUnionType*)malloc(sizeof(FklDefUnionType)+sizeof(FklDefStructMember)*num);
	FKL_ASSERT(tmp,__func__);
	if(unionName)
		tmp->type=fklAddSymbolToGlob(unionName)->id;
	else
		tmp->type=0;
	tmp->num=num;
	tmp->maxSize=maxSize;
	tmp->align=align;
	for(uint32_t i=0;i<num;i++)
	{
		tmp->layout[i].memberSymbol=symbols[i];
		tmp->layout[i].type=memberTypes[i];
	}
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_UNION_TYPE(tmp));
}

void fklFreeVMUnionType(FklDefUnionType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFreeVMStructType(FklDefStructType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[])
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_FUNC_TYPE_TAG)
		{
			FklDefFuncType* funcType=(FklDefTypeUnion){.ft=FKL_GET_TYPES_PTR(tmpType.all)}.ft;
			if(funcType->rtype==rtype&&funcType->anum==anum&&!memcmp(funcType->atypes,atypes,sizeof(FklTypeId_t)*anum))
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefFuncType* tmp=(FklDefFuncType*)malloc(sizeof(FklDefFuncType)+sizeof(FklTypeId_t)*anum);
		FKL_ASSERT(tmp,__func__);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_FUNC_TYPE(tmp));
	}
	return id;
}

void fklFreeVMFuncType(FklDefFuncType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

int fklAddDefTypes(FklDefTypes* otherTypes,FklSid_t typeName,FklTypeId_t type)
{
	if(otherTypes->num==0)
	{
		otherTypes->num+=1;
		FklDefTypesNode* node=(FklDefTypesNode*)malloc(sizeof(FklDefTypesNode));
		otherTypes->u=(FklDefTypesNode**)malloc(sizeof(FklDefTypesNode*)*1);
		FKL_ASSERT(otherTypes->u&&node,__func__);
		node->name=typeName;
		node->type=type;
		otherTypes->u[0]=node;
	}
	else
	{
		int64_t l=0;
		int64_t h=otherTypes->num-1;
		int64_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int64_t r=(int64_t)otherTypes->u[mid]->name-(int64_t)typeName;
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
				return 1;
		}
		if((int64_t)otherTypes->u[mid]->name-(int64_t)typeName<0)
			mid++;
		otherTypes->num+=1;
		int64_t i=otherTypes->num-1;
		otherTypes->u=(FklDefTypesNode**)realloc(otherTypes->u,sizeof(FklDefTypesNode*)*otherTypes->num);
		FKL_ASSERT(otherTypes->u,__func__);
		FklDefTypesNode* node=(FklDefTypesNode*)malloc(sizeof(FklDefTypesNode));
		FKL_ASSERT(otherTypes->u&&node,__func__);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

FklDefTypesNode* fklFindDefTypesNode(FklSid_t typeName,FklDefTypes* otherTypes)
{
	int64_t l=0;
	int64_t h=otherTypes->num-1;
	int64_t mid=0;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int64_t r=(int64_t)otherTypes->u[mid]->name-(int64_t)typeName;
		if(r>0)
			h=mid-1;
		else if(r<0)
			l=mid+1;
		else
			return otherTypes->u[mid];
	}
	return NULL;
	//return (FklDefTypeUnion){.all=NULL};
}

int isNativeType(FklSid_t typeName,FklDefTypes* otherTypes)
{
	FklDefTypesNode* typeNode=fklFindDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&FKL_GET_TYPES_TAG(fklGetVMTypeUnion(typeNode->type).all)==FKL_DEF_NATIVE_TYPE_TAG;
}

void fklInitNativeDefTypes(FklDefTypes* otherTypes)
{
	struct
	{
		char* typeName;
		size_t size;
	} nativeTypeList[]=
	{
		{"short",sizeof(short)},
		{"int",sizeof(int)},
		{"unsigned-short",sizeof(unsigned short)},
		{"unsigned",sizeof(unsigned)},
		{"long",sizeof(long)},
		{"unsigned-long",sizeof(unsigned long)},
		{"long-long",sizeof(long long)},
		{"unsigned-long-long",sizeof(unsigned long long)},
		{"ptrdiff",sizeof(ptrdiff_t)},
		{"size_t",sizeof(size_t)},
		{"ssize_t",sizeof(ssize_t)},
		{"char",sizeof(char)},
		{"wchar_t",sizeof(wchar_t)},
		{"float",sizeof(float)},
		{"double",sizeof(double)},
		{"int8_t",sizeof(int8_t)},
		{"uint8_t",sizeof(uint8_t)},
		{"int16_t",sizeof(int16_t)},
		{"uint16_t",sizeof(uint16_t)},
		{"int32_t",sizeof(int32_t)},
		{"uint32_t",sizeof(uint32_t)},
		{"int64_t",sizeof(int64_t)},
		{"uint64_t",sizeof(uint64_t)},
		{"iptr",sizeof(intptr_t)},
		{"uptr",sizeof(uintptr_t)},
		{"vptr",sizeof(void*)},
	};
	size_t num=sizeof(nativeTypeList)/(sizeof(char*)+sizeof(size_t));
	size_t i=0;
	for(;i<num;i++)
	{
		FklSid_t typeName=fklAddSymbolToGlob(nativeTypeList[i].typeName)->id;
		size_t size=nativeTypeList[i].size;
		FklTypeId_t t=fklNewVMNativeType(typeName,size,size);
		if(!CharTypeId&&!strcmp("char",nativeTypeList[i].typeName))
			CharTypeId=t;
		//FklDefTypeUnion t={.nt=fklNewVMNativeType(typeName,size)};
		fklAddDefTypes(otherTypes,typeName,t);
	}
	FklSid_t otherTypeName=fklAddSymbolToGlob("string")->id;
	StringTypeId=fklNewVMNativeType(otherTypeName,sizeof(char*),sizeof(char*));
	fklAddDefTypes(otherTypes,otherTypeName,StringTypeId);
	otherTypeName=fklAddSymbolToGlob("FILE*")->id;
	FILEpTypeId=fklNewVMNativeType(otherTypeName,sizeof(FILE*),sizeof(FILE*));
	fklAddDefTypes(otherTypes,otherTypeName,FILEpTypeId);
	LastNativeTypeId=num;
	//i=0;
	//for(;i<num;i++)
	//	fprintf(stderr,"i=%ld typeId=%d\n",i,otherTypes->u[i]->name);
	//fklPrintGlobSymbolTable(stderr);
}

void fklWriteTypeList(FILE* fp)
{
	FklTypeId_t num=GlobTypeUnionList.num;
	FklDefTypeUnion* ul=GlobTypeUnionList.ul;
	fwrite(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fwrite(&CharTypeId,sizeof(CharTypeId),1,fp);
	fwrite(&num,sizeof(num),1,fp);
	for(FklTypeId_t i=0;i<num;i++)
	{
		FklDefTypeUnion tu=ul[i];
		FklDefTypeTag tag=FKL_GET_TYPES_TAG(tu.all);
		void* p=(void*)FKL_GET_TYPES_PTR(tu.all);
		fwrite(&tag,sizeof(uint8_t),1,fp);
		switch(tag)
		{
			case FKL_DEF_NATIVE_TYPE_TAG:
				fwrite(&((FklDefNativeType*)p)->type,sizeof(((FklDefNativeType*)p)->type),1,fp);
				fwrite(&((FklDefNativeType*)p)->size,sizeof(((FklDefNativeType*)p)->size),1,fp);
				break;
			case FKL_DEF_ARRAY_TYPE_TAG:
				fwrite(&((FklDefArrayType*)p)->etype,sizeof(((FklDefArrayType*)p)->etype),1,fp);
				fwrite(&((FklDefArrayType*)p)->num,sizeof(((FklDefArrayType*)p)->num),1,fp);
				break;
			case FKL_DEF_PTR_TYPE_TAG:
				fwrite(&((FklDefPtrType*)p)->ptype,sizeof(((FklDefPtrType*)p)->ptype),1,fp);
				break;
			case FKL_DEF_STRUCT_TYPE_TAG:
				{
					uint32_t num=((FklDefStructType*)p)->num;
					fwrite(&((FklDefStructType*)p)->type,sizeof(((FklDefStructType*)p)->type),1,fp);
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((FklDefStructType*)p)->totalSize,sizeof(((FklDefStructType*)p)->totalSize),1,fp);
					FklDefStructMember* members=((FklDefStructType*)p)->layout;
					fwrite(members,sizeof(FklDefStructMember),num,fp);
				}
				break;
			case FKL_DEF_UNION_TYPE_TAG:
				{
					uint32_t num=((FklDefUnionType*)p)->num;
					fwrite(&((FklDefUnionType*)p)->type,sizeof(((FklDefUnionType*)p)->type),1,fp);
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((FklDefUnionType*)p)->maxSize,sizeof(((FklDefUnionType*)p)->maxSize),1,fp);
					FklDefStructMember* members=((FklDefUnionType*)p)->layout;
					fwrite(members,sizeof(FklDefStructMember),num,fp);
				}
				break;
			case FKL_DEF_FUNC_TYPE_TAG:
				{
					FklDefFuncType* ft=(FklDefFuncType*)p;
					fwrite(&ft->rtype,sizeof(ft->rtype),1,fp);
					uint32_t anum=ft->anum;
					fwrite(&anum,sizeof(anum),1,fp);
					FklTypeId_t* atypes=ft->atypes;
					fwrite(atypes,sizeof(*atypes),anum,fp);
				}
				break;
			default:
				break;
		}
	}
}

void fklLoadTypeList(FILE* fp)
{
	FklTypeId_t i=0;
	FklTypeId_t num=0;
	FklDefTypeUnion* ul=NULL;
	fread(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fread(&CharTypeId,sizeof(CharTypeId),1,fp);
	fread(&num,sizeof(num),1,fp);
	ul=(FklDefTypeUnion*)malloc(sizeof(FklDefTypeUnion)*num);
	FKL_ASSERT(ul,__func__);
	for(;i<num;i++)
	{
		FklDefTypeTag tag=FKL_DEF_NATIVE_TYPE_TAG;
		fread(&tag,sizeof(uint8_t),1,fp);
		FklDefTypeUnion tu={.all=NULL};
		switch(tag)
		{
			case FKL_DEF_NATIVE_TYPE_TAG:
				{
					FklDefNativeType* t=(FklDefNativeType*)malloc(sizeof(FklDefNativeType));
					FKL_ASSERT(t,__func__);
					fread(&t->type,sizeof(t->type),1,fp);
					fread(&t->size,sizeof(t->size),1,fp);
					tu.nt=(FklDefNativeType*)FKL_MAKE_NATIVE_TYPE(t);
				}
				break;
			case FKL_DEF_ARRAY_TYPE_TAG:
				{
					FklDefArrayType* t=(FklDefArrayType*)malloc(sizeof(FklDefArrayType));
					FKL_ASSERT(t,__func__);
					fread(&t->etype,sizeof(t->etype),1,fp);
					fread(&t->num,sizeof(t->num),1,fp);
					tu.at=(FklDefArrayType*)FKL_MAKE_ARRAY_TYPE(t);
				}
				break;
			case FKL_DEF_PTR_TYPE_TAG:
				{
					FklDefPtrType* t=(FklDefPtrType*)malloc(sizeof(FklDefPtrType));
					FKL_ASSERT(t,__func__);
					fread(&t->ptype,sizeof(t->ptype),1,fp);
					tu.pt=(FklDefPtrType*)FKL_MAKE_PTR_TYPE(t);
				}
				break;
			case FKL_DEF_STRUCT_TYPE_TAG:
				{
					uint32_t num=0;
					FklSid_t type=0;
					fread(&type,sizeof(type),1,fp);
					fread(&num,sizeof(num),1,fp);
					FklDefStructType* t=(FklDefStructType*)malloc(sizeof(FklDefStructType)+sizeof(FklDefStructMember)*num);
					FKL_ASSERT(t,__func__);
					t->type=type;
					t->num=num;
					fread(&t->totalSize,sizeof(t->totalSize),1,fp);
					fread(t->layout,sizeof(FklDefStructMember),num,fp);
					tu.st=(FklDefStructType*)FKL_MAKE_STRUCT_TYPE(t);
				}
				break;
			case FKL_DEF_UNION_TYPE_TAG:
				{
					uint32_t num=0;
					FklSid_t type=0;
					fread(&type,sizeof(type),1,fp);
					fread(&num,sizeof(num),1,fp);
					FklDefUnionType* t=(FklDefUnionType*)malloc(sizeof(FklDefUnionType)+sizeof(FklDefStructMember)*num);
					FKL_ASSERT(t,__func__);
					t->type=type;
					t->num=num;
					fread(&t->maxSize,sizeof(t->maxSize),1,fp);
					fread(t->layout,sizeof(FklDefStructMember),num,fp);
					tu.ut=(FklDefUnionType*)FKL_MAKE_UNION_TYPE(t);
				}
			case FKL_DEF_FUNC_TYPE_TAG:
				{
					FklTypeId_t rtype=0;
					fread(&rtype,sizeof(rtype),1,fp);
					uint32_t anum=0;
					fread(&anum,sizeof(anum),1,fp);
					FklDefFuncType* t=(FklDefFuncType*)malloc(sizeof(FklDefUnionType)+sizeof(FklTypeId_t)*anum);
					FKL_ASSERT(t,__func__);
					t->rtype=rtype;
					t->anum=anum;
					fread(t->atypes,sizeof(FklTypeId_t),anum,fp);
					tu.ft=t;
				}
				break;
			default:
				break;
		}
		ul[i]=tu;
	}
	GlobTypeUnionList.num=num;
	GlobTypeUnionList.ul=ul;
}

void fklFreeGlobTypeList()
{
	FklTypeId_t num=GlobTypeUnionList.num;
	FklDefTypeUnion* ul=GlobTypeUnionList.ul;
	FklTypeId_t i=0;
	for(;i<num;i++)
	{
		FklDefTypeUnion tu=ul[i];
		FklDefTypeTag tag=FKL_GET_TYPES_TAG(tu.all);
		switch(tag)
		{
			case FKL_DEF_NATIVE_TYPE_TAG:
				fklFreeVMNativeType((FklDefNativeType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			case FKL_DEF_PTR_TYPE_TAG:
				fklFreeVMPtrType((FklDefPtrType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			case FKL_DEF_ARRAY_TYPE_TAG:
				fklFreeVMArrayType((FklDefArrayType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			case FKL_DEF_STRUCT_TYPE_TAG:
				fklFreeVMStructType((FklDefStructType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			case FKL_DEF_UNION_TYPE_TAG:
				fklFreeVMUnionType((FklDefUnionType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			case FKL_DEF_FUNC_TYPE_TAG:
				fklFreeVMFuncType((FklDefFuncType*)FKL_GET_TYPES_PTR(tu.all));
				break;
			default:
				break;
		}
	}
	free(ul);
}

int fklIsNativeTypeId(FklTypeId_t type)
{
	return type>0&&type<=LastNativeTypeId;
	//FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	//return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_NATIVE_TYPE_TAG;
}

int fklIsArrayTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_ARRAY_TYPE_TAG;
}

int fklIsPtrTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_PTR_TYPE_TAG;
}

int fklIsStructTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_STRUCT_TYPE_TAG;
}

int fklIsUnionTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_UNION_TYPE_TAG;
}

int fklIsFunctionTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklGetVMTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_FUNC_TYPE_TAG;
}

FklTypeId_t fklGetLastNativeTypeId(void)
{
	return LastNativeTypeId;
}

FklTypeId_t fklGetCharTypeId(void)
{
	return CharTypeId;
}

FklTypeId_t fklGetStringTypeId(void)
{
	return StringTypeId;
}

FklTypeId_t fklGetFILEpTypeId(void)
{
	return FILEpTypeId;
}
