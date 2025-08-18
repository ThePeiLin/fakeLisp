#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>

static void *
fklNastTerminalCreate(const char *s, size_t len, size_t line, void *ctx) {
    FklNastNode *ast = fklCreateNastNode(FKL_NAST_STR, line);
    ast->str = fklCreateString(len, s);
    return ast;
}

static void *prod_action_symbol(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const char *start = "|";
    size_t start_size = 1;
    const char *end = "|";
    size_t end_size = 1;

    const FklNastNode *first = nodes[0].ast;
    const FklString *str = first->str;
    const char *cstr = str->str;
    size_t cstr_size = str->size;

    FklStringBuffer buffer;
    fklInitStringBuffer(&buffer);
    const char *end_cstr = cstr + str->size;
    while (cstr < end_cstr) {
        if (fklCharBufMatch(start, start_size, cstr, cstr_size) >= 0) {
            cstr += start_size;
            cstr_size -= start_size;
            size_t len = fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
            if (!len)
                return 0;
            size_t size = 0;
            char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
            fklStringBufferBincpy(&buffer, s, size);
            fklZfree(s);
            cstr += len;
            cstr_size -= len;
            continue;
        }
        size_t len = 0;
        for (; (cstr + len) < end_cstr; len++)
            if (fklCharBufMatch(start, start_size, cstr + len, cstr_size - len)
                    >= 0)
                break;
        fklStringBufferBincpy(&buffer, cstr, len);
        cstr += len;
        cstr_size -= len;
    }
    FklSid_t id = fklAddSymbolCharBuf(buffer.buf, buffer.index, outerCtx)->v;
    FklNastNode *node = fklCreateNastNode(FKL_NAST_SYM, first->curline);
    node->sym = id;
    fklUninitStringBuffer(&buffer);
    return node;
}

static inline void *prod_action_return_first(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    return fklMakeNastNodeRef(nodes[0].ast);
}

static inline void *prod_action_return_second(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    return fklMakeNastNodeRef(nodes[1].ast);
}

static inline void *prod_action_string(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    size_t start_size = 1;
    size_t end_size = 1;

    const FklNastNode *first = nodes[0].ast;
    const FklString *str = first->str;
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
            str->size - end_size - start_size,
            &size);
    FklNastNode *node = fklCreateNastNode(FKL_NAST_STR, first->curline);
    node->str = fklCreateString(size, s);
    fklZfree(s);
    return node;
}

static inline void *prod_action_nil(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    return fklCreateNastNode(FKL_NAST_NIL, line);
}

static inline void *prod_action_pair(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *car = nodes[0].ast;
    FklNastNode *cdr = nodes[2].ast;
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *prod_action_list(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *car = nodes[0].ast;
    FklNastNode *cdr = nodes[1].ast;
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *prod_action_dec_integer(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0].ast)->str;
    int64_t i = strtoll(str->str, NULL, 10);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithDecCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_hex_integer(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0].ast)->str;
    int64_t i = strtoll(str->str, NULL, 16);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithHexCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_oct_integer(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0].ast)->str;
    int64_t i = strtoll(str->str, NULL, 8);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithOctCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_float(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0].ast)->str;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_F64, line);
    double i = strtod(str->str, NULL);
    r->f64 = i;
    return r;
}

static inline void *prod_action_char(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0].ast)->str;
    if (!fklIsValidCharBuf(str->str + 2, str->size - 2))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_CHR, line);
    r->chr = fklCharBufToChar(str->str + 2, str->size - 2);
    return r;
}

static inline void *prod_action_box(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[1].ast);
    return box;
}

static inline void *prod_action_vector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline FklNastNode *
create_nast_list(FklNastNode *a[], size_t num, uint64_t line) {
    FklNastNode *r = NULL;
    FklNastNode **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateNastNode(FKL_NAST_PAIR, a[i]->curline);
        (*cur)->pair = fklCreateNastPair();
        (*cur)->pair->car = a[i];
        cur = &(*cur)->pair->cdr;
    }
    (*cur) = fklCreateNastNode(FKL_NAST_NIL, line);
    return r;
}

static inline void *prod_action_quote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklSid_t id = fklAddSymbolCstr("quote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_unquote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklSid_t id = fklAddSymbolCstr("unquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_qsquote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklSid_t id = fklAddSymbolCstr("qsquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_unqtesp(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklSid_t id = fklAddSymbolCstr("unqtesp", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_kv_list(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *car = nodes[1].ast;
    FklNastNode *cdr = nodes[3].ast;
    FklNastNode *rest = nodes[5].ast;

    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);

    FklNastNode *r = fklCreateNastNode(FKL_NAST_PAIR, line);
    r->pair = fklCreateNastPair();
    r->pair->car = pair;
    r->pair->cdr = fklMakeNastNodeRef(rest);
    return r;
}

static inline void *prod_action_hasheq(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_hasheqv(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_hashequal(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_bytevector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_BYTEVECTOR, line);
    size_t len = fklNastListLength(list);
    FklBytevector *bv = fklCreateBytevector(len, NULL);
    r->bvec = bv;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        FklNastNode *cur = list->pair->car;
        if (cur->type == FKL_NAST_FIX)
            bv->ptr[i] = cur->fix > UINT8_MAX ? UINT8_MAX
                                              : (cur->fix < 0 ? 0 : cur->fix);
        else
            bv->ptr[i] = cur->bigInt->num < 0 ? 0 : UINT8_MAX;
    }
    return r;
}

static const FklGrammerBuiltinAction builtin_actions[] = {
    { "symbol", prod_action_symbol },
    { "first", prod_action_return_first },
    { "second", prod_action_return_second },
    { "string", prod_action_string },
    { "nil", prod_action_nil },
    { "pair", prod_action_pair },
    { "list", prod_action_list },
    { "dec_integer", prod_action_dec_integer },
    { "hex_integer", prod_action_hex_integer },
    { "oct_integer", prod_action_oct_integer },
    { "float", prod_action_float },
    { "char", prod_action_char },
    { "box", prod_action_box },
    { "vector", prod_action_vector },
    { "quote", prod_action_quote },
    { "unquote", prod_action_unquote },
    { "qsquote", prod_action_qsquote },
    { "unqtesp", prod_action_unqtesp },
    { "pair_list", prod_action_kv_list },
    { "hasheq", prod_action_hasheq },
    { "hasheqv", prod_action_hasheqv },
    { "hashequal", prod_action_hashequal },
    { "bytes", prod_action_bytevector },
    { NULL, NULL },
};

static inline const FklGrammerBuiltinAction *
builtin_prod_action_resolver(void *ctx, const char *str, size_t len) {
    for (const FklGrammerBuiltinAction *cur = &builtin_actions[0]; cur->name;
            ++cur) {
        size_t cur_len = strlen(cur->name);
        if (cur_len == len && memcmp(cur->name, str, cur_len) == 0)
            return cur;
    }
    return NULL;
}

static const char example_grammer_rules[] =
        ""
        "## specials: ??, ?e, ?symbol, ?s-dint, ?s-xint, ?s-oint, ?s-char ...\n"
        "*s-exp* -> *list*       =>  first\n"
        "        -> *vector*     =>  first\n"
        "        -> *box*        =>  first\n"
        "        -> *hasheq*     =>  first\n"
        "        -> *hasheqv*    =>  first\n"
        "        -> *hashequal*  =>  first\n"
        "        -> *quote*      =>  first\n"
        "        -> *qsquote*    =>  first\n"
        "        -> *unquote*    =>  first\n"
        "        -> *unqtesp*    =>  first\n"
        "        -> *symbol*     =>  first\n"
        "        -> *string*     =>  first\n"
        "        -> *bytes*      =>  first\n"
        "        -> *integer*    =>  first\n"
        "        -> *float*      =>  first\n"
        "        -> *char*       =>  first\n"
        "*quote*   -> \"'\" *s-exp*   =>  quote\n"
        "*qsquote* -> \"`\" *s-exp*   =>  qsquote\n"
        "*unquote* -> \"~\" *s-exp*   =>  unquote\n"
        "*unqtesp* -> \"~@\" *s-exp*  =>  unqtesp\n"
        "*symbol* -> ?symbol[\"|\"]       =>  symbol\n"
        "*string* -> /\"\"|\"(\\\\.|.)*\"/    =>  string\n"
        "*bytes*  -> /#\"\"|#\"(\\\\.|.)*\"/  =>  bytes\n"
        "*integer* -> ?s-dint[\"|\"]  =>  dec_integer\n"
        "          -> ?s-xint[\"|\"]  =>  hex_integer\n"
        "          -> ?s-oint[\"|\"]  =>  oct_integer\n"
        "*float* -> ?s-dfloat[\"|\"]  =>  float\n"
        "        -> ?s-xfloat[\"|\"]  =>  float\n"
        "*char* -> ?s-char[\"#\\\\\"]  =>  char\n"
        "*box* -> \"#&\" *s-exp*  =>  box\n"
        "*list* -> \"(\" *list-items* \")\"  => second\n"
        "       -> \"[\" *list-items* \"]\"  => second\n"
        "*list-items* ->                       =>  nil\n"
        "             -> *s-exp* *list-items*  =>  list\n"
        "             -> *s-exp* \",\" *s-exp*   =>  pair\n"
        "*vector* -> \"#(\" *vector-items* \")\"  =>  vector\n"
        "         -> \"#[\" *vector-items* \"]\"  =>  vector\n"
        "*vector-items* ->                         =>  nil\n"
        "               -> *s-exp* *vector-items*  =>  list\n"
        "*hasheq*    -> \"#hash(\"      *hash-items* \")\"  =>  hasheq\n"
        "            -> \"#hash[\"      *hash-items* \"]\"  =>  hasheq\n"
        "*hasheqv*   -> \"#hasheqv(\"   *hash-items* \")\"  =>  hasheqv\n"
        "            -> \"#hasheqv[\"   *hash-items* \"]\"  =>  hasheqv\n"
        "*hashequal* -> \"#hashequal(\" *hash-items* \")\"  =>  hashequal\n"
        "            -> \"#hashequal[\" *hash-items* \"]\"  =>  hashequal\n"
        "*hash-items* ->                                           =>  nil\n"
        "             -> \"(\" *s-exp* \",\" *s-exp* \")\" *hash-items*  =>  pair_list\n"
        "             -> \"[\" *s-exp* \",\" *s-exp* \"]\" *hash-items*  =>  pair_list\n"
        "?e -> /#!.*\\n?/\n"
        "   -> /;.*\\n?/\n"
        "   -> /\\s+/\n"
        "   ;\n"
        "?? -> \"#!\"\n"
        "   -> \";\"\n"
        "   -> \"\\\"\"\n"
        "   -> \"#\\\"\"\n"
        "";

int main() {
    FklSymbolTable *st = fklCreateSymbolTable();

    FklParserGrammerParseArg args;
    FklGrammer *g = fklCreateEmptyGrammer(st);

    fklInitParserGrammerParseArg(&args,
            g,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, example_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        fprintf(stderr, "garmmer create fail\n");
        fklUninitParserGrammerParseArg(&args);
        exit(1);
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        fputs("nonterm: ", stderr);
        fklPrintRawSymbol(fklGetSymbolWithId(nonterm.sid, st), stderr);
        fputs(" is not defined\n", stderr);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        exit(1);
    }

    if (g->sorted_delimiters) {
        fputs("\nterminals:\n", stdout);
        for (size_t i = 0; i < g->sorted_delimiters_num; i++)
            fprintf(stdout, "%s\n", g->sorted_delimiters[i]->str);
        fputc('\n', stdout);
    }
    fputs("grammer:\n", stdout);
    fklPrintGrammer(stdout, g, st);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);

    fputc('\n', stdout);
    fputs("item sets:\n", stdout);
    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(itemSet, g, st, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fklPrintItemStateSet(itemSet, g, st, stdout);
    fklPrintItemStateSetAsDot(itemSet, g, st, lalrgzf);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStringBuffer(&err_msg);
        exit(1);
    }
    fklPrintAnalysisTable(g, st, stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStringBuffer(&err_msg);

    FILE *tablef = fopen("table.txt", "w");
    fklPrintAnalysisTableForGraphEasy(g, st, tablef);

    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

    fputc('\n', stdout);

    const char *exps[] = {
        "#\\\\11",
        "#\\\\z",
        "#\\\\n",
        "#\\\\",
        "#\\;",
        "#\\|",
        "#\\\"",
        "#\\(",
        "#\\\\s",
        "(abcd)abcd",
        ";comments\nabcd",
        "foobar|foo|foobar|bar|",
        "(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",
        "[(foobar;comments\nfoo bar),abcd]",
        "(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)",
        "#hash((a,1) (b,2))",
        "#hashequal((a,1) (b,2))",
        "#vu8(114 514 114514)",
        "114514",
        "#\\ ",
        "'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
        "\"foobar\"",
        "114514",
        NULL,
    };

    int retval = 0;
    FklGrammerMatchOuterCtx outerCtx = {
        .maxNonterminalLen = 0,
        .line = 1,
        .start = NULL,
        .cur = NULL,
        .create = fklNastTerminalCreate,
        .destroy = (void (*)(void *))fklDestroyNastNode,
        .ctx = st,
    };

    for (const char **exp = &exps[0]; *exp; exp++) {
        FklNastNode *ast =
                fklParseWithTableForCstr(g, *exp, &outerCtx, st, &retval);

        if (retval)
            break;

        fklPrintNastNode(ast, stdout, st);
        fklDestroyNastNode(ast);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroySymbolTable(st);
    return retval;
}
